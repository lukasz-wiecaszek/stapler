/*
 * stplr.c
 *
 * Copyright (C) 2022 Lukasz Wiecaszek <lukasz.wiecaszek(at)gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License (in file COPYING) for more details.
 */

//TODO:
// Replace this (pid, tid) tupple by something like connection_id
// Do not call init-deinit msg pages if the caller passes the same memory pointers
// Use inline messages
// Figure out better encoding for a handle
// debugfs
// Priority inheritance
// More, more, more tests

#define pr_fmt(fmt) "stplr: " fmt

#include <linux/types.h>
#include <linux/rbtree_types.h>
#include <linux/mm_types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/printk.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>

#include "stplr.h"

//#define STPLR_DEBUG

#define stplr_dbg_at1(args...) do { if (stplr_debug_level >= 1) pr_info(args); } while (0)
#define stplr_dbg_at2(args...) do { if (stplr_debug_level >= 2) pr_info(args); } while (0)
#define stplr_dbg_at3(args...) do { if (stplr_debug_level >= 3) pr_info(args); } while (0)
#define stplr_dbg_at4(args...) do { if (stplr_debug_level >= 4) pr_info(args); } while (0)

#define STPLR_DEVICE_NAME "stplr"

#define STPLR_VERSION_STR \
	__stringify(STPLR_VERSION_MAJOR) "." \
	__stringify(STPLR_VERSION_MINOR) "." \
	__stringify(STPLR_VERSION_MICRO)

#define STPLR_THREAD_SEND_BUFFER 0
#define STPLR_THREAD_REPLY_BUFFER 1
#define STPLR_THREAD_NUM_OF_BUFFERS 2

/* if stplr_process or stplr_thread does not exist, create one */
#define STPLR_F_CREAT (1U << 0)

/* create stplr_process or stplr_thread only if it does not exist */
#define STPLR_F_EXCL  (1U << 1)

/* increment reference counter for structure being queried */
#define STPLR_F_STRONG_REF (1U << 2)

/* module's params */
static int stplr_debug_level = 0; /* do not emmit any traces by default */
module_param_named(debug, stplr_debug_level, int, 0660);
MODULE_PARM_DESC(debug,
	"Verbosity of debug messages (range: [0(none)-3(max)], default: 0)");

/* number of stapler devices created by this module */
static int stplr_num_of_devices = 1;
module_param_named(devices, stplr_num_of_devices, int, 0660);
MODULE_PARM_DESC(devices,
	"Number of stapler devices created by this module (default: 1)");

/**
 * struct stplr_device - groups device related data structures
 * @hlist:		an element on the 'stplr_devices' list
 * @miscdev:		our character device
 * @processes_lock:	protects @processes rb tree
 * @processes:		root of the rb tree of stplr_process'es
 * @name:
 */
struct stplr_device {
	struct hlist_node hlist;
	struct miscdevice miscdev;
	struct mutex processes_lock;
	struct rb_root processes;
	char name[];
};

/**
 * struct stplr_process - groups process related data structures
 * @pid:		process id
 * @kref:		reference counter
 * @rb_node:		an element on the 'stplr_device::processes' rb tree
 * @dev:		parent stplr_device
 * @threads_lock:	protects @threads rb tree
 * @threads:		root of the rb tree of this process' threads
 */
struct stplr_process {
	pid_t pid;
	struct kref kref;
	struct rb_node rb_node;
	struct stplr_device *dev;
	struct mutex threads_lock;
	struct rb_root threads;
};

/**
 * struct stplr_msg_pages - describes user space message pages
 * @nr_pages:	number of pages used by user space message buffer (msgbuf)
 * @pages:	array of page pointers
 * @size:	size of the user space message buffer (buflen)
 * @offset:	offset into the first page
 * @sgt:	scatter-gather table of user pages
 */
struct stplr_msg_pages {
	int nr_pages;
	struct page **pages;
	__u32 size;
	__u32 offset;
	struct sg_table sgt;
};

/**
 * struct stplr_thread_msg_buffer - thread's message buffer
 * @msgs:	address of the buffer
 * @nmsgs:	number of structs (either 'struct stplr_msg' or
 * 		'struct stplr_msg_pages') stored in the @msgs buffer
 *
 * Thread's message buffer stores @nmsgs 'struct stplr_msg' objects
 * followed by @nmsgs 'struct stplr_msg_pages' objects.
 *
 * -------------------------------------------------------------
 * | struct stplr_msg | ... | struct stplr_msg_pages | ... |
 * -------------------------------------------------------------
 */
struct stplr_thread_msg_buffer {
	void *msgs;
	__u32 nmsgs;
};

/**
 * struct stplr_thread_queue - queue of clients for the receiving thread
 * @lock:	spinlock protecting the list
 * @head:	head of the list
 */
struct stplr_thread_queue {
	spinlock_t lock;
	struct list_head head;
};

/**
 * struct stplr_thread - groups thread related data structures
 * @tid:		thread id
 * @kref:		reference counter
 * @rb_node:		an element on the 'stplr_process::threads' rb tree
 * @parent:		parent stplr_process
 * @zombie:		thread is about to die but others keep reference to it
 * @waiting_for_reply:	whether client thread shall wait for reply
 * @wait:		wait queue
 * @list_node:		an element on the receiving thread queue
 * @queue:		receiving thread queue
 * @buffers:		buffer[0] handles send case,
 * 			buffer[1] handles reply case
 */
struct stplr_thread {
	pid_t tid;
	struct kref kref;
	struct rb_node rb_node;
	struct stplr_process *parent;
	atomic_t zombie;
	bool waiting_for_reply;
	wait_queue_head_t wait;
	struct list_head list_node;
	struct stplr_thread_queue queue;
	struct stplr_thread_msg_buffer buffers[STPLR_THREAD_NUM_OF_BUFFERS];
};

static HLIST_HEAD(stplr_devices);

static struct stplr_thread* stplr_thread_get_locked(struct stplr_process *process, pid_t tid, uint32_t flags)
{
	struct stplr_thread *thread;
	struct rb_node *parent = NULL;
	struct rb_node **p = &process->threads.rb_node;

	while (*p) {
		parent = *p;
		thread = rb_entry(parent, struct stplr_thread, rb_node);

		if (tid < thread->tid)
			p = &(*p)->rb_left;
		else
		if (tid > thread->tid)
			p = &(*p)->rb_right;
		else
		if ((flags & (STPLR_F_CREAT | STPLR_F_EXCL)) == (STPLR_F_CREAT | STPLR_F_EXCL))
			return ERR_PTR(-EBUSY);
		else
		if (flags & STPLR_F_STRONG_REF)
			return atomic_read(&thread->zombie) ?
				NULL : (kref_get(&thread->kref), thread);
		else
			return thread;
	}

	if (!(flags & STPLR_F_CREAT))
		return ERR_PTR(-ENODEV);

	thread = kzalloc(sizeof(*thread), GFP_KERNEL);
	if (!thread)
		return ERR_PTR(-ENOMEM);

	thread->tid = tid;
	thread->parent = process;
	atomic_set(&thread->zombie, 0);
	kref_init(&thread->kref);
	init_waitqueue_head(&thread->wait);
	INIT_LIST_HEAD(&thread->list_node);
	spin_lock_init(&thread->queue.lock);
	INIT_LIST_HEAD(&thread->queue.head);

	rb_link_node(&thread->rb_node, parent, p);
	rb_insert_color(&thread->rb_node, &process->threads);

	stplr_dbg_at3("[%d:%d] stapler thread structure created for thread %d\n",
		current->group_leader->pid, current->pid, thread->tid);

	return thread;
}

static struct stplr_thread* stplr_thread_get(struct stplr_process *process, pid_t tid, uint32_t flags)
{
	struct stplr_thread *thread;

	stplr_dbg_at3("[%d:%d] stplr_thread_get() for tid: %d (%s)\n",
		current->group_leader->pid, current->pid, tid,
		flags & STPLR_F_STRONG_REF ? "strong" : "weak");

	mutex_lock(&process->threads_lock);
	thread = stplr_thread_get_locked(process, tid, flags);
	mutex_unlock(&process->threads_lock);

	return thread;
}

static void stplr_thread_release(struct kref *kref)
{
	struct stplr_thread *thread = container_of(kref, struct stplr_thread, kref);
	struct stplr_process *process = thread->parent;
	pid_t tid = thread->tid;

	rb_erase(&thread->rb_node, &process->threads);
	kfree(thread);

	stplr_dbg_at3("[%d:%d] stapler thread structure released for thread %d\n",
		current->group_leader->pid, current->pid, tid);
}

static void stplr_thread_put_locked(struct stplr_thread *thread)
{
	stplr_dbg_at3("[%d:%d] stplr_thread_put() %d (%u)\n",
		current->group_leader->pid, current->pid,
		thread->tid, kref_read(&thread->kref));

	kref_put(&thread->kref, stplr_thread_release);
}

static void stplr_thread_put(struct stplr_thread *thread)
{
	struct stplr_process *process = thread->parent;

	mutex_lock(&process->threads_lock);
	stplr_thread_put_locked(thread);
	mutex_unlock(&process->threads_lock);
}

static __u32 stplr_thread_get_num_of_msgs(struct stplr_thread *thread, int buffer_id)
{
	return thread->buffers[buffer_id].nmsgs;
}

static struct stplr_msg *stplr_thread_get_msgs(struct stplr_thread *thread, int buffer_id)
{
	return thread->buffers[buffer_id].msgs;
}

static struct stplr_msg_pages *stplr_thread_get_msg_pages(struct stplr_thread *thread, int buffer_id)
{
	return thread->buffers[buffer_id].msgs +
		thread->buffers[buffer_id].nmsgs * sizeof(struct stplr_msg);
}

static bool stplr_thread_queue_has_clients(struct stplr_thread_queue *queue)
{
	bool status;

	spin_lock(&queue->lock);
	status = !list_empty(&queue->head);
	spin_unlock(&queue->lock);

	return status;
}

static struct stplr_process* stplr_process_get_locked(struct stplr_device *dev, pid_t pid, uint32_t flags)
{
	struct stplr_process *process;
	struct rb_node *parent = NULL;
	struct rb_node **p = &dev->processes.rb_node;

	while (*p) {
		parent = *p;
		process = rb_entry(parent, struct stplr_process, rb_node);

		if (pid < process->pid)
			p = &(*p)->rb_left;
		else
		if (pid > process->pid)
			p = &(*p)->rb_right;
		else
		if ((flags & (STPLR_F_CREAT | STPLR_F_EXCL)) == (STPLR_F_CREAT | STPLR_F_EXCL))
			return ERR_PTR(-EBUSY);
		else
		if (flags & STPLR_F_STRONG_REF)
			return kref_get(&process->kref), process;
		else
			return process;
	}

	if (!(flags & STPLR_F_CREAT))
		return ERR_PTR(-ENODEV);

	process = kzalloc(sizeof(*process), GFP_KERNEL);
	if (!process)
		return ERR_PTR(-ENOMEM);

	process->pid = pid;
	process->dev = dev;
	kref_init(&process->kref);
	mutex_init(&process->threads_lock);

	rb_link_node(&process->rb_node, parent, p);
	rb_insert_color(&process->rb_node, &dev->processes);

	stplr_dbg_at3("[%d:%d] stapler process structure created for process %d\n",
		current->group_leader->pid, current->pid, process->pid);

	return process;
}

static struct stplr_process* stplr_process_get(struct stplr_device *dev, pid_t pid, uint32_t flags)
{
	struct stplr_process *process;

	stplr_dbg_at3("[%d:%d] stplr_process_get() for pid: %d (%s)\n",
		current->group_leader->pid, current->pid, pid,
		flags & STPLR_F_STRONG_REF ? "strong" : "weak");

	mutex_lock(&dev->processes_lock);
	process = stplr_process_get_locked(dev, pid, flags);
	mutex_unlock(&dev->processes_lock);

	return process;
}

static void stplr_process_release(struct kref *kref)
{
	struct stplr_process *process = container_of(kref, struct stplr_process, kref);
	struct stplr_device *dev = process->dev;
	pid_t pid = process->pid;

	WARN_ON(!RB_EMPTY_ROOT(&process->threads));

	rb_erase(&process->rb_node, &dev->processes);
	kfree(process);

	stplr_dbg_at3("[%d:%d] stapler process structure released for process %d\n",
		current->group_leader->pid, current->pid, pid);
}

static void stplr_process_put_locked(struct stplr_process *process)
{
	stplr_dbg_at3("[%d:%d] stplr_process_put() %d (%u)\n",
		current->group_leader->pid, current->pid,
		process->pid, kref_read(&process->kref));

	kref_put(&process->kref, stplr_process_release);
}

static void stplr_process_put(struct stplr_process *process)
{
	struct stplr_device *dev = process->dev;

	mutex_lock(&dev->processes_lock);
	stplr_process_put_locked(process);
	mutex_unlock(&dev->processes_lock);
}

static int stplr_thread_to_handle(struct stplr_process *process, const struct stplr_thread *thread, struct stplr_handle *handle)
{
	handle->uuid = thread->tid; //TODO: Not sure this is ok.
	return 0;
}

static int stplr_handle_to_thread(struct stplr_process *process, const struct stplr_handle *handle, struct stplr_thread **thread)
{
	struct stplr_thread *t;
	pid_t tid = handle->uuid;

	t = stplr_thread_get(process, tid, 0);
	if (IS_ERR(t))
		return PTR_ERR(t);

	if (current->pid != t->tid)
		return -EBADR;

	*thread = t;

	return 0;
}

static int stplr_open(struct inode *inode, struct file *file)
{
	struct stplr_device *dev;
	struct stplr_process *process;

	stplr_dbg_at3("[%d:%d] %s()\n",
		current->group_leader->pid, current->pid, __func__);

	dev = container_of(file->private_data, struct stplr_device, miscdev);
	process = stplr_process_get(dev, current->group_leader->pid, STPLR_F_CREAT | STPLR_F_EXCL);
	if (IS_ERR(process))
		return PTR_ERR(process);

	file->private_data = process;

	return 0;
}

static int stplr_flush(struct file *file, fl_owner_t id)
{
	struct stplr_process *process;
	struct rb_node *node, *next;

	stplr_dbg_at3("[%d:%d] %s()\n",
		current->group_leader->pid, current->pid, __func__);

	process = file->private_data;

	mutex_lock(&process->threads_lock);

	for (node = rb_first(&process->threads); node != NULL; node = next) {
		struct stplr_thread *thread;
		bool is_zombie;

		next = rb_next(node);
		thread = rb_entry(node, struct stplr_thread, rb_node);
		is_zombie = atomic_read(&thread->zombie);
		stplr_dbg_at3("[%d:%d] flusing tid: %d is_zombie: %s\n",
			current->group_leader->pid, current->pid,
			thread->tid, is_zombie ? "true" : "false");
		if (!is_zombie)
			stplr_thread_put_locked(thread);
	}

	mutex_unlock(&process->threads_lock);

	return 0;
}

static int stplr_release(struct inode *inode, struct file *file)
{
	struct stplr_process *process;

	stplr_dbg_at3("[%d:%d] %s()\n",
		current->group_leader->pid, current->pid, __func__);

	process = file->private_data;
	stplr_process_put(process);

	return 0;
}

size_t stplr_copy_buffers(struct sg_table *dst, struct sg_table *src)
{
	struct sg_mapping_iter dst_miter;
	struct sg_mapping_iter src_miter;
	size_t len;
	size_t dst_offset = 0;
	size_t src_offset = 0;
	size_t count = 0;

	sg_miter_start(&dst_miter, dst->sgl, dst->nents, SG_MITER_TO_SG);
	sg_miter_start(&src_miter, src->sgl, src->nents, SG_MITER_FROM_SG);

	while ((dst_offset < dst_miter.length || (dst_offset = 0, sg_miter_next(&dst_miter))) &&
		(src_offset < src_miter.length || (src_offset = 0, sg_miter_next(&src_miter)))) {
		len = min(
			dst_miter.length - dst_offset,
			src_miter.length - src_offset);

		stplr_dbg_at4("[%d:%d] dst_miter.length: %zu, dst_offset: %zu\n",
			current->group_leader->pid, current->pid,
			dst_miter.length, dst_offset);
		stplr_dbg_at4("[%d:%d] src_miter.length: %zu, src_offset: %zu\n",
			current->group_leader->pid, current->pid,
			src_miter.length, src_offset);

		memcpy(
			dst_miter.addr + dst_offset,
			src_miter.addr + src_offset,
			len);
		count += len;
		dst_offset += len;
		src_offset += len;
	}

	sg_miter_stop(&src_miter);
	sg_miter_stop(&dst_miter);

	stplr_dbg_at3("[%d:%d] stplr_copy_buffers: count: %zu\n",
		current->group_leader->pid, current->pid, count);

	return count;
}

#if defined(STPLR_DEBUG)
static void stplr_print_buffer(struct sg_table *sgt)
{
	struct sg_mapping_iter miter;
	unsigned int sg_flags = SG_MITER_FROM_SG;

	stplr_dbg_at3("sgt->nents: %u\n", sgt->nents);

	sg_miter_start(&miter, sgt->sgl, sgt->nents, sg_flags);

	while (sg_miter_next(&miter)) {
		stplr_dbg_at4("addr: %px\n", miter.addr);
		stplr_dbg_at4("length: %zu\n", miter.length);
		stplr_dbg_at4("consumed: %zu\n", miter.consumed);
		stplr_dbg_at4("__offset: %u\n", miter.__offset);
		stplr_dbg_at4("__remaining: %u\n", miter.__remaining);
	}

	sg_miter_stop(&miter);
}
#endif

static int stplr_get_user_pages(const struct stplr_msg *msg, struct stplr_msg_pages *msg_pages)
{
	int status;
	__u64 msgbufaddr;
	unsigned long first, last;

	/* Calculate number of pages */
	msgbufaddr = (__u64)msg->msgbuf;
	first = (msgbufaddr & PAGE_MASK) >> PAGE_SHIFT;
	last = ((msgbufaddr + msg->buflen - 1) & PAGE_MASK) >> PAGE_SHIFT;

	/* Get number of pages used by user space message buffer (msgbuf) */
	msg_pages->nr_pages = last - first + 1;

	/* Allocate array of page pointers */
	msg_pages->pages = kmalloc_array(msg_pages->nr_pages, sizeof(struct page*), GFP_KERNEL);
	if (!msg_pages->pages)
		return -ENOMEM;

	/* Get offset into first page */
	msg_pages->offset = offset_in_page(msg->msgbuf);

	/* Copy size of the message */
	msg_pages->size = msg->buflen;

	status = get_user_pages_fast(msgbufaddr & PAGE_MASK, msg_pages->nr_pages, FOLL_WRITE /* gup_flags */, msg_pages->pages);
	if (status < 0) {
		stplr_dbg_at1("[%d:%d] failed to get user pages (nr_pages: %d)\n",
			current->group_leader->pid, current->pid,
			msg_pages->nr_pages);
		kfree(msg_pages->pages);
		return status;
	}

	msg_pages->nr_pages = status;

	stplr_dbg_at3("[%d:%d] size: %u, offset: %u, pinned_pages: %d\n",
		current->group_leader->pid, current->pid,
		msg_pages->size, msg_pages->offset, msg_pages->nr_pages);
	for (int i = 0; i < msg_pages->nr_pages; i++)
		stplr_dbg_at4("[%d:%d] pfn: %lu\n",
			current->group_leader->pid, current->pid,
			page_to_pfn(msg_pages->pages[i]));

	status = sg_alloc_table_from_pages(&msg_pages->sgt, msg_pages->pages, msg_pages->nr_pages, msg_pages->offset, msg_pages->size, GFP_KERNEL);
	if (status) {
		stplr_dbg_at1("[%d:%d] failed to initialize sg table (nr_pages: %d, size: %u)\n",
			current->group_leader->pid, current->pid,
			msg_pages->nr_pages, msg_pages->size);
		/* Release lock on all pages */
		for (int i = 0; i < msg_pages->nr_pages; i++)
			put_page(msg_pages->pages[i]);
		kfree(msg_pages->pages);
		return status;
	}

#if defined(STPLR_DEBUG)
	stplr_print_buffer(&msg_pages->sgt);
#endif

	return 0;
}

static void stplr_put_user_pages(struct stplr_msg_pages *msg_pages)
{
	sg_free_table(&msg_pages->sgt);
	/* Release lock on all pages */
	for (int i = 0; i < msg_pages->nr_pages; i++)
		put_page(msg_pages->pages[i]);
	kfree(msg_pages->pages);
}

static int stplr_thread_init_msgs(struct stplr_thread *thread, const struct stplr_msgs *msgs, int buffer_id)
{
	int ret = -EFAULT;
	struct stplr_msg *msg;
	struct stplr_msg_pages *msg_pages;
	struct stplr_thread_msg_buffer *buffer;
	__u32 n;

	buffer = &thread->buffers[buffer_id];

	BUG_ON(buffer->msgs);
	BUG_ON(buffer->nmsgs);

	buffer->nmsgs = msgs->count;
	buffer->msgs = kzalloc(buffer->nmsgs * (sizeof(struct stplr_msg) + sizeof(struct stplr_msg_pages)), GFP_KERNEL);
	if (!buffer->msgs)
		return -ENOMEM;

	if (copy_from_user(buffer->msgs, msgs->msgs, buffer->nmsgs * sizeof(struct stplr_msg)))
		return -EFAULT;

	msg = stplr_thread_get_msgs(thread, buffer_id);
	msg_pages = stplr_thread_get_msg_pages(thread, buffer_id);

	for (n = 0; n < buffer->nmsgs; n++) {
		int status;

		status = stplr_get_user_pages(&msg[n], &msg_pages[n]);
		if (status) {
			ret = status;
			break;
		}
	}

	if (n < buffer->nmsgs) {
		while (n-- > 0)
			stplr_put_user_pages(&msg_pages[n]);
		return ret;
	}

	return 0;
}

static void stplr_thread_deinit_msgs(struct stplr_thread *thread, int buffer_id)
{
	struct stplr_msg_pages *msg_pages;
	struct stplr_thread_msg_buffer *buffer;
	__u32 n;

	buffer = &thread->buffers[buffer_id];
	msg_pages = stplr_thread_get_msg_pages(thread, buffer_id);

	for (n = 0; n < buffer->nmsgs; n++)
		stplr_put_user_pages(&msg_pages[n]);

	kfree(buffer->msgs);
	buffer->msgs = NULL;
	buffer->nmsgs = 0;
}

static long stplr_ioctl_version(void __user *ubuf, size_t size)
{
	struct stplr_version version;

	if (size != sizeof(struct stplr_version))
		return -EINVAL;

	memset(&version, 0, sizeof(version));
	version.major = STPLR_VERSION_MAJOR;
	version.minor = STPLR_VERSION_MINOR;
	version.micro = STPLR_VERSION_MICRO;

	if (copy_to_user(ubuf, &version, sizeof(struct stplr_version)))
		return -EINVAL;

	return 0;
}

static long stplr_ioctl_handle_get(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	struct stplr_thread *lthread;
	struct stplr_handle handle;
	pid_t tid = current->pid;

	if (size != sizeof(struct stplr_handle))
		return -EINVAL;

	lthread = stplr_thread_get(lprocess, tid, STPLR_F_CREAT | STPLR_F_EXCL);
	if (IS_ERR(lthread))
		return PTR_ERR(lthread);

	if (stplr_thread_to_handle(lprocess, lthread, &handle))
		return -EBADE;

	if (copy_to_user(ubuf, &handle, sizeof(struct stplr_handle)))
		return -EFAULT;

	return 0;
}

static long stplr_ioctl_handle_put(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	struct stplr_thread *lthread;
	struct stplr_handle handle;

	if (size != sizeof(struct stplr_handle))
		return -EINVAL;

	if (copy_from_user(&handle, ubuf, sizeof(struct stplr_handle)))
		return -EFAULT;

	if (stplr_handle_to_thread(lprocess, &handle, &lthread))
		return -EBADE;

	atomic_set(&lthread->zombie, 1);
	stplr_thread_put(lthread);

	return 0;
}

static long stplr_ioctl_msg_send(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	int ret = -EFAULT;
	struct stplr_device *dev = lprocess->dev;
	struct stplr_msg_send msg_send;
	struct stplr_thread *lthread;
	struct stplr_process *rprocess;
	struct stplr_thread *rthread;
	struct stplr_msg_pages *lmsg_pages;
	__u32 lnmsgs;
	__u32 n;

	if (size != sizeof(struct stplr_msg_send))
		return -EINVAL;

	if (copy_from_user(&msg_send, ubuf, sizeof(msg_send)))
		return -EFAULT;

	ret = stplr_handle_to_thread(lprocess, &msg_send.handle, &lthread);
	if (ret)
		return ret;

	stplr_dbg_at3("[%d:%d] send to %d:%d\n",
		current->group_leader->pid, current->pid,
		msg_send.pid, msg_send.tid);

	rprocess = stplr_process_get(dev, msg_send.pid, STPLR_F_STRONG_REF);
	if (IS_ERR(rprocess)) {
		stplr_dbg_at1("[%d:%d] cannot find process with pid %d\n",
			current->group_leader->pid, current->pid,
			msg_send.pid);
		ret = PTR_ERR(rprocess);
		goto out1;
	}

	rthread = stplr_thread_get(rprocess, msg_send.tid, STPLR_F_STRONG_REF);
	if (IS_ERR(rthread)) {
		stplr_dbg_at1("[%d:%d] cannot find thread with tid %d\n",
			current->group_leader->pid, current->pid,
			msg_send.tid);
		ret = PTR_ERR(rthread);
		goto out2;
	}

	ret = stplr_thread_init_msgs(lthread, &msg_send.smsgs, STPLR_THREAD_SEND_BUFFER);
	if (ret) {
		stplr_dbg_at1("[%d:%d] stplr_thread_init_msgs() failed\n",
			current->group_leader->pid, current->pid);
		goto out3;
	}

	lthread->waiting_for_reply = false;

	spin_lock(&rthread->queue.lock);
	list_add_tail(&lthread->list_node, &rthread->queue.head);
	spin_unlock(&rthread->queue.lock);
	wake_up(&rthread->wait);

	ret = wait_event_interruptible(lthread->wait, list_empty(&lthread->list_node));
	if (ret) {
		stplr_dbg_at1("[%d:%d] wait_event_interruptible() failed with code %d\n",
			current->group_leader->pid, current->pid, ret);
		goto out4;
	}

	/* gather number of actually copied bytes in send buffers (needed to pass to user space) */
	/* real copying happened in receiving (remote) thread */
	lmsg_pages = stplr_thread_get_msg_pages(lthread, STPLR_THREAD_SEND_BUFFER);
	lnmsgs = stplr_thread_get_num_of_msgs(lthread, STPLR_THREAD_SEND_BUFFER);

	for (n = 0; n < lnmsgs; n++)
		put_user(lmsg_pages[n].size, (__u32 __user *)&msg_send.smsgs.msgs[n].buflen);

out4:
	stplr_thread_deinit_msgs(lthread, STPLR_THREAD_SEND_BUFFER);

out3:
	stplr_thread_put(rthread);

out2:
	stplr_process_put(rprocess);

out1:
	return ret;
}

static long stplr_ioctl_msg_send_receive(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	int ret = -EFAULT;
	struct stplr_device *dev = lprocess->dev;
	struct stplr_msg_send_receive msg_send_receive;
	struct stplr_thread *lthread;
	struct stplr_process *rprocess;
	struct stplr_thread *rthread;
	struct stplr_msg_pages *lmsg_pages;
	struct stplr_msg_pages *rmsg_pages;
	__u32 lnmsgs;
	__u32 rnmsgs;
	__u32 nmsgs;
	__u32 n;

	if (size != sizeof(struct stplr_msg_send_receive))
		return -EINVAL;

	if (copy_from_user(&msg_send_receive, ubuf, sizeof(msg_send_receive)))
		return -EFAULT;

	ret = stplr_handle_to_thread(lprocess, &msg_send_receive.handle, &lthread);
	if (ret)
		return ret;

	stplr_dbg_at3("[%d:%d] send to %d:%d\n",
		current->group_leader->pid, current->pid,
		msg_send_receive.pid, msg_send_receive.tid);

	rprocess = stplr_process_get(dev, msg_send_receive.pid, STPLR_F_STRONG_REF);
	if (IS_ERR(rprocess)) {
		stplr_dbg_at1("[%d:%d] cannot find process with pid %d\n",
			current->group_leader->pid, current->pid,
			msg_send_receive.pid);
		ret = PTR_ERR(rprocess);
		goto out1;
	}

	rthread = stplr_thread_get(rprocess, msg_send_receive.tid, STPLR_F_STRONG_REF);
	if (IS_ERR(rthread)) {
		stplr_dbg_at1("[%d:%d] cannot find thread with tid %d\n",
			current->group_leader->pid, current->pid,
			msg_send_receive.tid);
		ret = PTR_ERR(rthread);
		goto out2;
	}

	ret = stplr_thread_init_msgs(lthread, &msg_send_receive.smsgs, STPLR_THREAD_SEND_BUFFER);
	if (ret) {
		stplr_dbg_at1("[%d:%d] stplr_thread_init_msgs() failed\n",
			current->group_leader->pid, current->pid);
		goto out3;
	}

	ret = stplr_thread_init_msgs(lthread, &msg_send_receive.rmsgs, STPLR_THREAD_REPLY_BUFFER);
	if (ret) {
		stplr_dbg_at1("[%d:%d] stplr_thread_init_msgs() failed\n",
			current->group_leader->pid, current->pid);
		goto out4;
	}

	lthread->waiting_for_reply = true;

	spin_lock(&rthread->queue.lock);
	list_add_tail(&lthread->list_node, &rthread->queue.head);
	spin_unlock(&rthread->queue.lock);
	wake_up(&rthread->wait);

	ret = wait_event_interruptible(lthread->wait, list_empty(&lthread->list_node) && (lthread->waiting_for_reply == false));
	if (ret) {
		stplr_dbg_at1("[%d:%d] wait_event_interruptible() failed with code %d\n",
			current->group_leader->pid, current->pid, ret);
		goto out5;
	}

	/* gather number of actually copied bytes in send buffers (needed to pass to user space) */
	/* real copying happened in receiving (remote) thread */
	lmsg_pages = stplr_thread_get_msg_pages(lthread, STPLR_THREAD_SEND_BUFFER);
	lnmsgs = stplr_thread_get_num_of_msgs(lthread, STPLR_THREAD_SEND_BUFFER);

	for (n = 0; n < lnmsgs; n++)
		put_user(lmsg_pages[n].size, (__u32 __user *)&msg_send_receive.smsgs.msgs[n].buflen);

	/* here copying of reply buffers will take place */
	lmsg_pages = stplr_thread_get_msg_pages(lthread, STPLR_THREAD_REPLY_BUFFER);
	lnmsgs = stplr_thread_get_num_of_msgs(lthread, STPLR_THREAD_REPLY_BUFFER);
	rmsg_pages = stplr_thread_get_msg_pages(rthread, STPLR_THREAD_REPLY_BUFFER);
	rnmsgs = stplr_thread_get_num_of_msgs(rthread, STPLR_THREAD_REPLY_BUFFER);

	nmsgs = min(lnmsgs, rnmsgs);
	for (n = 0; n < nmsgs; n++)
		lmsg_pages[n].size =
		rmsg_pages[n].size =
			stplr_copy_buffers(
				&lmsg_pages[n].sgt, &rmsg_pages[n].sgt);

	if (nmsgs == rnmsgs)
		for (; n < lnmsgs; n++)
			lmsg_pages[n].size = 0;
	else
		for (; n < rnmsgs; n++)
			rmsg_pages[n].size = 0;

	for (n = 0; n < lnmsgs; n++)
		put_user(lmsg_pages[n].size, (__u32 __user *)&msg_send_receive.rmsgs.msgs[n].buflen);

	rthread->waiting_for_reply = false;

	/* finally wake up replying thread */
	wake_up(&rthread->wait);

out5:
	stplr_thread_deinit_msgs(lthread, STPLR_THREAD_REPLY_BUFFER);

out4:
	stplr_thread_deinit_msgs(lthread, STPLR_THREAD_SEND_BUFFER);

out3:
	stplr_thread_put(rthread);

out2:
	stplr_process_put(rprocess);

out1:
	return ret;
}

static long stplr_ioctl_msg_receive(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	int ret = -EFAULT;
	struct stplr_msg_receive msg_receive;
	struct stplr_thread *lthread;
	struct stplr_thread *rthread;
	struct stplr_msg_pages *lmsg_pages;
	struct stplr_msg_pages *rmsg_pages;
	__u32 lnmsgs;
	__u32 rnmsgs;
	__u32 nmsgs;
	__u32 n;

	if (size != sizeof(struct stplr_msg_receive))
		return -EINVAL;

	if (copy_from_user(&msg_receive, ubuf, sizeof(msg_receive)))
		return -EFAULT;

	ret = stplr_handle_to_thread(lprocess, &msg_receive.handle, &lthread);
	if (ret)
		return ret;

	ret = stplr_thread_init_msgs(lthread, &msg_receive.rmsgs, STPLR_THREAD_SEND_BUFFER);
	if (ret) {
		stplr_dbg_at1("[%d:%d] stplr_thread_init_msgs() failed\n",
			current->group_leader->pid, current->pid);
		return ret;
	}

	ret = wait_event_interruptible(lthread->wait, stplr_thread_queue_has_clients(&lthread->queue));
	if (ret) {
		stplr_dbg_at1("[%d:%d] wait_event_interruptible() failed with code %d\n",
			current->group_leader->pid, current->pid, ret);
		goto out1;
	}

	/* pick the first client from the list */
	spin_lock(&lthread->queue.lock);
	rthread = list_first_entry(&lthread->queue.head, struct stplr_thread, list_node);
	spin_unlock(&lthread->queue.lock);

	/* here copying of send buffers will take place */
	lmsg_pages = stplr_thread_get_msg_pages(lthread, STPLR_THREAD_SEND_BUFFER);
	lnmsgs = stplr_thread_get_num_of_msgs(lthread, STPLR_THREAD_SEND_BUFFER);
	rmsg_pages = stplr_thread_get_msg_pages(rthread, STPLR_THREAD_SEND_BUFFER);
	rnmsgs = stplr_thread_get_num_of_msgs(rthread, STPLR_THREAD_SEND_BUFFER);

	nmsgs = min(lnmsgs, rnmsgs);
	for (n = 0; n < nmsgs; n++)
		lmsg_pages[n].size =
		rmsg_pages[n].size =
			stplr_copy_buffers(
				&lmsg_pages[n].sgt, &rmsg_pages[n].sgt);

	if (nmsgs == rnmsgs)
		for (; n < lnmsgs; n++)
			lmsg_pages[n].size = 0;
	else
		for (; n < rnmsgs; n++)
			rmsg_pages[n].size = 0;

	for (n = 0; n < lnmsgs; n++)
		put_user(lmsg_pages[n].size, (__u32 __user *)&msg_receive.rmsgs.msgs[n].buflen);

	put_user(rthread->parent->pid, (__u32 __user *)&(((struct stplr_msg_receive*)ubuf)->pid));
	put_user(rthread->tid, (__u32 __user *)&(((struct stplr_msg_receive*)ubuf)->tid));
	put_user(rthread->waiting_for_reply, (__u32 __user *)&(((struct stplr_msg_receive*)ubuf)->reply_required));

	list_del_init(&rthread->list_node);

	/*
	 * Wake up client thread only if reply is not needed.
	 * In case reply is needed client thread will be woken up
	 * from STPLR_MSG_REPLY ioctl().
	 */
	if (!rthread->waiting_for_reply)
		wake_up(&rthread->wait);

out1:
	stplr_thread_deinit_msgs(lthread, STPLR_THREAD_SEND_BUFFER);

	return ret;
}

static long stplr_ioctl_msg_reply(struct stplr_process *lprocess, void __user *ubuf, size_t size)
{
	int ret = -EFAULT;
	struct stplr_device *dev = lprocess->dev;
	struct stplr_msg_reply msg_reply;
	struct stplr_thread *lthread;
	struct stplr_process *rprocess;
	struct stplr_thread *rthread;
	struct stplr_msg_pages *lmsg_pages;
	__u32 lnmsgs;
	__u32 n;

	if (size != sizeof(struct stplr_msg_reply))
		return -EINVAL;

	if (copy_from_user(&msg_reply, ubuf, sizeof(msg_reply)))
		return -EFAULT;

	ret = stplr_handle_to_thread(lprocess, &msg_reply.handle, &lthread);
	if (ret)
		return ret;

	stplr_dbg_at3("[%d:%d] reply to %d:%d\n",
		current->group_leader->pid, current->pid,
		msg_reply.pid, msg_reply.tid);

	rprocess = stplr_process_get(dev, msg_reply.pid, STPLR_F_STRONG_REF);
	if (IS_ERR(rprocess)) {
		stplr_dbg_at1("[%d:%d] cannot find process with pid %d\n",
			current->group_leader->pid, current->pid,
			msg_reply.pid);
		ret = PTR_ERR(rprocess);
		goto out1;
	}

	rthread = stplr_thread_get(rprocess, msg_reply.tid, STPLR_F_STRONG_REF);
	if (IS_ERR(rthread)) {
		stplr_dbg_at1("[%d:%d] cannot find thread with tid %d\n",
			current->group_leader->pid, current->pid,
			msg_reply.tid);
		ret = PTR_ERR(rthread);
		goto out2;
	}

	ret = stplr_thread_init_msgs(lthread, &msg_reply.rmsgs, STPLR_THREAD_REPLY_BUFFER);
	if (ret) {
		stplr_dbg_at1("[%d:%d] stplr_thread_init_msgs() failed\n",
			current->group_leader->pid, current->pid);
		goto out3;
	}

	lthread->waiting_for_reply = true;
	rthread->waiting_for_reply = false;
	wake_up(&rthread->wait);

	ret = wait_event_interruptible(lthread->wait, lthread->waiting_for_reply == false);
	if (ret) {
		stplr_dbg_at1("[%d:%d] wait_event_interruptible() failed with code %d\n",
			current->group_leader->pid, current->pid, ret);
		goto out4;
	}

	lmsg_pages = stplr_thread_get_msg_pages(lthread, STPLR_THREAD_REPLY_BUFFER);
	lnmsgs = stplr_thread_get_num_of_msgs(lthread, STPLR_THREAD_REPLY_BUFFER);

	for (n = 0; n < lnmsgs; n++)
		put_user(lmsg_pages[n].size, (__u32 __user *)&msg_reply.rmsgs.msgs[n].buflen);

out4:
	stplr_thread_deinit_msgs(lthread, STPLR_THREAD_REPLY_BUFFER);

out3:
	stplr_thread_put(rthread);

out2:
	stplr_process_put(rprocess);

out1:
	return ret;
}

static long stplr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct stplr_process *process;
	size_t size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	stplr_dbg_at3("[%d:%d] %s() cmd (enter): %u '%s'\n",
		current->group_leader->pid, current->pid,
		__func__, cmd, stplr_cmd_to_string(cmd));

	process = file->private_data;

	switch (cmd) {
	case STPLR_VERSION:
		ret = stplr_ioctl_version(ubuf, size);
		break;
	case STPLR_HANDLE_GET:
		ret = stplr_ioctl_handle_get(process, ubuf, size);
		break;
	case STPLR_HANDLE_PUT:
		ret = stplr_ioctl_handle_put(process, ubuf, size);
		break;
	case STPLR_MSG_SEND:
		ret = stplr_ioctl_msg_send(process, ubuf, size);
		break;
	case STPLR_MSG_SEND_RECEIVE:
		ret = stplr_ioctl_msg_send_receive(process, ubuf, size);
		break;
	case STPLR_MSG_RECEIVE:
		ret = stplr_ioctl_msg_receive(process, ubuf, size);
		break;
	case STPLR_MSG_REPLY:
		ret = stplr_ioctl_msg_reply(process, ubuf, size);
		break;
	default:
		msleep(1000); /* deliberately sleep for 1 second */
		ret = -EINVAL;
		break;
	}

	stplr_dbg_at3("[%d:%d] %s() cmd (exit): %u '%s'\n",
		current->group_leader->pid, current->pid,
		__func__, cmd, stplr_cmd_to_string(cmd));

	return ret;
}

const struct file_operations stplr_fops = {
	.owner = THIS_MODULE,
	.open = stplr_open,
	.flush = stplr_flush,
	.release = stplr_release,
	.unlocked_ioctl = stplr_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static void stplr_free_devices(void)
{
	struct stplr_device *dev;
	struct hlist_node *node;

	hlist_for_each_entry_safe(dev, node, &stplr_devices, hlist) {
		misc_deregister(&dev->miscdev);
		hlist_del(&dev->hlist);
		stplr_dbg_at1("'%s' device destroyed\n", dev->name);

		kfree(dev);
	}
}

static int __init stplr_init_device(int device_id)
{
	int status;
	int name_len1, name_len2;
	struct stplr_device *dev;

	name_len1 = snprintf(NULL, 0, STPLR_DEVICE_NAME"-%d", device_id);

	dev = kzalloc(offsetof(struct stplr_device, name) + name_len1, GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	name_len2 = snprintf(dev->name, name_len1 + 1, STPLR_DEVICE_NAME"-%d", device_id);

	if (name_len1 != name_len2) {
		pr_err("different return values (%d, %d) from snprintf()\n", name_len1, name_len2);
		kfree(dev);
		return -EFAULT;
	}

	dev->miscdev.fops = &stplr_fops;
	dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dev->miscdev.name = dev->name;
	status = misc_register(&dev->miscdev);
	if (status < 0) {
		pr_err("misc_register(%s) failed with code %d\n", dev->name, status);
		kfree(dev);
		return status;
	}

	mutex_init(&dev->processes_lock);
	dev->processes.rb_node = NULL;

	hlist_add_head(&dev->hlist, &stplr_devices);
	stplr_dbg_at1("'%s' device created\n", dev->name);

	return 0;
}

static int __init stplr_init(void)
{
	int i;
	int status;

	for (i = 0; i < stplr_num_of_devices; i++) {
		status = stplr_init_device(i);
		if (status)
			goto out1;
	}

	pr_info("module loaded (version: %s)\n", STPLR_VERSION_STR);
	return 0;

out1:
	stplr_free_devices();
	return status;
}
module_init(stplr_init);

#ifdef MODULE
static void __exit stplr_exit(void)
{
	stplr_free_devices();
	pr_info("module removed\n");
}
module_exit(stplr_exit);
#endif

MODULE_DESCRIPTION("stapler driver");
MODULE_AUTHOR("Lukasz Wiecaszek <lukasz.wiecaszek(at)gmail.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(STPLR_VERSION_STR);

/* SPDX-License-Identifier: MIT */
/**
 * @file server.c
 *
 * Small application showing basic usage of the stapler api (server side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <pthread.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "common.h"
#include "../../stplr.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
static int debug_level = 3;

#define dbg_at1(args...) do { if (debug_level >= 1) fprintf(stderr, args); } while (0)
#define dbg_at2(args...) do { if (debug_level >= 2) fprintf(stdout, args); } while (0)
#define dbg_at3(args...) do { if (debug_level >= 3) fprintf(stdout, args); } while (0)

#define NUM_THREADS 1

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/
struct thread_args {
    int       thread_num;
    pthread_t thread_id;
    int       fd;
};

/*===========================================================================*\
 * local (internal linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * global (external linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) functions definitions
\*===========================================================================*/
static void p(const char *prefix, const struct stplr_msg *msg)
{
    dbg_at3("[%d] %s: %p [%4u, %u page(s) offset %u]\n",
        gettid(), prefix, msg->msgbuf, msg->buflen, DIV_ROUND_UP(msg->buflen, PAGE_SIZE), msg->buflen % PAGE_SIZE);
}

static int msg_receive(int fd, int thread_num, const struct stplr_handle *handle, int* pid, int* tid, int *reply_required)
{
    int i, j;

    char buf1[1]; /* 1 page on stack */
    const uint32_t buf1len = sizeof(buf1);
    memset(buf1, 0, buf1len);

    static __thread char buf2[3]; /* 1 page on ... 'thread local storage' */
    const uint32_t buf2len = sizeof(buf2);
    memset(buf2, 0, buf2len);

    static char buf3container[NUM_THREADS][1 * PAGE_SIZE + 1];
    char* buf3 = (char*)&buf3container[thread_num]; /* 2 pages on ... 'static storage duration' */
    const uint32_t buf3len = sizeof(buf3container[thread_num]);
    memset(buf3, 0, buf3len);

    const uint32_t buf4len = 2 * PAGE_SIZE + 1;
    char* buf4 = malloc(buf4len); /* 3 pages on heap */
    if (!buf4) {
        dbg_at1("malloc(%u) failed\n", buf4len);
        exit(EXIT_FAILURE);
    }
    memset(buf4, 0, buf4len);

    struct stplr_msg msgs[] = {
        {.msgbuf = buf1, .buflen = buf1len},
        {.msgbuf = buf2, .buflen = buf2len},
        {.msgbuf = buf3, .buflen = buf3len},
        {.msgbuf = buf4, .buflen = buf4len},
    };

    p("buf1", &msgs[0]);
    p("buf2", &msgs[1]);
    p("buf3", &msgs[2]);
    p("buf4", &msgs[3]);

    struct stplr_msg_receive msg_receive = {};
    msg_receive.handle = *handle;
    msg_receive.rmsgs.msgs = msgs;
    msg_receive.rmsgs.count = sizeof(msgs)/sizeof(msgs[0]);

    dbg_at3("[%d] waiting for a messages ...\n", gettid());

    int ret = ioctl(fd, STPLR_MSG_RECEIVE, &msg_receive);
    if (ret < 0)
        dbg_at1("ioctl(STPLR_MSG_RECEIVE) failed with code %d : %s\n", errno, strerror(errno));
    else
        dbg_at3("ioctl(STPLR_MSG_RECEIVE) returned %d\n", ret);

    if (ret < 0) {
        free(buf4);
        return ret;
    }

    dbg_at3("[%d] received %u message(s) from pid: %d, tid: %d, reply_required: %d\n",
        gettid(), msg_receive.rmsgs.count, msg_receive.pid, msg_receive.tid, msg_receive.reply_required);

    for (i = 0; i < msg_receive.rmsgs.count; i++) {
        uint8_t *p = msg_receive.rmsgs.msgs[i].msgbuf;
        dbg_at3("message #%d size: %u '%s' ", i, msg_receive.rmsgs.msgs[i].buflen, (char*)p);
        for (j = 0; j < msg_receive.rmsgs.msgs[i].buflen; j++) {
            dbg_at3("0x%02x ", p[j]);
        }
        dbg_at3("\n");
    }

    if (pid)
        *pid = msg_receive.pid;

    if (tid)
        *tid = msg_receive.tid;

    if (reply_required)
        *reply_required = msg_receive.reply_required;

    free(buf4);

    return 0;
}

static int msg_reply(int fd, int thread_num, const struct stplr_handle *handle, int pid, int tid)
{
    int i;

    char buf1[1]; /* 1 page on stack */
    const uint32_t buf1len = sizeof(buf1);
    memset(buf1, 0, buf1len);

    static __thread char buf2[3]; /* 1 page on ... 'thread local storage' */
    const uint32_t buf2len = sizeof(buf2);
    memset(buf2, 0, buf2len);

    static char buf3[1 * PAGE_SIZE + 1]; /* 2 pages on ... 'static storage duration' */
    const uint32_t buf3len = sizeof(buf3);
    memset(buf3, 0, buf3len);

    const uint32_t buf4len = 2 * PAGE_SIZE + 1;
    char *buf4 = malloc(buf4len); /* 3 pages on heap */
    if (!buf4) {
        dbg_at1("malloc(%u) failed\n", buf4len);
        exit(EXIT_FAILURE);
    }
    memset(buf4, 0, buf4len);

    struct stplr_msg msgs[] = {
        {.msgbuf = buf1, .buflen = buf1len},
        {.msgbuf = buf2, .buflen = buf2len},
        {.msgbuf = buf3, .buflen = buf3len},
        {.msgbuf = buf4, .buflen = buf4len},
    };

    p("buf1", &msgs[0]);
    p("buf2", &msgs[1]);
    p("buf3", &msgs[2]);
    p("buf4", &msgs[3]);

    struct stplr_msg_reply msg_reply = {};
    msg_reply.handle = *handle;
    msg_reply.pid = pid;
    msg_reply.tid = tid;
    msg_reply.rmsgs.msgs = msgs;
    msg_reply.rmsgs.count = sizeof(msgs)/sizeof(msgs[0]);

    dbg_at3("replying to pid: %d, tid: %d\n", pid, tid);

    int ret = ioctl(fd, STPLR_MSG_REPLY, &msg_reply);
    if (ret < 0)
        dbg_at1("ioctl(STPLR_MSG_REPLY) failed with code %d : %s\n", errno, strerror(errno));
    else
        dbg_at3("ioctl(STPLR_MSG_REPLY) returned %d\n", ret);

    if (ret < 0) {
        free(buf4);
        return ret;
    }

    for (i = 0; i < msg_reply.rmsgs.count; i++)
        dbg_at3("message #%d consumed %u bytes\n", i, msg_reply.rmsgs.msgs[i].buflen);

    free(buf4);

    return 0;
}

static void* server_function(void *ptr)
{
    int status;
    int pid, tid;
    int reply_required;
    struct stplr_handle handle;
    const struct thread_args *args = (const struct thread_args *)ptr;

    dbg_at2("starting thread %d\n", gettid());

    status = ioctl(args->fd, STPLR_HANDLE_GET, &handle);
    if (status < 0) {
        dbg_at1("ioctl(STPLR_HANDLE_GET) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (;;) {
        int ret = msg_receive(args->fd, args->thread_num, &handle, &pid, &tid, &reply_required);
        if (ret != 0)
            break;
        dbg_at3("reply_required: %d\n", reply_required);
        if (reply_required)
            msg_reply(args->fd, args->thread_num, &handle, pid, tid);
    }

    status = ioctl(args->fd, STPLR_HANDLE_PUT, &handle);
    if (status < 0) {
        dbg_at1("ioctl(STPLR_HANDLE_PUT) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    dbg_at2("terminating thread %d\n", gettid());
}

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    int fd;
    int i;
    int c;
    int status;
    struct stplr_version version;
    struct thread_args thread_args[NUM_THREADS];
    pthread_attr_t thread_attrs;

    static struct option long_options[] = {
        {"pid", required_argument, 0, 'p'},
        {"tid", required_argument, 0, 't'},
        {"verbose", required_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    for (;;) {
        c = getopt_long(argc, argv, "v:", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'v':
                debug_level = atoi(optarg);
                break;
        }
    }

    dbg_at2("server's pid: %d\n", getpid());

    fd = open(STPLR_DEVICENAME, O_RDWR);
    assert(fd >= -1);
    if (fd == -1) {
        dbg_at1("cannot open '%s': %s\n",
            STPLR_DEVICENAME, strerror(errno));
        exit(EXIT_FAILURE);
    }

    status = ioctl(fd, STPLR_VERSION, &version);
    if (status < 0) {
        dbg_at1("ioctl(STPLR_VERSION) failed with code %d : %s\n",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    dbg_at2("version: %d.%d.%d\n", version.major, version.minor, version.micro);

    if (version.major != STPLR_VERSION_MAJOR) {
        dbg_at1("incompatible kernel module/header major version (%d/%d)\n",
            version.major, STPLR_VERSION_MAJOR);
        exit(EXIT_FAILURE);
    }

    status = pthread_attr_init(&thread_attrs);
    if (status != 0) {
        dbg_at1("pthread_attr_init() failed with code %d : %s\n",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        thread_args[i].thread_num = i;
        thread_args[i].fd = fd;
        status = pthread_create(&thread_args[i].thread_id, &thread_attrs, server_function, &thread_args[i]);
        if (status != 0) {
            dbg_at1("pthread_create() failed with code %d : %s\n",
                errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < NUM_THREADS; i++) {
        status = pthread_join(thread_args[i].thread_id, NULL);
        if (status != 0) {
            dbg_at1("pthread_join() failed with code %d : %s\n",
                errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    status = pthread_attr_destroy(&thread_attrs);
    if (status != 0) {
        dbg_at1("pthread_attr_destroy() failed with code %d : %s\n",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    close(fd);

    return 0;
}

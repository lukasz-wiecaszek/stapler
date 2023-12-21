/*
 * stplr.h
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

#ifndef _UAPI_LINUX_STPLR_H_
#define _UAPI_LINUX_STPLR_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define STPLR_VERSION_MAJOR 0
#define STPLR_VERSION_MINOR 0
#define STPLR_VERSION_MICRO 7

/**
 * struct stplr_version - used by STPLR_VERSION ioctl
 * @major:	major version of the driver
 * @minor:	minor version of the driver
 * @micro:	micro version of the driver
 *
 * The structure is used to query current version of the stapler driver.
 * Major version will change with every incompatible API change.
 * Minor version will change with compatible API changes.
 * Micro version is for micro changes ;-).
 */
struct stplr_version {
	__s32 major;
	__s32 minor;
	__s32 micro;
};

/**
 * struct stplr_handle - acquired by STPLR_HANDLE_GET and
 *                       released by STPLR_HANDLE_PUT ioctls
 * @uuid:	opaque unique handle identifier
 *
 * struct stplr_handle is required in the following ioctls:
 * - STPLR_MSG_SEND
 * - STPLR_MSG_SEND_RECEIVE
 * - STPLR_MSG_RECEIVE
 * - STPLR_MSG_REPLY
 *
 * So to use those ioctls the caller first needs to acquire
 * a handle (STPLR_HANDLE_GET), and once they finished with them,
 * the handle shall be released (STPLR_HANDLE_PUT).
 */
struct stplr_handle {
	__u64 uuid;
	//__u8 uuid[16];
};

/**
 * struct stplr_msg - describes one ipc message buffer
 * @msgbuf:	starting address of the message buffer
 * @buflen:	size of the message buffer pointed to by @msgbuf
 */
struct stplr_msg {
	void *msgbuf;
	__u32 buflen;
};

/**
 * struct stplr_msgs - describes an array of ipc message buffers
 * @msgs:	pointer to array of ipc message buffers
 * @count:	number of elements in the array @msgs
 */
struct stplr_msgs {
	const struct stplr_msg* msgs;
	__u32 count;
};

/**
 * struct stplr_msg_send - used by STPLR_MSG_SEND ioctl
 * @handle:	ipc handle (acquired by STPLR_HANDLE_GET)
 * @pid:	process id of the process to send the message(s) to
 * @tid:	thread id of the thread to send the message(s) to
 * @smsgs:	an array of message buffers to be sent (on return @buflen
 * 		fields will contain actual number of copied bytes)
 *
 * STPLR_MSG_SEND attempts to copy specified message(s) to the thread
 * which waits for it/them using STPLR_MSG_RECEIVE.
 * The sending thread becomes blocked waiting for a receving thread
 * to accept/read the message(s).
 * Once the receiving thread will read the message(s), the sending thread
 * will be unblocked, passing back the number of actually copied bytes
 * (which will be the minimum of that specified by both the sender
 * and the receiver).
 *
 * The send data will not overflow the receive buffer area provided
 * by the receiver.
 */
struct stplr_msg_send {
	struct stplr_handle handle;
	struct {
		pid_t pid;
		pid_t tid;
		struct stplr_msgs smsgs;
	};
};

/**
 * struct stplr_msg_send_receive - used by STPLR_MSG_SEND_RECEIVE ioctl
 * @handle:	ipc handle (acquired by STPLR_HANDLE_GET)
 * @pid:	process id of the process to send the message(s) to
 * @tid:	thread id of the thread to send the message(s) to
 * @smsgs:	an array of message buffers to be sent (on return @buflen
 * 		fields will contain actual number of copied bytes)
 * @rmsgs:	an array of message buffers to be filled by replying
 * 		thread
 *
 * STPLR_MSG_SEND attempts to copy specified message(s) to the thread
 * which waits for it/them using STPLR_MSG_RECEIVE.
 * The sending thread becomes blocked waiting for a receving thread
 * not only to accept/read the message(s) but also to reply to it/them
 * by STPLR_MSG_REPLY. Once the receiving thread will reply, the sending
 * thread will be unblocked, passing back:
 * - in @smsgs the number of actually copied bytes (which will be the
 *   minimum of that specified by both the sender and the receiver).
 * - in @rmsgs the reply message(s)
 *
 * The send data will not overflow the receive buffer area provided
 * by the receiver.
 * The reply data will not overflow the reply buffer area provided
 * by the sender.
 */
struct stplr_msg_send_receive {
	struct stplr_handle handle;
	struct {
		pid_t pid;
		pid_t tid;
		struct stplr_msgs smsgs;
		struct stplr_msgs rmsgs;
	};
};

/**
 * struct stplr_msg_receive - used by STPLR_MSG_RECEIVE ioctl
 * @handle:		ipc handle (acquired by STPLR_HANDLE_GET)
 * @pid:		process id of the sender process
 * @tid:		thread id of the sender thread
 * @reply_required:	1 if we shall reply to this message via STPLR_MSG_REPLY,
 * 			0 if the reply shall not be sent
 * @rmsgs:		an array of message buffers to be filled by sender
 * 			message(s)
 *
 * If the receiving thread (the thread which invokes STPLR_MSG_RECEIVE)
 * has already senders waiting to copy its message(s), the first sender
 * is picked from the list and its message(s) is/are transfered into
 * address space of the receiving thread immediately.
 * If there are no senders ready to transfer its message(s),
 * the receiving thread becomes blocked waiting for such sender.
 *
 * The number of bytes transferred is the minimum of that specified
 * by both the sender and the receiver. The send data will not overflow
 * the receive buffer area provided by the receiver.
 */
struct stplr_msg_receive {
	struct stplr_handle handle;
	struct {
		pid_t pid;
		pid_t tid;
		int reply_required;
		struct stplr_msgs rmsgs;
	};
};

/**
 * struct stplr_msg_reply - used by STPLR_MSG_REPLY ioctl
 * @handle:	ipc handle (acquired by STPLR_HANDLE_GET)
 * @pid:	process id of the process to reply the message(s) to
 *		(taken from struct stplr_msg_receive)
 * @tid:	thread id of the thread to reply the message(s) to
 * 		(taken from struct stplr_msg_receive)
 * @rmsgs:	an array of messages you will reply with (on return @buflen
 * 		fields will contain actual number of copied bytes)
 *
 * STPLR_MSG_REPLY attempts to copy specified message(s) to the thread
 * which waits for it/them using STPLR_MSG_SEND_RECEIVE.
 * The replying thread becomes blocked waiting for a thread
 * which issued STPLR_MSG_SEND_RECEIVE (sender) to accept/read the message(s).
 * Once the sender thread will read the message(s), the sending thread
 * will be unblocked, passing back the number of actually copied bytes.
 *
 * The reply data will not overflow the reply buffer area provided
 * by the sender.
 */
struct stplr_msg_reply {
	struct stplr_handle handle;
	struct {
		pid_t pid;
		pid_t tid;
		struct stplr_msgs rmsgs;
	};
};

#define STPLR_MAGIC 'i'
#define STPLR_IO(nr)		_IO(STPLR_MAGIC, nr)
#define STPLR_IOR(nr, type)	_IOR(STPLR_MAGIC, nr, type)
#define STPLR_IOW(nr, type)	_IOW(STPLR_MAGIC, nr, type)
#define STPLR_IOWR(nr, type)	_IOWR(STPLR_MAGIC, nr, type)

#define STPLR_VERSION		STPLR_IOR (42, struct stplr_version)
#define STPLR_HANDLE_GET	STPLR_IOR (43, struct stplr_handle)
#define STPLR_HANDLE_PUT	STPLR_IOW (44, struct stplr_handle)
#define STPLR_MSG_SEND		STPLR_IOWR(45, struct stplr_msg_send)
#define STPLR_MSG_SEND_RECEIVE	STPLR_IOWR(46, struct stplr_msg_send_receive)
#define STPLR_MSG_RECEIVE	STPLR_IOWR(47, struct stplr_msg_receive)
#define STPLR_MSG_REPLY		STPLR_IOWR(48, struct stplr_msg_reply)

static inline const char *stplr_cmd_to_string(size_t cmd)
{
	switch (cmd) {
	case STPLR_VERSION:
		return "STPLR_VERSION";
	case STPLR_HANDLE_GET:
		return "STPLR_HANDLE_GET";
	case STPLR_HANDLE_PUT:
		return "STPLR_HANDLE_PUT";
	case STPLR_MSG_SEND:
		return "STPLR_MSG_SEND";
	case STPLR_MSG_SEND_RECEIVE:
		return "STPLR_MSG_SEND_RECEIVE";
	case STPLR_MSG_RECEIVE:
		return "STPLR_MSG_RECEIVE";
	case STPLR_MSG_REPLY:
		return "STPLR_MSG_REPLY";
	default:
		return "STPLR_UNRECOGNIZED_COMMAND";
	}
}

#endif /* _UAPI_LINUX_STPLR_H_ */

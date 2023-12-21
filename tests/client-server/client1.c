/* SPDX-License-Identifier: MIT */
/**
 * @file client1.c
 *
 * Small application showing basic usage of the stapler api (client side).
 * This client uses STPLR_MSG_SEND ioctl to communicate with the server.
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
#include <time.h>

#include <sys/ioctl.h>

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

#define NUM_OF_REPETITIONS 1000

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/

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
    dbg_at3("%s: %p [%4u, %u page(s) offset %u]\n",
        prefix, msg->msgbuf, msg->buflen, DIV_ROUND_UP(msg->buflen, PAGE_SIZE), msg->buflen % PAGE_SIZE);
}

static int send_message(int fd, const struct stplr_handle *handle, int pid, int tid)
{
    int i;
    int ret;

    char buf1[3] = {'a', 'b', 'c'};
    const uint32_t buf1len = sizeof(buf1);

    static char __thread buf2[3] = {'a', 'b', 'c'};
    const uint32_t buf2len = sizeof(buf2);

    static char buf3[5] = {'1', '2', '3', '4', '5'};
    const uint32_t buf3len = sizeof(buf3);

    const uint32_t buf4len = 7;
    char *buf4 = malloc(buf4len);
    if (!buf4) {
        dbg_at1("malloc(%u) failed\n", buf4len);
        exit(EXIT_FAILURE);
    }
    memcpy(buf4, "ABCDEF", MIN(buf4len, sizeof("ABCDEF")));

    struct stplr_msg smsgs[] = {
        {.msgbuf = buf1, .buflen = buf1len},
        {.msgbuf = buf2, .buflen = buf2len},
        {.msgbuf = buf3, .buflen = buf3len},
        {.msgbuf = buf4, .buflen = buf4len},
    };

    p("sbuf1", &smsgs[0]);
    p("sbuf2", &smsgs[1]);
    p("sbuf3", &smsgs[2]);
    p("sbuf4", &smsgs[3]);

    struct stplr_msg_send msg_send;
    msg_send.handle = *handle;
    msg_send.pid = pid;
    msg_send.tid = tid;
    msg_send.smsgs.msgs = smsgs;
    msg_send.smsgs.count = sizeof(smsgs)/sizeof(smsgs[0]);

    ret = ioctl(fd, STPLR_MSG_SEND, &msg_send);
    if (ret < 0)
        dbg_at1("ioctl() failed with code %d : %s\n", errno, strerror(errno));
    else
        dbg_at3("ioctl() returned %d\n", ret);

    if (ret < 0) {
        free(buf4);
        return ret;
    }

    for (i = 0; i < msg_send.smsgs.count; i++)
        dbg_at3("send message #%d consumed %u bytes\n", i, msg_send.smsgs.msgs[i].buflen);

    free(buf4);
}

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    int fd;
    int c;
    int i;
    int pid = -1;
    int tid = -1;
    int ret;
    int status;
    struct stplr_version version;
    struct stplr_handle handle;
    struct timespec t1, t2;
    uint64_t microseconds;

    dbg_at2("pid: %d, tid: %d\n", getpid(), gettid());

    static struct option long_options[] = {
        {"pid", required_argument, 0, 'p'},
        {"tid", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    for (;;) {
        c = getopt_long(argc, argv, "p:t:", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 't':
                tid = atoi(optarg);
                break;
            case 'v':
                debug_level = atoi(optarg);
                break;
        }
    }

    fd = open(STPLR_DEVICENAME, O_RDWR);
    assert(fd >= -1);
    if (fd == -1) {
        dbg_at1("cannot open '%s': %s\n",
            STPLR_DEVICENAME, strerror(errno));
        exit(EXIT_FAILURE);
    }

    ret = ioctl(fd, STPLR_VERSION, &version);
    if (ret < 0) {
        dbg_at1("ioctl(STPLR_VERSION) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    dbg_at2("version: %d.%d.%d\n", version.major, version.minor, version.micro);

    if (version.major != STPLR_VERSION_MAJOR) {
        dbg_at1("incompatible kernel module/header major version (%d/%d)\n",
            version.major, STPLR_VERSION_MAJOR);
        exit(EXIT_FAILURE);
    }

    ret = ioctl(fd, STPLR_HANDLE_GET, &handle);
    if (ret < 0) {
        dbg_at1("ioctl(STPLR_HANDLE_GET) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (int i = 0; i < NUM_OF_REPETITIONS; i++) {
        status = send_message(fd, &handle, pid, tid);
        if (status != 0) {
            dbg_at1("Test failed\n");
            exit(EXIT_FAILURE);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    dbg_at2("Test took %lu microseconds\n", microseconds);

    //dbg_at2("press any key to continue ...\n");
    //getchar();

    ret = ioctl(fd, STPLR_HANDLE_PUT, &handle);
    if (ret < 0) {
        dbg_at1("ioctl(STPLR_HANDLE_PUT) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    close(fd);

    return 0;
}

/* SPDX-License-Identifier: MIT */
/**
 * @file timer-server.c
 *
 * Implementation of the D-Bus 'timer' interface (server side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <sys/epoll.h>

#include <dbus/dbus.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "timer-common.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define TIMER_SIGNAL_DBUS     SIGRTMIN + 0
#define TIMER_SIGNAL_PROTOCOL SIGRTMIN + 1

#define TIMER_USE_ASYNCHRONOUS_LOOP

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) objects definitions
\*===========================================================================*/
static uint64_t timer_counter;
static int epollfd;

/*===========================================================================*\
 * global (external linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) functions definitions
\*===========================================================================*/
static int timer_method_name(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    char* result = TIMER_INTERFACE_NAME;
    dbus_uint32_t serial = 0;

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // add the arguments to the reply
    if (!dbus_message_append_args(reply,
        DBUS_TYPE_STRING, &result,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // send the reply
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "dbus_connection_send() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // flush the connection
    dbus_connection_flush(conn);

    // free the reply
    dbus_message_unref(reply);

    return 0;
}

static int timer_method_version(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_uint32_t serial = 0;
    struct version version = {
        TIMER_INTERFACE_VERSION_MAJOR,
        TIMER_INTERFACE_VERSION_MINOR,
        TIMER_INTERFACE_VERSION_MICRO
    };

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // fill the reply message
    if (timer_message_fill_version(reply, &version) == FALSE) {
        fprintf(stderr, "timer_message_fill_version() failed\n");
        // free message
        dbus_message_unref(reply);
        return -1;
    }

    // send the reply
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "dbus_connection_send() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // flush the connection
    dbus_connection_flush(conn);

    // free the reply
    dbus_message_unref(reply);

    return 0;
}

static int timer_method_start(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_uint64_t interval;
    dbus_uint64_t retval;
    dbus_uint32_t serial = 0;
    sigset_t mask;
    struct sigevent sigevent;
    struct itimerspec itimerspec;
    timer_t timerid;

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_UINT64, &interval,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }

    // create the timer
    sigevent.sigev_notify = SIGEV_SIGNAL;
    sigevent.sigev_signo = TIMER_SIGNAL_PROTOCOL;
    sigevent.sigev_value.sival_ptr = conn;
    if (timer_create(CLOCK_MONOTONIC, &sigevent, &timerid) == -1) {
        fprintf(stderr, "timer_create(CLOCK_MONOTONIC) failed with code %d : %s\n",
            errno, strerror(errno));
        return -1;
    }

    // start the timer
    itimerspec.it_value.tv_sec = interval / 1000000;
    itimerspec.it_value.tv_nsec = (interval % 1000000) * 1000;
    itimerspec.it_interval.tv_sec = itimerspec.it_value.tv_sec;
    itimerspec.it_interval.tv_nsec = itimerspec.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &itimerspec, NULL) == -1) {
        fprintf(stderr, "timer_settime() failed with code %d : %s\n",
            errno, strerror(errno));
        return -1;
    }

    // unblock the timer signal, so that timer notification can be delivered
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL_PROTOCOL);
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        fprintf(stderr, "sigprocmask(SIG_UNBLOCK) failed with code %d : %s\n", errno, strerror(errno));
        return -1;
    }

    timer_counter = 0;

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // add the arguments to the reply
    retval = (dbus_uint64_t)timerid;
    if (!dbus_message_append_args(reply,
        DBUS_TYPE_UINT64, &retval,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // send the reply
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "dbus_connection_send() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // flush the connection
    dbus_connection_flush(conn);

    // free the reply
    dbus_message_unref(reply);

    return 0;
}

static int timer_method_stop(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_uint64_t arg;
    dbus_uint32_t serial = 0;
    sigset_t mask;
    timer_t timerid;

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_UINT64, &arg,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }
    timerid = (timer_t)arg;

    if (timer_delete(timerid) == -1) {
        fprintf(stderr, "timer_delete() failed with code %d : %s\n",
            errno, strerror(errno));
        return -1;
    }

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // add the arguments to the reply
    if (!dbus_message_append_args(reply,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // send the reply
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "dbus_connection_send() failed\n");
        // free the reply
        dbus_message_unref(reply);
        return -1;
    }

    // flush the connection
    dbus_connection_flush(conn);

    // free the reply
    dbus_message_unref(reply);

    return 0;
}

static int timer_signal_tick(DBusConnection* conn, dbus_uint64_t counter, dbus_uint64_t abstime)
{
    DBusMessage* msg;
    DBusMessageIter iter;
    DBusMessageIter iter_struct;
    dbus_uint32_t serial = 0;

    // create a signal and check for errors
    msg = dbus_message_new_signal(
        TIMER_OBJECT_PATH,     // object name of the signal
        TIMER_INTERFACE_NAME,  // interface name of the signal
        TIMER_SIGNAL_TICK);    // name of the signal
    if (msg == NULL) {
        fprintf(stderr, "dbus_message_new_signal(%s, %s, %s) failed\n",
            TIMER_OBJECT_PATH,
            TIMER_INTERFACE_NAME,
            TIMER_SIGNAL_TICK);
        return -1;
    }

    // append arguments onto signal
    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &iter_struct);

    dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_UINT64, &counter);
    dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_UINT64, &abstime);

    dbus_message_iter_close_container(&iter, &iter_struct);

   // broadcast the signal
    if (!dbus_connection_send(conn, msg, &serial)) {
        fprintf(stderr, "dbus_connection_send() failed\n");
        // free the message
        dbus_message_unref(msg);
        return -1;
    }

    // and flush the connection
    dbus_connection_flush(conn);

    // free the message
    dbus_message_unref(msg);

    return 0;
}

#if defined(TIMER_USE_ASYNCHRONOUS_LOOP)
static void timer_unregister_func(DBusConnection *conn, void *data)
{
    fprintf(stdout, "%s()\n", __func__);
}

static DBusHandlerResult timer_message_func(DBusConnection *conn, DBusMessage *msg, void *data)
{
    //fprintf(stdout, "got dbus message sent to %s::%s\n",
    //    dbus_message_get_interface(msg), dbus_message_get_member(msg));

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(dbus_message_get_interface(msg), TIMER_INTERFACE_NAME))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(dbus_message_get_member(msg), TIMER_METHOD_NAME) == 0)
        timer_method_name(conn, msg);
    else
    if (strcmp(dbus_message_get_member(msg), TIMER_METHOD_VERSION) == 0)
        timer_method_version(conn, msg);
    else
    if (strcmp(dbus_message_get_member(msg), TIMER_METHOD_START) == 0)
        timer_method_start(conn, msg);
    else
    if (strcmp(dbus_message_get_member(msg), TIMER_METHOD_STOP) == 0)
        timer_method_stop(conn, msg);
    else
        /* do nothing */;

    return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable timer_dbus_vtable = {
    .unregister_function = timer_unregister_func,
    .message_function = timer_message_func,
};

static void timer_signal_handler_dbus(int sig, siginfo_t *si, void *uc)
{
    DBusTimeout *timeout = (DBusTimeout*)si->si_value.sival_ptr;
    dbus_timeout_handle(timeout);
}
#endif

static void timer_signal_handler_protocol(int sig, siginfo_t *si, void *uc)
{
    DBusConnection* conn = (DBusConnection*)si->si_value.sival_ptr;
    struct timespec ts;
    uint64_t abstime;

    timer_counter++;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0)
        abstime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    else
        abstime = 0;

    // i have mixed emotions sending dbus messages from within signal handler
    // but on the other hand this is only an test/example piece of software, so ...
    timer_signal_tick(conn, timer_counter, abstime);
}

#if defined(TIMER_USE_ASYNCHRONOUS_LOOP)
static dbus_bool_t timer_watch_add(DBusWatch *watch, void *data)
{
    struct epoll_event ev;
    unsigned int flags;
    uint32_t events;
    int fd;

    // if watch is not enabled,
    // it should not be polled by the main loop
    if (!dbus_watch_get_enabled(watch))
        return TRUE;

    events = EPOLLPRI;
    flags = dbus_watch_get_flags(watch);
    if (flags & DBUS_WATCH_READABLE)
        events |= EPOLLIN;
    if (flags & DBUS_WATCH_WRITABLE)
        events |= EPOLLOUT;

    fd = dbus_watch_get_unix_fd(watch);

    ev.events = events;
    ev.data.ptr = watch;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        fprintf(stderr, "epoll_ctl(EPOLL_CTL_ADD) failed with code %d : %s\n", errno, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static void timer_watch_remove(DBusWatch *watch, void *data)
{
    int fd;

    fd = dbus_watch_get_unix_fd(watch);

    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) == -1)
        fprintf(stderr, "epoll_ctl(EPOLL_CTL_DEL) failed with code %d : %s\n", errno, strerror(errno));
}

static void timer_watch_toggled(DBusWatch *watch, void *data)
{
    // enable/disable is no different from add/remove
    if (dbus_watch_get_enabled(watch))
        timer_watch_add(watch, data);
    else
        timer_watch_remove(watch, data);
}

static dbus_bool_t timer_timeout_add(DBusTimeout *timeout, void *data)
{
    struct sigevent sigevent;
    struct itimerspec itimerspec;
    sigset_t mask;
    timer_t timerid;
    int interval_ms;

    // if timeout is not enabled,
    // it should not be polled by the main loop
    if (!dbus_timeout_get_enabled(timeout))
        return TRUE;

    interval_ms = dbus_timeout_get_interval(timeout);

    // create the timer
    sigevent.sigev_notify = SIGEV_SIGNAL;
    sigevent.sigev_signo = TIMER_SIGNAL_DBUS;
    sigevent.sigev_value.sival_ptr = timeout;
    if (timer_create(CLOCK_MONOTONIC, &sigevent, &timerid) == -1) {
        fprintf(stderr, "timer_create(CLOCK_MONOTONIC) failed with code %d : %s\n",
            errno, strerror(errno));
        return FALSE;
    }

    // start the timer
    itimerspec.it_value.tv_sec = interval_ms / 1000;
    itimerspec.it_value.tv_nsec = (interval_ms % 1000) * 1000000;
    itimerspec.it_interval.tv_sec = itimerspec.it_value.tv_sec;
    itimerspec.it_interval.tv_nsec = itimerspec.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &itimerspec, NULL) == -1) {
        fprintf(stderr, "timer_settime() failed with code %d : %s\n",
            errno, strerror(errno));
        return FALSE;
    }

    // unblock the timer signal, so that timer notification can be delivered
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL_DBUS);
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        fprintf(stderr, "sigprocmask(SIG_UNBLOCK) failed with code %d : %s\n", errno, strerror(errno));
        return FALSE;
    }

    dbus_timeout_set_data(timeout, timerid, NULL);

    return TRUE;
}

static void timer_timeout_remove(DBusTimeout *timeout, void *data)
{
    timer_t timerid;

    timerid = dbus_timeout_get_data(timeout);

    if (timer_delete(timerid) == -1)
        fprintf(stderr, "timer_delete() failed with code %d : %s\n",
            errno, strerror(errno));

    dbus_timeout_set_data(timeout, NULL, NULL);
}

static void timer_timeout_toggled(DBusTimeout *timeout, void *data)
{
    // enable/disable is no different from add/remove
    if (dbus_timeout_get_enabled(timeout))
        timer_timeout_add(timeout, data);
    else
        timer_timeout_remove(timeout, data);
}

static void timer_handle_dispatch_status(DBusConnection *conn, DBusDispatchStatus status, void *data)
{
    /* Is there anything to be done here? */
}

static void timer_loop(DBusConnection* conn)
{
    struct epoll_event events[32];
    int nfds;
    int n;

    fprintf(stdout, "Server started\n");

    for (;;) {
        nfds = epoll_wait(epollfd, events, ARRAY_SIZE(events), -1);
        if (nfds == -1) {
            if (errno == EINTR)
                continue; // if you play with signals you might be interrupted
            fprintf(stderr, "epoll_wait() failed with code %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; n++) {
            unsigned int flags = 0;
            dbus_bool_t status;

            if (events[n].events & EPOLLIN)
                flags |= DBUS_WATCH_READABLE;
            if (events[n].events & EPOLLOUT)
                flags |= DBUS_WATCH_WRITABLE;

            status = dbus_watch_handle(events[n].data.ptr, flags);
            if (status == FALSE) {
                fprintf(stderr, "dbus_watch_handle() failed\n");
            }

            while (dbus_connection_get_dispatch_status(conn) == DBUS_DISPATCH_DATA_REMAINS)
                dbus_connection_dispatch(conn);
        }
    }
}
#else
static void timer_loop(DBusConnection* conn)
{
    DBusMessage* msg;

    fprintf(stdout, "Server started\n");

    // loop, testing for new messages
    for (;;) {
        // block until message from client is received
        if (!dbus_connection_read_write_dispatch(conn, -1)) {
            fprintf(stderr, "dbus_connection_read_write_dispatch() failed\n");
            exit(EXIT_FAILURE);
        }

        msg = dbus_connection_pop_message(conn);

        // loop again if we haven't got a message
        // this shall not happen as dbus_connection_read_write_dispatch()
        // blocks until it can read or write, but, who knows
        if (msg == NULL)
            continue;

        // check this is a method call for the right interface & method
        if (dbus_message_is_method_call(msg, TIMER_INTERFACE_NAME, TIMER_METHOD_NAME))
            timer_method_name(conn, msg);
        else
        if (dbus_message_is_method_call(msg, TIMER_INTERFACE_NAME, TIMER_METHOD_VERSION))
            timer_method_version(conn, msg);
        else
        if (dbus_message_is_method_call(msg, TIMER_INTERFACE_NAME, TIMER_METHOD_START))
            timer_method_start(conn, msg);
        else
        if (dbus_message_is_method_call(msg, TIMER_INTERFACE_NAME, TIMER_METHOD_STOP))
            timer_method_stop(conn, msg);
        else
            /* do nothing */;

        // free the message
        dbus_message_unref(msg);
    }
}
#endif

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    DBusConnection* conn;
    DBusError dbus_error;
    struct sigaction sa;
    sigset_t mask;
    int status;

    fprintf(stdout, "Starting 'timer' server ...\n");

#if defined(TIMER_USE_ASYNCHRONOUS_LOOP)
    // setup the handler for dbus related timer signal
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timer_signal_handler_dbus;
    sigemptyset(&sa.sa_mask);
    if (sigaction(TIMER_SIGNAL_DBUS, &sa, NULL) == -1) {
        fprintf(stderr, "sigaction(%d) failed with code %d : %s\n",
            TIMER_SIGNAL_DBUS, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // block timer signal until it is started
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL_DBUS);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        fprintf(stderr, "sigprocmask(SIG_SETMASK) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
#endif

    // setup the handler for protocol related timer signal
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timer_signal_handler_protocol;
    sigemptyset(&sa.sa_mask);
    if (sigaction(TIMER_SIGNAL_PROTOCOL, &sa, NULL) == -1) {
        fprintf(stderr, "sigaction(%d) failed with code %d : %s\n",
            TIMER_SIGNAL_PROTOCOL, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // block timer signal until it is started
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL_PROTOCOL);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        fprintf(stderr, "sigprocmask(SIG_SETMASK) failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // initialise the error value
    dbus_error_init(&dbus_error);

    // connect to the DBUS session bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_error);
    if (conn == NULL) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_get(DBUS_BUS_SESSION) failed (%s:%s)\n",
                dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    // request well-known name on the bus and check for errors
    status = dbus_bus_request_name(conn, TIMER_WELL_KNOWN_NAME_SERVER,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                TIMER_WELL_KNOWN_NAME_SERVER, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    dbus_connection_set_exit_on_disconnect(conn, FALSE);

#if defined(TIMER_USE_ASYNCHRONOUS_LOOP)
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        fprintf(stderr, "epoll_create1() failed with code %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    dbus_connection_set_dispatch_status_function(conn,
        timer_handle_dispatch_status, NULL, NULL);

    status = dbus_connection_set_watch_functions(conn,
        timer_watch_add, timer_watch_remove, timer_watch_toggled, NULL, NULL);
    if (!status) {
        fprintf(stderr, "dbus_connection_set_watch_functions() failed\n");
        exit(EXIT_FAILURE);
    }

    status = dbus_connection_set_timeout_functions(conn,
        timer_timeout_add, timer_timeout_remove, timer_timeout_toggled, NULL, NULL);
    if (!status) {
        fprintf(stderr, "dbus_connection_set_timeout_functions() failed\n");
        exit(EXIT_FAILURE);
    }

    status = dbus_connection_register_object_path(conn,
        TIMER_OBJECT_PATH, &timer_dbus_vtable, NULL);
    if (!status) {
        fprintf(stderr, "dbus_connection_register_object_path() failed\n");
        exit(EXIT_FAILURE);
    }

    timer_loop(conn);
#else
    // we want to asynchronously send signals using this connection
    // so we cannot use synchronous loop here
    // (because of blocking dbus_connection_read_write_dispatch())
    timer_loop(conn);
#endif

    return 0;
}

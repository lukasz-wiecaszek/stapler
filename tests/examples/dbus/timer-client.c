/* SPDX-License-Identifier: MIT */
/**
 * @file timer-client.c
 *
 * Implementation of the D-Bus 'timer' interface (client side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dbus/dbus.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "timer-common.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define NUM_OF_REPETITIONS 20
#define TIMER_INTERVAL 100000 /* microseconds */

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) objects definitions
\*===========================================================================*/
static timer_id timerid;

/*===========================================================================*\
 * global (external linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) functions definitions
\*===========================================================================*/
static int timer_send_message_name(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;
    char* retval;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        TIMER_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        TIMER_OBJECT_PATH,            // object path the message should be sent to
        TIMER_INTERFACE_NAME,         // interface to invoke method on
        TIMER_METHOD_NAME);           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            TIMER_WELL_KNOWN_NAME_SERVER,
            TIMER_OBJECT_PATH,
            TIMER_INTERFACE_NAME,
            TIMER_METHOD_NAME);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply(conn, msg_send, &pending, DBUS_TIMEOUT_INFINITE)) {
        fprintf(stderr, "dbus_connection_send_with_reply() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    if (pending == NULL) {
        fprintf(stderr, "DBusPendingCall object is NULL\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    dbus_connection_flush(conn);

    // free message
    dbus_message_unref(msg_send);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg_reply = dbus_pending_call_steal_reply(pending);
    if (msg_reply == NULL) {
        fprintf(stderr, "dbus_pending_call_steal_reply() failed\n");
        return -1;
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg_reply, &dbus_error,
        DBUS_TYPE_STRING, &retval,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    if (strcmp(retval, TIMER_INTERFACE_NAME) != 0) {
        fprintf(stderr, "test failed - expected: '%s', received: '%s'\n",
            TIMER_INTERFACE_NAME, retval);
        return -1;
    }

    return 0;
}

static int timer_send_message_version(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    struct version version;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        TIMER_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        TIMER_OBJECT_PATH,            // object path the message should be sent to
        TIMER_INTERFACE_NAME,         // interface to invoke method on
        TIMER_METHOD_VERSION);        // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            TIMER_WELL_KNOWN_NAME_SERVER,
            TIMER_OBJECT_PATH,
            TIMER_INTERFACE_NAME,
            TIMER_METHOD_VERSION);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply(conn, msg_send, &pending, DBUS_TIMEOUT_INFINITE)) {
        fprintf(stderr, "dbus_connection_send_with_reply() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    if (pending == NULL) {
        fprintf(stderr, "DBusPendingCall object is NULL\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    dbus_connection_flush(conn);

    // free message
    dbus_message_unref(msg_send);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg_reply = dbus_pending_call_steal_reply(pending);
    if (msg_reply == NULL) {
        fprintf(stderr, "dbus_pending_call_steal_reply() failed\n");
        return -1;
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // get the reply message
    if (!timer_message_get_version(msg_reply, &version)) {
        fprintf(stderr, "timer_message_get_version() failed\n");
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    if (version.major != TIMER_INTERFACE_VERSION_MAJOR ||
        version.minor != TIMER_INTERFACE_VERSION_MINOR ||
        version.micro != TIMER_INTERFACE_VERSION_MICRO) {
        fprintf(stderr, "test failed - version: %d.%d.%d\n", version.major, version.minor, version.micro);
        return -1;
    }

    return 0;
}

static int timer_send_message_start(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;
    dbus_uint64_t interval = TIMER_INTERVAL;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        TIMER_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        TIMER_OBJECT_PATH,            // object path the message should be sent to
        TIMER_INTERFACE_NAME,         // interface to invoke method on
        TIMER_METHOD_START);          // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            TIMER_WELL_KNOWN_NAME_SERVER,
            TIMER_OBJECT_PATH,
            TIMER_INTERFACE_NAME,
            TIMER_METHOD_START);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_UINT64, &interval,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply(conn, msg_send, &pending, DBUS_TIMEOUT_INFINITE)) {
        fprintf(stderr, "dbus_connection_send_with_reply() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    if (pending == NULL) {
        fprintf(stderr, "DBusPendingCall object is NULL\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    dbus_connection_flush(conn);

    // free message
    dbus_message_unref(msg_send);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg_reply = dbus_pending_call_steal_reply(pending);
    if (msg_reply == NULL) {
        fprintf(stderr, "dbus_pending_call_steal_reply() failed\n");
        return -1;
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg_reply, &dbus_error,
        DBUS_TYPE_UINT64, &timerid,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    return 0;
}

static int timer_send_message_stop(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        TIMER_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        TIMER_OBJECT_PATH,            // object path the message should be sent to
        TIMER_INTERFACE_NAME,         // interface to invoke method on
        TIMER_METHOD_STOP);           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            TIMER_WELL_KNOWN_NAME_SERVER,
            TIMER_OBJECT_PATH,
            TIMER_INTERFACE_NAME,
            TIMER_METHOD_STOP);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_UINT64, &timerid,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_append_args() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply(conn, msg_send, &pending, DBUS_TIMEOUT_INFINITE)) {
        fprintf(stderr, "dbus_connection_send_with_reply() failed\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    if (pending == NULL) {
        fprintf(stderr, "DBusPendingCall object is NULL\n");
        // free message
        dbus_message_unref(msg_send);
        return -1;
    }
    dbus_connection_flush(conn);

    // free message
    dbus_message_unref(msg_send);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg_reply = dbus_pending_call_steal_reply(pending);
    if (msg_reply == NULL) {
        fprintf(stderr, "dbus_pending_call_steal_reply() failed\n");
        return -1;
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg_reply, &dbus_error,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    return 0;
}

static int timer_signal_tick(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessageIter iter;
    DBusMessageIter iter_struct;
    dbus_uint64_t counter;
    dbus_uint64_t abstime;
    uint64_t client_abstime;
    struct timespec ts;
    int arg_type;

    // read the arguments
    if (!dbus_message_iter_init(msg, &iter)) {
        fprintf(stderr, "dbus_message_iter_init() failed\n");
        return -1;
    }

    arg_type = dbus_message_iter_get_arg_type(&iter);
    if (arg_type != DBUS_TYPE_STRUCT) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRUCT\n");
        return -1;
    }

    // initialize iterator for struct
    dbus_message_iter_recurse(&iter, &iter_struct);

    // 1st struct member is uint64
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_UINT64) {
        fprintf(stderr, "(1) arg_type != DBUS_TYPE_UINT64\n");
        return -1;
    }

    dbus_message_iter_get_basic(&iter_struct, &counter);
    dbus_message_iter_next(&iter_struct); // go to next argument of struct

    // 2nd struct member is uint64
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_UINT64) {
        fprintf(stderr, "(2) arg_type != DBUS_TYPE_UINT64\n");
        return -1;
    }

    dbus_message_iter_get_basic(&iter_struct, &abstime);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0)
        client_abstime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    else
        client_abstime = 0;

    fprintf(stdout, "counter: %lu, client: %lu, server: %lu, diff: %lu\n",
        counter, client_abstime, abstime, client_abstime - abstime);

    return 0;
}

static void timer_loop(DBusConnection* conn)
{
    int i = 0;
    DBusMessage* msg;

    // loop, testing for new messages
    while (i < NUM_OF_REPETITIONS) {
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

        // check if the message is a signal from the correct interface and with the correct name
        if (dbus_message_is_signal(msg, TIMER_INTERFACE_NAME, TIMER_SIGNAL_TICK)) {
            if (timer_signal_tick(conn, msg) != 0) {
                fprintf(stderr, "signal_tick() failed\n");
            }
            i++;
        }

        // free the message
        dbus_message_unref(msg);
    }
}

static dbus_int32_t timer_send_receive_messages(DBusConnection* conn)
{
    int retval;

    retval = timer_send_message_name(conn);
    if (retval != 0)
        return -1;

    retval = timer_send_message_version(conn);
    if (retval != 0)
        return -1;

    retval = timer_send_message_start(conn);
    if (retval != 0)
        return -1;

    timer_loop(conn);

    retval = timer_send_message_stop(conn);
    if (retval != 0)
        return -1;

    return 0;
}

int main(int argc, char *argv[])
{
    DBusConnection* conn;
    DBusError dbus_error;
    dbus_int32_t retval;
    struct timespec t1, t2;
    uint64_t microseconds;
    char match_buffer[1024];
    int status;
    int i, n;

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
    status = dbus_bus_request_name(conn, TIMER_WELL_KNOWN_NAME_CLIENT,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                TIMER_WELL_KNOWN_NAME_CLIENT, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    // add a rule which signals we want to subscribe to
    snprintf(match_buffer, sizeof(match_buffer),
        "type='signal',interface='%s'", TIMER_INTERFACE_NAME);
    dbus_bus_add_match(conn, match_buffer, &dbus_error);
    if (dbus_error_is_set(&dbus_error)) {
        fprintf(stderr, "dbus_bus_add_match(%s) failed (%s:%s)\n",
            match_buffer, dbus_error.name, dbus_error.message);
        dbus_error_free(&dbus_error);
    }
    dbus_connection_flush(conn);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    status = timer_send_receive_messages(conn);
    if (status != 0) {
        fprintf(stdout, "Test failed\n");
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    fprintf(stdout, "Test took %lu microseconds\n", microseconds);

    return 0;
}

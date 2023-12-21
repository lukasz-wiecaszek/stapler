/* SPDX-License-Identifier: MIT */
/**
 * @file ping-client.c
 *
 * Implementation of the D-Bus 'ping' interface (client side).
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
#include <getopt.h>

#include <dbus/dbus.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "ping-common.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define NUM_OF_REPETITIONS 10000

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
static dbus_int32_t ping_send_message_name(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;
    char* retval;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        PING_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        PING_OBJECT_PATH,            // object path the message should be sent to
        PING_INTERFACE_NAME,         // interface to invoke method on
        PING_METHOD_NAME);           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            PING_WELL_KNOWN_NAME_SERVER,
            PING_OBJECT_PATH,
            PING_INTERFACE_NAME,
            PING_METHOD_NAME);
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

    if (strcmp(retval, PING_INTERFACE_NAME) != 0) {
        fprintf(stderr, "test failed - expected: '%s', received: '%s'\n",
            PING_INTERFACE_NAME, retval);
        return -1;
    }

    return 0;
}

static dbus_int32_t ping_send_message_version(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    struct version version;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        PING_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        PING_OBJECT_PATH,            // object path the message should be sent to
        PING_INTERFACE_NAME,         // interface to invoke method on
        PING_METHOD_VERSION);        // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            PING_WELL_KNOWN_NAME_SERVER,
            PING_OBJECT_PATH,
            PING_INTERFACE_NAME,
            PING_METHOD_VERSION);
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
    if (!ping_message_get_version(msg_reply, &version)) {
        fprintf(stderr, "ping_message_get_version() failed\n");
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    if (version.major != PING_INTERFACE_VERSION_MAJOR ||
        version.minor != PING_INTERFACE_VERSION_MINOR ||
        version.micro != PING_INTERFACE_VERSION_MICRO) {
        fprintf(stderr, "test failed - version: %d.%d.%d\n", version.major, version.minor, version.micro);
        return -1;
    }

    return 0;
}

static dbus_int32_t ping_send_message_ping(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    struct test_struct send = {
        .op = PING,
        {
            .v1 = 1,
            .v2 = 2,
            .v3 = 3,
            .v4 = 4,
            .v5 = 5,
            .v6 = 6,
            .v7 = "7",
            .v8 = "8",
        },
        {
            {{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}},
            {1, 2, 3, 4, 5, 6, 7, 8}
        }
    };
    struct test_struct receive;
    int i;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        PING_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        PING_OBJECT_PATH,            // object path the message should be sent to
        PING_INTERFACE_NAME,         // interface to invoke method on
        PING_METHOD_PING);           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            PING_WELL_KNOWN_NAME_SERVER,
            PING_OBJECT_PATH,
            PING_INTERFACE_NAME,
            PING_METHOD_PING);
        return -1;
    }

    // fill the send message
    if (ping_message_fill_test_struct(msg_send, &send) == FALSE) {
        fprintf(stderr, "ping_message_fill_test_struct() failed\n");
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

    // get the message
    if (!ping_message_get_test_struct(msg_reply, &receive)) {
        fprintf(stderr, "ping_message_get_test_struct() failed\n");
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    //ping_print_test_struct(&receive);

    if (receive.op != PONG) {
        fprintf(stderr, "test failed - expected PONG operation\n");
        return -1;
    }
    if (ping_compare_bdt(&receive.bdt, &send.bdt) == FALSE) {
        fprintf(stderr, "test failed - send/receive basic data types mismatch\n");
        return -1;
    }
    if (ping_compare_cdt(&receive.cdt, &send.cdt) == FALSE) {
        fprintf(stderr, "test failed - send/receive container data types mismatch\n");
        return -1;
    }

    return 0;
}

static dbus_int32_t ping_send_message_hello(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusError dbus_error;
    DBusPendingCall* pending;

    char strbuffer[16];
    char* strptr = strbuffer;
    static int cnt = 0;
    snprintf(strbuffer, sizeof(strbuffer), "hello #%d", cnt);
    cnt++;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        PING_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        PING_OBJECT_PATH,            // object path the message should be sent to
        PING_INTERFACE_NAME,         // interface to invoke method on
        PING_METHOD_HELLO);          // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            PING_WELL_KNOWN_NAME_SERVER,
            PING_OBJECT_PATH,
            PING_INTERFACE_NAME,
            PING_METHOD_HELLO);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_STRING, &strptr,
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

    // free the pending message handle
    dbus_pending_call_unref(pending);

    return 0;
}

static dbus_int32_t ping_ping(DBusConnection* conn)
{
    dbus_int32_t status;

    status = ping_send_message_name(conn);
    if (status != 0)
        return status;

    status = ping_send_message_version(conn);
    if (status != 0)
        return status;

    status = ping_send_message_ping(conn);
    if (status != 0)
        return status;

    return 0;
}

static dbus_int32_t ping_hello(DBusConnection* conn)
{
    dbus_int32_t status;

    status = ping_send_message_name(conn);
    if (status != 0)
        return status;

    status = ping_send_message_version(conn);
    if (status != 0)
        return status;

    status = ping_send_message_hello(conn);
    if (status != 0)
        return status;

    return 0;
}

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    DBusConnection* conn;
    DBusError dbus_error;
    dbus_int32_t retval;
    struct timespec t1, t2;
    uint64_t microseconds;
    bool ping = false;
    bool hello = false;
    int status;
    int i;
    int n = 0;

    static struct option long_options[] = {
        {"ping",  no_argument,       0, 'g'},
        {"hello", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    for (;;) {
        int c = getopt_long(argc, argv, "gh", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'g':
                ping = true;
                break;
            case 'h':
                hello = true;
                break;
        }
    }

    if (ping == false && hello == false) {
        fprintf(stderr, "Neither 'ping' nor 'hello' test is selected. Terminating.\n");
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
    status = dbus_bus_request_name(conn, PING_WELL_KNOWN_NAME_CLIENT,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                PING_WELL_KNOWN_NAME_CLIENT, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (; n < NUM_OF_REPETITIONS; n++) {
        if (ping)
            if (ping_ping(conn) != 0)
                break;

        if (hello)
            if (ping_hello(conn) != 0)
                break;
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    fprintf(stderr, "%d out of %d messages sent with success\n", n, NUM_OF_REPETITIONS);
    fprintf(stdout, "Test took %lu microseconds\n", microseconds);

    return 0;
}

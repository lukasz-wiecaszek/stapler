/* SPDX-License-Identifier: MIT */
/**
 * @file calculator-client.c
 *
 * Implementation of the D-Bus 'calculator' interface (client side).
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
#include "calculator-common.h"

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
static dbus_int32_t calculator_send_message_name(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;
    char* retval;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        CALCULATOR_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        CALCULATOR_OBJECT_PATH,            // object path the message should be sent to
        CALCULATOR_INTERFACE_NAME,         // interface to invoke method on
        CALCULATOR_METHOD_NAME);           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            CALCULATOR_WELL_KNOWN_NAME_SERVER,
            CALCULATOR_OBJECT_PATH,
            CALCULATOR_INTERFACE_NAME,
            CALCULATOR_METHOD_NAME);
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

    if (strcmp(retval, CALCULATOR_INTERFACE_NAME) != 0) {
        fprintf(stderr, "test failed - expected: '%s', received: '%s'\n",
            CALCULATOR_INTERFACE_NAME, retval);
        return -1;
    }

    return 0;
}

static dbus_int32_t calculator_send_message_version(DBusConnection* conn)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    struct version version;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        CALCULATOR_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        CALCULATOR_OBJECT_PATH,            // object path the message should be sent to
        CALCULATOR_INTERFACE_NAME,         // interface to invoke method on
        CALCULATOR_METHOD_VERSION);        // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            CALCULATOR_WELL_KNOWN_NAME_SERVER,
            CALCULATOR_OBJECT_PATH,
            CALCULATOR_INTERFACE_NAME,
            CALCULATOR_METHOD_VERSION);
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
    if (!calculator_message_get_version(msg_reply, &version)) {
        fprintf(stderr, "calculator_message_get_version() failed\n");
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    if (version.major != CALCULATOR_INTERFACE_VERSION_MAJOR ||
        version.minor != CALCULATOR_INTERFACE_VERSION_MINOR ||
        version.micro != CALCULATOR_INTERFACE_VERSION_MICRO) {
        fprintf(stderr, "test failed - version: %d.%d.%d\n", version.major, version.minor, version.micro);
        return -1;
    }

    return 0;
}

static dbus_int32_t calculator_send_message(DBusConnection* conn, const char* method, dbus_int32_t arg1, dbus_int32_t arg2)
{
    DBusMessage* msg_send;
    DBusMessage* msg_reply;
    DBusPendingCall* pending;
    DBusError dbus_error;
    dbus_int32_t retval;

    // create a new method call and check for errors
    msg_send = dbus_message_new_method_call(
        CALCULATOR_WELL_KNOWN_NAME_SERVER, // name that the message should be sent to
        CALCULATOR_OBJECT_PATH,            // object path the message should be sent to
        CALCULATOR_INTERFACE_NAME,         // interface to invoke method on
        method);                           // method to invoke
    if (msg_send == NULL) {
        fprintf(stderr, "dbus_message_new_method_call(%s, %s, %s, %s) failed\n",
            CALCULATOR_WELL_KNOWN_NAME_SERVER,
            CALCULATOR_OBJECT_PATH,
            CALCULATOR_INTERFACE_NAME,
            method);
        return -1;
    }

    // append arguments
    if (!dbus_message_append_args(msg_send,
        DBUS_TYPE_INT32, &arg1,
        DBUS_TYPE_INT32, &arg2,
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
        DBUS_TYPE_INT32, &retval,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        // free reply
        dbus_message_unref(msg_reply);
        return -1;
    }

    // free reply
    dbus_message_unref(msg_reply);

    return retval;
}

static dbus_int32_t calculator_send_messages(DBusConnection* conn)
{
    dbus_int32_t retval;
    int i;

    static const char* methods[] = {
        CALCULATOR_METHOD_NAME,
        CALCULATOR_METHOD_VERSION,
        CALCULATOR_METHOD_ADD,
        CALCULATOR_METHOD_SUBTRACT,
        CALCULATOR_METHOD_MULTIPLY,
        CALCULATOR_METHOD_DIVIDE
    };

    for (i = 0; i < ARRAY_SIZE(methods); i++) {
        switch (i) {
            case 0: {
                retval = calculator_send_message_name(conn);
                if (retval != 0) {
                    return -1;
                }
                break;
            }
            case 1: {
                retval = calculator_send_message_version(conn);
                if (retval != 0) {
                    return -1;
                }
                break;
            }
            case 2: {
                retval = calculator_send_message(conn, methods[i], 100, 3);
                if ((100 + 3) != retval) {
                    fprintf(stderr, "test failed - expected: 100 + 3, received: %d\n", retval);
                    return -1;
                }
                break;
            }
            case 3: {
                retval = calculator_send_message(conn, methods[i], 100, 3);
                if ((100 - 3) != retval) {
                    fprintf(stderr, "test failed - expected: 100 - 3, received: %d\n", retval);
                    return -1;
                }
                break;
            }
            case 4: {
                retval = calculator_send_message(conn, methods[i], 100, 3);
                if ((100 * 3) != retval) {
                    fprintf(stderr, "test failed - expected: 100 * 3, received: %d\n", retval);
                    return -1;
                }
                break;
            }
            case 5: {
                retval = calculator_send_message(conn, methods[i], 100, 3);
                if ((100 / 3) != retval) {
                    fprintf(stderr, "test failed - expected: 100 / 3, received: %d\n", retval);
                    return -1;
                }
                break;
            }
            default:
                fprintf(stderr, "test failed - Invalid method selected\n");
                return -1;
        }
    }

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
    int status;
    int i;
    int n = 0;

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
    status = dbus_bus_request_name(conn, CALCULATOR_WELL_KNOWN_NAME_CLIENT,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                CALCULATOR_WELL_KNOWN_NAME_CLIENT, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (; n < NUM_OF_REPETITIONS; n++)
        if (calculator_send_messages(conn) != 0)
            break;

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    fprintf(stderr, "%d out of %d messages sent with success\n", n, NUM_OF_REPETITIONS);
    fprintf(stdout, "Test took %lu microseconds\n", microseconds);

    return 0;
}

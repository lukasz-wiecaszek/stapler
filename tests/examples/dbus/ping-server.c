/* SPDX-License-Identifier: MIT */
/**
 * @file ping-server.c
 *
 * Implementation of the D-Bus 'ping' interface (server side).
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

#include <dbus/dbus.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "ping-common.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/

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
static int ping_method_name(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    char* result = PING_INTERFACE_NAME;
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

static int ping_method_version(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_uint32_t serial = 0;
    struct version version = {
        PING_INTERFACE_VERSION_MAJOR,
        PING_INTERFACE_VERSION_MINOR,
        PING_INTERFACE_VERSION_MICRO
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
    if (ping_message_fill_version(reply, &version) == FALSE) {
        fprintf(stderr, "ping_message_fill_version() failed\n");
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

static int ping_method_ping(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    dbus_uint32_t serial = 0;
    struct test_struct test_struct;

    // get the message
    if (!ping_message_get_test_struct(msg, &test_struct)) {
        fprintf(stderr, "ping_message_get_test_struct() failed\n");
        return -1;
    }

    //ping_print_test_struct(&test_struct);
    test_struct.op = PONG;

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // fill the reply message
    if (ping_message_fill_test_struct(reply, &test_struct) == FALSE) {
        fprintf(stderr, "ping_message_fill_test_struct() failed\n");
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

static int ping_method_hello(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    dbus_uint32_t serial = 0;
    DBusError dbus_error;
    char* text;
    static int cnt = 0;

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_STRING, &text,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }

    if ((cnt % 100) == 0)
        fprintf(stdout, "%s\n", text);
    cnt++;

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

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

static int ping_loop(DBusConnection* conn)
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
        // this shall not happen as dbus_connection_read_write() blocks until it can read or write,
        // but, who knows
        if (msg == NULL)
            continue;

        // check this is a method call for the right interface & method
        if (dbus_message_is_method_call(msg, PING_INTERFACE_NAME, PING_METHOD_NAME))
            ping_method_name(conn, msg);
        else
        if (dbus_message_is_method_call(msg, PING_INTERFACE_NAME, PING_METHOD_VERSION))
            ping_method_version(conn, msg);
        else
        if (dbus_message_is_method_call(msg, PING_INTERFACE_NAME, PING_METHOD_PING))
            ping_method_ping(conn, msg);
        else
        if (dbus_message_is_method_call(msg, PING_INTERFACE_NAME, PING_METHOD_HELLO))
            ping_method_hello(conn, msg);
        else
            /* do nothing */;

        // free the message
        dbus_message_unref(msg);
   }
}

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    DBusConnection* conn;
    DBusError dbus_error;
    int status;

    fprintf(stdout, "Starting 'ping' server ...\n");

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
    status = dbus_bus_request_name(conn, PING_WELL_KNOWN_NAME_SERVER,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                PING_WELL_KNOWN_NAME_SERVER, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    ping_loop(conn);

    return 0;
}

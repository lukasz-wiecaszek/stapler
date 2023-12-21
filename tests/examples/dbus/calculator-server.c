/* SPDX-License-Identifier: MIT */
/**
 * @file calculator-server.c
 *
 * Implementation of the D-Bus 'calculator' interface (server side).
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
#include "calculator-common.h"

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
static int calculator_method_name(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    char* result = CALCULATOR_INTERFACE_NAME;
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

static int calculator_method_version(DBusConnection* conn, DBusMessage* msg)
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_uint32_t serial = 0;
    struct version version = {
        CALCULATOR_INTERFACE_VERSION_MAJOR,
        CALCULATOR_INTERFACE_VERSION_MINOR,
        CALCULATOR_INTERFACE_VERSION_MICRO
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
    if (calculator_message_fill_version(reply, &version) == FALSE) {
        fprintf(stderr, "calculator_message_fill_version() failed\n");
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

static dbus_int32_t calculator_method_add(dbus_int32_t arg1, dbus_int32_t arg2)
{
    return arg1 + arg2;
}

static dbus_int32_t calculator_method_subtract(dbus_int32_t arg1, dbus_int32_t arg2)
{
    return arg1 - arg2;
}

static dbus_int32_t calculator_method_multiply(dbus_int32_t arg1, dbus_int32_t arg2)
{
    return arg1 * arg2;
}

static dbus_int32_t calculator_method_divide(dbus_int32_t arg1, dbus_int32_t arg2)
{
    return arg1 / arg2;
}

static int calculator_method_call(DBusConnection* conn, DBusMessage* msg, dbus_int32_t (*method)(dbus_int32_t arg1, dbus_int32_t arg2))
{
    DBusMessage* reply;
    DBusError dbus_error;
    dbus_int32_t arg1 = -1;
    dbus_int32_t arg2 = -1;
    dbus_int32_t result;
    dbus_uint32_t serial = 0;

    // initialise the error value
    dbus_error_init(&dbus_error);

    // read the arguments
    if (!dbus_message_get_args(msg, &dbus_error,
        DBUS_TYPE_INT32, &arg1,
        DBUS_TYPE_INT32, &arg2,
        DBUS_TYPE_INVALID)) {
        fprintf(stderr, "dbus_message_get_args() failed (%s:%s)\n", dbus_error.name, dbus_error.message);
        return -1;
    }

    result = method(arg1, arg2);

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    // add the arguments to the reply
    if (!dbus_message_append_args(reply,
        DBUS_TYPE_INT32, &result,
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

static int calculator_loop(DBusConnection* conn)
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
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_NAME))
            calculator_method_name(conn, msg);
        else
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_VERSION))
            calculator_method_version(conn, msg);
        else
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_ADD))
            calculator_method_call(conn, msg, calculator_method_add);
        else
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_SUBTRACT))
            calculator_method_call(conn, msg, calculator_method_subtract);
        else
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_MULTIPLY))
            calculator_method_call(conn, msg, calculator_method_multiply);
        else
        if (dbus_message_is_method_call(msg, CALCULATOR_INTERFACE_NAME, CALCULATOR_METHOD_DIVIDE))
            calculator_method_call(conn, msg, calculator_method_divide);
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

    fprintf(stdout, "Starting 'calculator' server ...\n");

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
    status = dbus_bus_request_name(conn, CALCULATOR_WELL_KNOWN_NAME_SERVER,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_error);
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&dbus_error)) {
            fprintf(stderr, "dbus_bus_request_name(%s) failed (%s:%s)\n",
                CALCULATOR_WELL_KNOWN_NAME_SERVER, dbus_error.name, dbus_error.message);
            dbus_error_free(&dbus_error);
        }
        exit(EXIT_FAILURE);
    }

    calculator_loop(conn);

    return 0;
}

/* SPDX-License-Identifier: MIT */
/**
 * @file calculator-common.h
 *
 * Common definitions and data types used both by client and server side.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _CALCULATOR_COMMON_H_
#define _CALCULATOR_COMMON_H_

#if defined(__cplusplus)
    #define LTS_EXTERN extern "C"
#else
    #define LTS_EXTERN extern
#endif

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <string.h>
#include <dbus/dbus.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define CALCULATOR_WELL_KNOWN_NAME_CLIENT     "calculator.dbus.client"
#define CALCULATOR_WELL_KNOWN_NAME_SERVER     "calculator.dbus.server"

#define CALCULATOR_OBJECT_PATH                "/lts/calculator/object1"
#define CALCULATOR_INTERFACE_NAME             "lts.calculator"
#define CALCULATOR_METHOD_NAME                "Name"
#define CALCULATOR_METHOD_VERSION             "Version"
#define CALCULATOR_METHOD_ADD                 "Add"
#define CALCULATOR_METHOD_SUBTRACT            "Subtract"
#define CALCULATOR_METHOD_MULTIPLY            "Multiply"
#define CALCULATOR_METHOD_DIVIDE              "Divide"

#define CALCULATOR_INTERFACE_VERSION_MAJOR 1
#define CALCULATOR_INTERFACE_VERSION_MINOR 2
#define CALCULATOR_INTERFACE_VERSION_MICRO 3

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*===========================================================================*\
 * global types definitions
\*===========================================================================*/
struct version {
    dbus_int32_t major;
    dbus_int32_t minor;
    dbus_int32_t micro;
};

/*===========================================================================*\
 * static (internal linkage) functions definitions
\*===========================================================================*/
static dbus_bool_t calculator_message_fill_version(DBusMessage* msg, const struct version* version)
{
    dbus_bool_t retval = FALSE;

    do {
        DBusMessageIter iter;
        DBusMessageIter iter_struct;
        dbus_bool_t status = TRUE;

        dbus_message_iter_init_append(msg, &iter);

        status &= dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &iter_struct);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &version->major);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &version->minor);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &version->micro);
        status &= dbus_message_iter_close_container(&iter, &iter_struct);

        retval = status;
    } while (0);

    return retval;
}

static dbus_bool_t calculator_message_get_version(DBusMessage* msg, struct version* version)
{
    DBusMessageIter iter;
    DBusMessageIter iter_struct;
    int arg_type;

    memset(version, 0, sizeof(*version));

    // read the arguments
    if (!dbus_message_iter_init(msg, &iter)) {
        fprintf(stderr, "dbus_message_iter_init() failed\n");
        return FALSE;
    }

    arg_type = dbus_message_iter_get_arg_type(&iter);
    if (arg_type != DBUS_TYPE_STRUCT) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRUCT\n");
        return FALSE;
    }

    // initialize iterator for struct
    dbus_message_iter_recurse(&iter, &iter_struct);

    // 1st struct member is int32
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "(1) arg_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &version->major);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 2nd struct member is int32
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "(2) arg_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &version->minor);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 3rd struct member is int32
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "(3) arg_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &version->micro);

    return TRUE;
}

/*===========================================================================*\
 * global (external linkage) objects declarations
\*===========================================================================*/

/*===========================================================================*\
 * function forward declarations (external linkage)
\*===========================================================================*/

#endif /* _CALCULATOR_COMMON_H_ */

/* SPDX-License-Identifier: MIT */
/**
 * @file timer-common.h
 *
 * Common definitions and data types used both by client and server side.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _TIMER_COMMON_H_
#define _TIMER_COMMON_H_

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
#define TIMER_WELL_KNOWN_NAME_CLIENT     "timer.dbus.client"
#define TIMER_WELL_KNOWN_NAME_SERVER     "timer.dbus.server"

#define TIMER_OBJECT_PATH                "/lts/timer/object1"
#define TIMER_INTERFACE_NAME             "lts.timer"
#define TIMER_METHOD_NAME                "Name"
#define TIMER_METHOD_VERSION             "Version"
#define TIMER_METHOD_START               "Start"
#define TIMER_METHOD_STOP                "Stop"
#define TIMER_SIGNAL_TICK                "Tick"

#define TIMER_INTERFACE_VERSION_MAJOR 1
#define TIMER_INTERFACE_VERSION_MINOR 2
#define TIMER_INTERFACE_VERSION_MICRO 3

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define INVALID_TIMER_ID ((timer_id)-1)

/*===========================================================================*\
 * global types definitions
\*===========================================================================*/
struct version {
    dbus_int32_t major;
    dbus_int32_t minor;
    dbus_int32_t micro;
};

typedef dbus_uint64_t timer_id;

struct timestamp {
    dbus_uint64_t counter;
    dbus_uint64_t abstime;
};

/*===========================================================================*\
 * static (internal linkage) functions definitions
\*===========================================================================*/
static dbus_bool_t timer_message_fill_version(DBusMessage* msg, const struct version* version)
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

static dbus_bool_t timer_message_get_version(DBusMessage* msg, struct version* version)
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

#endif /* _TIMER_COMMON_H_ */

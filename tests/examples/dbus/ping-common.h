/* SPDX-License-Identifier: MIT */
/**
 * @file ping-common.h
 *
 * Common definitions and data types used both by client and server side.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _PING_COMMON_H_
#define _PING_COMMON_H_

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
#define PING_WELL_KNOWN_NAME_CLIENT     "ping.dbus.client"
#define PING_WELL_KNOWN_NAME_SERVER     "ping.dbus.server"

#define PING_OBJECT_PATH                "/lts/ping/object1"
#define PING_INTERFACE_NAME             "lts.ping"
#define PING_METHOD_NAME                "Name"
#define PING_METHOD_VERSION             "Version"
#define PING_METHOD_PING                "Ping"
#define PING_METHOD_HELLO               "Hello"

#define PING_INTERFACE_VERSION_MAJOR 1
#define PING_INTERFACE_VERSION_MINOR 2
#define PING_INTERFACE_VERSION_MICRO 3

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*===========================================================================*\
 * global types definitions
\*===========================================================================*/
struct version {
    dbus_int32_t major;
    dbus_int32_t minor;
    dbus_int32_t micro;
};

/* Operation code
(PING from client -> server, PONG from server -> client) */
enum operation {
    PING = 0,
    PONG = 1
};

/* Basic Data Types */
struct bdt {
    dbus_bool_t  v1;
    char         v2;
    dbus_int16_t v3;
    dbus_int32_t v4;
    dbus_int64_t v5;
    double       v6;
    char*        v7;
    void*        v8;
};

/* Compound Data Types */
struct cdt {
    struct {
        int8_t key;
        const char* value;
    } v1[4];
    dbus_int32_t v2[8];
};

/* Main structure encapsulating different data types */
struct test_struct {
    enum operation op;
    struct bdt bdt;
    struct cdt cdt;
};

/*===========================================================================*\
 * static (internal linkage) functions definitions
\*===========================================================================*/
static void ping_print_test_struct(const struct test_struct* test_struct)
{
    int i;

    fprintf(stdout, "op: %d\n", test_struct->op);
    fprintf(stdout, "bdt.v1: %d\n", test_struct->bdt.v1);
    fprintf(stdout, "bdt.v2: %hhd\n", test_struct->bdt.v2);
    fprintf(stdout, "bdt.v3: %hd\n", test_struct->bdt.v3);
    fprintf(stdout, "bdt.v4: %d\n", test_struct->bdt.v4);
    fprintf(stdout, "bdt.v5: %ld\n", test_struct->bdt.v5);
    fprintf(stdout, "bdt.v6: %f\n", test_struct->bdt.v6);
    fprintf(stdout, "bdt.v7: \"%s\"\n", test_struct->bdt.v7);
    fprintf(stdout, "bdt.v8: \"%s\"\n", test_struct->bdt.v8);

    fprintf(stdout, "cdt.v1: ");
    for (i = 0; i < ARRAY_SIZE(test_struct->cdt.v1); i++) {
        fprintf(stdout, "{%hhd, \"%s\"} ",
            test_struct->cdt.v1[i].key, test_struct->cdt.v1[i].value);
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "cdt.v2: ");
    for (i = 0; i < ARRAY_SIZE(test_struct->cdt.v2); i++)
        fprintf(stdout, "%d ", test_struct->cdt.v2[i]);
    fprintf(stdout, "\n");
}

static bool ping_compare_bdt(const struct bdt* s1, const struct bdt* s2)
{
    if (s1->v1 != s2->v1) return false;
    if (s1->v2 != s2->v2) return false;
    if (s1->v3 != s2->v3) return false;
    if (s1->v4 != s2->v4) return false;
    if (s1->v5 != s2->v5) return false;
    if (s1->v6 != s2->v6) return false;
    if (strcmp(s1->v7, s2->v7)) return false;
    if (strcmp(s1->v8, s2->v8)) return false;

    return true;
}

static bool ping_compare_cdt(const struct cdt* s1, const struct cdt* s2)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(s1->v1); i++) {
        if (s1->v1[i].key != s2->v1[i].key)
            return false;
    }

    for (i = 0; i < ARRAY_SIZE(s1->v2); i++) {
        if (s1->v2[i] != s2->v2[i])
            return false;
    }

    return true;
}

static dbus_bool_t ping_message_fill_version(DBusMessage* msg, const struct version* version)
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

static dbus_bool_t ping_message_get_version(DBusMessage* msg, struct version* version)
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

static dbus_bool_t ping_message_fill_test_struct(DBusMessage* msg, const struct test_struct* test_struct)
{
    dbus_bool_t retval = FALSE;

    do {
        DBusMessageIter iter;
        DBusMessageIter iter_struct;
        DBusMessageIter iter_array;
        DBusMessageIter iter_dict;
        dbus_bool_t status = TRUE;
        int i;

        dbus_message_iter_init_append(msg, &iter);

        status &= dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &test_struct->op);

        status &= dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &iter_struct);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_BOOLEAN, &test_struct->bdt.v1);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_BYTE,    &test_struct->bdt.v2);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT16,   &test_struct->bdt.v3);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32,   &test_struct->bdt.v4);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT64,   &test_struct->bdt.v5);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_DOUBLE,  &test_struct->bdt.v6);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_STRING,  &test_struct->bdt.v7);
        status &= dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_STRING,  &test_struct->bdt.v8);
        status &= dbus_message_iter_close_container(&iter, &iter_struct);

        status &= dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &iter_struct);

        status &= dbus_message_iter_open_container(&iter_struct, DBUS_TYPE_ARRAY, "{ys}", &iter_array);
        for (i = 0; i < ARRAY_SIZE(test_struct->cdt.v1); i++) {
            status &= dbus_message_iter_open_container(&iter_array, DBUS_TYPE_DICT_ENTRY, NULL, &iter_dict);
            status &= dbus_message_iter_append_basic(&iter_dict, DBUS_TYPE_BYTE, &test_struct->cdt.v1[i].key);
            status &= dbus_message_iter_append_basic(&iter_dict, DBUS_TYPE_STRING, &test_struct->cdt.v1[i].value);
            status &= dbus_message_iter_close_container(&iter_array, &iter_dict);
        }
        status &= dbus_message_iter_close_container(&iter_struct, &iter_array);

        status &= dbus_message_iter_open_container(&iter_struct, DBUS_TYPE_ARRAY, "i", &iter_array);
        for (i = 0; i < ARRAY_SIZE(test_struct->cdt.v2); i++)
            status &= dbus_message_iter_append_basic(&iter_array, DBUS_TYPE_INT32, &test_struct->cdt.v2[i]);
        status &= dbus_message_iter_close_container(&iter_struct, &iter_array);

        status &= dbus_message_iter_close_container(&iter, &iter_struct);

        retval = status;
    } while (0);

    return retval;
}

static dbus_bool_t ping_message_get_test_struct(DBusMessage* msg, struct test_struct* test_struct)
{
    DBusMessageIter iter;
    DBusMessageIter iter_struct;
    DBusMessageIter iter_array;
    DBusMessageIter iter_dict;
    int arg_type;
    int elem_type;
    int i;

    memset(test_struct, 0, sizeof(*test_struct));

    // read the arguments
    if (!dbus_message_iter_init(msg, &iter)) {
        fprintf(stderr, "dbus_message_iter_init() failed\n");
        return FALSE;
    }

    arg_type = dbus_message_iter_get_arg_type(&iter);
    if (arg_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "arg_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter, &test_struct->op);
    dbus_message_iter_next(&iter);

    arg_type = dbus_message_iter_get_arg_type(&iter);
    if (arg_type != DBUS_TYPE_STRUCT) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRUCT\n");
        return FALSE;
    }

    // initialize iterator for 'struct bdt'
    dbus_message_iter_recurse(&iter, &iter_struct);

    // 1st struct member is 'dbus_bool_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_BOOLEAN) {
        fprintf(stderr, "arg_type != DBUS_TYPE_BOOLEAN\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v1);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 2nd struct member is 'dbus_int8_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_BYTE) {
        fprintf(stderr, "arg_type != DBUS_TYPE_BYTE\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v2);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 3rd struct member is 'dbus_int16_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT16) {
        fprintf(stderr, "arg_type != DBUS_TYPE_INT16\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v3);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 4th struct member is 'dbus_int32_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "arg_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v4);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 5th struct member is 'dbus_int64_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_INT64) {
        fprintf(stderr, "arg_type != DBUS_TYPE_INT64\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v5);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 6th struct member is 'double'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_DOUBLE) {
        fprintf(stderr, "arg_type != DBUS_TYPE_DOUBLE\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v6);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 7th struct member is 'char*'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_STRING) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRING\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v7);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 8th struct member is 'void*'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_STRING) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRING\n");
        return FALSE;
    }

    dbus_message_iter_get_basic(&iter_struct, &test_struct->bdt.v8);
    dbus_message_iter_next(&iter_struct); // go to next struct member

    dbus_message_iter_next(&iter);

    arg_type = dbus_message_iter_get_arg_type(&iter);
    if (arg_type != DBUS_TYPE_STRUCT) {
        fprintf(stderr, "arg_type != DBUS_TYPE_STRUCT\n");
        return FALSE;
    }

    // initialize iterator for 'struct cdt'
    dbus_message_iter_recurse(&iter, &iter_struct);

    // 1st struct member is 'array of {dbus_int8_t, char*}'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_ARRAY) {
        fprintf(stderr, "arg_type != DBUS_TYPE_ARRAY\n");
        return FALSE;
    }

    elem_type = dbus_message_iter_get_element_type(&iter_struct);
    if (elem_type != DBUS_TYPE_DICT_ENTRY) {
        fprintf(stderr, "elem_type != DBUS_TYPE_DICT_ENTRY\n");
        return FALSE;
    }

    // initialize iterator for 'array of {dbus_int8_t, char*}'
    dbus_message_iter_recurse(&iter_struct, &iter_array);

    i = 0;
    while (dbus_message_iter_get_arg_type(&iter_array) != DBUS_TYPE_INVALID) {
        dbus_message_iter_recurse(&iter_array, &iter_dict);

        // dictionary key is 'dbus_int8_t'
        arg_type = dbus_message_iter_get_arg_type(&iter_dict);
        if (arg_type != DBUS_TYPE_BYTE) {
            fprintf(stderr, "arg_type != DBUS_TYPE_BYTE\n");
            return FALSE;
        }
        dbus_message_iter_get_basic(&iter_dict, &test_struct->cdt.v1[i].key);
        dbus_message_iter_next(&iter_dict);

        // dictionary value is 'char*'
        arg_type = dbus_message_iter_get_arg_type(&iter_dict);
        if (arg_type != DBUS_TYPE_STRING) {
            fprintf(stderr, "arg_type != DBUS_TYPE_STRING\n");
            return FALSE;
        }
        dbus_message_iter_get_basic(&iter_dict, &test_struct->cdt.v1[i].value);
        dbus_message_iter_next(&iter_dict);

        dbus_message_iter_next(&iter_array); // go to next argument of array
        i++;
    }

    dbus_message_iter_next(&iter_struct); // go to next struct member

    // 2nd struct member is 'array of dbus_int32_t'
    arg_type = dbus_message_iter_get_arg_type(&iter_struct);
    if (arg_type != DBUS_TYPE_ARRAY) {
        fprintf(stderr, "arg_type != DBUS_TYPE_ARRAY\n");
        return FALSE;
    }

    elem_type = dbus_message_iter_get_element_type(&iter_struct);
    if (elem_type != DBUS_TYPE_INT32) {
        fprintf(stderr, "elem_type != DBUS_TYPE_INT32\n");
        return FALSE;
    }

    // initialize iterator for 'array of dbus_int32_t'
    dbus_message_iter_recurse(&iter_struct, &iter_array);

    i = 0;
    while (dbus_message_iter_get_arg_type(&iter_array) != DBUS_TYPE_INVALID) {
        dbus_message_iter_get_basic(&iter_array, &test_struct->cdt.v2[i]);
        dbus_message_iter_next(&iter_array); // go to next argument of array
        i++;
    }

    return TRUE;
}

/*===========================================================================*\
 * global (external linkage) objects declarations
\*===========================================================================*/

/*===========================================================================*\
 * function forward declarations (external linkage)
\*===========================================================================*/

#endif /* _PING_COMMON_H_ */

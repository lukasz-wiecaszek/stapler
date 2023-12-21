/* SPDX-License-Identifier: MIT */
/**
 * @file common.h
 *
 * Common definitions and data types used both by client and server side.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#if defined(__cplusplus)
    #define LTS_EXTERN extern "C"
#else
    #define LTS_EXTERN extern
#endif

/*===========================================================================*\
 * system header files
\*===========================================================================*/

/*===========================================================================*\
 * project header files
\*===========================================================================*/

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define STPLR_DEVICENAME      "/dev/stplr-0"
#define PAGE_SIZE               4096
#define DIV_ROUND_UP(n,d)       (((n) + (d) - 1) / (d))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*===========================================================================*\
 * global types definitions
\*===========================================================================*/

/*===========================================================================*\
 * static (internal linkage) functions definitions
\*===========================================================================*/

/*===========================================================================*\
 * global (external linkage) objects declarations
\*===========================================================================*/

/*===========================================================================*\
 * function forward declarations (external linkage)
\*===========================================================================*/

#endif /* _COMMON_H_ */

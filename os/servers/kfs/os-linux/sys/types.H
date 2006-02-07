#ifndef __TYPES_H_
#define __TYPES_H_
/*****************************************************************************
 * Module Description: Include TYPES from machine specific file,
 * the basic types that should be defined are:
 *   sval          - signed value of the natural machine type
 *   uval          - unsigned value of the natural machine type,
 *                   is large enough to hold a pointer
 *   sval8/uval8   - signed/unsigned 8 bit quantities
 *   sval16/uval16 - signed/unsigned 16 bit quantities
 *   sval32/uval32 - signed/unsigned 32 bit quantities
 *   sval64/uval64 - signed/unsigned 64 bit quantities
 *
 ***************************************************************************/
#define _SIZEUVAL 8             /* size UVAL in bytes */
typedef long                    sval;
typedef signed char             sval8;
typedef short                   sval16;
typedef int                     sval32;
typedef long long               sval64;

typedef unsigned long           uval;
typedef unsigned char           uval8;
typedef unsigned short          uval16;
typedef unsigned int            uval32;
typedef unsigned long long      uval64;
#endif /* #ifndef __TYPES_H_ */

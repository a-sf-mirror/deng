/**
 * @file types.h
 * Types for libdeng.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_BASIC_TYPES_H
#define LIBDENG_BASIC_TYPES_H

#include <de/libdeng.h>

enum {
    VX = 0,
    VY = 1,
    VZ = 2
};

#ifndef __cplusplus
#  ifndef true
#     define true   1
#  endif
#  ifndef false
#     define false  0
#  endif
#endif

/*
 * Basic types.
 */

#ifndef _MSC_VER
#  include <stdint.h> // use C99 standard header (MSVC doesn't have C99)
#else
/* MSVC must define them ouselves.
ISO C9x Integer types - not all of them though, just what we need
If we need more the this, best of taking the header from MinGW for MSVC users */

/* 7.18.1.1  Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char   uint8_t;
typedef short  int16_t;
typedef unsigned short  uint16_t;
typedef int  int32_t;
typedef unsigned   uint32_t;
typedef long long  int64_t;
typedef unsigned long long   uint64_t;

/* 7.18.1.2  Minimum-width integer types */
typedef signed char int_least8_t;
typedef unsigned char   uint_least8_t;
typedef short  int_least16_t;
typedef unsigned short  uint_least16_t;
typedef int  int_least32_t;
typedef unsigned   uint_least32_t;
typedef long long  int_least64_t;
typedef unsigned long long   uint_least64_t;

/*  7.18.1.3  Fastest minimum-width integer types
 *  Not actually guaranteed to be fastest for all purposes
 *  Here we use the exact-width types for 8 and 16-bit ints.
 */
typedef char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef short  int_fast16_t;
typedef unsigned short  uint_fast16_t;
typedef int  int_fast32_t;
typedef unsigned  int  uint_fast32_t;
typedef long long  int_fast64_t;
typedef unsigned long long   uint_fast64_t;

/* 7.18.1.5  Greatest-width integer types */
typedef long long  intmax_t;
typedef unsigned long long   uintmax_t;

#endif

#ifndef MACOSX
typedef unsigned int    uint;
typedef unsigned short  ushort;
#endif
#ifdef UNIX
#  include <sys/types.h>
#endif

typedef unsigned char   byte;
typedef int32_t         fixed_t;
typedef uint32_t        angle_t;
typedef uint32_t        ident_t;
typedef int32_t         gameid_t;

typedef uint32_t        fontid_t;
typedef uint32_t        materialid_t;
typedef int             patchid_t;
typedef int32_t         spritenum_t;
typedef uint16_t        nodeindex_t;
typedef uint16_t        thid_t;
typedef double          timespan_t;

/// All points in the map coordinate space should be defined using this type.
typedef double  coord_t;

typedef int     ddboolean_t;
#define boolean ddboolean_t

// Pointer-integer conversion (used for legacy code).
#ifdef __64BIT__
typedef int64_t         int_from_pointer_t;
#else
typedef int32_t         int_from_pointer_t;
#endif
#define PTR2INT(x)      ( (int_from_pointer_t) ((void*)(x)) )
#define INT2PTR(type,x) ( (type*) ((int_from_pointer_t)(x)) )

/*
 * Limits.
 */

#define DDMAXCHAR   ((char)0x7f)
#define DDMAXSHORT  ((int16_t)0x7fff)
#define DDMAXUSHORT ((uint16_t)0xffff)
#define DDMAXINT    ((int32_t)0x7fffffff)   ///< Max positive 32-bit int
#define DDMAXUINT   ((uint32_t)0xffffffff)  ///< Max positive 32-bit unsigned int
#define DDMAXLONG   ((int32_t)0x7fffffff)
#define DDMAXFLOAT  ((float)1E+37)

#define DDMINCHAR   ((char)0x80)
#define DDMINSHORT  ((int16_t)0x8000)
#define DDMININT    ((int32_t)0x80000000)   ///< Min negative 32-bit integer
#define DDMINLONG   ((int32_t)0x80000000)
#define DDMINFLOAT  ((float)-(1E+37))

#endif // LIBDENG_BASIC_TYPES_H

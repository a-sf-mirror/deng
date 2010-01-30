/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file deng.h
 *
 * Contains common definitions, constants, and types that will be used by libdeng2. 
 * The deng types should be used instead of the standard C++ types.
 */ 

#ifndef LIBDENG2_H
#define LIBDENG2_H

#include <cassert>
#include <typeinfo>
#include <memory>

#ifdef WIN32
#   ifdef LIBDENG2_EXPORTS
#       define LIBDENG2_API __declspec(dllexport)
#   else
#       define LIBDENG2_API __declspec(dllimport)
#   endif
#   define LIBDENG2_EXPORT __declspec(dllexport)

    // Disable warnings about non-exported (C++ standard library) base classes.
#   pragma warning(disable: 4275)
#   pragma warning(disable: 4251)
    // Disable warning about using this pointer in initializer list.
#   pragma warning(disable: 4355)

#   if defined(_MSC_VER)
        // Microsoft Visual C++ lacks stdint.h, so define the integer types manually.
        typedef signed __int8       int8_t;
        typedef unsigned __int8     uint8_t;
        typedef signed __int16      int16_t;
        typedef unsigned __int16    uint16_t;
        typedef signed __int32      int32_t;
        typedef unsigned __int32    uint32_t;
        typedef signed __int64      int64_t;
        typedef unsigned __int64    uint64_t;
#   endif
#else
#   define LIBDENG2_API
#   define LIBDENG2_EXPORT

    // Standard integer types.
#   include <stdint.h>
#endif

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define TYPE_NAME(x) (typeid(x).name())

/**
 * Macro for iterating through a container.
 *
 * @param Var           Name of the iterator variable.
 * @param ContainerRef  Container.
 * @param IterClass     Class of the iterator.
 */
#define FOR_EACH(Var, ContainerRef, IterClass) \
    for(IterClass Var = ContainerRef.begin(); Var != ContainerRef.end(); ++Var)

/**
 * Macro for iterating through a container in reverse.
 *
 * @param Var           Name of the iterator variable.
 * @param ContainerRef  Container.
 * @param IterClass     Class of the iterator.
 */
#define FOR_EACH_REVERSE(Var, ContainerRef, IterClass) \
    for(IterClass Var = ContainerRef.rbegin(); Var != ContainerRef.rend(); ++Var)

#define FOREVER for(;;)

/**
 * @namespace de
 *
 * The @c de namespace contains all the classes, functions and other
 * identifiers of libdeng2.
 */
namespace de 
{
    /**
     * @defgroup types Basic Types
     * 
     * Basic data types.
     */
    
    //@{
    /// @ingroup types
    typedef int8_t                  dchar;      ///< 8-bit signed integer.
    typedef uint8_t                 dbyte;      ///< 8-bit unsigned integer.
    typedef uint8_t                 duchar;     ///< 8-bit unsigned integer.
    typedef dchar                   dint8;      ///< 8-bit signed integer.
    typedef dbyte                   duint8;     ///< 8-bit unsigned integer.
    typedef int16_t                 dint16;     ///< 16-bit signed integer.
    typedef uint16_t                duint16;    ///< 16-bit unsigned integer.
    typedef dint16                  dshort;     ///< 16-bit signed integer.
    typedef duint16                 dushort;    ///< 16-bit unsigned integer.
    typedef int32_t                 dint32;     ///< 32-bit signed integer.
    typedef uint32_t                duint32;    ///< 32-bit unsigned integer.
    typedef dint32                  dint;       ///< 32-bit signed integer.
    typedef duint32                 duint;      ///< 32-bit unsigned integer.
    typedef int64_t                 dint64;     ///< 64-bit signed integer.
    typedef uint64_t                duint64;    ///< 64-bit unsigned integer.
    typedef float                   dfloat;     ///< 32-bit floating point number.
    typedef double                  ddouble;    ///< 64-bit floating point number.
    typedef size_t                  dsize;
    //@}
}

#include "Error"
#include "version.h"
#include "math.h"

#endif /* LIBDENG2_H */

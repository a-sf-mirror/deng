/*
 * The Doomsday Engine Project -- wadconverter
 *
 * Copyright © 2006-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBWADCONVERTER_H
#define LIBWADCONVERTER_H

#include <de/deng.h>

#ifdef WIN32
#   ifdef LIBWADCONVERTER_EXPORTS
#       define LIBWADCONVERTER_API __declspec(dllexport)
#   else
#       define LIBWADCONVERTER_API __declspec(dllimport)
#   endif
#   define LIBWADCONVERTER_EXPORT __declspec(dllexport)
#else
#   define LIBWADCONVERTER_API
#   define LIBWADCONVERTER_EXPORT
#endif

// Versioning.
#ifdef LIBWADCONVERTER_RELEASE_LABEL
#   define LIBWADCONVERTER_VERSION LIBDENG2_VER4( \
        LIBWADCONVERTER_RELEASE_LABEL, \
        LIBWADCONVERTER_MAJOR_VERSION, \
        LIBWADCONVERTER_MINOR_VERSION, \
        LIBWADCONVERTER_PATCHLEVEL)
#else
#   define LIBWADCONVERTER_VERSION LIBDENG2_VER3( \
        LIBWADCONVERTER_MAJOR_VERSION, \
        LIBWADCONVERTER_MINOR_VERSION, \
        LIBWADCONVERTER_PATCHLEVEL)
#endif

namespace wadconverter
{
    typedef unsigned int lumpnum_t;

    void LoadANIMATED(lumpnum_t lump);
    void LoadANIMDEFS(lumpnum_t lump);
}

#endif /* LIBWADCONVERTER_H */

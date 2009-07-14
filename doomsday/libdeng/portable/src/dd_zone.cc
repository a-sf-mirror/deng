/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * dd_zone.cc: Memory Zone Wrappers
 */

#include <de/App>
#include <de/Zone>

using namespace de;

extern "C" {
#include "de_base.h"
}
#include "doomsday.h"

void* _DECALL Z_Malloc(size_t size, int tag, void* user)
{
    return App::memory().alloc(size, Zone::PurgeTag(tag), user);
}

void* _DECALL Z_Calloc(size_t size, int tag, void* user)
{
    return App::memory().allocClear(size, Zone::PurgeTag(tag), user);
}

void* Z_Realloc(void* ptr, size_t n, int mallocTag)
{
    return App::memory().resize(ptr, n, Zone::PurgeTag(mallocTag));
}

void* Z_Recalloc(void* ptr, size_t n, int callocTag)
{
    return App::memory().resizeClear(ptr, n, Zone::PurgeTag(callocTag));
}

void _DECALL Z_Free(void* ptr)
{
    App::memory().free(ptr);
}

void Z_FreeTags(int lowTag, int highTag)
{
    App::memory().purgeRange(Zone::PurgeTag(lowTag), Zone::PurgeTag(highTag));
}

void Z_ChangeTag2(void* ptr, int tag)
{
    App::memory().setTag(ptr, Zone::PurgeTag(tag));
}

void Z_CheckHeap(void)
{
    App::memory().verify();
}

void Z_EnableFastMalloc(boolean isEnabled)
{
    App::memory().enableFastMalloc(isEnabled != 0);
}

void Z_ChangeUser(void* ptr, void* newUser)
{
    App::memory().setUser(ptr, newUser);
}

void* Z_GetUser(void* ptr)
{
    return App::memory().getUser(ptr);
}

int Z_GetTag(void* ptr)
{
    return App::memory().getTag(ptr);
}

size_t Z_FreeMemory(void)
{
    return App::memory().availableMemory();
}

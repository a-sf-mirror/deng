/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_think.c: Thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct thinkerlist_s {
    boolean         isPublic; /* @c true = all thinkers in this list are
                                 visible publically */
    thinker_t       thinkerCap;
} thinkerlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void initThinkerList(thinkerlist_t* list)
{
    list->thinkerCap.prev = list->thinkerCap.next = &list->thinkerCap;
}

static void addThinkerToList(thinkerlist_t* list, thinker_t* th)
{
    // Link the thinker to the thinker list.
    list->thinkerCap.prev->next = th;
    th->next = &list->thinkerCap;
    th->prev = list->thinkerCap.prev;
    list->thinkerCap.prev = th;
}

static void removeThinkerFromList(thinkerlist_t* list, thinker_t* th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static thinkerlist_t* listForThinkFunc(thinkers_t* thinkers, think_t func, boolean isPublic,
                                       boolean canCreate)
{
    size_t i;

    for(i = 0; i < thinkers->numLists; ++i)
    {
        thinkerlist_t* list = thinkers->lists[i];

        if(list->thinkerCap.function == func && list->isPublic == isPublic)
            return list;
    }

    if(!canCreate)
        return NULL;

    // A new thinker type.
    {
    thinkerlist_t* list;

    thinkers->lists = Z_Realloc(thinkers->lists,
        sizeof(thinkerlist_t*) * ++thinkers->numLists, PU_STATIC);
    thinkers->lists[thinkers->numLists-1] = list =
        Z_Calloc(sizeof(thinkerlist_t), PU_STATIC, 0);

    initThinkerList(list);
    list->isPublic = isPublic;
    list->thinkerCap.function = func;
    // Set the list sentinel to instasis (safety measure).
    list->thinkerCap.inStasis = true;

    return list;
    }
}

static boolean iterateThinkerList(thinkerlist_t* list,
                                  int (*callback) (void* p, void*),
                                  void* context)
{
    boolean result = true;

    if(list && callback)
    {
        thinker_t* th, *next;

        th = list->thinkerCap.next;
        while(th != &list->thinkerCap && th)
        {
#ifdef FAKE_MEMORY_ZONE
            assert(th->next != NULL);
            assert(th->prev != NULL);
#endif

            next = th->next;
            if((result = callback(th, context)) == 0)
                break;
            th = next;
        }
    }

    return result;
}

thinkers_t* P_CreateThinkers(void)
{
    thinkers_t* thinkers = Z_Malloc(sizeof(*thinkers), PU_STATIC, 0);

    thinkers->inited = false;
    memset(thinkers->idtable, 0, sizeof(thinkers->idtable));
    thinkers->iddealer = 0;
    thinkers->lists = NULL;
    thinkers->numLists = 0;

    return thinkers;
}

static int destroyThinker(void* p, void* context)
{
    Z_Free(p);
    return true; // Continue iteration.
}

void P_DestroyThinkers(thinkers_t* thinkers)
{
    size_t i;

    assert(thinkers);

    for(i = 0; i < thinkers->numLists; ++i)
    {
        thinkerlist_t* list = thinkers->lists[i];

        iterateThinkerList(list, destroyThinker, NULL);
        Z_Free(list);
    }
    Z_Free(thinkers->lists);
    Z_Free(thinkers);
}

void Thinkers_Init(thinkers_t* thinkers, byte flags)
{
    assert(thinkers);

    if(!thinkers->inited)
    {
        thinkers->numLists = 0;
        thinkers->lists = NULL;
    }
    else
    {
        size_t i;

        for(i = 0; i < thinkers->numLists; ++i)
        {
            thinkerlist_t* list = thinkers->lists[i];

            if(list->isPublic && !(flags & ITF_PUBLIC))
                continue;
            if(!list->isPublic && !(flags & ITF_PRIVATE))
                continue;

            initThinkerList(list);
        }
    }
    thinkers->inited = true;

    Thinkers_ClearMobjIDs(thinkers);
}

boolean Thinkers_Inited(thinkers_t* thinkers)
{
    assert(thinkers);
    return thinkers->inited;
}

boolean Thinkers_IsUsedMobjID(thinkers_t* thinkers, thid_t id)
{
    assert(thinkers);

    return thinkers->idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void Thinkers_SetMobjID(thinkers_t* thinkers, thid_t id, boolean state)
{
    int c, bit;

    assert(thinkers);

    c = id >> 5; bit = 1 << (id & 31); //(id % 32);
    if(state)
        thinkers->idtable[c] |= bit;
    else
        thinkers->idtable[c] &= ~bit;
}

void Thinkers_ClearMobjIDs(thinkers_t* thinkers)
{
    assert(thinkers);

    memset(thinkers->idtable, 0, sizeof(thinkers->idtable));
    thinkers->idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

void Thinkers_Add(thinkers_t* thinkers, thinker_t* th, boolean makePublic)
{
    assert(thinkers);
    assert(th);

    addThinkerToList(listForThinkFunc(thinkers, th->function, makePublic, true), th);
}

void Thinkers_Remove(thinkers_t* thinkers, thinker_t* th)
{
    assert(thinkers);
    assert(th);

    // @todo thinker should return the list it is in.
    removeThinkerFromList(NULL, th);
}

boolean Thinkers_Iterate(thinkers_t* thinkers, think_t func, byte flags,
                         int (*callback) (void* p, void*), void* context)
{
    assert(thinkers);

    if(!thinkers->inited || !callback)
        return true;

    if(func != NULL)
    {   // We might have both public and shared lists for this type.
        boolean result = true;

        if(flags & ITF_PUBLIC)
            result = iterateThinkerList(listForThinkFunc(thinkers, func, true, false),
                                        callback, context);
        if(result && (flags & ITF_PRIVATE))
            result = iterateThinkerList(listForThinkFunc(thinkers, func, false, false),
                                        callback, context);
        return result;
    }

    {
    boolean result = true;
    size_t i;

    for(i = 0; i < thinkers->numLists; ++i)
    {
        thinkerlist_t* list = thinkers->lists[i];

        if(list->isPublic && !(flags & ITF_PUBLIC))
            continue;
        if(!list->isPublic && !(flags & ITF_PRIVATE))
            continue;

        if((result = iterateThinkerList(list, callback, context)) == 0)
            break;
    }
    return result;
    }
}

/**
 * Returns the map of the thinker.
 * @note Part of the Doomsday public API.
 */
map_t* Thinker_Map(thinker_t* th)
{
    assert(th);
    return th->_map;
}

/**
 * Sets the map of the thinker. Every thinker is in at most one map.
 *
 * @param map  Map.
 */
void Thinker_SetMap(thinker_t* th, map_t* map)
{
    assert(th);
    th->_map = map;
}

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param th            The thinker to change.
 * @param on            @c true, put into stasis.
 */
void Thinker_SetStasis(thinker_t* th, boolean on)
{
    assert(th);
    Map_ThinkerSetStasis(Thinker_Map(th), th, on);
}

/**
 * @todo Does not belong in this file?
 */
boolean P_IsMobjThinker(thinker_t* th, void* context)
{
    if(th && th->function && th->function == gx.MobjThinker)
        return true;

    return false;
}

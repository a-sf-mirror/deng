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

typedef struct {
    boolean         isPublic; /* @c true = all thinkers in this list are
                                 visible publically */
    thinker_t       thinkerCap;
} thinkerlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int idtable[2048]; // 65536 bits telling which IDs are in use.
unsigned short iddealer = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static size_t numThinkerLists;
static thinkerlist_t** thinkerLists;
static boolean inited = false;

// CODE --------------------------------------------------------------------

static thid_t newMobjID(map_t* map)
{
    // Increment the ID dealer until a free ID is found.
    // \fixme What if all IDs are in use? 65535 thinkers!?
    while(P_IsUsedMobjID(map, ++iddealer));
    // Mark this ID as used.
    P_SetMobjID(map, iddealer, true);
    return iddealer;
}

void P_ClearMobjIDs(map_t* map)
{
    memset(idtable, 0, sizeof(idtable));
    idtable[0] |= 1; // ID zero is always "used" (it's not a valid ID).
}

boolean P_IsUsedMobjID(map_t* map, thid_t id)
{
    return idtable[id >> 5] & (1 << (id & 31) /*(id % 32) */ );
}

void P_SetMobjID(map_t* map, thid_t id, boolean state)
{
    int c = id >> 5, bit = 1 << (id & 31); //(id % 32);

    if(state)
        idtable[c] |= bit;
    else
        idtable[c] &= ~bit;
}

static void linkThinkerToList(thinker_t* th, thinkerlist_t* list)
{
    // Link the thinker to the thinker list.
    list->thinkerCap.prev->next = th;
    th->next = &list->thinkerCap;
    th->prev = list->thinkerCap.prev;
    list->thinkerCap.prev = th;
}

static void unlinkThinkerFromList(thinker_t* th)
{
    th->next->prev = th->prev;
    th->prev->next = th->next;
}

static void initThinkerList(thinkerlist_t* list)
{
    list->thinkerCap.prev = list->thinkerCap.next = &list->thinkerCap;
}

static thinkerlist_t* listForThinkFunc(map_t* map, think_t func, boolean isPublic,
                                       boolean canCreate)
{
    size_t i;

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinkerlist_t* list = thinkerLists[i];

        if(list->thinkerCap.function == func && list->isPublic == isPublic)
            return list;
    }

    if(!canCreate)
        return NULL;

    // A new thinker type.
    {
    thinkerlist_t* list;

    thinkerLists = Z_Realloc(thinkerLists, sizeof(thinkerlist_t*) *
                             ++numThinkerLists, PU_STATIC);
    thinkerLists[numThinkerLists-1] = list =
        Z_Calloc(sizeof(thinkerlist_t), PU_STATIC, 0);

    initThinkerList(list);
    list->isPublic = isPublic;
    list->thinkerCap.function = func;
    // Set the list sentinel to instasis (safety measure).
    list->thinkerCap.inStasis = true;

    return list;
    }
}

static int runThinker(void* p, void* context)
{
    thinker_t* th = (thinker_t*) p;

    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        if(th->function == (think_t) -1)
        {
            // Time to remove it.
            unlinkThinkerFromList(th);

            if(th->id)
            {   // Its a mobj.
                P_MobjRecycle((mobj_t*) th);
            }
            else
            {
                Z_Free(th);
            }
        }
        else if(th->function)
        {
            th->function(th);
        }
    }

    return true; // Continue iteration.
}

static boolean iterateThinkers(thinkerlist_t* list,
                               int (*callback) (void* p, void*),
                               void* context)
{
    boolean result = true;

    if(list)
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

/**
 * @param th            Thinker to be added.
 * @param makePublic    If @c true, this thinker will be visible publically
 *                      via the Doomsday public API thinker interface(s).
 */
void P_ThinkerAdd(thinker_t* th, boolean makePublic)
{
    map_t* map = P_CurrentMap();

    if(!map)
        return;

    if(!th)
        return;

    if(!th->function)
    {
        Con_Error("P_ThinkerAdd: Invalid thinker function.");
    }

    // Will it need an ID?
    if(P_IsMobjThinker(th, NULL))
    {
        // It is a mobj, give it an ID.
        th->id = newMobjID(map);
    }
    else
    {
        // Zero is not a valid ID.
        th->id = 0;
    }

    // Link the thinker to the thinker list.
    linkThinkerToList(th, listForThinkFunc(map, th->function, makePublic, true));
}

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 */
void P_ThinkerRemove(thinker_t* th)
{
    map_t* map = P_CurrentMap();

    if(!th)
        return;

    if(map)
    {
        // Has got an ID?
        if(th->id)
        {   // Then it must be a mobj.
            mobj_t* mo = (mobj_t *) th;

            // Flag the ID as free.
            P_SetMobjID(map, th->id, false);

            // If the state of the mobj is the NULL state, this is a
            // predictable mobj removal (result of animation reaching its
            // end) and shouldn't be included in netGame deltas.
            if(!isClient)
            {
                if(!mo->state || mo->state == states)
                {
                    Sv_MobjRemoved(map, th->id);
                }
            }
        }
    }

    th->function = (think_t) - 1;
}

boolean P_IsMobjThinker(thinker_t* th, void* context)
{
    if(th && th->function && th->function == gx.MobjThinker)
        return true;

    return false;
}

/**
 * Init the thinker lists.
 *
 * @params flags        0x1 = Init public thinkers.
 *                      0x2 = Init private (engine-internal) thinkers.
 */
void P_InitThinkerLists(map_t* map, byte flags)
{
    if(!inited)
    {
        numThinkerLists = 0;
        thinkerLists = NULL;
    }
    else
    {
        size_t i;

        for(i = 0; i < numThinkerLists; ++i)
        {
            thinkerlist_t* list = thinkerLists[i];

            if(list->isPublic && !(flags & ITF_PUBLIC))
                continue;
            if(!list->isPublic && !(flags & ITF_PRIVATE))
                continue;

            initThinkerList(list);
        }
    }
    inited = true;

    P_ClearMobjIDs(map);
}

boolean P_ThinkerListInited(map_t* map)
{
    return inited;
}

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param func          If not @c NULL, only make a callback for objects whose
 *                      thinker matches.
 * @param flags         Thinker filter flags.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean P_IterateThinkers(map_t* map, think_t func, byte flags,
                          int (*callback) (void* p, void*), void* context)
{
    if(!map)
        return true;

    if(!inited)
        return true;

    if(func != NULL)
    {   // We might have both public and shared lists for this type.
        boolean result = true;

        if(flags & ITF_PUBLIC)
            result = iterateThinkers(listForThinkFunc(map, func, true, false),
                                     callback, context);
        if(result && (flags & ITF_PRIVATE))
            result = iterateThinkers(listForThinkFunc(map, func, false, false),
                                     callback, context);
        return result;
    }

    {
    boolean result = true;
    size_t i;

    for(i = 0; i < numThinkerLists; ++i)
    {
        thinkerlist_t* list = thinkerLists[i];

        if(list->isPublic && !(flags & ITF_PUBLIC))
            continue;
        if(!list->isPublic && !(flags & ITF_PRIVATE))
            continue;

        if((result = iterateThinkers(list, callback, context)) == 0)
            break;
    }
    return result;
    }
}

/**
 * Part of the Doomsday public API.
 */
void DD_InitThinkers(void)
{
    P_InitThinkerLists(P_CurrentMap(), ITF_PUBLIC); // Init the public thinker lists.
}

/**
 * Part of the Doomsday public API.
 */
void DD_RunThinkers(void)
{
    P_IterateThinkers(P_CurrentMap(), NULL, ITF_PUBLIC | ITF_PRIVATE, runThinker, NULL);
}

/**
 * Part of the Doomsday public API.
 */
int DD_IterateThinkers(think_t func, int (*callback) (void* p, void* ctx),
                       void* context)
{
    return P_IterateThinkers(P_CurrentMap(), func, ITF_PUBLIC, callback, context);
}

/**
 * Adds a new thinker to the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerAdd(thinker_t* th)
{
    P_ThinkerAdd(th, true); // This is a public thinker.
}

/**
 * Removes a thinker from the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerRemove(thinker_t* th)
{
    P_ThinkerRemove(th);
}

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param th            The thinker to change.
 * @param on            @c true, put into stasis.
 */
void DD_ThinkerSetStasis(thinker_t* th, boolean on)
{
    if(th)
    {
        th->inStasis = on;
    }
}

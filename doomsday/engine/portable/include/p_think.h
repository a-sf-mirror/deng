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
 * p_think.h: Thinkers
 */

#ifndef DOOMSDAY_THINKERS_H
#define DOOMSDAY_THINKERS_H

/**
 * Thinkers.
 *
 * @ingroup map
 */
typedef struct {
    boolean         inited;
    int             idtable[2048]; // 65536 bits telling which IDs are in use.
    unsigned short  iddealer;

    size_t          numLists;
    struct thinkerlist_s** lists;
} thinkers_t;

thinkers_t*     P_CreateThinkers(void);
void            P_DestroyThinkers(thinkers_t* thinkers);

void            Thinkers_Init(thinkers_t* thinkers, byte flags);
boolean         Thinkers_Inited(thinkers_t* thinkers);

void            Thinkers_SetMobjID(thinkers_t* thinkers, thid_t id, boolean state);
boolean         Thinkers_IsUsedMobjID(thinkers_t* thinkers, thid_t id);
void            Thinkers_ClearMobjIDs(thinkers_t* thinkers);

void            Thinkers_Add(thinkers_t* thinkers, thinker_t* th, boolean makePublic);
void            Thinkers_Remove(thinkers_t* thinkers, thinker_t* th);

boolean         Thinkers_Iterate(thinkers_t* thinkers, think_t func, byte flags,
                                 int (*callback) (void* p, void*), void* context);

struct map_s*   Thinker_Map(thinker_t* th);
void            Thinker_SetMap(thinker_t* th, struct map_s* map);

// @todo Does not belong in this file?
boolean         P_IsMobjThinker(thinker_t* th, void*);

#endif /* DOOMSDAY_THINKERLIST_H */

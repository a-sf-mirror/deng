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

#ifndef __DOOMSDAY_THINKER_H__
#define __DOOMSDAY_THINKER_H__

void            P_InitThinkerLists(byte flags);
boolean         P_ThinkerListInited(void);

/**
 * @defgroup iterateThinkerFlags Iterate Thinker Flags
 * Used with P_IterateThinkers to specify which thinkers to iterate.
 */
/*@{*/
#define ITF_PUBLIC          0x1
#define ITF_PRIVATE         0x2
/*@}*/

boolean         P_IterateThinkers(think_t func, byte flags,
                                  int (*callback) (void* p, void*),
                                  void* context);

void            P_ThinkerAdd(thinker_t* th, boolean makePublic);
void            P_ThinkerRemove(thinker_t* th);

void            P_SetMobjID(thid_t id, boolean state);
boolean         P_IsUsedMobjID(thid_t id);

boolean         P_IsMobjThinker(thinker_t* th, void*);

// Public interface:
void            DD_InitThinkers(void);
void            DD_RunThinkers(void);
void            DD_ThinkerAdd(thinker_t* th);
void            DD_ThinkerRemove(thinker_t* th);
void            DD_ThinkerSetStasis(thinker_t* th, boolean on);
#endif

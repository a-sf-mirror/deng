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
 * r_objlink.h: Objlink management.
 */

#ifndef DOOMSDAY_OBJLINK_H
#define DOOMSDAY_OBJLINK_H

typedef enum {
    OT_MOBJ,
    OT_LUMOBJ,
    OT_PARTICLE,
    NUM_OBJ_TYPES
} objtype_t;

void            R_InitObjLinksForMap(struct map_s* map);
void            R_ClearObjLinksForFrame(struct map_s* map);

void            R_ObjLinkCreate(void* obj, objtype_t type);
void            R_LinkObjs(struct map_s* map);
void            R_InitForSubsector(subsector_t* subsector);
void            R_InitForNewFrame(struct map_s* map);

typedef struct {
    void*               obj;
    objtype_t           type;
} linkobjtosubSectorparams_t;

boolean         RIT_LinkObjToSubsector(subsector_t* subsector, void* params);

boolean         R_IterateSubsectorContacts(subsector_t* subsector, objtype_t type,
                                           boolean (*func) (void*, void*),
                                           void* data);
#endif /* DOOMSDAY_OBJLINK_H */

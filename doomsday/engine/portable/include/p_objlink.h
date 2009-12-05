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
 * p_objlink.h: Objlink blockmap management.
 */

#ifndef DOOMSDAY_OBJLINK_H
#define DOOMSDAY_OBJLINK_H

typedef enum {
    OT_FIRST = 0,
    OT_MOBJ = OT_FIRST,
    OT_LUMOBJ,
    OT_PARTICLE,
    NUM_OBJ_TYPES
} objtype_t;

typedef struct objlink_s {
    struct objlink_s* nextInBlock; // Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    struct objlink_s* next; // Next in list of ALL objlinks.
    objtype_t       type;
    void*           obj;
} objlink_t;

typedef struct objblock_s {
    objlink_t*      head;

    // Used to prevent multiple processing of a block during one refresh frame.
    boolean         doneSpread;
} objblock_t;

typedef struct objblockmap_s {
    objblock_t*     blocks;
    fixed_t         origin[2];
    int             width, height; // In blocks.
} objblockmap_t;

typedef struct objcontact_s {
    struct objcontact_s* next; // Next in the subsector.
    struct objcontact_s* nextUsed; // Next used contact.
    void*           obj;
} objcontact_t;

typedef struct objcontactlist_s {
    objcontact_t*   head[NUM_OBJ_TYPES];
} objcontactlist_t;

objblockmap_t*  P_CreateObjBlockmap(float originX, float originY, int width,
                                    int height);
void            P_DestroyObjBlockmap(objblockmap_t* obm);

void            ObjBlockmap_AddLinks(objblockmap_t* obm, objlink_t* objLinks);
void            ObjBlockmap_Clear(objblockmap_t* obm);

objlink_t*      P_CreateObjLink(void* obj, objtype_t type);

void            Subsector_SpreadObjLinks(subsector_t* subsector);

#endif /* DOOMSDAY_OBJLINK_H */

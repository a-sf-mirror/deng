/**
 * @file p_objlink.h
 * Objlink management. @ingroup map
 *
 * Object => BspLeaf contacts and object => BspLeaf spreading.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_OBJLINK_BLOCKMAP_H
#define LIBDENG_OBJLINK_BLOCKMAP_H

#include "p_mapdata.h"
#include "r_lumobjs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OT_MOBJ,
    OT_LUMOBJ,
    NUM_OBJ_TYPES
} objtype_t;

#define VALID_OBJTYPE(val) ((val) >= OT_MOBJ && (val) < NUM_OBJ_TYPES)

/**
 * To be called before a map change to destroy any existing objlink blockmaps.
 */
void R_DestroyObjlinkBlockmaps(void);

/**
 * Construct the objlink blockmap for the current map.
 */
void R_InitObjlinkBlockmapForMap(void);

/**
 * Initialize the object => BspLeaf contact lists, ready for linking to
 * objects. To be called at the beginning of a new world frame.
 */
void R_InitForNewFrame(void);

/**
 * To be called at the begining of a render frame to clear the objlink
 * blockmap prior to linking objects for the new viewer.
 */
void R_ClearObjlinksForFrame(void);

/**
 * Create a new mobj object link in the objlink blockmap.
 */
void R_CreateMobjLink(mobj_t* mobj);

/**
 * Create a new lumobj object link in the objlink blockmap.
 */
void R_CreateLumobjLink(lumobj_t* lumobj);

/**
 * To be called at the beginning of a render frame to link all objects
 * into the objlink blockmap.
 */
void R_LinkObjs(void);

/**
 * Spread object => BSP leaf contacts for the given @a BspLeaf.
 * @note All contacts for all object types will be spread at this time.
 */
void R_InitForBspLeaf(BspLeaf* bspLeaf);

/**
 * Create a new mobj => BSP leaf contact in the objlink blockmap.
 */
void R_MobjContactBspLeaf(mobj_t* mobj, BspLeaf* bspLeaf);

/**
 * Create a new lumobj => BSP leaf contact in the objlink blockmap.
 */
void R_LumobjContactBspLeaf(lumobj_t* lumobj, BspLeaf* bspLeaf);

/**
 * Traverse the list of objects of the specified @a type which have been linked
 * with @a bspLeaf for the current render frame.
 */
int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*func) (void* object, void* parameters), void* parameters);
int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*func) (void* object, void* parameters)); /*parameters=NULL*/

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_OBJLINK_BLOCKMAP_H */

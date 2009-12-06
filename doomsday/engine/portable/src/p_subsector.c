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
 * r_subsector.c: World subsectors.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct contactfinder_data_s {
    void*           obj;
    objcontacttype_t objType;
    vec3_t          objPos;
    float           objRadius;
    float           box[4];
} contactfinderparams_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void processSeg(hedge_t* seg, void* data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Attempt to spread the obj from the given contact from the source
 * subsector and into the (relative) back subsector.
 *
 * @param subsector     Ptr to subsector to attempt to spread over to.
 * @param data          Ptr to contactfinder_data structure.
 *
 * @return              @c true, because this function is also used as an
 *                      iterator.
 */
static void spreadInSubsector(subsector_t* subsector, void* data)
{
    hedge_t* hEdge;

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            processSeg(hEdge, data);
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
}

static void processSeg(hedge_t* hEdge, void* data)
{
    contactfinderparams_t* params = (contactfinderparams_t*) data;
    subsector_t* src, *dst;
    float distance;
    vertex_t* vtx;
    seg_t* seg = (seg_t*) hEdge->data;

    // Can not spread over one-sided lines.
    if(!HE_BACKSECTOR(hEdge))
        return;

    // Which way does the spread go?
    if(HE_FRONTSUBSECTOR(hEdge)->validCount == validCount &&
       HE_BACKSUBSECTOR(hEdge)->validCount != validCount)
    {
        src = (subsector_t*) hEdge->face->data;
        dst = (subsector_t*) hEdge->twin->face->data;
    }
    else
    {
        // Not eligible for spreading.
        return;
    }

    // Is the dst subSector inside the objlink's AABB?
    if(dst->bBox[1][VX] <= params->box[BOXLEFT] ||
       dst->bBox[0][VX] >= params->box[BOXRIGHT] ||
       dst->bBox[1][VY] <= params->box[BOXBOTTOM] ||
       dst->bBox[0][VY] >= params->box[BOXTOP])
    {
        // The subSector is not inside the params's bounds.
        return;
    }

    // Can the spread happen?
    if(seg->sideDef)
    {
        if(dst->sector)
        {
            if(dst->sector->planes[PLN_CEILING]->height <= dst->sector->planes[PLN_FLOOR]->height ||
               dst->sector->planes[PLN_CEILING]->height <= src->sector->planes[PLN_FLOOR]->height ||
               dst->sector->planes[PLN_FLOOR]->height   >= src->sector->planes[PLN_CEILING]->height)
            {
                // No; destination sector is closed with no height.
                return;
            }
        }

        // Don't spread if the middle material completely fills the gap between
        // floor and ceiling (direction is from dst to src).
        if(R_DoesMiddleMaterialFillGap(seg->sideDef->lineDef,
            dst == ((subsector_t*) hEdge->twin->face->data)? false : true))
            return;
    }

    // Calculate 2D distance to hEdge.
    {
    float dx, dy;

    dx = hEdge->HE_v2->pos[VX] - hEdge->HE_v1->pos[VX];
    dy = hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY];
    vtx = hEdge->HE_v1;
    distance = ((vtx->pos[VY] - params->objPos[VY]) * dx -
                (vtx->pos[VX] - params->objPos[VX]) * dy) / seg->length;
    }

    if(seg->sideDef)
    {
        if((src == ((subsector_t*) hEdge->face->data) && distance < 0) ||
           (src == ((subsector_t*) hEdge->twin->face->data) && distance > 0))
        {
            // Can't spread in this direction.
            return;
        }
    }

    // Check distance against the obj radius.
    if(distance < 0)
        distance = -distance;
    if(distance >= params->objRadius)
    {   // The obj doesn't reach that far.
        return;
    }

    // During next step, obj will continue spreading from there.
    dst->validCount = validCount;

    // Add this obj to the destination subsector.
    // @todo Subsector should return the map its linked to.
    Map_AddSubsectorContact(P_CurrentMap(), P_ObjectRecord(DMU_SUBSECTOR, dst)->id - 1, params->objType, params->obj);
    spreadInSubsector(dst, data);
}

/**
 * Create a contact for the objlink in all the subsectors the linked obj
 * is contacting (tests done on bounding boxes and the subsector spread
 * test).
 *
 * @param oLink         Ptr to objlink to find subsector contacts for.
 */
static void findContacts(objcontacttype_t type, void* obj)
{
#define SUBSECTORSPREAD_MINRADIUS 16 // In world units.

    contactfinderparams_t params;
    subsector_t* subsector;

    switch(type)
    {
    case OCT_LUMOBJ:
        {
        lumobj_t* lum = (lumobj_t*) obj;

        if(lum->type != LT_OMNI)
            return; // Only omni lights spread.

        V3_Copy(params.objPos, lum->pos);
        // Use a slightly smaller radius than what the obj really is.
        params.objRadius = LUM_OMNI(lum)->radius * .9f;
        subsector = lum->subsector;
        break;
        }
    case OCT_MOBJ:
        {
        mobj_t* mo = (mobj_t*) obj;

        V3_Copy(params.objPos, mo->pos);
        params.objRadius = R_VisualRadius(mo);
        subsector = (subsector_t*) ((objectrecord_t*) mo->subsector)->obj;
        break;
        }
    case OCT_PARTICLE:
        {
        particle_t* pt = (particle_t*) obj;

        V3_SetFixed(params.objPos, pt->pos[VX], pt->pos[VY], pt->pos[VZ]);
        // Use a slightly smaller radius than what the obj really is.
        params.objRadius = FIX2FLT(pt->gen->stages[pt->stage].radius) * .9f;
        subsector = pt->subsector;
        break;
        }
    default:
        Con_Error("Internal Error: Invalid value (%i) for objtype "
                  "in findContacts.", (int) type);
        return; // Unreachable.
    }

    params.obj = obj;
    params.objType = type;

    // Always contact the obj's own subsector.
    // @todo Subsector should return the map its linked to.
    Map_AddSubsectorContact(P_CurrentMap(), P_ObjectRecord(DMU_SUBSECTOR, subsector)->id - 1, type, obj);
    subsector->validCount = ++validCount;

    if(params.objRadius < SUBSECTORSPREAD_MINRADIUS)
        return;

    // Do the subsector spread. Begin from the obj's own subsector.
    params.box[BOXLEFT]   = params.objPos[VX] - params.objRadius;
    params.box[BOXRIGHT]  = params.objPos[VX] + params.objRadius;
    params.box[BOXBOTTOM] = params.objPos[VY] - params.objRadius;
    params.box[BOXTOP]    = params.objPos[VY] + params.objRadius;

    spreadInSubsector(subsector, &params);

#undef SUBSECTORSPREAD_MINRADIUS
}

static boolean PTR_SpreadContacts(void* obj, void* context)
{
    objcontacttype_t type = *((objcontacttype_t*)context);

    switch(type)
    {
    case OCT_LUMOBJ:
        if(((lumobj_t*) obj)->spreadFrameCount == frameCount)
            return true;
        break;
    case OCT_PARTICLE:
        if(((particle_t*) obj)->spreadFrameCount == frameCount)
            return true;
        break;
    }

    findContacts(type, obj);

    switch(type)
    {
    case OCT_LUMOBJ: ((lumobj_t*) obj)->spreadFrameCount = frameCount; break;
    case OCT_PARTICLE: ((particle_t*) obj)->spreadFrameCount = frameCount; break;
    }

    return true; // Continue iteration.
}

/**
 * Spread mobjs in the blockmap, generating subsector contacts.
 */
static void spreadMobjs(const subsector_t* subsector)
{
    // @todo Subsector should return the map its linked to.
    mobjblockmap_t* bmap = Map_MobjBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_MOBJ;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][VX] = subsector->bBox[0][VX] - DDMOBJ_RADIUS_MAX;
    bbox[0][VY] = subsector->bBox[0][VY] - DDMOBJ_RADIUS_MAX;
    bbox[1][VX] = subsector->bBox[1][VX] + DDMOBJ_RADIUS_MAX;
    bbox[1][VY] = subsector->bBox[1][VY] + DDMOBJ_RADIUS_MAX;

    MobjBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    MobjBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}

/**
 * Spread lumobjs in the blockmap, generating subsector contacts.
 */
static void spreadLumobjs(const subsector_t* subsector)
{
    // @todo Subsector should return the map its linked to.
    lumobjblockmap_t* bmap = Map_LumobjBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_LUMOBJ;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][VX] = subsector->bBox[0][VX] - loMaxRadius;
    bbox[0][VY] = subsector->bBox[0][VY] - loMaxRadius;
    bbox[1][VX] = subsector->bBox[1][VX] + loMaxRadius;
    bbox[1][VY] = subsector->bBox[1][VY] + loMaxRadius;

    LumobjBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    LumobjBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}

/**
 * Spread particles in the blockmap, generating subsector contacts.
 */
static void spreadParticles(const subsector_t* subsector)
{
    // @todo Subsector should return the map its linked to.
    particleblockmap_t* bmap = Map_ParticleBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_PARTICLE;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][VX] = subsector->bBox[0][VX] - 128;
    bbox[0][VY] = subsector->bBox[0][VY] - 128;
    bbox[1][VX] = subsector->bBox[1][VX] + 128;
    bbox[1][VY] = subsector->bBox[1][VY] + 128;

    ParticleBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    ParticleBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param subSector          Ptr to the subsector to process.
 */
void Subsector_SpreadObjs(subsector_t* subsector)
{
    assert(subsector);

    spreadMobjs(subsector);
    spreadLumobjs(subsector);
    spreadParticles(subsector);
}

void Subsector_UpdateMidPoint(subsector_t* subsector)
{
    hedge_t* hEdge;

    assert(subsector);

    // Find the center point. First calculate the bounding box.
    if((hEdge = subsector->face->hEdge))
    {
        vertex_t* vtx;

        vtx = hEdge->HE_v1;
        subsector->bBox[0][VX] = subsector->bBox[1][VX] = subsector->midPoint[VX] = vtx->pos[VX];
        subsector->bBox[0][VY] = subsector->bBox[1][VY] = subsector->midPoint[VY] = vtx->pos[VY];

        while((hEdge = hEdge->next) != subsector->face->hEdge)
        {
            vtx = hEdge->HE_v1;

            if(vtx->pos[VX] < subsector->bBox[0][VX])
                subsector->bBox[0][VX] = vtx->pos[VX];
            if(vtx->pos[VY] < subsector->bBox[0][VY])
                subsector->bBox[0][VY] = vtx->pos[VY];
            if(vtx->pos[VX] > subsector->bBox[1][VX])
                subsector->bBox[1][VX] = vtx->pos[VX];
            if(vtx->pos[VY] > subsector->bBox[1][VY])
                subsector->bBox[1][VY] = vtx->pos[VY];

            subsector->midPoint[VX] += vtx->pos[VX];
            subsector->midPoint[VY] += vtx->pos[VY];
        }

        subsector->midPoint[VX] /= subsector->hEdgeCount; // num vertices.
        subsector->midPoint[VY] /= subsector->hEdgeCount;
    }

    // Calculate the worldwide grid offset.
    subsector->worldGridOffset[VX] = fmod(subsector->bBox[0][VX], 64);
    subsector->worldGridOffset[VY] = fmod(subsector->bBox[1][VY], 64);
}

/**
 * Update the subsector, property is selected by DMU_* name.
 */
boolean Subsector_SetProperty(subsector_t* subsector, const setargs_t* args)
{
    Con_Error("Subsector_SetProperty: Property %s is not writable.\n",
              DMU_Str(args->prop));

    return true; // Continue iteration.
}

/**
 * Get the value of a subsector property, selected by DMU_* name.
 */
boolean Subsector_GetProperty(const subsector_t* subsector, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        sector_t* sec = subsector->sector;
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_SUBSECTOR_SECTOR, &r, args, 0);
        break;
        }
    default:
        Con_Error("Subsector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

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

#include "de/Subsector"
#include "de/Map"

using namespace de;

#if 0
namespace de
{
    typedef struct contactfinder_data_s {
        void* obj;
        Map::objcontacttype_t objType;
        Vector3f objPos;
        dfloat objRadius;
        dfloat box[4];
    } contactfinderparams_t;

    static void processSeg(Seg& seg, void* data);
}

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
static void spreadInSubsector(Subsector& subsector, void* data)
{
    if(!subsector.face->halfEdge)
        return;

    HalfEdge& halfEdge = *subsector.face->halfEdge;
    do
    {
        processSeg(*reinterpret_cast<Seg*>(halfEdge.data), data);
        if(!(halfEdge.next))
            break;
        halfEdge = *halfEdge.next;
    } while((&halfEdge != subsector.face->halfEdge));
}

static void processSeg(const Seg& seg, void* data)
{
    contactfinderparams_t* params = reinterpret_cast<contactfinderparams_t*>(data);

    // Can not spread over one-sided segs.
    if(!seg.hasBack() || !seg.back().hasSubsector())
        return;

    // Which way does the spread go?
    if(!(seg.subsector().validCount == validCount &&
         seg.back().subsector().validCount != validCount))
        return; // Not eligible for spreading.

    Subsector& src = seg.subsector();
    Subsector& dst = seg.back().subsector();

    // Is the dst subSector inside the objlink's AABB?
    if(dst.bBox[1][0] <= params->box[BOXLEFT] ||
       dst.bBox[0][0] >= params->box[BOXRIGHT] ||
       dst.bBox[1][1] <= params->box[BOXBOTTOM] ||
       dst.bBox[0][1] >= params->box[BOXTOP])
    {
        // The subSector is not inside the params's bounds.
        return;
    }

    // Can the spread happen?
    if(seg.hasSideDef())
    {
        if(dst.sector)
        {
            if(dst.sector->ceiling().height <= dst.sector->floor().height ||
               dst.sector->ceiling().height <= src.sector->floor().height ||
               dst.sector->floor().height   >= src.sector->ceiling().height)
            {
                // No; destination sector is closed with no height.
                return;
            }
        }

        // Don't spread if the middle material completely fills the gap between
        // floor and ceiling (direction is from dst to src).
        if(R_DoesMiddleMaterialFillGap(seg.sideDef().lineDef(),
            &dst == &seg.back().subsector()? false : true))
            return;
    }

    // Calculate 2D distance to halfEdge.
    dfloat distance;
    
    {
    Vector2d delta = seg.back().vertex().pos - seg.vertex().pos;
    distance = dfloat((seg.vertex().pos.y - ddouble(params->objPos.y)) * delta.x -
                (seg.vertex().pos.x - ddouble(params->objPos.x)) * delta.y) / seg.length;
    }

    if(seg.hasSideDef())
    {
        if((&src == &seg.subsector() && distance < 0) ||
           (&src == &seg.back().subsector() && distance > 0))
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
    dst.validCount = validCount;

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
static void findContacts(Map::objcontacttype_t type, void* obj)
{
#define SUBSECTORSPREAD_MINRADIUS 16 // In world units.

    contactfinderparams_t params;
    Subsector* subsector;

    switch(type)
    {
    case Map::OCT_LUMOBJ:
        {
        lumobj_t* lum = reinterpret_cast<lumobj_t*>(obj);

        if(lum->type != LT_OMNI)
            return; // Only omni lights spread.

        V3_Copy(params.objPos, lum->pos);
        // Use a slightly smaller radius than what the obj really is.
        params.objRadius = LUM_OMNI(lum)->radius * .9f;
        subsector = lum->subsector;
        break;
        }
    case Map::OCT_MOBJ:
        {
        mobj_t* mo = reinterpret_cast<mobj_t*>(obj);

        V3_Copy(params.objPos, mo->pos);
        params.objRadius = R_VisualRadius(mo);
        subsector = (Subsector*) ((objectrecord_t*) mo->subsector)->obj;
        break;
        }
    case Map::OCT_PARTICLE:
        {
        particle_t* pt = reinterpret_cast<particle_t*>(obj);

        V3_SetFixed(params.objPos, pt->pos[0], pt->pos[1], pt->pos[VZ]);
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
    params.box[BOXLEFT]   = params.objPos.x - params.objRadius;
    params.box[BOXRIGHT]  = params.objPos.x + params.objRadius;
    params.box[BOXBOTTOM] = params.objPos.y - params.objRadius;
    params.box[BOXTOP]    = params.objPos.y + params.objRadius;

    spreadInSubsector(*subsector, &params);

#undef SUBSECTORSPREAD_MINRADIUS
}

static bool PTR_SpreadContacts(void* obj, void* context)
{
    Map::objcontacttype_t type = *((Map::objcontacttype_t*)context);

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
    default:
        break;
    }

    findContacts(type, obj);

    switch(type)
    {
    case OCT_LUMOBJ: ((lumobj_t*) obj)->spreadFrameCount = frameCount; break;
    case OCT_PARTICLE: ((particle_t*) obj)->spreadFrameCount = frameCount; break;
    default:
        break;
    }

    return true; // Continue iteration.
}

/**
 * Spread mobjs in the blockmap, generating subsector contacts.
 */
static void spreadMobjs(const Subsector* subsector)
{
    // @todo Subsector should return the map its linked to.
    ThingBlockmap* bmap = Map_MobjBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_MOBJ;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][0] = subsector->bBox[0][0] - DDMOBJ_RADIUS_MAX;
    bbox[0][1] = subsector->bBox[0][1] - DDMOBJ_RADIUS_MAX;
    bbox[1][0] = subsector->bBox[1][0] + DDMOBJ_RADIUS_MAX;
    bbox[1][1] = subsector->bBox[1][1] + DDMOBJ_RADIUS_MAX;

    MobjBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    MobjBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}

/**
 * Spread lumobjs in the blockmap, generating subsector contacts.
 */
static void spreadLumobjs(const Subsector* subsector)
{
    // @todo Subsector should return the map its linked to.
    LumObjBlockmap* bmap = Map_LumobjBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_LUMOBJ;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][0] = subsector->bBox[0][0] - loMaxRadius;
    bbox[0][1] = subsector->bBox[0][1] - loMaxRadius;
    bbox[1][0] = subsector->bBox[1][0] + loMaxRadius;
    bbox[1][1] = subsector->bBox[1][1] + loMaxRadius;

    LumobjBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    LumobjBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}

/**
 * Spread particles in the blockmap, generating subsector contacts.
 */
static void spreadParticles(const Subsector* subsector)
{
    // @todo Subsector should return the map its linked to.
    ParticleBlockmap* bmap = Map_ParticleBlockmap(P_CurrentMap());
    objcontacttype_t type = OCT_PARTICLE;
    uint blockBox[4];
    vec2_t bbox[2];

    bbox[0][0] = subsector->bBox[0][0] - 128;
    bbox[0][1] = subsector->bBox[0][1] - 128;
    bbox[1][0] = subsector->bBox[1][0] + 128;
    bbox[1][1] = subsector->bBox[1][1] + 128;

    ParticleBlockmap_BoxToBlocks(bmap, blockBox, bbox);
    ParticleBlockmap_BoxIterate(bmap, blockBox, PTR_SpreadContacts, &type);
}
#endif

Subsector::~Subsector()
{
    /*shadowlink_t* slink;
    while((slink = shadows))
    {
        shadows = slink->next;
        Z_Free(slink);
    }*/
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param subSector          Ptr to the subsector to process.
 */
void Subsector::spreadObjs()
{
#pragma message("Warning: Subsector::spreadObjs not yet implemented.")
#if 0
    spreadMobjs(this);
    spreadLumobjs(this);
    spreadParticles(this);
#endif
}

void Subsector::updateMidPoint()
{
    HalfEdge* halfEdge;

    // Find the center point. First calculate the bounding box.
    if((halfEdge = face->halfEdge))
    {
        const Vertex& vtx = *halfEdge->vertex;

        bBox[0][0] = bBox[1][0] = midPoint.x = dfloat(vtx.pos.x);
        bBox[0][1] = bBox[1][1] = midPoint.y = dfloat(vtx.pos.y);

        while((halfEdge = halfEdge->next) != face->halfEdge)
        {
            const Vertex& vtx = *halfEdge->vertex;

            if(vtx.pos.x < bBox[0][0])
                bBox[0][0] = dfloat(vtx.pos.x);
            if(vtx.pos.y < bBox[0][1])
                bBox[0][1] = dfloat(vtx.pos.y);
            if(vtx.pos.x > bBox[1][0])
                bBox[1][0] = dfloat(vtx.pos.x);
            if(vtx.pos.y > bBox[1][1])
                bBox[1][1] = dfloat(vtx.pos.y);

            midPoint.x += dfloat(vtx.pos.x);
            midPoint.y += dfloat(vtx.pos.y);
        }

        midPoint.x /= hEdgeCount; // num vertices.
        midPoint.y /= hEdgeCount;
    }

    // Calculate the worldwide grid offset.
    worldGridOffset.x = fmod(bBox[0][0], 64);
    worldGridOffset.y = fmod(bBox[1][1], 64);
}

void Subsector::pickFanBaseSeg()
{
#define TRIFAN_LIMIT    0.1

    HalfEdge* base = NULL;

    if(firstFanHEdge)
        return; // Already chosen.

    useMidPoint = false;
    if(hEdgeCount > 3)
    {
        duint baseIDX = 0;
        HalfEdge* halfEdge = face->halfEdge;
        do
        {
            base = halfEdge;
            Vector2d& basepos = &base->vertex->pos;
            HalfEdge* hEdge2 = face->halfEdge;
            duint i = 0;
            do
            {
                if(!(baseIDX > 0 && (i == baseIDX || i == baseIDX - 1)))
                {
                    if(TRIFAN_LIMIT >= M_TriangleArea(basepos, hEdge2->vertex->pos, hEdge2->twin->vertex->pos))
                    {
                        base = NULL;
                    }
                }

                i++;
            } while(base && (hEdge2 = hEdge2->next) != face->halfEdge);

            if(!base)
                baseIDX++;
        } while(!base && (halfEdge = halfEdge->next) != face->halfEdge);

        if(!base)
            useMidPoint = true;
    }

    if(base)
        firstFanHEdge = base;
    else
        firstFanHEdge = face->halfEdge;

#undef TRIFAN_LIMIT
}

bool Subsector::pointInside(dfloat x, dfloat y) const
{
    const HalfEdge* halfEdge = face->halfEdge;

    do
    {
        const Vertex& v1 = *halfEdge->vertex;
        const Vertex& v2 = *halfEdge->next->vertex;

        if(((v1.pos.y - y) * (v2.pos.x - v1.pos.x) -
            (v1.pos.x - x) * (v2.pos.y - v1.pos.y)) < 0)
        {
            return false; // Outside the subsector's edges.
        }
    } while((halfEdge = halfEdge->next) != face->halfEdge);

    return true;
}

#if 0
bool Subsector::setProperty(const setargs_t* args)
{
    LOG_ERROR("Subsector::setProperty: Property %s is not writable.")
        << DMU_Str(args->prop);
    return true; // Continue iteration.
}

bool Subsector::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        Sector& sec = sector();
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
#endif

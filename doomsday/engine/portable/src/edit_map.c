/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * edit_map.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_bsp.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"

#include "s_environ.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gamemap_t* editMap = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gamemap_t* lastBuiltMap = NULL;

static vertex_t* rootVtx; // Used when sorting vertex line owners.

// CODE --------------------------------------------------------------------

vertex_t* HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS)
{
    vertex_t* vtx = Z_Malloc(sizeof(*vtx), PU_STATIC, 0);

    halfEdgeDS->vertices = Z_Realloc(halfEdgeDS->vertices,
        sizeof(vtx) * (++halfEdgeDS->numVertices + 1), PU_STATIC);
    halfEdgeDS->vertices[halfEdgeDS->numVertices-1] = vtx;
    halfEdgeDS->vertices[halfEdgeDS->numVertices] = NULL;

    vtx->data = Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0);
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->numVertices; // 1-based index, 0 = NIL.

    return vtx;
}

halfedgeds_t* Map_HalfEdgeDS(gamemap_t* map)
{
    return &map->halfEdgeDS;
}

vertex_t* Map_CreateVertex(gamemap_t* map, float x, float y)
{
    vertex_t* vertex;

    if(!map)
        return NULL;
    if(!map->editActive)
        return NULL;

    vertex = HalfEdgeDS_CreateVertex(Map_HalfEdgeDS(map));
    vertex->pos[VX] = x;
    vertex->pos[VY] = y;

    return vertex;
}

static linedef_t* createLineDef(gamemap_t* map)
{
    linedef_t* line = Z_Calloc(sizeof(*line), PU_STATIC, 0);

    map->lineDefs =
        Z_Realloc(map->lineDefs, sizeof(line) * (++map->numLineDefs + 1), PU_STATIC);
    map->lineDefs[map->numLineDefs-1] = line;
    map->lineDefs[map->numLineDefs] = NULL;

    line->buildData.index = map->numLineDefs; // 1-based index, 0 = NIL.
    return line;
}

linedef_t* Map_CreateLineDef(gamemap_t* map, vertex_t* vtx1, vertex_t* vtx2,
                             sidedef_t* front, sidedef_t* back)
{
    linedef_t* l;
    
    if(!map->editActive)
        return NULL;

    l = createLineDef(map);

    l->buildData.v[0] = vtx1;
    l->buildData.v[1] = vtx2;

    ((mvertex_t*) l->buildData.v[0]->data)->refCount++;
    ((mvertex_t*) l->buildData.v[1]->data)->refCount++;

    l->dX = vtx2->pos[VX] - vtx1->pos[VX];
    l->dY = vtx2->pos[VY] - vtx1->pos[VY];
    l->length = P_AccurateDistance(l->dX, l->dY);

    l->angle =
        bamsAtan2((int) (l->buildData.v[1]->pos[VY] - l->buildData.v[0]->pos[VY]),
                  (int) (l->buildData.v[1]->pos[VX] - l->buildData.v[0]->pos[VX])) << FRACBITS;

    if(l->dX == 0)
        l->slopeType = ST_VERTICAL;
    else if(l->dY == 0)
        l->slopeType = ST_HORIZONTAL;
    else
    {
        if(l->dY / l->dX > 0)
            l->slopeType = ST_POSITIVE;
        else
            l->slopeType = ST_NEGATIVE;
    }

    if(l->buildData.v[0]->pos[VX] < l->buildData.v[1]->pos[VX])
    {
        l->bBox[BOXLEFT]   = l->buildData.v[0]->pos[VX];
        l->bBox[BOXRIGHT]  = l->buildData.v[1]->pos[VX];
    }
    else
    {
        l->bBox[BOXLEFT]   = l->buildData.v[1]->pos[VX];
        l->bBox[BOXRIGHT]  = l->buildData.v[0]->pos[VX];
    }

    if(l->buildData.v[0]->pos[VY] < l->buildData.v[1]->pos[VY])
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[VY];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[VY];
    }
    else
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[VY];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[VY];
    }

    // Remember the number of unique references.
    if(front)
    {
        front->lineDef = l;
        front->buildData.refCount++;
    }

    if(back)
    {
        back->lineDef = l;
        back->buildData.refCount++;
    }

    l->buildData.sideDefs[FRONT] = front;
    l->buildData.sideDefs[BACK] = back;

    l->inFlags = 0;

    // Determine the default linedef flags.
    l->flags = 0;
    if(!front || !back)
        l->flags |= DDLF_BLOCKING;

    return l;
}

static sidedef_t* createSideDef(gamemap_t* map)
{
    sidedef_t* side = Z_Calloc(sizeof(*side), PU_STATIC, 0);

    map->sideDefs = Z_Realloc(map->sideDefs, sizeof(side) * (++map->numSideDefs + 1), PU_STATIC);
    map->sideDefs[map->numSideDefs-1] = side;
    map->sideDefs[map->numSideDefs] = NULL;

    side->buildData.index = map->numSideDefs; // 1-based index, 0 = NIL.
    side->SW_bottomsurface.owner = (void*) side;
    return side;
}

sidedef_t* Map_CreateSideDef(gamemap_t* map, sector_t* sector, short flags, material_t* topMaterial,
                             float topOffsetX, float topOffsetY, float topRed, float topGreen,
                             float topBlue, material_t* middleMaterial, float middleOffsetX,
                             float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
                             float middleAlpha, material_t* bottomMaterial, float bottomOffsetX,
                             float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue)
{
    sidedef_t* s = NULL;

    if(map->editActive)
    {
        s = createSideDef(map);
        s->flags = flags;
        s->sector = sector;

        Surface_SetMaterial(&s->SW_topsurface, topMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_topsurface, topOffsetX, topOffsetY);
        Surface_SetColorRGBA(&s->SW_topsurface, topRed, topGreen, topBlue, 1);

        Surface_SetMaterial(&s->SW_middlesurface, middleMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_middlesurface, middleOffsetX, middleOffsetY);
        Surface_SetColorRGBA(&s->SW_middlesurface, middleRed, middleGreen, middleBlue, middleAlpha);

        Surface_SetMaterial(&s->SW_bottomsurface, bottomMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_bottomsurface, bottomOffsetX, bottomOffsetY);
        Surface_SetColorRGBA(&s->SW_bottomsurface, bottomRed, bottomGreen, bottomBlue, 1);
    }

    return s;
}

static sector_t* createSector(gamemap_t* map)
{
    sector_t* sec = Z_Calloc(sizeof(*sec), PU_STATIC, 0);

    map->sectors = Z_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1), PU_STATIC);
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index, 0 = NIL.
    return sec;
}

sector_t* Map_CreateSector(gamemap_t* map, float lightLevel, float red, float green, float blue)
{
    sector_t* s;

    if(!map->editActive)
        return NULL;

    s = createSector(map);

    s->planeCount = 0;
    s->planes = NULL;
    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightLevel = MINMAX_OF(0, lightLevel, 1);

    return s;
}

static polyobj_t* createPolyobj(gamemap_t* map)
{
    polyobj_t* po = Z_Calloc(POLYOBJ_SIZE, PU_STATIC, 0);

    map->polyObjs = Z_Realloc(map->polyObjs, sizeof(po) * (++map->numPolyObjs + 1), PU_STATIC);
    map->polyObjs[map->numPolyObjs-1] = po;
    map->polyObjs[map->numPolyObjs] = NULL;

    po->buildData.index = map->numPolyObjs; // 1-based index, 0 = NIL.
    return po;
}

polyobj_t* Map_CreatePolyobj(gamemap_t* map, dmuobjrecordid_t* lines, uint lineCount, int tag,
                             int sequenceType, float anchorX, float anchorY)
{
    polyobj_t* po;
    uint i;

    if(!map->editActive)
        return NULL;

    po = createPolyobj(map);

    po->idx = map->numPolyObjs; // 0-based index.
    po->lineDefs = Z_Calloc(sizeof(linedef_t*) * (lineCount+1), PU_STATIC, 0);
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line = map->lineDefs[lines[i] - 1];

        // This line is part of a polyobj.
        line->inFlags |= LF_POLYOBJ;

        po->lineDefs[i] = line;
    }
    po->lineDefs[i] = NULL;
    po->numLineDefs = lineCount;

    po->tag = tag;
    po->seqType = sequenceType;
    po->pos[VX] = anchorX;
    po->pos[VY] = anchorY;

    return po;
}

static int C_DECL vertexCompare(const void* p1, const void* p2)
{
    const vertex_t* a = *((const void**) p1);
    const vertex_t* b = *((const void**) p2);

    if(a == b)
        return 0;

    if((int) a->pos[VX] != (int) b->pos[VX])
        return (int) a->pos[VX] - (int) b->pos[VX];

    return (int) a->pos[VY] - (int) b->pos[VY];
}

static void detectDuplicateVertices(gamemap_t* map)
{
    size_t i;
    vertex_t** hits;

    hits = M_Malloc(map->halfEdgeDS.numVertices * sizeof(vertex_t*));

    // Sort array of ptrs.
    for(i = 0; i < map->halfEdgeDS.numVertices; ++i)
        hits[i] = map->halfEdgeDS.vertices[i];
    qsort(hits, map->halfEdgeDS.numVertices, sizeof(vertex_t*), vertexCompare);

    // Now mark them off.
    for(i = 0; i < map->halfEdgeDS.numVertices - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            vertex_t* a = hits[i];
            vertex_t* b = hits[i + 1];

            ((mvertex_t*) b->data)->equiv =
                (((mvertex_t*) a->data)->equiv ? ((mvertex_t*) a->data)->equiv : a);
        }
    }

    M_Free(hits);
}

static void findEquivalentVertexes(gamemap_t* map)
{
    uint i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        // Handle duplicated vertices.
        while(((mvertex_t*) l->buildData.v[0]->data)->equiv)
        {
            ((mvertex_t*) l->buildData.v[0]->data)->refCount--;
            l->buildData.v[0] = ((mvertex_t*) l->buildData.v[0]->data)->equiv;
            ((mvertex_t*) l->buildData.v[0]->data)->refCount++;
        }

        while(((mvertex_t*) l->buildData.v[1]->data)->equiv)
        {
            ((mvertex_t*) l->buildData.v[1]->data)->refCount--;
            l->buildData.v[1] = ((mvertex_t*) l->buildData.v[1]->data)->equiv;
            ((mvertex_t*) l->buildData.v[1]->data)->refCount++;
        }

        l->buildData.index = newNum + 1;
        map->lineDefs[newNum++] = map->lineDefs[i];
    }
}

static void pruneLineDefs(gamemap_t* map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        if(!l->buildData.sideDefs[FRONT] && !l->buildData.sideDefs[BACK])
        {
            unused++;

            Z_Free(l);
            continue;
        }

        l->buildData.index = newNum + 1;
        map->lineDefs[newNum++] = l;
    }

    if(newNum < map->numLineDefs)
    {
        if(unused > 0)
            Con_Message("  Pruned %d unused linedefs\n", unused);

        map->numLineDefs = newNum;
    }
}

static void pruneVertices(gamemap_t* map)
{
    uint i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < map->halfEdgeDS.numVertices; ++i)
    {
        vertex_t* v = map->halfEdgeDS.vertices[i];

        if(((mvertex_t*) v->data)->refCount < 0)
            Con_Error("Vertex %d ref_count is %d", i, ((mvertex_t*) v->data)->refCount);

        if(((mvertex_t*) v->data)->refCount == 0)
        {
            if(((mvertex_t*) v->data)->equiv == NULL)
                unused++;

            Z_Free(v);
            continue;
        }

        ((mvertex_t*) v->data)->index = newNum + 1;
        map->halfEdgeDS.vertices[newNum++] = v;
    }

    if(newNum < map->halfEdgeDS.numVertices)
    {
        int dupNum = map->halfEdgeDS.numVertices - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices\n", dupNum);

        map->halfEdgeDS.numVertices = newNum;
    }
}

static void pruneUnusedSideDefs(gamemap_t* map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* s = map->sideDefs[i];

        if(s->buildData.refCount == 0)
        {
            unused++;

            Z_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sideDefs[newNum++] = s;
    }

    if(newNum < map->numSideDefs)
    {
        int dupNum = map->numSideDefs - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused sidedefs\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate sidedefs\n", dupNum);

        map->numSideDefs = newNum;
    }
}

static void pruneUnusedSectors(gamemap_t* map)
{
    uint i, newNum;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < map->numSectors; ++i)
    {
        sector_t* s = map->sectors[i];

        if(s->buildData.refCount == 0)
        {
            Z_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sectors[newNum++] = s;
    }

    if(newNum < map->numSectors)
    {
        Con_Message("  Pruned %d unused sectors\n", map->numSectors - newNum);
        map->numSectors = newNum;
    }
}

/**
 * \note Order here is critical!
 */
static void pruneUnusedObjects(gamemap_t* map, int flags)
{
    /**
     * \fixme Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     */
    findEquivalentVertexes(map);

/*    if(flags & PRUNE_LINEDEFS)
        pruneLineDefs(map);*/

    if(flags & PRUNE_VERTEXES)
        pruneVertices(map);

/*    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSideDefs(map);

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors(map);*/
}

/**
 * Called to begin the map building process.
 */
boolean MPE_Begin(const char* mapID)
{
    if(editMap)
        Con_Error("MPE_Begin: Error, already editing map %s.\n", editMap->mapID);

    editMap = P_CreateMap(mapID);

    return true;
}

static void hardenSectorSubsectorList(gamemap_t* map, uint secIDX)
{
    uint i, n, count;
    sector_t* sec = map->sectors[secIDX];

    count = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        const subsector_t* subsector = map->subsectors[i];

        if(subsector->sector == sec)
            count++;
    }

    sec->subsectors = Z_Malloc((count + 1) * sizeof(subsector_t*), PU_STATIC, NULL);

    n = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];

        if(subsector->sector == sec)
        {
            sec->subsectors[n++] = subsector;
        }
    }

    sec->subsectors[n] = NULL; // Terminate.
    sec->subsectorCount = count;
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSubsectorLists(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        hardenSectorSubsectorList(map, i);
    }
}

static void buildSectorLineLists(gamemap_t* map)
{
    typedef struct linelink_s {
        linedef_t*      line;
        struct linelink_s *next;
    } linelink_t;

    uint i, j;
    zblockset_t* lineLinksBlockSet;
    linelink_t** sectorLineLinks;
    uint* sectorLinkLinkCounts;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numSectors);
    sectorLinkLinkCounts = M_Calloc(sizeof(uint) * map->numSectors);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* li = map->lineDefs[i];
        uint secIDX;
        linelink_t* link;

        if(LINE_FRONTSIDE(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = DMU_GetObjRecord(DMU_SECTOR, LINE_FRONTSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            sectorLinkLinkCounts[secIDX]++;
            LINE_FRONTSECTOR(li)->lineDefCount++;
        }

        if(LINE_BACKSIDE(li) && LINE_BACKSECTOR(li) != LINE_FRONTSECTOR(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = DMU_GetObjRecord(DMU_SECTOR, LINE_BACKSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            sectorLinkLinkCounts[secIDX]++;
            LINE_BACKSECTOR(li)->lineDefCount++;
        }
    }

    // Harden the sector line links into arrays.
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        if(sectorLineLinks[i])
        {
            linelink_t* link = sectorLineLinks[i];

            sector->lineDefs =
                Z_Malloc((sectorLinkLinkCounts[i] + 1) * sizeof(linedef_t*), PU_STATIC, 0);

            j = 0;
            while(link)
            {
                sector->lineDefs[j++] = link->line;
                link = link->next;
            }

            sector->lineDefs[j] = NULL; // terminate.
            sector->lineDefCount = j;
        }
        else
        {
            sector->lineDefs = NULL;
            sector->lineDefCount = 0;
        }
    }

    // Free temporary storage.
    Z_BlockDestroy(lineLinksBlockSet);
    M_Free(sectorLineLinks);
    M_Free(sectorLinkLinkCounts);
}

/**
 * \pre Lines in sector must be setup before this is called!
 */
static void updateSectorBounds(sector_t* sec)
{
    uint i;
    float* bbox;
    vertex_t* vtx;

    if(!sec)
        return;

    bbox = sec->bBox;

    if(!(sec->lineDefCount > 0))
    {
        memset(sec->bBox, 0, sizeof(sec->bBox));
        return;
    }

    bbox[BOXLEFT] = DDMAXFLOAT;
    bbox[BOXRIGHT] = DDMINFLOAT;
    bbox[BOXBOTTOM] = DDMAXFLOAT;
    bbox[BOXTOP] = DDMINFLOAT;

    for(i = 1; i < sec->lineDefCount; ++i)
    {
        linedef_t* li = sec->lineDefs[i];

        if(li->inFlags & LF_POLYOBJ)
            continue;

        vtx = li->L_v1;

        if(vtx->pos[VX] < bbox[BOXLEFT])
            bbox[BOXLEFT]   = vtx->pos[VX];
        if(vtx->pos[VX] > bbox[BOXRIGHT])
            bbox[BOXRIGHT]  = vtx->pos[VX];
        if(vtx->pos[VY] < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = vtx->pos[VY];
        if(vtx->pos[VY] > bbox[BOXTOP])
            bbox[BOXTOP]    = vtx->pos[VY];
    }

    // This is very rough estimate of sector area.
    sec->approxArea = ((bbox[BOXRIGHT] - bbox[BOXLEFT]) / 128) *
        ((bbox[BOXTOP] - bbox[BOXBOTTOM]) / 128);
}

/**
 * \pre Sector bounds must be setup before this is called!
 */
void P_GetSectorBounds(sector_t* sec, float* min, float* max)
{
    min[VX] = sec->bBox[BOXLEFT];
    min[VY] = sec->bBox[BOXBOTTOM];

    max[VX] = sec->bBox[BOXRIGHT];
    max[VY] = sec->bBox[BOXTOP];
}

static void finishSectors2(gamemap_t* map)
{
    uint i;
    vec2_t bmapOrigin;
    uint bmapSize[2];

    Blockmap_Bounds(map->blockMap, bmapOrigin, NULL);
    Blockmap_Dimensions(map->blockMap, bmapSize);

    for(i = 0; i < map->numSectors; ++i)
    {
        uint k;
        float min[2], max[2];
        sector_t* sec = map->sectors[i];

        updateSectorBounds(sec);
        P_GetSectorBounds(sec, min, max);

        // Set the degenmobj_t to the middle of the bounding box.
        sec->soundOrg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundOrg.pos[VY] = (min[VY] + max[VY]) / 2;

        // Set the z height of the sector sound origin.
        sec->soundOrg.pos[VZ] =
            (sec->SP_ceilheight - sec->SP_floorheight) / 2;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planeCount; ++k)
        {
            sec->planes[k]->soundOrg.pos[VX] = sec->soundOrg.pos[VX];
            sec->planes[k]->soundOrg.pos[VY] = sec->soundOrg.pos[VY];
            sec->planes[k]->soundOrg.pos[VZ] = sec->planes[k]->height;

            sec->planes[k]->target = sec->planes[k]->height;
        }
    }
}

/**
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 */
static void finishLineDefs2(gamemap_t* map)
{
    uint i;

    VERBOSE2(Con_Message("Finalizing LineDefs...\n"));

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* ld = map->lineDefs[i];
        vertex_t* v[2];

        if(!ld->hEdges[0])
            continue;

        v[0] = ld->hEdges[0]->HE_v1;
        v[1] = ld->hEdges[1]->HE_v2;

        ld->dX = v[1]->pos[VX] - v[0]->pos[VX];
        ld->dY = v[1]->pos[VY] - v[0]->pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistance(ld->dX, ld->dY);
        ld->angle = bamsAtan2((int) (v[1]->pos[VY] - v[0]->pos[VY]),
                              (int) (v[1]->pos[VX] - v[0]->pos[VX])) << FRACBITS;

        if(!ld->dX)
            ld->slopeType = ST_VERTICAL;
        else if(!ld->dY)
            ld->slopeType = ST_HORIZONTAL;
        else
        {
            if(ld->dY / ld->dX > 0)
                ld->slopeType = ST_POSITIVE;
            else
                ld->slopeType = ST_NEGATIVE;
        }

        if(v[0]->pos[VX] < v[1]->pos[VX])
        {
            ld->bBox[BOXLEFT]   = v[0]->pos[VX];
            ld->bBox[BOXRIGHT]  = v[1]->pos[VX];
        }
        else
        {
            ld->bBox[BOXLEFT]   = v[1]->pos[VX];
            ld->bBox[BOXRIGHT]  = v[0]->pos[VX];
        }

        if(v[0]->pos[VY] < v[1]->pos[VY])
        {
            ld->bBox[BOXBOTTOM] = v[0]->pos[VY];
            ld->bBox[BOXTOP]    = v[1]->pos[VY];
        }
        else
        {
            ld->bBox[BOXBOTTOM] = v[1]->pos[VY];
            ld->bBox[BOXTOP]    = v[0]->pos[VY];
        }
    }
}

static void updateMapBounds(gamemap_t* map)
{
    uint i;

    memset(map->bBox, 0, sizeof(map->bBox));
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(map->bBox, sec->bBox, sizeof(map->bBox));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(map->bBox, sec->bBox);
        }
    }
}

static void updateSubsectorMidPoint(subsector_t* subsector)
{
    hedge_t* hEdge;

    // Find the center point. First calculate the bounding box.
    if((hEdge = subsector->face->hEdge))
    {
        vertex_t* vtx;

        vtx = hEdge->HE_v1;
        subsector->bBox[0].pos[VX] = subsector->bBox[1].pos[VX] = subsector->midPoint.pos[VX] = vtx->pos[VX];
        subsector->bBox[0].pos[VY] = subsector->bBox[1].pos[VY] = subsector->midPoint.pos[VY] = vtx->pos[VY];

        while((hEdge = hEdge->next) != subsector->face->hEdge)
        {
            vtx = hEdge->HE_v1;

            if(vtx->pos[VX] < subsector->bBox[0].pos[VX])
                subsector->bBox[0].pos[VX] = vtx->pos[VX];
            if(vtx->pos[VY] < subsector->bBox[0].pos[VY])
                subsector->bBox[0].pos[VY] = vtx->pos[VY];
            if(vtx->pos[VX] > subsector->bBox[1].pos[VX])
                subsector->bBox[1].pos[VX] = vtx->pos[VX];
            if(vtx->pos[VY] > subsector->bBox[1].pos[VY])
                subsector->bBox[1].pos[VY] = vtx->pos[VY];

            subsector->midPoint.pos[VX] += vtx->pos[VX];
            subsector->midPoint.pos[VY] += vtx->pos[VY];
        }

        subsector->midPoint.pos[VX] /= subsector->hEdgeCount; // num vertices.
        subsector->midPoint.pos[VY] /= subsector->hEdgeCount;
    }

    // Calculate the worldwide grid offset.
    subsector->worldGridOffset[VX] = fmod(subsector->bBox[0].pos[VX], 64);
    subsector->worldGridOffset[VY] = fmod(subsector->bBox[1].pos[VY], 64);
}

static void prepareSubsectors(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];

        updateSubsectorMidPoint(subsector);
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void* a, const void* b)
{
    uint i;
    fixed_t dx, dy;
    binangle_t angles[2];
    lineowner_t* own[2];
    linedef_t* line;

    own[0] = (lineowner_t*) a;
    own[1] = (lineowner_t*) b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            vertex_t* otherVtx;

            line = own[i]->lineDef;
            otherVtx = line->buildData.v[line->buildData.v[0] == rootVtx? 1:0];

            dx = otherVtx->pos[VX] - rootVtx->pos[VX];
            dy = otherVtx->pos[VY] - rootVtx->pos[VY];

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static lineowner_t* mergeLineOwners(lineowner_t* left, lineowner_t* right,
                                    int (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t* splitLineOwners(lineowner_t* list)
{
    lineowner_t* lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t* sortLineOwners(lineowner_t* list,
                                   int (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t* p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(vertex_t* vtx, linedef_t* lineptr, lineowner_t** storage)
{
    lineowner_t* p, *newOwner;

    if(!lineptr)
        return;

    // Has this line already been registered with this vertex?
    if(((mvertex_t*) vtx->data)->numLineOwners != 0)
    {
        p = ((mvertex_t*) vtx->data)->lineOwners;
        while(p)
        {
            if(p->lineDef == lineptr)
                return;             // Yes, we can exit.

            p = p->LO_next;
        }
    }

    //Add a new owner.
    ((mvertex_t*) vtx->data)->numLineOwners++;

    newOwner = (*storage)++;
    newOwner->lineDef = lineptr;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = ((mvertex_t*) vtx->data)->lineOwners;
    ((mvertex_t*) vtx->data)->lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == lineptr->buildData.v[0])
        lineptr->L_vo1 = newOwner;
    else
        lineptr->L_vo2 = newOwner;
}

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwnerRings(gamemap_t* map)
{
    uint i;
    lineowner_t* lineOwners, *allocator;

    // We know how many vertex line owners we need (numLineDefs * 2).
    lineOwners = Z_Malloc(sizeof(lineowner_t) * map->numLineDefs * 2, PU_MAP, 0);
    allocator = lineOwners;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        uint p;
        linedef_t* line = map->lineDefs[i];

        for(p = 0; p < 2; ++p)
        {
            vertex_t* vtx = line->buildData.v[p];

            setVertexLineOwner(vtx, line, &allocator);
        }
    }
}

static void hardenVertexOwnerRings(gamemap_t* map)
{
    uint i;

    // Sort line owners and then finish the rings.
    for(i = 0; i < map->halfEdgeDS.numVertices; ++i)
    {
        vertex_t* v = map->halfEdgeDS.vertices[i];

        // Line owners:
        if(((mvertex_t*) v->data)->numLineOwners != 0)
        {
            lineowner_t* p, *last;
            binangle_t firstAngle;

            // Redirect the linedef links to the hardened map.
            p = ((mvertex_t*) v->data)->lineOwners;
            while(p)
            {
                p->lineDef = map->lineDefs[p->lineDef->buildData.index - 1];
                p = p->LO_next;
            }

            // Sort them; ordered clockwise by angle.
            rootVtx = v;
            ((mvertex_t*) v->data)->lineOwners =
                sortLineOwners(((mvertex_t*) v->data)->lineOwners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            firstAngle = ((mvertex_t*) v->data)->lineOwners->angle;
            last = ((mvertex_t*) v->data)->lineOwners;
            p = last->LO_next;
            while(p)
            {
                p->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - p->angle;

                last = p;
                p = p->LO_next;
            }
            last->LO_next = ((mvertex_t*) v->data)->lineOwners;
            ((mvertex_t*) v->data)->lineOwners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = last->angle - firstAngle;
/*
#if _DEBUG
{
// For checking the line owner link rings are formed correctly.
lineowner_t *base;
uint        idx;

if(verbose >= 2)
    Con_Message("Vertex #%i: line owners #%i\n", i, ((mvertex_t*) v->data)->numLineOwners);

p = base = ((mvertex_t*) v->data)->lineOwners;
idx = 0;
do
{
    if(verbose >= 2)
        Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f\n",
                    idx, p->LO_prev->line - map->lineDefs,
                    p->line - map->lineDefs,
                    p->LO_next->line - map->lineDefs, BANG2DEG(p->angle));

    if(p->LO_prev->LO_next != p || p->LO_next->LO_prev != p)
       Con_Error("Invalid line owner link ring!");

    p = p->LO_next;
    idx++;
} while(p != base);
}
#endif
*/
        }
    }
}

static void addLineDefsToDMU(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];

        DMU_AddObjRecord(DMU_LINEDEF, lineDef);
    }
}

static void finishSideDefs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* sideDef = map->sideDefs[i];

        sideDef->sector = map->sectors[sideDef->sector->buildData.index - 1];
        sideDef->SW_bottomsurface.owner = sideDef;
        sideDef->SW_middlesurface.owner = sideDef;
        sideDef->SW_topsurface.owner = sideDef;
        sideDef->SW_bottomsurface.visOffset[0] = sideDef->SW_bottomsurface.offset[0];
        sideDef->SW_bottomsurface.visOffset[1] = sideDef->SW_bottomsurface.offset[1];
        sideDef->SW_middlesurface.visOffset[0] = sideDef->SW_middlesurface.offset[0];
        sideDef->SW_middlesurface.visOffset[1] = sideDef->SW_middlesurface.offset[1];
        sideDef->SW_topsurface.visOffset[0] = sideDef->SW_topsurface.offset[0];
        sideDef->SW_topsurface.visOffset[1] = sideDef->SW_topsurface.offset[1];

        DMU_AddObjRecord(DMU_SIDEDEF, sideDef);
    }
}

static void addSectorsToDMU(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        DMU_AddObjRecord(DMU_SECTOR, sector);
    }
}

static void hardenPlanes(gamemap_t* map)
{
    uint i, j;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        for(j = 0; j < sector->planeCount; ++j)
        {
            plane_t* plane = sector->planes[j]; //R_NewPlaneForSector(destS);

            plane->height = plane->oldHeight[0] = plane->oldHeight[1] =
                plane->visHeight = plane->height;
            plane->visHeightDelta = 0;
            plane->sector = sector;

            DMU_AddObjRecord(DMU_PLANE, plane);
        }
    }
}

static void finishPolyobjs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* lineDef = po->lineDefs[po->lineDefs[j]->buildData.index - 1];
            hedge_t* hEdge;

            hEdge = Z_Calloc(sizeof(hedge_t), PU_MAP, 0);
            hEdge->face = NULL;
            lineDef->hEdges[0] = lineDef->hEdges[1] = hEdge;

            po->lineDefs[j] = (linedef_t*) DMU_GetObjRecord(DMU_LINEDEF, lineDef);
        }
        po->lineDefs[j] = NULL;

        po->originalPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);
        po->prevPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);

        // Temporary: Create a seg for each line of this polyobj.
        po->numSegs = po->numLineDefs;
        po->segs = Z_Calloc(sizeof(poseg_t) * (po->numSegs+1), PU_STATIC, 0);

        for(j = 0; j < po->numSegs; ++j)
        {
            linedef_t* lineDef = ((dmuobjrecord_t*) po->lineDefs[j])->obj;
            poseg_t* seg = &po->segs[j];

            lineDef->hEdges[0]->data = seg;

            seg->lineDef = lineDef;
            seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[FRONT]->buildData.index - 1];
        }
    }
}

/**
 * @algorithm Cast a line horizontally or vertically and see what we hit.
 * @todo Construct the lineDef blockmap first so it may be used for this
 */
static void testForWindowEffect(gamemap_t* map, linedef_t* l)
{
// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

    uint i;
    double mX, mY, dX, dY;
    boolean castHoriz;
    double backDist = DDMAXFLOAT;
    sector_t* backOpen = NULL;
    double frontDist = DDMAXFLOAT;
    sector_t* frontOpen = NULL;
    linedef_t* frontLine = NULL, *backLine = NULL;

    mX = (l->buildData.v[0]->pos[VX] + l->buildData.v[1]->pos[VX]) / 2.0;
    mY = (l->buildData.v[0]->pos[VY] + l->buildData.v[1]->pos[VY]) / 2.0;

    dX = l->buildData.v[1]->pos[VX] - l->buildData.v[0]->pos[VX];
    dY = l->buildData.v[1]->pos[VY] - l->buildData.v[0]->pos[VY];

    castHoriz = (fabs(dX) < fabs(dY)? true : false);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* n = map->lineDefs[i];
        double dist;
        boolean isFront;
        sidedef_t* hitSide;
        double dX2, dY2;

        if(n == l || (n->buildData.sideDefs[FRONT] && n->buildData.sideDefs[BACK] &&
            n->buildData.sideDefs[FRONT]->sector == n->buildData.sideDefs[BACK]->sector) /*|| n->buildData.overlap ||
           (n->buildData.mlFlags & MLF_ZEROLENGTH)*/)
            continue;

        dX2 = n->buildData.v[1]->pos[VX] - n->buildData.v[0]->pos[VX];
        dY2 = n->buildData.v[1]->pos[VY] - n->buildData.v[0]->pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VX] + (mY - n->buildData.v[0]->pos[VY]) * dX2 / dY2) - mX;

            isFront = (((dY > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->buildData.sideDefs[(dY > 0) ^ (dY2 > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(dX2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VY] + (mX - n->buildData.v[0]->pos[VX]) * dY2 / dX2) - mY;

            isFront = (((dX > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->buildData.sideDefs[(dX > 0) ^ (dX2 > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON) // Too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = hitSide->sector;
                else
                    frontOpen = NULL;

                frontLine = n;
            }
        }
        else
        {
            if(dist < backDist)
            {
                backDist = dist;
                if(hitSide)
                    backOpen = hitSide->sector;
                else
                    backOpen = NULL;

                backLine = n;
            }
        }
    }

/*#if _DEBUG
Con_Message("back line: %d  back dist: %1.1f  back_open: %s\n",
            (backLine? backLine->buildData.index : -1), backDist,
            (backOpen? "OPEN" : "CLOSED"));
Con_Message("front line: %d  front dist: %1.1f  front_open: %s\n",
            (frontLine? frontLine->buildData.index : -1), frontDist,
            (frontOpen? "OPEN" : "CLOSED"));
#endif*/

    if(backOpen && frontOpen && l->buildData.sideDefs[FRONT]->sector == backOpen)
    {
        Con_Message("LineDef #%d seems to be a One-Sided Window "
                    "(back faces sector #%d).\n", l->buildData.index - 1,
                    backOpen->buildData.index - 1);

        l->buildData.windowEffect = frontOpen;
    }

#undef DIST_EPSILON
}

static void countVertexLineOwners(vertex_t* vtx, uint* oneSided,
                                  uint* twoSided)
{
    lineowner_t* p;

    p = ((mvertex_t*) vtx->data)->lineOwners;
    while(p)
    {
        if(!p->lineDef->buildData.sideDefs[FRONT] ||
           !p->lineDef->buildData.sideDefs[BACK])
            (*oneSided)++;
        else
            (*twoSided)++;

        p = p->LO_next;
    }
}

/**
 * \note Algorithm:
 * Scan the linedef list looking for possible candidates, checking for an
 * odd number of one-sided linedefs connected to a single vertex.
 * This idea courtesy of Graham Jackson.
 */
static void detectOnesidedWindows(gamemap_t* map)
{
    uint i, oneSiders, twoSiders;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        if((l->buildData.sideDefs[FRONT] && l->buildData.sideDefs[BACK]) ||
           !l->buildData.sideDefs[FRONT] /*||
           (l->buildData.mlFlags & MLF_ZEROLENGTH) ||
           l->buildData.overlap*/)
            continue;

        oneSiders = twoSiders = 0;
        countVertexLineOwners(l->buildData.v[0], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
            i, l->buildData.v[0]->index);
#endif*/
            testForWindowEffect(map, l);
            continue;
        }

        oneSiders = twoSiders = 0;
        countVertexLineOwners(l->buildData.v[1], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
            i, l->buildData.v[1]->index));
#endif*/
            testForWindowEffect(map, l);
        }
    }
}

#if 0 /* Currently unused. */
/**
 * @return              The "lowest" vertex (normally the left-most, but if
 *                      the line is vertical, then the bottom-most).
 *                      @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const linedef_t* l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] < (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

size_t numOverlaps;

boolean testOverlaps(linedef_t* b, void* data)
{
    linedef_t*          a = (linedef_t*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {   // Found an overlap!
                b->buildData.overlap =
                    (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return true; // Continue iteration.
}

typedef struct {
    blockmap_t*         blockMap;
    uint                block[2];
} findoverlaps_params_t;

boolean findOverlapsForLineDef(linedef_t* l, void* data)
{
    findoverlaps_params_t* params = (findoverlaps_params_t*) data;

    Blockmap_IterateLineDefs(params->blockMap, params->block, testOverlaps, l, false);
    return true; // Continue iteration.
}

/**
 * \note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(gamemap_t* map)
{
    uint                x, y, bmapDimensions[2];
    findoverlaps_params_t params;

    params.blockMap = map->blockMap;
    numOverlaps = 0;

    Blockmap_Dimensions(map->blockMap, bmapDimensions);

    for(y = 0; y < bmapDimensions[VY]; ++y)
        for(x = 0; x < bmapDimensions[VX]; ++x)
        {
            params.block[VX] = x;
            params.block[VY] = y;

            Blockmap_IterateLineDefs(map->blockMap, params.block,
                                    findOverlapsForLineDef, &params, false);
        }

    if(numOverlaps > 0)
        VERBOSE(Con_Message("Detected %lu overlapped linedefs\n",
                            (unsigned long) numOverlaps));
}
#endif

void Map_EditEnd(gamemap_t* map)
{
    uint i;

    if(!map)
        return;
    if(!map->editActive)
        return;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    detectDuplicateVertices(map);
    pruneUnusedObjects(map, PRUNE_ALL);

    buildVertexOwnerRings(map);

    detectOnesidedWindows(map);

    /**
     * Harden most of the map data so that we can construct some of the more
     * intricate data structures early on (and thus make use of them during
     * the BSP generation).
     *
     * \todo I'm sure this can be reworked further so that we destroy as we
     * go and reduce the current working memory surcharge.
     */
    addSectorsToDMU(map);
    finishSideDefs(map);
    addLineDefsToDMU(map);
    finishPolyobjs(map);

    hardenVertexOwnerRings(map);

    /**
     * Build a blockmap for this map.
     */
    Map_BuildBlockmap(map);

    /**
     * Build a BSP for this map.
     */
    /*builtOK =*/ BSP_Build(map);

    // Finish the polyobjs (after the vertexes are hardened).
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[j])->obj;

            line->L_v1 = map->halfEdgeDS.vertices[
                ((mvertex_t*) line->buildData.v[0]->data)->index - 1];
            line->L_v2 = map->halfEdgeDS.vertices[
                ((mvertex_t*) line->buildData.v[1]->data)->index - 1];

            // The original Pts are based off the anchor Pt, and are unique
            // to each linedef.
            po->originalPts[j].pos[VX] = line->L_v1->pos[VX] - po->pos[VX];
            po->originalPts[j].pos[VY] = line->L_v1->pos[VY] - po->pos[VY];
        }
    }

    buildSectorSubsectorLists(map);
    hardenPlanes(map);
    buildSectorLineLists(map);
    finishLineDefs2(map);
    finishSectors2(map);
    updateMapBounds(map);

    S_DetermineSubSecsAffectingSectorReverb(map);
    prepareSubsectors(map);

    map->editActive = false;

    /*if(!builtOK)
    {   // Argh, failed.
        // Need to clean up.
        P_DestroyGameMapObjDB(&map->gameObjData);
        return false;
    }*/
}

/**
 * Called to complete the map building process.
 */
boolean MPE_End(void)
{
    if(!editMap)
        return false;

    Map_EditEnd(editMap);

    lastBuiltMap = editMap;
    editMap = NULL;

    return true;
}

gamemap_t* MPE_GetLastBuiltMap(void)
{
    return lastBuiltMap;
}

/**
 * Create a new vertex in currently loaded editable map.
 *
 * @param x             X coordinate of the new vertex.
 * @param y             Y coordinate of the new vertex.
 *
 * @return              Index number of the newly created vertex else 0 if
 *                      the vertex could not be created for some reason.
 */
dmuobjrecordid_t MPE_VertexCreate(float x, float y)
{
    vertex_t* v;
    gamemap_t* map = editMap;
    
    if(!map)
        return 0;

    v = Map_CreateVertex(map, x, y);

    return v ? ((mvertex_t*) v->data)->index : 0;
}

/**
 * Create many new vertexs in the currently loaded editable map.
 *
 * @param num           Number of vertexes to be created.
 * @param values        Ptr to an array containing the coordinates for the
 *                      vertexs to create [v0 X, vo Y, v1 X, v1 Y...]
 * @param indices       If not @c NULL, the indices of the newly created
 *                      vertexes will be written back here.
 */
boolean MPE_VertexCreatev(size_t num, float* values, dmuobjrecordid_t* indices)
{
    uint n;
    gamemap_t* map = editMap;

    if(!map)
        return false;
    if(!num || !values)
        return false;

    if(!map->editActive)
        return false;

    // Create many vertexes.
    for(n = 0; n < num; ++n)
    {
        vertex_t* v = Map_CreateVertex(map, values[n * 2], values[n * 2 + 1]);

        if(indices)
            indices[n] = ((mvertex_t*) v->data)->index;
    }

    return true;
}

dmuobjrecordid_t MPE_SideDefCreate(dmuobjrecordid_t sector, short flags,
                                   material_t* topMaterial,
                                   float topOffsetX, float topOffsetY, float topRed,
                                   float topGreen, float topBlue,
                                   material_t* middleMaterial,
                                   float middleOffsetX, float middleOffsetY,
                                   float middleRed, float middleGreen,
                                   float middleBlue, float middleAlpha,
                                   material_t* bottomMaterial,
                                   float bottomOffsetX, float bottomOffsetY,
                                   float bottomRed, float bottomGreen,
                                   float bottomBlue)
{
    sidedef_t* s;
    gamemap_t* map = editMap;

    if(!map)
        return 0;
    if(sector > map->numSectors)
        return 0;

    s = Map_CreateSideDef(map, (sector == 0? NULL: map->sectors[sector-1]), flags,
                          topMaterial? ((dmuobjrecord_t*) topMaterial)->obj : NULL,
                          topOffsetX, topOffsetY, topRed, topGreen, topBlue,
                          middleMaterial? ((dmuobjrecord_t*) middleMaterial)->obj : NULL,
                          middleOffsetX, middleOffsetY, middleRed, middleGreen, middleBlue,
                          middleAlpha, bottomMaterial? ((dmuobjrecord_t*) bottomMaterial)->obj : NULL,
                          bottomOffsetX, bottomOffsetY, bottomRed, bottomGreen, bottomBlue);

    return s ? s->buildData.index : 0;
}

/**
 * Create a new linedef in the editable map.
 *
 * @param v1            Idx of the start vertex.
 * @param v2            Idx of the end vertex.
 * @param frontSide     Idx of the front sidedef.
 * @param backSide      Idx of the back sidedef.
 * @param flags         Currently unused.
 *
 * @return              Idx of the newly created linedef else @c 0 if there
 *                      was an error.
 */
dmuobjrecordid_t MPE_LineDefCreate(dmuobjrecordid_t v1, dmuobjrecordid_t v2,
                                   uint frontSide, uint backSide, int flags)
{
    linedef_t* l;
    sidedef_t* front = NULL, *back = NULL;
    vertex_t* vtx1, *vtx2;
    float length, dx, dy;
    gamemap_t* map = editMap;

    if(!map)
        return 0;
    if(frontSide > map->numSideDefs)
        return 0;
    if(backSide > map->numSideDefs)
        return 0;
    if(v1 == 0 || v1 > Map_HalfEdgeDS(map)->numVertices)
        return 0;
    if(v2 == 0 || v2 > Map_HalfEdgeDS(map)->numVertices)
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide && map->sideDefs[frontSide - 1]->buildData.refCount)
        return 0; // Already in use.
    if(backSide && map->sideDefs[backSide - 1]->buildData.refCount)
        return 0; // Already in use.

    // Next, check the length is not zero.
    vtx1 = Map_HalfEdgeDS(map)->vertices[v1 - 1];
    vtx2 = Map_HalfEdgeDS(map)->vertices[v2 - 1];
    dx = vtx2->pos[VX] - vtx1->pos[VX];
    dy = vtx2->pos[VY] - vtx1->pos[VY];
    length = P_AccurateDistance(dx, dy);
    if(!(length > 0))
        return 0;

    if(frontSide > 0)
        front = map->sideDefs[frontSide - 1];
    if(backSide > 0)
        back = map->sideDefs[backSide - 1];

    l = Map_CreateLineDef(map, vtx1, vtx2, front, back);

    return l->buildData.index;
}

void Map_CreatePlane(gamemap_t* map, sector_t* sector, float height, material_t* material,
                     float matOffsetX, float matOffsetY, float r, float g, float b, float a,
                     float normalX, float normalY, float normalZ)
{
    uint i;
    plane_t** newList, *pln;

    if(!map->editActive)
        return;

    pln = Z_Calloc(sizeof(plane_t), PU_STATIC, 0);
    pln->height = height;

    Surface_SetMaterial(&pln->surface, material? ((dmuobjrecord_t*) material)->obj : NULL, false);
    Surface_SetColorRGBA(&pln->surface, r, g, b, a);
    Surface_SetMaterialOffsetXY(&pln->surface, matOffsetX, matOffsetY);
    pln->PS_normal[VX] = normalX;
    pln->PS_normal[VY] = normalY;
    pln->PS_normal[VZ] = normalZ;
    if(pln->PS_normal[VZ] < 0)
        pln->type = PLN_CEILING;
    else
        pln->type = PLN_FLOOR;
    M_Normalize(pln->PS_normal);
    pln->sector = sector;

    newList = Z_Malloc(sizeof(plane_t*) * (++sector->planeCount + 1), PU_STATIC, 0);
    for(i = 0; i < sector->planeCount - 1; ++i)
    {
        newList[i] = sector->planes[i];
    }
    newList[i++] = pln;
    newList[i] = NULL; // Terminate.

    if(sector->planes)
        Z_Free(sector->planes);
    sector->planes = newList;
}

void MPE_PlaneCreate(dmuobjrecordid_t sector, float height, material_t* material,
                     float matOffsetX, float matOffsetY,
                     float r, float g, float b, float a,
                     float normalX, float normalY, float normalZ)
{
    gamemap_t* map = editMap;

    if(!map)
        return;
    if(sector == 0 || sector > map->numSectors)
        return;

    Map_CreatePlane(map, map->sectors[sector - 1], height, material, matOffsetX, matOffsetY,
                    r, g, b, a, normalX, normalY, normalZ);
}

dmuobjrecordid_t MPE_SectorCreate(float lightLevel, float red, float green, float blue)
{
    sector_t* s;
    gamemap_t* map = editMap;

    if(!map)
        return 0;

    s = Map_CreateSector(map, lightLevel, red, green, blue);

    return s ? s->buildData.index : 0;
}

dmuobjrecordid_t MPE_PolyobjCreate(dmuobjrecordid_t* lines, uint lineCount, int tag,
                                   int sequenceType, float anchorX, float anchorY)
{
    uint i;
    polyobj_t* po;
    gamemap_t* map = editMap;

    if(!map)
        return 0;
    if(!lineCount || !lines)
        return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line;

        if(lines[i] == 0 || lines[i] > map->numLineDefs)
            return 0;

        line = map->lineDefs[lines[i] - 1];
        if(line->inFlags & LF_POLYOBJ)
            return 0;
    }

    po = Map_CreatePolyobj(map, lines, lineCount, tag, sequenceType, anchorX, anchorY);

    return po ? po->buildData.index : 0;
}

boolean MPE_GameObjProperty(const char* objName, uint idx,
                            const char* propName, valuetype_t type,
                            void* data)
{
    uint i;
    size_t len;
    gamemapobjdef_t* def;
    gamemap_t* map = editMap;

    if(!map || !map->editActive)
        return false;
    if(!objName || !propName || !data)
        return false; // Hmm...

    // Is this a known object?
    if((def = P_GetGameMapObjDef(0, objName, false)) == NULL)
        return false; // Nope.

    // Is this a known property?
    len = strlen(propName);
    for(i = 0; i < def->numProps; ++i)
    {
        if(!strnicmp(propName, def->props[i].name, len))
        {   // Found a match!
            // Create a record of this so that the game can query it later.
            P_AddGameMapObjValue(&map->gameObjData, def, i, idx, type, data);
            return true; // We're done.
        }
    }

    // An unknown property.
    VERBOSE(Con_Message("MPE_GameObjProperty: %s has no property \"%s\".\n",
                        def->name, propName));

    return false;
}

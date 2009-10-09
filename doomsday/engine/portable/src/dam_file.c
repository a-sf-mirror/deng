/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * dam_file.c: Doomsday Archived Map (DAM) reader/writer.
 */

// HEADER FILES ------------------------------------------------------------

#include <lzss.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_refresh.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// Global archived map format version identifier. Increment when making
// changes to the structure of the format.
#define DAM_VERSION             1

#define MAX_ARCHIVED_MATERIALS  2048
#define BADTEXNAME  "DD_BADTX"  // string that will be written in the texture
                                // archives to denote a missing texture.

// TYPES -------------------------------------------------------------------

// Segments of a doomsday archived map file.
typedef enum damsegment_e {
    DAMSEG_END = -1,                // Terminates a segment.
    DAMSEG_HEADER = 100,            // File-level meta.
    DAMSEG_RELOCATIONTABLES,        // Tables of offsets to file positions.
    DAMSEG_SYMBOLTABLES,            // Global symbol tables.

    DAMSEG_MAP = 200,               // Start of the map data.
    DAMSEG_POLYOBJS,
    DAMSEG_VERTEXES,
    DAMSEG_LINES,
    DAMSEG_SIDES,
    DAMSEG_SECTORS,
    DAMSEG_SSECTORS,
    DAMSEG_SEGS,
    DAMSEG_NODES,
    DAMSEG_BLOCKMAP,
    DAMSEG_REJECT
} damsegment_t;

typedef struct {
    char            name[9];
    material_namespace_t mnamespace;
} dictentry_t;

typedef struct {
    //// \todo Remove fixed limit.
    dictentry_t     table[MAX_ARCHIVED_MATERIALS];
    int             count;
} materialdict_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static LZFILE *mapFile;
static int mapFileVersion;

static materialdict_t *materialDict;

// CODE --------------------------------------------------------------------

/**
 * Called for every material in the map before saving by
 * initMaterialArchives.
 */
static void addMaterialToDict(materialdict_t* dict, material_t* mat)
{
#if 0
    int                 c;
    dictentry_t*        e;

    // Has this already been registered?
    for(c = 0; c < dict->count; c++)
    {
        if(dict->table[c].mnamespace == mat->mnamespace &&
           !stricmp(dict->table[c].name, mat->name))
        {   // Yes. skip it...
            return;
        }
    }

    e = &dict->table[dict->count];
    dict->count++;

    strncpy(e->name, mat->name, 8);
    e->name[8] = '\0';
    e->mnamespace = mat->mnamespace;
#endif
}

/**
 * Initializes the material archives (translation tables).
 * Must be called before writing the tables!
 */
static void initMaterialDict(const gamemap_t* map, materialdict_t* dict)
{
    uint                i, j;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t           *sec = &map->sectors[i];

        for(j = 0; j < sec->planeCount; ++j)
            addMaterialToDict(dict, sec->SP_planematerial(j));
    }

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t             *side = &map->sideDefs[i];

        addMaterialToDict(dict, side->SW_middlematerial);
        addMaterialToDict(dict, side->SW_topmaterial);
        addMaterialToDict(dict, side->SW_bottommaterial);
    }
}

static uint searchMaterialDict(materialdict_t *dict, const material_t* mat)
{
#if 0
    int                 i;

    for(i = 0; i < dict->count; i++)
        if(dict->table[i].mnamespace == mat->mnamespace &&
           !stricmp(dict->table[i].name, mat->name))
            return i;
#endif
    // Not found?!!!
    return 0;
}

/**
 * @return              The archive number of the given texture.
 */
static uint getMaterialDictID(materialdict_t* dict, const material_t* mat)
{
    return searchMaterialDict(dict, mat);
}

static material_t* lookupMaterialFromDict(materialdict_t* dict, int idx)
{
    dictentry_t*        e = &dict->table[idx];

    if(!strncmp(e->name, BADTEXNAME, 8))
        return NULL;

    return Materials_ToMaterial(Materials_IndexForName(e->mnamespace, e->name));
}

static boolean openMapFile(char* path, boolean write)
{
    mapFile = NULL;
    mapFileVersion = 0;
    mapFile = lzOpen(path, (write? F_WRITE_PACKED : F_READ_PACKED));

    return ((mapFile)? true : false);
}

static boolean closeMapFile(void)
{
    return (lzClose(mapFile)? true : false);
}

static void writeNBytes(void* data, long len)
{
    lzWrite(data, len, mapFile);
}

static void writeByte(byte val)
{
    lzPutC(val, mapFile);
}

static void writeShort(short val)
{
    lzPutW(val, mapFile);
}

static void writeLong(long val)
{
    lzPutL(val, mapFile);
}

static void writeFloat(float val)
{
    long temp = 0;
    memcpy(&temp, &val, 4);
    lzPutL(temp, mapFile);
}

static void readNBytes(void *ptr, long len)
{
    lzRead(ptr, len, mapFile);
}

static byte readByte(void)
{
    return lzGetC(mapFile);
}

static short readShort(void)
{
    return lzGetW(mapFile);
}

static long readLong(void)
{
    return lzGetL(mapFile);
}

static float readFloat(void)
{
    long    val = lzGetL(mapFile);
    float   returnValue = 0;

    memcpy(&returnValue, &val, 4);
    return returnValue;
}

/**
 * Exit with a fatal error if the value at the current location in the
 * map file does not match that associated with the specified segment.
 *
 * @param segType       Value by which to check for alignment.
 */
static void assertSegment(damsegment_t segment)
{
    if(readLong() != segment)
    {
        Con_Error("assertSegment: Segment [%d] failed alignment check",
                  (int) segment);
    }
}

static void beginSegment(damsegment_t segment)
{
    writeLong(segment);
}

static void endSegment(void)
{
    writeLong(DAMSEG_END);
}

static void writeVertex(const gamemap_t *map, uint idx)
{
    vertex_t           *v = &map->vertexes[idx];

    writeFloat(v->V_pos[VX]);
    writeFloat(v->V_pos[VY]);
    writeLong((long) v->numLineOwners);

    if(v->numLineOwners > 0)
    {
        lineowner_t        *own, *base;

        own = base = (v->lineOwners)->LO_prev;
        do
        {
            writeLong((long) ((own->lineDef - map->lineDefs) + 1));
            writeLong((long) own->angle);
            own = own->LO_prev;
        } while(own != base);
    }
}

static void readVertex(const gamemap_t* map, uint idx)
{
    uint                i;
    vertex_t*           v = &map->vertexes[idx];

    v->V_pos[VX] = readFloat();
    v->V_pos[VY] = readFloat();
    v->numLineOwners = (uint) readLong();

    if(v->numLineOwners > 0)
    {
        lineowner_t        *own;

        v->lineOwners = NULL;
        for(i = 0; i < v->numLineOwners; ++i)
        {
            own = Z_Malloc(sizeof(lineowner_t), PU_MAP, 0);
            own->lineDef = &map->lineDefs[(unsigned) (readLong() - 1)];
            own->angle = (binangle_t) readLong();

            own->LO_next = v->lineOwners;
            v->lineOwners = own;
        }

        own = v->lineOwners;
        do
        {
            own->LO_next->LO_prev = own;
            own = own->LO_next;
        } while(own);
        own->LO_prev = v->lineOwners;
    }
}

static void archiveVertexes(gamemap_t* map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_VERTEXES);
    else
        assertSegment(DAMSEG_VERTEXES);

    if(write)
    {
        writeLong((long) map->numVertexes);
        for(i = 0; i < map->numVertexes; ++i)
            writeVertex(map, i);
    }
    else
    {
        map->numVertexes = (uint) readLong();
        map->vertexes = Z_Malloc(sizeof(vertex_t) * map->numVertexes, PU_MAP, 0);
        for(i = 0; i < map->numVertexes; ++i)
            readVertex(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeLine(const gamemap_t* map, uint idx)
{
    int                 i;
    linedef_t*          l = &map->lineDefs[idx];

    writeLong(l->flags);
    writeByte(l->inFlags);
    writeFloat(l->dX);
    writeFloat(l->dY);
    writeLong((long) l->slopeType);
    writeFloat(l->bBox[BOXLEFT]);
    writeFloat(l->bBox[BOXRIGHT]);
    writeFloat(l->bBox[BOXBOTTOM]);
    writeFloat(l->bBox[BOXTOP]);
    writeFloat(l->length);
    writeLong((long) l->angle);
    for(i = 0; i < DDMAXPLAYERS; ++i)
        writeByte(l->mapped[DDMAXPLAYERS]? 1 : 0);
}

static void readLine(const gamemap_t* map, uint idx)
{
    int                 i;
    linedef_t*          l = &map->lineDefs[idx];

    l->flags = (int) readLong();
    l->inFlags = readByte();
    l->dX = readFloat();
    l->dY = readFloat();
    l->slopeType = (slopetype_t) readLong();
    l->bBox[BOXLEFT] = readFloat();
    l->bBox[BOXRIGHT] = readFloat();
    l->bBox[BOXBOTTOM] = readFloat();
    l->bBox[BOXTOP] = readFloat();
    l->length = readFloat();
    l->angle = (binangle_t) readLong();
    for(i = 0; i < DDMAXPLAYERS; ++i)
        l->mapped[i] = (readByte()? true : false);
}

static void archiveLines(gamemap_t* map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_LINES);
    else
        assertSegment(DAMSEG_LINES);

    if(write)
    {
        writeLong(map->numLineDefs);
        for(i = 0; i < map->numLineDefs; ++i)
            writeLine(map, i);
    }
    else
    {
        map->numLineDefs = readLong();
        map->lineDefs = Z_Malloc(sizeof(linedef_t) * map->numLineDefs, PU_MAP, 0);
        for(i = 0; i < map->numLineDefs; ++i)
            readLine(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSide(const gamemap_t *map, uint idx)
{
    uint                i;
    sidedef_t*          s = &map->sideDefs[idx];

    for(i = 0; i < 3; ++i)
    {
        surface_t*          suf = &s->sections[3];

        writeLong(suf->flags);
        writeLong(getMaterialDictID(materialDict, suf->material));
        writeLong((long) suf->blendMode);
        writeFloat(suf->normal[VX]);
        writeFloat(suf->normal[VY]);
        writeFloat(suf->normal[VZ]);
        writeFloat(suf->offset[VX]);
        writeFloat(suf->offset[VY]);
        writeFloat(suf->rgba[CR]);
        writeFloat(suf->rgba[CG]);
        writeFloat(suf->rgba[CB]);
        writeFloat(suf->rgba[CA]);
    }
    writeLong(s->sector? ((s->sector - map->sectors) + 1) : 0);
    writeShort(s->flags);
}

static void readSide(const gamemap_t* map, uint idx)
{
    uint                i;
    long                secIdx;
    float               offset[2], rgba[4];
    sidedef_t*          s = &map->sideDefs[idx];

    for(i = 0; i < 3; ++i)
    {
        surface_t          *suf = &s->sections[3];

        suf->flags = (int) readLong();
        Surface_SetMaterial(suf, lookupMaterialFromDict(materialDict, readLong()), false);
        Surface_SetBlendMode(suf, (blendmode_t) readLong());
        suf->normal[VX] = readFloat();
        suf->normal[VY] = readFloat();
        suf->normal[VZ] = readFloat();
        offset[VX] = readFloat();
        offset[VY] = readFloat();
        Surface_SetMaterialOffsetXY(suf, offset[VX], offset[VY]);
        rgba[CR] = readFloat();
        rgba[CG] = readFloat();
        rgba[CB] = readFloat();
        rgba[CA] = readFloat();
        Surface_SetColorRGBA(suf, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
        suf->decorations = NULL;
        suf->numDecorations = 0;
    }
    secIdx = readLong();
    s->sector = (secIdx == 0? NULL : &map->sectors[secIdx -1]);
    s->flags = readShort();
}

static void archiveSides(gamemap_t* map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SIDES);
    else
        assertSegment(DAMSEG_SIDES);

    if(write)
    {
        writeLong(map->numSideDefs);
        for(i = 0; i < map->numSideDefs; ++i)
            writeSide(map, i);
    }
    else
    {
        map->numSideDefs = readLong();
        map->sideDefs = Z_Malloc(sizeof(sidedef_t) * map->numSideDefs, PU_MAP, 0);
        for(i = 0; i < map->numSideDefs; ++i)
            readSide(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSector(const gamemap_t *map, uint idx)
{
    uint                i;
    sector_t           *s = &map->sectors[idx];

    writeFloat(s->lightLevel);
    writeFloat(s->rgb[CR]);
    writeFloat(s->rgb[CG]);
    writeFloat(s->rgb[CB]);
    writeLong(s->planeCount);
    for(i = 0; i < s->planeCount; ++i)
    {
        plane_t            *p = s->planes[i];

        writeFloat(p->height);
        writeFloat(p->glow);
        writeFloat(p->glowRGB[CR]);
        writeFloat(p->glowRGB[CG]);
        writeFloat(p->glowRGB[CB]);
        writeFloat(p->target);
        writeFloat(p->speed);
        writeFloat(p->visHeight);
        writeFloat(p->visHeightDelta);

        writeLong((long) p->surface.flags);
        writeLong(getMaterialDictID(materialDict, p->surface.material));
        writeLong((long) p->surface.blendMode);
        writeFloat(p->surface.normal[VX]);
        writeFloat(p->surface.normal[VY]);
        writeFloat(p->surface.normal[VZ]);
        writeFloat(p->surface.offset[VX]);
        writeFloat(p->surface.offset[VY]);
        writeFloat(p->surface.rgba[CR]);
        writeFloat(p->surface.rgba[CG]);
        writeFloat(p->surface.rgba[CB]);
        writeFloat(p->surface.rgba[CA]);

        writeFloat(p->soundOrg.pos[VX]);
        writeFloat(p->soundOrg.pos[VY]);
        writeFloat(p->soundOrg.pos[VZ]);
    }

    writeLong(s->flags);
    writeFloat(s->bBox[BOXLEFT]);
    writeFloat(s->bBox[BOXRIGHT]);
    writeFloat(s->bBox[BOXBOTTOM]);
    writeFloat(s->bBox[BOXTOP]);
    writeLong(s->lightSource? ((s->lightSource - map->sectors) + 1) : 0);
    writeFloat(s->soundOrg.pos[VX]);
    writeFloat(s->soundOrg.pos[VY]);
    writeFloat(s->soundOrg.pos[VZ]);

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeFloat(s->reverb[i]);

    // Lightgrid block indices.
    writeLong((long) s->changedBlockCount);
    writeLong((long) s->blockCount);
    for(i = 0; i < s->blockCount; ++i)
        writeShort(s->blocks[i]);

    // Line list.
    writeLong((long) s->lineDefCount);
    for(i = 0; i < s->lineDefCount; ++i)
        writeLong((s->lineDefs[i] - map->lineDefs) + 1);

    // SubSector list.
    writeLong((long) s->faceCount);
    for(i = 0; i < s->faceCount; ++i)
        writeLong((s->faces[i] - map->faces) + 1);

    // Reverb subsector attributors.
    writeLong((long) s->numReverbFaceAttributors);
    for(i = 0; i < s->numReverbFaceAttributors; ++i)
        writeLong((s->reverbFaces[i] - map->faces) + 1);
}

static void readSector(const gamemap_t* map, uint idx)
{
    uint                i, numPlanes;
    long                secIdx;
    float               offset[2], rgba[4];
    sector_t*           s = &map->sectors[idx];

    s->lightLevel = readFloat();
    s->rgb[CR] = readFloat();
    s->rgb[CG] = readFloat();
    s->rgb[CB] = readFloat();
    numPlanes = (uint) readLong();
    for(i = 0; i < numPlanes; ++i)
    {
        plane_t            *p = R_NewPlaneForSector(s);

        p->height = readFloat();
        p->glow = readFloat();
        p->glowRGB[CR] = readFloat();
        p->glowRGB[CG] = readFloat();
        p->glowRGB[CB] = readFloat();
        p->target = readFloat();
        p->speed = readFloat();
        p->visHeight = readFloat();
        p->visHeightDelta = readFloat();

        p->surface.flags = (int) readLong();
        Surface_SetMaterial(&p->surface, lookupMaterialFromDict(materialDict, readLong()), false);
        Surface_SetBlendMode(&p->surface, (blendmode_t) readLong());
        p->surface.normal[VX] = readFloat();
        p->surface.normal[VY] = readFloat();
        p->surface.normal[VZ] = readFloat();
        offset[VX] = readFloat();
        offset[VY] = readFloat();
        Surface_SetMaterialOffsetXY(&p->surface, offset[VX], offset[VY]);
        rgba[CR] = readFloat();
        rgba[CG] = readFloat();
        rgba[CB] = readFloat();
        rgba[CA] = readFloat();
        Surface_SetColorRGBA(&p->surface, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);

        p->soundOrg.pos[VX] = readFloat();
        p->soundOrg.pos[VY] = readFloat();
        p->soundOrg.pos[VZ] = readFloat();

        p->surface.decorations = NULL;
        p->surface.numDecorations = 0;
    }

    secIdx = readLong();
    s->flags = readLong();
    s->bBox[BOXLEFT] = readFloat();
    s->bBox[BOXRIGHT] = readFloat();
    s->bBox[BOXBOTTOM] = readFloat();
    s->bBox[BOXTOP] = readFloat();
    secIdx = readLong();
    s->lightSource = (secIdx == 0? NULL : &map->sectors[secIdx - 1]);
    s->soundOrg.pos[VX] = readFloat();
    s->soundOrg.pos[VY] = readFloat();
    s->soundOrg.pos[VZ] = readFloat();

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = readFloat();

    // Lightgrid block indices.
    s->changedBlockCount = (uint) readLong();
    s->blockCount = (uint) readLong();
    s->blocks = Z_Malloc(sizeof(short) * s->blockCount, PU_MAP, 0);
    for(i = 0; i < s->blockCount; ++i)
        s->blocks[i] = readShort();

    // Line list.
    s->lineDefCount = (uint) readLong();
    s->lineDefs = Z_Malloc(sizeof(linedef_t*) * (s->lineDefCount + 1), PU_MAP, 0);
    for(i = 0; i < s->lineDefCount; ++i)
        s->lineDefs[i] = &map->lineDefs[(unsigned) readLong() - 1];
    s->lineDefs[i] = NULL; // Terminate.

    // SubSector list.
    s->faceCount = (uint) readLong();
    s->faces =
        Z_Malloc(sizeof(face_t*) * (s->faceCount + 1), PU_MAP, 0);
    for(i = 0; i < s->faceCount; ++i)
        s->faces[i] = &map->faces[(unsigned) readLong() - 1];
    s->faces[i] = NULL; // Terminate.

    // Reverb subsector attributors.
    s->numReverbFaceAttributors = (uint) readLong();
    s->reverbFaces =
        Z_Malloc(sizeof(face_t*) * (s->numReverbFaceAttributors + 1), PU_MAP, 0);
    for(i = 0; i < s->numReverbFaceAttributors; ++i)
        s->reverbFaces[i] = &map->faces[(unsigned) readLong() - 1];
    s->reverbFaces[i] = NULL; // Terminate.
}

static void archiveSectors(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SECTORS);
    else
        assertSegment(DAMSEG_SECTORS);

    if(write)
    {
        writeLong(map->numSectors);
        for(i = 0; i < map->numSectors; ++i)
            writeSector(map, i);
    }
    else
    {
        map->numSectors = readLong();
        map->sectors = Z_Malloc(sizeof(sector_t) * map->numSectors, PU_MAP, 0);
        for(i = 0; i < map->numSectors; ++i)
            readSector(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSubSector(const gamemap_t* map, uint idx)
{
    uint                i;
    const subsector_t*  s = (const subsector_t*) map->faces[idx].data;

    writeFloat(s->bBox[0].pos[VX]);
    writeFloat(s->bBox[0].pos[VY]);
    writeFloat(s->bBox[0].pos[VZ]);
    writeFloat(s->bBox[1].pos[VX]);
    writeFloat(s->bBox[1].pos[VY]);
    writeFloat(s->bBox[1].pos[VZ]);
    writeFloat(s->midPoint.pos[VX]);
    writeFloat(s->midPoint.pos[VY]);
    writeFloat(s->midPoint.pos[VZ]);
    writeLong(s->sector? ((s->sector - map->sectors) + 1) : 0);
    writeLong(s->polyObj? (s->polyObj->idx + 1) : 0);

    // SubSector reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeLong((long) s->reverb[i]);

    // SubSector segs list.
/*    writeLong((long) s->hEdgeCount);
    for(i = 0; i < s->hEdgeCount; ++i)
        writeLong((s->hEdges[i] - map->hEdges) + 1);*/
}

static void readSubSector(const gamemap_t* map, uint idx)
{
    uint                i;
    long                obIdx;
    subsector_t*        s = (subsector_t*) map->faces[idx].data;

    s->bBox[0].pos[VX] = readFloat();
    s->bBox[0].pos[VY] = readFloat();
    s->bBox[0].pos[VZ] = readFloat();
    s->bBox[1].pos[VX] = readFloat();
    s->bBox[1].pos[VY] = readFloat();
    s->bBox[1].pos[VZ] = readFloat();
    s->midPoint.pos[VX] = readFloat();
    s->midPoint.pos[VY] = readFloat();
    s->midPoint.pos[VZ] = readFloat();
    obIdx = readLong();
    s->sector = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->polyObj = (obIdx == 0? NULL : map->polyObjs[(unsigned) obIdx - 1]);

    // SubSector reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = (uint) readLong();

    // SubSector segs list.
/*    s->hEdgeCount = (uint) readLong();
    s->hEdges = Z_Malloc(sizeof(hedge_t*) * (s->hEdgeCount + 1), PU_MAP, 0);
    for(i = 0; i < s->hEdgeCount; ++i)
        s->hEdges[i] = &map->hEdges[(unsigned) readLong() - 1];
    s->hEdges[i] = NULL; // Terminate.*/
}

static void archiveSubSectors(gamemap_t* map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SSECTORS);
    else
        assertSegment(DAMSEG_SSECTORS);

    if(write)
    {
        writeLong(map->numFaces);
        for(i = 0; i < map->numFaces; ++i)
            writeSubSector(map, i);
    }
    else
    {
        map->numFaces = readLong();
        map->faces = Z_Malloc(sizeof(face_t) * map->numFaces, PU_MAP, 0);
        for(i = 0; i < map->numFaces; ++i)
            readSubSector(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSeg(const gamemap_t* map, uint idx)
{
    const hedge_t*        hEdge = &map->hEdges[idx];

    writeLong(hEdge->next? ((hEdge->next - map->hEdges) + 1) : 0);
    writeLong(hEdge->prev? ((hEdge->prev - map->hEdges) + 1) : 0);
    writeLong(hEdge->twin? ((hEdge->twin - map->hEdges) + 1) : 0);
    writeLong((hEdge->v[0] - map->vertexes) + 1);
    writeLong((hEdge->v[1] - map->vertexes) + 1);
    writeLong(hEdge->face? ((hEdge->face - map->faces) + 1) : 0);

    {
    const seg_t*    s = (seg_t*) hEdge->data;

    writeFloat(s->length);
    writeFloat(s->offset);
    writeLong(s->sideDef? ((s->sideDef - map->sideDefs) + 1) : 0);
    writeLong((long) s->angle);
    writeByte(s->side);
    }
}

static void readSeg(const gamemap_t* map, uint idx)
{
    long                obIdx;
    hedge_t*              hEdge = &map->hEdges[idx];

    obIdx = readLong();
    hEdge->next = (obIdx == 0? NULL : &map->hEdges[(unsigned) obIdx - 1]);
    obIdx = readLong();
    hEdge->prev = (obIdx == 0? NULL : &map->hEdges[(unsigned) obIdx - 1]);
    obIdx = readLong();
    hEdge->twin = (obIdx == 0? NULL : &map->hEdges[(unsigned) obIdx - 1]);
    hEdge->v[0] = &map->vertexes[(unsigned) readLong() - 1];
    hEdge->v[1] = &map->vertexes[(unsigned) readLong() - 1];
    hEdge->face = (obIdx == 0? NULL : &map->faces[(unsigned) obIdx - 1]);

    {
    seg_t*          s = (seg_t*) hEdge->data;

    s->length = readFloat();
    s->offset = readFloat();
    obIdx = readLong();
    s->sideDef = (obIdx == 0? NULL : &map->sideDefs[(unsigned) obIdx - 1]);
    s->angle = (angle_t) readLong();
    s->side = readByte();
    }
}

static void archiveSegs(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SEGS);
    else
        assertSegment(DAMSEG_SEGS);

    if(write)
    {
        writeLong(map->numHEdges);
        for(i = 0; i < map->numHEdges; ++i)
            writeSeg(map, i);
    }
    else
    {
        map->numHEdges = readLong();
        map->hEdges = Z_Malloc(sizeof(hedge_t) * map->numHEdges, PU_MAP, 0);
        for(i = 0; i < map->numHEdges; ++i)
            readSeg(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeNode(const gamemap_t *map, uint idx)
{
    node_t             *n = &map->nodes[idx];

    writeFloat(n->partition.x);
    writeFloat(n->partition.y);
    writeFloat(n->partition.dX);
    writeFloat(n->partition.dY);
    writeFloat(n->bBox[RIGHT][BOXLEFT]);
    writeFloat(n->bBox[RIGHT][BOXRIGHT]);
    writeFloat(n->bBox[RIGHT][BOXBOTTOM]);
    writeFloat(n->bBox[RIGHT][BOXTOP]);
    writeFloat(n->bBox[LEFT][BOXLEFT]);
    writeFloat(n->bBox[LEFT][BOXRIGHT]);
    writeFloat(n->bBox[LEFT][BOXBOTTOM]);
    writeFloat(n->bBox[LEFT][BOXTOP]);
    writeLong((long) n->children[RIGHT]);
    writeLong((long) n->children[LEFT]);
}

static void readNode(const gamemap_t *map, uint idx)
{
    node_t             *n = &map->nodes[idx];

    n->partition.x = readFloat();
    n->partition.y = readFloat();
    n->partition.dX = readFloat();
    n->partition.dY = readFloat();
    n->bBox[RIGHT][BOXLEFT] = readFloat();
    n->bBox[RIGHT][BOXRIGHT] = readFloat();
    n->bBox[RIGHT][BOXBOTTOM] = readFloat();
    n->bBox[RIGHT][BOXTOP] = readFloat();
    n->bBox[LEFT][BOXLEFT] = readFloat();
    n->bBox[LEFT][BOXRIGHT] = readFloat();
    n->bBox[LEFT][BOXBOTTOM] = readFloat();
    n->bBox[LEFT][BOXTOP] = readFloat();
    n->children[RIGHT] = (uint) readLong();
    n->children[LEFT] = (uint) readLong();
}

static void archiveNodes(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_NODES);
    else
        assertSegment(DAMSEG_NODES);

    if(write)
    {
        writeLong(map->numNodes);
        for(i = 0; i < map->numNodes; ++i)
            writeNode(map, i);
    }
    else
    {
        map->numNodes = readLong();
        map->nodes = Z_Malloc(sizeof(node_t) * map->numNodes, PU_MAP, 0);
        for(i = 0; i < map->numNodes; ++i)
            readNode(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveBlockmap(gamemap_t *map, boolean write)
{
    if(write)
        beginSegment(DAMSEG_BLOCKMAP);
    else
        assertSegment(DAMSEG_BLOCKMAP);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveReject(gamemap_t *map, boolean write)
{
    if(write)
        beginSegment(DAMSEG_REJECT);
    else
        assertSegment(DAMSEG_REJECT);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writePolyobj(const gamemap_t* map, uint idx)
{
    //uint                i;
    const polyobj_t*    p = map->polyObjs[idx];

    writeLong((long) p->idx);
    writeFloat(p->pos[VX]);
    writeFloat(p->pos[VY]);
    writeFloat(p->pos[VZ]);
    writeLong((long) p->angle);
    writeLong((long) p->tag);
    writeFloat(p->box[0][VX]);
    writeFloat(p->box[0][VY]);
    writeFloat(p->box[1][VX]);
    writeFloat(p->box[1][VY]);
    writeFloat(p->dest[VX]);
    writeFloat(p->dest[VY]);
    writeFloat(p->speed);
    writeLong((long) p->destAngle);
    writeLong((long) p->angleSpeed);
    writeByte(p->crush? 1 : 0);
    writeLong((long) p->seqType);

    /*writeLong((long) p->numHEdges);
    for(i = 0; i < p->numHEdges; ++i)
    {
        const hedge_t*      hEdge = p->hEdges[i];
        const seg_t*        s = (seg_t*) hEdge->data;

        writeLong((hEdge->v[0] - map->vertexes) + 1);
        writeLong((hEdge->v[1] - map->vertexes) + 1);
        writeFloat(s->length);
        writeFloat(s->offset);
        writeLong(s->lineDef? ((s->lineDef - map->lineDefs) + 1) : 0);
        writeLong(s->sec[FRONT]? ((s->sec[FRONT] - map->sectors) + 1) : 0);
        writeLong((long) s->angle);
        writeByte(s->side);
        writeByte(s->flags);
    }*/
}

static void readPolyobj(const gamemap_t* map, uint idx)
{
    //uint                i;
    //long                obIdx;
    polyobj_t*          p = map->polyObjs[idx];

    p->idx = (uint) readLong();
    p->pos[VX] = readFloat();
    p->pos[VY] = readFloat();
    p->pos[VZ] = readFloat();
    p->angle = (angle_t) readLong();
    p->tag = (int) readLong();
    p->box[0][VX] = readFloat();
    p->box[0][VY] = readFloat();
    p->box[1][VX] = readFloat();
    p->box[1][VY] = readFloat();
    p->dest[VX] = readFloat();
    p->dest[VY] = readFloat();
    p->speed = readFloat();
    p->destAngle = (angle_t) readLong();
    p->angleSpeed = (angle_t) readLong();
    p->crush = (readByte()? true : false);
    p->seqType = (int) readLong();

    // Polyobj seg list.
    /*p->numHEdges = (uint) readLong();
    p->hEdges = Z_Malloc(sizeof(hedge_t*) * (p->numHEdges + 1), PU_MAP, 0);
    for(i = 0; i < p->numHEdges; ++i)
    {
        hedge_t*            hEdge =
            Z_Calloc(sizeof(*hEdge)+sizeof(seg_t), PU_MAP, 0);
        seg_t*              s;

        hEdge->data = ((byte*) hEdge) + sizeof(hedge_t);
        s = (seg_t*) hEdge->data;

        hEdge->v[0] = &map->vertexes[(unsigned) readLong() - 1];
        hEdge->v[1] = &map->vertexes[(unsigned) readLong() - 1];
        s->length = readFloat();
        s->offset = readFloat();
        obIdx = readLong();
        s->lineDef = (obIdx == 0? NULL : &map->lineDefs[(unsigned) obIdx - 1]);
        obIdx = readLong();
        s->sec[FRONT] = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
        s->angle = (angle_t) readLong();
        s->side = (readByte()? 1 : 0);
        s->flags = readByte();

        p->hEdges[i] = hEdge;
    }
    p->hEdges[i] = NULL; // Terminate.*/
}

static void archivePolyobjs(gamemap_t* map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_POLYOBJS);
    else
        assertSegment(DAMSEG_POLYOBJS);

    if(write)
    {
        writeLong(map->numPolyObjs);
        for(i = 0; i < map->numPolyObjs; ++i)
            writePolyobj(map, i);
    }
    else
    {
        map->numPolyObjs = readLong();
        map->polyObjs = Z_Malloc(sizeof(polyobj_t*) * map->numPolyObjs, PU_MAP, 0);
        for(i = 0; i < map->numPolyObjs; ++i)
            readPolyobj(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeThing(const gamemap_t *map, uint idx)
{

}

static void readThing(const gamemap_t *map, uint idx)
{

}

static void archiveMap(gamemap_t *map, boolean write)
{
    if(write)
        beginSegment(DAMSEG_MAP);
    else
    {
        assertSegment(DAMSEG_MAP);

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_VERTEX, map->numVertexes);
            gx.SetupForMapData(DMU_LINEDEF, map->numLineDefs);
            gx.SetupForMapData(DMU_SIDEDEF, map->numSideDefs);
            gx.SetupForMapData(DMU_SECTOR, map->numSectors);
        }
    }

    archivePolyobjs(map, write);
    archiveVertexes(map, write);
    archiveLines(map, write); // Must follow vertexes (lineowner nodes).
    archiveSides(map, write);
    archiveSectors(map, write);
    archiveSubSectors(map, write);
    archiveSegs(map, write);
    archiveNodes(map, write);
    archiveBlockmap(map, write);
    archiveReject(map, write);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveMaterialDict(materialdict_t *dict, boolean write)
{
    int                 i;

    if(write)
    {
        writeLong((long) dict->count);
        for(i = 0; i < dict->count; ++i)
        {
            writeNBytes(dict->table[i].name, 8);
        }
    }
    else
    {
        dict->count = readLong();
        for(i = 0; i < dict->count; ++i)
        {
            readNBytes(dict->table[i].name, 8);
            dict->table[i].name[8] = 0;
        }
    }
}
static void archiveSymbolTables(boolean write)
{
    if(write)
        beginSegment(DAMSEG_SYMBOLTABLES);
    else
        assertSegment(DAMSEG_SYMBOLTABLES);

    archiveMaterialDict(materialDict, write);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveRelocationTables(boolean write)
{
    if(write)
        beginSegment(DAMSEG_RELOCATIONTABLES);
    else
        assertSegment(DAMSEG_RELOCATIONTABLES);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveHeader(boolean write)
{
    if(write)
        beginSegment(DAMSEG_HEADER);
    else
        assertSegment(DAMSEG_HEADER);

    if(write)
    {
        writeLong(DAM_VERSION);
    }
    else
    {
        mapFileVersion = readLong();
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static boolean doArchiveMap(gamemap_t *map, filename_t path,
                            boolean write)
{
    if(!path)
        return false;

    // Open the file.
    if(!openMapFile(path, write))
        return false; // Hmm, invalid path?

    materialDict = M_Calloc(sizeof(*materialDict));

    if(write)
        initMaterialDict(map, materialDict);

    archiveHeader(write);
    archiveRelocationTables(write);
    archiveSymbolTables(write);
    archiveMap(map, write);

    // Close the file.
    closeMapFile();

    M_Free(materialDict);

    return true;
}

/**
 * Load data from a Doomsday archived map file.
 */
boolean DAM_MapWrite(gamemap_t *map, filename_t path)
{
    return doArchiveMap(map, path, true);
}

/**
 * Write the current state of a map into a Doomsday archived map file.
 */
boolean DAM_MapRead(gamemap_t *map, filename_t path)
{
    Con_Message("DAM_MapRead: Loading cached map. %s\n", path);
    return doArchiveMap(map, path, false);
}

/**
 * Check if archived map file is current.
 */
boolean DAM_MapIsValid(filename_t cachedMapDataFile, int markerLumpNum)
{
    uint                sourceTime, buildTime;

    // The source data must not be newer than the cached map data.
    sourceTime = F_LastModified(W_LumpSourceFile(markerLumpNum));
    buildTime = F_LastModified(cachedMapDataFile);

    if(F_Access(cachedMapDataFile) && !(buildTime < sourceTime))
    {   // Ok, lets check the header.
        if(openMapFile(cachedMapDataFile, false))
        {
            archiveHeader(false);
            closeMapFile();

            if(mapFileVersion == DAM_VERSION)
                return true; // Its good.
        }
    }

    return false;
}

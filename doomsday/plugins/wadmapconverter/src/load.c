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
 * load.c: Load and analyzation of the map data structures.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// MACROS ------------------------------------------------------------------

// Size of the map data structures in bytes in the arrived WAD format.
#define SIZEOF_64VERTEX         (4 * 2)
#define SIZEOF_VERTEX           (2 * 2)
#define SIZEOF_64THING          (2 * 7)
#define SIZEOF_XTHING           (2 * 7 + 1 * 6)
#define SIZEOF_THING            (2 * 5)
#define SIZEOF_XLINEDEF         (2 * 5 + 1 * 6)
#define SIZEOF_64LINEDEF        (2 * 6 + 1 * 4)
#define SIZEOF_LINEDEF          (2 * 7)
#define SIZEOF_64SIDEDEF        (2 * 6)
#define SIZEOF_SIDEDEF          (2 * 3 + 8 * 3)
#define SIZEOF_64SECTOR         (2 * 12)
#define SIZEOF_SECTOR           (2 * 5 + 8 * 2)
#define SIZEOF_LIGHT            (1 * 6)

#define PO_LINE_START           (1) // polyobj line start special
#define PO_LINE_EXPLICIT        (5)
#define PO_ANCHOR_DOOMEDNUM     (3000)
#define PO_SPAWN_DOOMEDNUM      (3001)
#define PO_SPAWNCRUSH_DOOMEDNUM (3002)

#define SEQTYPE_NUMSEQ          (10)

// TYPES -------------------------------------------------------------------

typedef enum lumptype_e {
    ML_INVALID = -1,
    FIRST_LUMP_TYPE,
    ML_LABEL = FIRST_LUMP_TYPE, // A separator, name, ExMx or MAPxx
    ML_THINGS,                  // Monsters, items..
    ML_LINEDEFS,                // LineDefs, from editing
    ML_SIDEDEFS,                // SideDefs, from editing
    ML_VERTEXES,                // Vertices, edited and BSP splits generated
    ML_SEGS,                    // LineSegs, from LineDefs split by BSP
    ML_SSECTORS,                // SubSectors, list of LineSegs
    ML_NODES,                   // BSP nodes
    ML_SECTORS,                 // Sectors, from editing
    ML_REJECT,                  // LUT, sector-sector visibility
    ML_BLOCKMAP,                // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR,                // ACS Scripts (compiled).
    ML_SCRIPTS,                 // ACS Scripts (source).
    ML_LIGHTS,                  // Surface color tints.
    ML_MACROS,                  // DOOM64 format, macro scripts.
    ML_LEAFS,                   // DOOM64 format, segs (close subsectors).
    ML_GLVERT,                  // GL vertexes
    ML_GLSEGS,                  // GL segs
    ML_GLSSECT,                 // GL subsectors
    ML_GLNODES,                 // GL nodes
    ML_GLPVS,                   // GL PVS dataset
    NUM_LUMP_TYPES
} lumptype_t;

/*typedef struct usecrecord_s {
    sector_t*           sec;
    double              nearPos[2];
} usecrecord_t;*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

/*boolean         registerUnclosedSectorNear(sector_t* sec, double x, double y);
void            printUnclosedSectorList(void);*/

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint PolyLineCount;
static int16_t PolyStart[2];

/*static uint numUnclosedSectors;
static usecrecord_t* unclosedSectors;*/

// CODE --------------------------------------------------------------------

static int C_DECL compareMaterialNames(const void* a, const void* b)
{
    return stricmp((*(materialref_t**)a)->name, (*(materialref_t**)b)->name);
}

static materialref_t* getMaterial2(const char* regName,
                                   materialref_t* const ** list, size_t size)
{
    int                 result;
    size_t              bottomIdx, topIdx, pivot;
    materialref_t*      m;
    boolean             isDone;
    char                name[9];

    if(size == 0)
        return NULL;

    if(map->format == MF_DOOM64)
    {
        int                 idx = *((int*) regName);

        dd_snprintf(name, 9, "UNK%05i", idx);
    }
    else
    {
        size_t              len = MIN_OF(8, strlen(regName));

        strncpy(name, regName, len);
        name[len] = '\0';
    }

    bottomIdx = 0;
    topIdx = size-1;
    m = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        materialref_t*      cand;

        pivot = bottomIdx + (topIdx - bottomIdx)/2;
        cand = (*list)[pivot];

        result = stricmp(cand->name, name);
        if(result == 0)
        {   // Found.
            m = cand;
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {   // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return m;
}

static materialref_t* getMaterial(const char* name, boolean isFlat)
{
    return getMaterial2(name, isFlat? &map->flats : &map->textures,
                        isFlat? map->numFlats : map->numTextures);
}

static void addMaterialToList(materialref_t* m, materialref_t*** list,
                              size_t* size)
{
    size_t              i;

    // Enlarge the list.
    (*list) = realloc((*list), sizeof(m) * ++(*size));

    // Find insertion point.
    for(i = 0; i < (*size) - 1; ++i)
        if(compareMaterialNames(&(*list)[i], &m) > 0)
            break;

    // Shift the rest over.
    if((*size) > 1)
        memmove(&((*list)[i+1]), &((*list)[i]), sizeof(m) * ((*size)-1-i));

    // Insert the new element.
    (*list)[i] = m;
}

/**
 * Is the name of the material reference known to Doomsday?
 */
static __inline boolean isUnknownMaterialRef(const materialref_t* m, boolean isFlat)
{
    return m->material == NULL ? true : false;
}

const materialref_t* RegisterMaterial(const char* name, boolean isFlat)
{
    materialref_t*      m;

    // When loading DOOM or Hexen format maps check for the special case "-"
    // texture name (no texture).
    if(!isFlat && (map->format == MF_DOOM || map->format == MF_HEXEN) &&
       !stricmp(name, "-"))
        return NULL;

    // Check if this material has already been registered.
    if((m = getMaterial(name, isFlat)) != NULL)
    {
        m->refCount++;
        return m;
    }
    else
    {
        /**
         * A new material.
         */
        m = malloc(sizeof(*m));

        if(map->format == MF_DOOM64)
        {
            int                 idx = *((int*) name);

            dd_snprintf(m->name, 9, "UNK%05i", idx);

            // First try the prefered namespace, then any.
            if(!(m->material = R_MaterialForTextureId((isFlat? MN_FLATS : MN_TEXTURES), idx)))
                m->material = R_MaterialForTextureId(MN_ANY, idx);
        }
        else
        {
            size_t          len = MIN_OF(8, strlen(name));

            strncpy(m->name, name, len);
            m->name[len] = '\0';

            // First try the prefered namespace, then any.
            if(!(m->material = P_MaterialForName((isFlat? MN_FLATS : MN_TEXTURES), m->name)))
                m->material = P_MaterialForName(MN_ANY, m->name);
        }

        // Add it to the material reference list.
        addMaterialToList(m, isFlat? &map->flats : &map->textures,
                          isFlat? &map->numFlats : &map->numTextures);
        m->refCount = 1;

        if(isUnknownMaterialRef(m, isFlat))
        {
            if(isFlat)
                ++map->numUnknownFlats;
            else
                ++map->numUnknownTextures;
        }

        return m;
    }
}

void LogUnknownMaterials(void)
{
    static const char* nameStr[] = { "name", "names" };

    if(map->numUnknownFlats)
    {
        size_t              i;

        Con_Message("WadMapConverter: Warning: Found %u bad flat %s:\n",
                    map->numUnknownFlats,
                    nameStr[map->numUnknownFlats? 1 : 0]);

        for(i = 0; i < map->numFlats; ++i)
        {
            materialref_t*      m = map->flats[i];

            if(isUnknownMaterialRef(m, true))
                Con_Message(" %4u x \"%s\"\n", m->refCount, m->name);
        }
    }

    if(map->numUnknownTextures)
    {
        size_t              i;

        Con_Message("WadMapConverter: Warning: Found %u bad texture %s:\n",
                    map->numUnknownTextures,
                    nameStr[map->numUnknownTextures? 1 : 0]);

        for(i = 0; i < map->numTextures; ++i)
        {
            materialref_t*      m = map->textures[i];

            if(isUnknownMaterialRef(m, false))
                Con_Message(" %4u x \"%s\"\n", m->refCount, m->name);
        }
    }
}

#if 0
/**
 * Register the specified sector in the list of unclosed sectors.
 *
 * @param sec           Ptr to the sector to be registered.
 * @param x             Approximate X coordinate to the sector's origin.
 * @param y             Approximate Y coordinate to the sector's origin.
 *
 * @return              @c true, if sector was registered.
 */
static boolean registerUnclosedSectorNear(sector_t* sec, double x, double y)
{
    uint i;
    usecrecord_t* usec;

    if(!sec)
        return false; // Wha?

    // Has this sector already been registered as unclosed?
    for(i = 0; i < numUnclosedSectors; ++i)
    {
        if(unclosedSectors[i].sec == sec)
            return true;
    }

    // A new one.
    unclosedSectors = M_Realloc(unclosedSectors,
                                ++numUnclosedSectors * sizeof(usecrecord_t));
    usec = &unclosedSectors[numUnclosedSectors-1];
    usec->sec = sec;
    usec->nearPos[VX] = x;
    usec->nearPos[VY] = y;

    return true;
}

/**
 * Print the list of unclosed sectors.
 */
static void printUnclosedSectorList(void)
{
    uint i;

    if(!editMapInited)
        return;

    if(numUnclosedSectors)
    {
        Con_Printf("Warning, found %u unclosed sectors:\n", numUnclosedSectors);

        for(i = 0; i < numUnclosedSectors; ++i)
        {
            usecrecord_t* usec = &unclosedSectors[i];

            Con_Printf("  #%d near [%1.1f, %1.1f]\n", usec->sec->buildData.index - 1,
                       usec->nearPos[VX], usec->nearPos[VY]);
        }
    }
}

/**
 * Free the list of unclosed sectors.
 */
static void freeUnclosedSectorList(void)
{
    if(unclosedSectors)
        M_Free(unclosedSectors);
    unclosedSectors = NULL;
    numUnclosedSectors = 0;
}
#endif

/**
 * Attempts to load the BLOCKMAP data resource.
 */
#if 0 // Needs updating.
static boolean loadBlockmap(tempmap_t* map, maplumpinfo_t* maplump)
{
#define MAPBLOCKUNITS       128

    boolean generateBMap = (createBMap == 2)? true : false;

    Con_Message("WadMapConverter::loadBlockmap: Processing...\n");

    // Do we have a lump to process?
    if(maplump->lumpNum == -1 || maplump->length == 0)
        generateBMap = true; // We'll HAVE to generate it.

    // Are we generating new blockmap data?
    if(generateBMap)
    {
        // Only announce if the user has choosen to always generate
        // new data (we will have already announced it if the lump
        // was missing).
        if(maplump->lumpNum != -1)
            VERBOSE(
            Con_Message("loadBlockMap: Generating NEW blockmap...\n"));
    }
    else
    {   // No, the existing data is valid - so load it in.
        uint startTime;
        blockmap_t* blockmap;
        uint x, y, width, height;
        float v[2];
        vec2_t bounds[2];
        long* lineListOffsets, i, n, numBlocks, blockIdx;
        short* blockmapLump;

        VERBOSE(
        Con_Message("loadBlockMap: Converting existing blockmap...\n"));

        startTime = Sys_GetRealTime();

        blockmapLump =
            (short *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        v[VX] = (float) SHORT(blockmapLump[0]);
        v[VY] = (float) SHORT(blockmapLump[1]);
        width  = ((SHORT(blockmapLump[2])) & 0xffff);
        height = ((SHORT(blockmapLump[3])) & 0xffff);

        numBlocks = (long) width * (long) height;

        /**
         * Expand WAD blockmap into a larger one, by treating all
         * offsets except -1 as unsigned and zero-extending them.
         * This potentially doubles the size of blockmaps allowed
         * because DOOM originally considered the offsets as always
         * signed.
         */

        lineListOffsets = M_Malloc(sizeof(long) * numBlocks);
        n = 4;
        for(i = 0; i < numBlocks; ++i)
        {
            short t = SHORT(blockmapLump[n++]);
            lineListOffsets[i] = (t == -1? -1 : (long) t & 0xffff);
        }

        /**
         * Finally, convert the blockmap into our internal representation.
         * We'll ensure the blockmap is formed correctly as we go.
         *
         * \todo We could gracefully handle malformed blockmaps by
         * cleaning up and then generating our own.
         */

        V2_Set(bounds[0], v[VX], v[VY]);
        v[VX] += (float) (width * MAPBLOCKUNITS);
        v[VY] += (float) (height * MAPBLOCKUNITS);
        V2_Set(bounds[1], v[VX], v[VY]);

        blockmap = P_BlockmapCreate(bounds[0], bounds[1],
                                    width, height);
        blockIdx = 0;
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
            {
                long offset = lineListOffsets[blockIdx];
                long idx;
                uint count;

#if _DEBUG
if(SHORT(blockmapLump[offset]) != 0)
{
    Con_Error("loadBlockMap: Offset (%li) for block %u [%u, %u] "
              "does not index the beginning of a line list!\n",
              offset, blockIdx, x, y);
}
#endif

                // Count the number of lines in this block.
                count = 0;
                while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    count++;

                if(count > 0)
                {
                    linedef_t** lines, **ptr;

                    // A NULL-terminated array of pointers to lines.
                    lines = Z_Malloc((count + 1) * sizeof(linedef_t *), PU_STATIC, NULL);

                    // Copy pointers to the array, delete the nodes.
                    ptr = lines;
                    count = 0;
                    while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    {
#if _DEBUG
if(idx < 0 || idx >= (long) map->numLines)
{
    Con_Error("loadBlockMap: Invalid linedef id %li\n!", idx);
}
#endif
                        *ptr++ = &map->lines[idx];
                        count++;
                    }
                    // Terminate.
                    *ptr = NULL;

                    // Link it into the BlockMap.
                    P_BlockmapSetBlock(blockmap, x, y, lines, NULL);
                }

                blockIdx++;
            }

        // Don't need this anymore.
        M_Free(lineListOffsets);

        map->blockMap = blockmap;

        // How much time did we spend?
        VERBOSE(Con_Message
                ("loadBlockMap: Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - startTime) / 1000.0f));
    }

    return true;

#undef MAPBLOCKUNITS
}
#endif

#if 0
/**
 * The REJECT resource is a LUT that provides the results of trivial
 * line-of-sight tests between sectors. This is done with a matrix of sector
 * pairs i.e. if a monster in sector 4 can see the player in sector 2; the
 * inverse should be true.
 *
 * Note however, some PWADS have carefully constructed REJECT data to create
 * special effects. For example it is possible to make a player completely
 * invissible in certain sectors.
 *
 * The format of the table is a simple matrix of boolean values, a (true)
 * value indicates that it is impossible for mobjs in sector A to see mobjs
 * in sector B (and vice-versa). A (false) value indicates that a
 * line-of-sight MIGHT be possible and a more accurate (thus more expensive)
 * calculation will have to be made.
 *
 * The table itself is constructed as follows:
 *
 *     X = sector num player is in
 *     Y = sector num monster is in
 *
 *         X
 *
 *       0 1 2 3 4 ->
 *     0 1 - 1 - -
 *  Y  1 - - 1 - -
 *     2 1 1 - - 1
 *     3 - - - 1 -
 *    \|/
 *
 * These results are read left-to-right, top-to-bottom and are packed into
 * bytes (each byte represents eight results). As are all lumps in WAD the
 * data is in little-endian order.
 *
 * Thus the size of a valid REJECT lump can be calculated as:
 *
 *     ceiling(numSectors^2)
 *
 * For now we only do very basic reject processing, limited to determining
 * all isolated sector groups (islands that are surrounded by void space).
 *
 * \note Algorithm:
 * Initially all sectors are in individual groups. Next, we scan the linedef
 * list. For each 2-sectored line, merge the two sector groups into one.
 */
static void buildReject(gamemap_t *map)
{
/**
 * \todo We can do something much better now that we are building the BSP.
 */
    int         i;
    int         group;
    int        *secGroups;
    int         view, target;
    size_t      rejectSize;
    byte       *matrix;

    secGroups = M_Malloc(sizeof(int) * numSectors);
    for(i = 0; i < numSectors; ++i)
    {
        sector_t  *sec = LookupSector(i);
        secGroups[i] = group++;
        sec->rejNext = sec->rejPrev = sec;
    }

    for(i = 0; i < numLinedefs; ++i)
    {
        linedef_t  *line = LookupLinedef(i);
        sector_t   *sec1, *sec2, *p;

        if(!line->sideDefs[FRONT] || !line->sideDefs[BACK])
            continue;

        sec1 = line->sideDefs[FRONT]->sector;
        sec2 = line->sideDefs[BACK]->sector;

        if(!sec1 || !sec2 || sec1 == sec2)
            continue;

        // Already in the same group?
        if(secGroups[sec1->index] == secGroups[sec2->index])
            continue;

        // Swap sectors so that the smallest group is added to the biggest
        // group. This is based on the assumption that sector numbers in
        // wads will generally increase over the set of linedefs, and so
        // (by swapping) we'll tend to add small groups into larger
        // groups, thereby minimising the updates to 'rej_group' fields
        // that is required when merging.
        if(secGroups[sec1->index] > secGroups[sec2->index])
        {
            p = sec1;
            sec1 = sec2;
            sec2 = p;
        }

        // Update the group numbers in the second group
        secGroups[sec2->index] = secGroups[sec1->index];
        for(p = sec2->rejNext; p != sec2; p = p->rejNext)
            secGroups[p->index] = secGroups[sec1->index];

        // Merge 'em baby...
        sec1->rejNext->rejPrev = sec2;
        sec2->rejNext->rejPrev = sec1;

        p = sec1->rejNext;
        sec1->rejNext = sec2->rejNext;
        sec2->rejNext = p;
    }

    rejectSize = (numSectors * numSectors + 7) / 8;
    matrix = Z_Calloc(rejectSize, PU_STATIC, 0);

    for(view = 0; view < numSectors; ++view)
        for(target = 0; target < view; ++target)
        {
            int         p1, p2;

            if(secGroups[view] == secGroups[target])
                continue;

            // For symmetry, do two bits at a time.
            p1 = view * numSectors + target;
            p2 = target * numSectors + view;

            matrix[p1 >> 3] |= (1 << (p1 & 7));
            matrix[p2 >> 3] |= (1 << (p2 & 7));
        }

    M_Free(secGroups);
}
#endif

int DataTypeForLumpName(const char* name)
{
    struct lumptype_s {
        lumptype_t      type;
        const char*     name;
    } knownLumps[] =
    {
        {ML_LABEL,      "*"},
        {ML_THINGS,     "THINGS"},
        {ML_LINEDEFS,   "LINEDEFS"},
        {ML_SIDEDEFS,   "SIDEDEFS"},
        {ML_VERTEXES,   "VERTEXES"},
        {ML_SEGS,       "SEGS"},
        {ML_SSECTORS,   "SSECTORS"},
        {ML_NODES,      "NODES"},
        {ML_SECTORS,    "SECTORS"},
        {ML_REJECT,     "REJECT"},
        {ML_BLOCKMAP,   "BLOCKMAP"},
        {ML_BEHAVIOR,   "BEHAVIOR"},
        {ML_SCRIPTS,    "SCRIPTS"},
        {ML_LIGHTS,     "LIGHTS"},
        {ML_MACROS,     "MACROS"},
        {ML_LEAFS,      "LEAFS"},
        {ML_GLVERT,     "GL_VERT"},
        {ML_GLSEGS,     "GL_SEGS"},
        {ML_GLSSECT,    "GL_SSECT"},
        {ML_GLNODES,    "GL_NODES"},
        {ML_GLPVS,      "GL_PVS"},
        {ML_INVALID,    NULL},
    };
    lumptype_t          i;

    if(name && name[0])
    {
        for(i = FIRST_LUMP_TYPE; knownLumps[i].type != ML_INVALID; ++i)
        {
            if(!strncmp(knownLumps[i].name, name, 8))
                return knownLumps[i].type;
        }
    }

    return ML_INVALID;
}

/**
 * @return              @c  0 = Unclosed polygon.
 */
static int isClosedPolygon(mline_t **lineList, size_t num)
{
    uint            i;

    for(i = 0; i < num; ++i)
    {
        mline_t         *line = lineList[i];
        mline_t         *next = (i == num-1? lineList[0] : lineList[i+1]);

        if(!(line->v[1] == next->v[0]))
             return false;
    }

    // The polygon is closed.
    return true;
}

/**
 * Create a temporary polyobj (read from the original map data).
 */
static boolean createPolyobj(mline_t **lineList, uint num, uint *poIdx,
                             int tag, int sequenceType, int16_t anchorX,
                             int16_t anchorY)
{
    uint                i;
    mpolyobj_t          *po, **newList;

    if(!lineList || num == 0)
        return false;

    // Ensure that lineList is a closed polygon.
    if(isClosedPolygon(lineList, num) == 0)
    {   // Nope, perhaps it needs sorting?
        Con_Error("WadMapConverter::createPolyobj: Linelist does not form a closed polygon.");
    }

    // Allocate the new polyobj.
    po = calloc(1, sizeof(*po));

    /**
     * Link the new polyobj into the global list.
     */
    newList = malloc(((++map->numPolyobjs) + 1) * sizeof(mpolyobj_t*));
    // Copy the existing list.
    for(i = 0; i < map->numPolyobjs - 1; ++i)
    {
        newList[i] = map->polyobjs[i];
    }
    newList[i++] = po; // Add the new polyobj.
    newList[i] = NULL; // Terminate.

    if(map->numPolyobjs-1 > 0)
        free(map->polyobjs);
    map->polyobjs = newList;

    po->idx = map->numPolyobjs-1;
    po->tag = tag;
    po->seqType = sequenceType;
    po->anchor[VX] = anchorX;
    po->anchor[VY] = anchorY;
    po->lineCount = num;
    po->lineIndices = malloc(sizeof(uint) * num);
    for(i = 0; i < num; ++i)
    {
        // This line is part of a polyobj.
        lineList[i]->aFlags |= LAF_POLYOBJ;

        po->lineIndices[i] = lineList[i] - map->lines;
    }

    if(poIdx)
        *poIdx = po->idx;

    return true; // Success!
}

/**
 * @param lineList      @c NULL, will cause IterFindPolyLines to count
 *                      the number of lines in the polyobj.
 */
static boolean iterFindPolyLines(int16_t x, int16_t y, mline_t **lineList)
{
    uint                i;

    if(x == PolyStart[VX] && y == PolyStart[VY])
    {
        return true;
    }

    for(i = 0; i < map->numLines; ++i)
    {
        mline_t        *line = &map->lines[i];
        int16_t        v1[2], v2[2];

        v1[VX] = (int16_t) map->vertexes[(line->v[0] - 1) * 2];
        v1[VY] = (int16_t) map->vertexes[(line->v[0] - 1) * 2 + 1];
        v2[VX] = (int16_t) map->vertexes[(line->v[1] - 1) * 2];
        v2[VY] = (int16_t) map->vertexes[(line->v[1] - 1) * 2 + 1];

        if(v1[VX] == x && v1[VY] == y)
        {
            if(!lineList)
                PolyLineCount++;
            else
                *lineList++ = line;

            iterFindPolyLines(v2[VX], v2[VY], lineList);
            return true;
        }
    }

    return false;
}

/**
 * Find all linedefs marked as belonging to a polyobject with the given tag
 * and attempt to create a polyobject from them.
 *
 * @param tag           Line tag of linedefs to search for.
 *
 * @return              @c true = successfully created polyobj.
 */
static boolean findAndCreatePolyobj(int16_t tag, int16_t anchorX,
                                    int16_t anchorY)
{
#define MAXPOLYLINES         32

    uint                i;

    for(i = 0; i < map->numLines; ++i)
    {
        mline_t            *line = &map->lines[i];

        if(line->xType == PO_LINE_START && line->xArgs[0] == tag)
        {
            byte                seqType;
            mline_t           **lineList;
            int16_t             v1[2], v2[2];
            uint                poIdx;

            line->xType = 0;
            line->xArgs[0] = 0;
            PolyLineCount = 1;

            v1[VX] = (int16_t) map->vertexes[(line->v[0]-1) * 2];
            v1[VY] = (int16_t) map->vertexes[(line->v[0]-1) * 2 + 1];
            v2[VX] = (int16_t) map->vertexes[(line->v[1]-1) * 2];
            v2[VY] = (int16_t) map->vertexes[(line->v[1]-1) * 2 + 1];
            PolyStart[VX] = v1[VX];
            PolyStart[VY] = v1[VY];
            if(!iterFindPolyLines(v2[VX], v2[VY], NULL))
            {
                Con_Error("WadMapConverter::findAndCreatePolyobj: Found unclosed polyobj.\n");
            }

            lineList = malloc((PolyLineCount+1) * sizeof(mline_t*));

            lineList[0] = line; // Insert the first line.
            iterFindPolyLines(v2[VX], v2[VY], lineList + 1);
            lineList[PolyLineCount] = 0; // Terminate.

            seqType = line->xArgs[2];
            if(seqType >= SEQTYPE_NUMSEQ)
                seqType = 0;

            if(createPolyobj(lineList, PolyLineCount, &poIdx, tag,
                             seqType, anchorX, anchorY))
            {
                free(lineList);
                return true;
            }

            free(lineList);
        }
    }

    /**
     * Didn't find a polyobj through PO_LINE_START.
     * We'll try another approach...
     */
    {
    mline_t         *polyLineList[MAXPOLYLINES];
    uint            lineCount = 0;
    uint            j, psIndex, psIndexOld;

    psIndex = 0;
    for(j = 1; j < MAXPOLYLINES; ++j)
    {
        psIndexOld = psIndex;
        for(i = 0; i < map->numLines; ++i)
        {
            mline_t         *line = &map->lines[i];

            if(line->xType == PO_LINE_EXPLICIT &&
               line->xArgs[0] == tag)
            {
                if(!line->xArgs[1])
                {
                    Con_Error
                        ("WadMapConverter::findAndCreatePolyobj: Explicit line missing order number "
                         "(probably %d) in poly %d.\n",
                         j + 1, tag);
                }

                if(line->xArgs[1] == j)
                {
                    // Add this line to the list.
                    polyLineList[psIndex] = line;
                    lineCount++;
                    psIndex++;
                    if(psIndex > MAXPOLYLINES)
                    {
                        Con_Error
                            ("WadMapConverter::findAndCreatePolyobj: psIndex > MAXPOLYLINES\n");
                    }

                    // Clear out any special.
                    line->xType = 0;
                    line->xArgs[0] = 0;
                }
            }
        }

        if(psIndex == psIndexOld)
        {   // Check if an explicit line order has been skipped
            // A line has been skipped if there are any more explicit
            // lines with the current tag value
            for(i = 0; i < map->numLines; ++i)
            {
                mline_t         *line = &map->lines[i];

                if(line->xType == PO_LINE_EXPLICIT &&
                   line->xArgs[0] == tag)
                {
                    Con_Error
                        ("WadMapConverter::findAndCreatePolyobj: Missing explicit line %d for poly %d\n",
                         j, tag);
                }
            }
        }
    }

    if(lineCount)
    {
        uint            poIdx;
        int             seqType = polyLineList[0]->xArgs[3];

        if(createPolyobj(polyLineList, lineCount, &poIdx, tag,
                         seqType, anchorX, anchorY))
        {
            mline_t        *line = polyLineList[0];

            // Next, change the polyobjs first line to point to a mirror
            // if it exists.
            line->xArgs[1] = line->xArgs[2];

            return true;
        }
    }
    }

    return false;

#undef MAXPOLYLINES
}

static void findPolyobjs(void)
{
    uint                i;

    VERBOSE(Con_Message("WadMapConverter::findPolyobjs: Processing...\n"));

    for(i = 0; i < map->numThings; ++i)
    {
        mthing_t           *thing = &map->things[i];

        if(thing->doomEdNum == PO_ANCHOR_DOOMEDNUM)
        {   // A polyobj anchor.
            int                 tag = thing->angle;

            findAndCreatePolyobj(tag, thing->pos[VX], thing->pos[VY]);
        }
    }
}

void AnalyzeMap(void)
{
    if(map->format == MF_HEXEN)
        findPolyobjs();
}

boolean IsSupportedFormat(const int *lumpList, int numLumps)
{
    int                 i;
    boolean             supported = false;

    // Lets first check for format specific lumps, as their prescense
    // determines the format of the map data. Assume DOOM format by default.
    map->format = MF_DOOM;
    for(i = 0; i < numLumps; ++i)
    {
        const char*         lumpName = W_LumpName(lumpList[i]);

        if(!lumpName || !lumpName[0])
            continue;

        if(!strncmp(lumpName, "BEHAVIOR", 8))
        {
            map->format = MF_HEXEN;
            break;
        }

        if(!strncmp(lumpName, "MACROS", 6) ||
           !strncmp(lumpName, "LIGHTS", 6) ||
           !strncmp(lumpName, "LEAFS", 5))
        {
            map->format = MF_DOOM64;
            break;
        }
    }

    for(i = 0; i < numLumps; ++i)
    {
        uint*               ptr;
        size_t              elmSize = 0; // Num of bytes.
        const char*         lumpName = W_LumpName(lumpList[i]);

        // Determine the number of map data objects of each data type.
        ptr = NULL;
        switch(DataTypeForLumpName(lumpName))
        {
        case ML_VERTEXES:
            ptr = &map->numVertexes;
            elmSize = (map->format == MF_DOOM64? SIZEOF_64VERTEX :
                SIZEOF_VERTEX);
            break;

        case ML_THINGS:
            ptr = &map->numThings;
            elmSize = (map->format == MF_DOOM64? SIZEOF_64THING :
                map->format == MF_HEXEN? SIZEOF_XTHING : SIZEOF_THING);
            break;

        case ML_LINEDEFS:
            ptr = &map->numLines;
            elmSize = (map->format == MF_DOOM64? SIZEOF_64LINEDEF :
                map->format == MF_HEXEN? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
            break;

        case ML_SIDEDEFS:
            ptr = &map->numSides;
            elmSize = (map->format == MF_DOOM64? SIZEOF_64SIDEDEF :
                SIZEOF_SIDEDEF);
            break;

        case ML_SECTORS:
            ptr = &map->numSectors;
            elmSize =
                (map->format == MF_DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR);
            break;

        case ML_LIGHTS:
            ptr = &map->numLights;
            elmSize = SIZEOF_LIGHT;
            break;

        default:
            break;
        }

        if(ptr)
        {
            size_t              lumpLength = W_LumpLength(lumpList[i]);

            if(0 != lumpLength % elmSize)
                return false; // What is this??

            *ptr += lumpLength / elmSize;
        }
    }

    if(map->numVertexes > 0 && map->numLines > 0 && map->numSides > 0 &&
       map->numSectors > 0)
    {
        supported = true;
    }

    return supported;
}

static void freeMapData(void)
{
    if(map->vertexes)
        free(map->vertexes);
    map->vertexes = NULL;

    if(map->lines)
        free(map->lines);
    map->lines = NULL;

    if(map->sides)
        free(map->sides);
    map->sides = NULL;

    if(map->sectors)
        free(map->sectors);
    map->sectors = NULL;

    if(map->things)
        free(map->things);
    map->things = NULL;

    if(map->polyobjs)
    {
        uint                i;
        for(i = 0; i < map->numPolyobjs; ++i)
        {
            mpolyobj_t*         po = map->polyobjs[i];
            free(po->lineIndices);
            free(po);
        }
        free(map->polyobjs);
    }
    map->polyobjs = NULL;

    if(map->lights)
        free(map->lights);
    map->lights = NULL;

    /*if(map->macros)
        free(map->macros);
    map->macros = NULL;*/

    if(map->textures)
    {
        size_t              i;
        for(i = 0; i < map->numTextures; ++i)
        {
            materialref_t*      m = map->textures[i];
            free(m);
        }
        free(map->textures);
    }
    map->textures = NULL;

    if(map->flats)
    {
        size_t              i;
        for(i = 0; i < map->numFlats; ++i)
        {
            materialref_t*      m = map->flats[i];
            free(m);
        }
        free(map->flats);
    }
    map->flats = NULL;
}

static boolean loadVertexes(const byte* buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadVertexes: Processing...\n"));

    elmSize = (map->format == MF_DOOM64? SIZEOF_64VERTEX : SIZEOF_VERTEX);
    num = len / elmSize;
    switch(map->format)
    {
    default:
    case MF_DOOM:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            map->vertexes[n * 2] = (float) SHORT(*((const int16_t*) (ptr)));
            map->vertexes[n * 2 + 1] = (float) SHORT(*((const int16_t*) (ptr+2)));
        }
        break;

    case MF_DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            map->vertexes[n * 2] = FIX2FLT(LONG(*((const int32_t*) (ptr))));
            map->vertexes[n * 2 + 1] = FIX2FLT(LONG(*((const int32_t*) (ptr+4))));
        }
        break;
    }

    return true;
}

static boolean loadLinedefs(const byte* buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadLinedefs: Processing...\n"));

    elmSize = (map->format == MF_DOOM64? SIZEOF_64LINEDEF :
        map->format == MF_HEXEN? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
    num = len / elmSize;

    switch(map->format)
    {
    default:
    case MF_DOOM:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            mline_t*            l = &map->lines[n];

            idx = USHORT(*((const uint16_t*) (ptr)));
            if(idx == 0xFFFF)
                l->v[0] = 0;
            else
                l->v[0] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+2)));
            if(idx == 0xFFFF)
                l->v[1] = 0;
            else
                l->v[1] = idx + 1;
            l->flags = SHORT(*((const int16_t*) (ptr+4)));
            l->dType = SHORT(*((const int16_t*) (ptr+6)));
            l->dTag = SHORT(*((const int16_t*) (ptr+8)));
            idx = USHORT(*((const uint16_t*) (ptr+10)));
            if(idx == 0xFFFF)
                l->sides[RIGHT] = 0;
            else
                l->sides[RIGHT] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+12)));
            if(idx == 0xFFFF)
                l->sides[LEFT] = 0;
            else
                l->sides[LEFT] = idx + 1;
            l = l;
        }
        break;

    case MF_DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            mline_t*            l = &map->lines[n];

            idx = USHORT(*((const uint16_t*) (ptr)));
            if(idx == 0xFFFF)
                l->v[0] = 0;
            else
                l->v[0] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+2)));
            if(idx == 0xFFFF)
                l->v[1] = 0;
            else
                l->v[1] = idx + 1;
            l->flags = USHORT(*((const uint16_t*) (ptr+4)));
            l->d64drawFlags = *((const byte*) (ptr + 6));
            l->d64texFlags = *((const byte*) (ptr + 7));
            l->d64type = *((const byte*) (ptr + 8));
            l->d64useType = *((const byte*) (ptr + 9));
            l->d64tag = SHORT(*((const int16_t*) (ptr+10)));
            idx = USHORT(*((const uint16_t*) (ptr+12)));
            if(idx == 0xFFFF)
                l->sides[RIGHT] = 0;
            else
                l->sides[RIGHT] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+14)));
            if(idx == 0xFFFF)
                l->sides[LEFT] = 0;
            else
                l->sides[LEFT] = idx + 1;
            l = l;
        }
        break;

    case MF_HEXEN:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            mline_t*            l = &map->lines[n];

            idx = USHORT(*((const uint16_t*) (ptr)));
            if(idx == 0xFFFF)
                l->v[0] = 0;
            else
                l->v[0] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+2)));
            if(idx == 0xFFFF)
                l->v[1] = 0;
            else
                l->v[1] = idx + 1;
            l->flags = SHORT(*((const int16_t*) (ptr+4)));
            l->xType = *((const byte*) (ptr+6));
            l->xArgs[0] = *((const byte*) (ptr+7));
            l->xArgs[1] = *((const byte*) (ptr+8));
            l->xArgs[2] = *((const byte*) (ptr+9));
            l->xArgs[3] = *((const byte*) (ptr+10));
            l->xArgs[4] = *((const byte*) (ptr+11));
            idx = USHORT(*((const uint16_t*) (ptr+12)));
            if(idx == 0xFFFF)
                l->sides[RIGHT] = 0;
            else
                l->sides[RIGHT] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+14)));
            if(idx == 0xFFFF)
                l->sides[LEFT] = 0;
            else
                l->sides[LEFT] = idx + 1;
        }
        break;
    }

    return true;
}

static boolean loadSidedefs(const byte* buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadSidedefs: Processing...\n"));

    elmSize = (map->format == MF_DOOM64? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF);
    num = len / elmSize;

    switch(map->format)
    {
    default:
    case MF_DOOM:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            char                name[9];
            mside_t*            s = &map->sides[n];

            s->offset[VX] = SHORT(*((const int16_t*) (ptr)));
            s->offset[VY] = SHORT(*((const int16_t*) (ptr+2)));
            memcpy(name, ptr+4, 8);
            name[8] = '\0';
            s->topMaterial = RegisterMaterial(name, false);
            memcpy(name, ptr+12, 8);
            name[8] = '\0';
            s->bottomMaterial = RegisterMaterial(name, false);
            memcpy(name, ptr+20, 8);
            name[8] = '\0';
            s->middleMaterial = RegisterMaterial(name, false);
            idx = USHORT(*((const uint16_t*) (ptr+28)));
            if(idx == 0xFFFF)
                s->sector = 0;
            else
                s->sector = idx + 1;
        }
        break;

    case MF_DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            mside_t*            s = &map->sides[n];

            s->offset[VX] = SHORT(*((const int16_t*) (ptr)));
            s->offset[VY] = SHORT(*((const int16_t*) (ptr+2)));
            idx = USHORT(*((const uint16_t*) (ptr+4)));
            s->topMaterial = RegisterMaterial((const char*) &idx, false);
            idx = USHORT(*((const uint16_t*) (ptr+6)));
            s->bottomMaterial = RegisterMaterial((const char*) &idx, false);
            idx = USHORT(*((const uint16_t*) (ptr+8)));
            s->middleMaterial = RegisterMaterial((const char*) &idx, false);
            idx = USHORT(*((const uint16_t*) (ptr+10)));
            if(idx == 0xFFFF)
                s->sector = 0;
            else
                s->sector = idx + 1;
        }
        break;
    }

    return true;
}

static boolean loadSectors(const byte* buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadSectors: Processing...\n"));

    elmSize = (map->format == MF_DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR);
    num = len / elmSize;

    switch(map->format)
    {
    default:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            char                name[9];
            msector_t*          s = &map->sectors[n];

            s->floorHeight = SHORT(*((const int16_t*) ptr));
            s->ceilHeight = SHORT(*((const int16_t*) (ptr+2)));
            memcpy(name, ptr+4, 8);
            name[8] = '\0';
            s->floorMaterial = RegisterMaterial(name, true);
            memcpy(name, ptr+12, 8);
            name[8] = '\0';
            s->ceilMaterial = RegisterMaterial(name, true);
            s->lightLevel = SHORT(*((const int16_t*) (ptr+20)));
            s->type = SHORT(*((const int16_t*) (ptr+22)));
            s->tag = SHORT(*((const int16_t*) (ptr+24)));
        }
        break;

    case MF_DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int                 idx;
            msector_t*          s = &map->sectors[n];

            s->floorHeight = SHORT(*((const int16_t*) ptr));
            s->ceilHeight = SHORT(*((const int16_t*) (ptr+2)));
            idx = USHORT(*((const uint16_t*) (ptr+4)));
            s->floorMaterial = RegisterMaterial((const char*) &idx, false);
            idx = USHORT(*((const uint16_t*) (ptr+6)));
            s->ceilMaterial = RegisterMaterial((const char*) &idx, false);
            s->d64ceilingColor = USHORT(*((const uint16_t*) (ptr+8)));
            s->d64floorColor = USHORT(*((const uint16_t*) (ptr+10)));
            s->d64unknownColor = USHORT(*((const uint16_t*) (ptr+12)));
            s->d64wallTopColor = USHORT(*((const uint16_t*) (ptr+14)));
            s->d64wallBottomColor = USHORT(*((const uint16_t*) (ptr+16)));
            s->type = SHORT(*((const int16_t*) (ptr+18)));
            s->tag = SHORT(*((const int16_t*) (ptr+20)));
            s->d64flags = USHORT(*((const uint16_t*) (ptr+22)));
            s->lightLevel = 160;
        }
        break;
    }

    return true;
}

static boolean loadThings(const byte* buf, size_t len)
{
// New flags: \todo get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

#define ANG45               0x20000000
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadThings: Processing...\n"));

    elmSize = (map->format == MF_DOOM64? SIZEOF_64THING :
        map->format == MF_HEXEN? SIZEOF_XTHING : SIZEOF_THING);
    num = len / elmSize;

    switch(map->format)
    {
    default:
    case MF_DOOM:
/**
 * DOOM Thing flags:
 */
#define MTF_EASY            0x00000001 // Can be spawned in Easy skill modes.
#define MTF_MEDIUM          0x00000002 // Can be spawned in Medium skill modes.
#define MTF_HARD            0x00000004 // Can be spawned in Hard skill modes.
#define MTF_DEAF            0x00000008 // Mobj will be deaf spawned deaf.
#define MTF_NOTSINGLE       0x00000010 // (BOOM) Can not be spawned in single player gamemodes.
#define MTF_NOTDM           0x00000020 // (BOOM) Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x00000040 // (BOOM) Can not be spawned in the Co-op gameMode.
#define MTF_FRIENDLY        0x00000080 // (BOOM) friendly monster.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_NOTDM|MTF_NOTCOOP|MTF_FRIENDLY))

        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            mthing_t*           t = &map->things[n];

            t->pos[VX] = SHORT(*((const int16_t*) (ptr)));
            t->pos[VY] = SHORT(*((const int16_t*) (ptr+2)));
            t->pos[VZ] = 0;
            t->angle = ANG45 * (SHORT(*((const int16_t*) (ptr+4))) / 45);
            t->doomEdNum = SHORT(*((const int16_t*) (ptr+6)));
            t->flags = SHORT(*((const int16_t*) (ptr+8)));
            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM format things spawn on the floor by default unless their
            // type-specific flags override.
            t->flags |= MTF_Z_FLOOR;
        }

#undef MTF_EASY
#undef MTF_MEDIUM
#undef MTF_HARD
#undef MTF_AMBUSH
#undef MTF_NOTSINGLE
#undef MTF_NOTDM
#undef MTF_NOTCOOP
#undef MTF_FRIENDLY
#undef MASK_UNKNOWN_THING_FLAGS
        break;

    case MF_DOOM64:
/**
 * DOOM64 Thing flags:
 */
#define MTF_EASY            0x00000001 // Appears in easy skill modes.
#define MTF_MEDIUM          0x00000002 // Appears in medium skill modes.
#define MTF_HARD            0x00000004 // Appears in hard skill modes.
#define MTF_DEAF            0x00000008 // Thing is deaf.
#define MTF_NOTSINGLE       0x00000010 // Appears in multiplayer game modes only.
#define MTF_DONTSPAWNATSTART 0x00000020 // Do not spawn this thing at map start.
#define MTF_SCRIPT_TOUCH    0x00000040 // Mobjs spawned from this spot will envoke a script when touched.
#define MTF_SCRIPT_DEATH    0x00000080 // Mobjs spawned from this spot will envoke a script on death.
#define MTF_SECRET          0x00000100 // A secret (bonus) item.
#define MTF_NOTARGET        0x00000200 // Mobjs spawned from this spot will not target their attacker when hurt.
#define MTF_NOTDM           0x00000400 // Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x00000800 // Can not be spawned in the Co-op gameMode.

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_DONTSPAWNATSTART|MTF_SCRIPT_TOUCH|MTF_SCRIPT_DEATH|MTF_SECRET|MTF_NOTARGET|MTF_NOTDM|MTF_NOTCOOP))

        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            mthing_t*           t = &map->things[n];

            t->pos[VX] = SHORT(*((const int16_t*) (ptr)));
            t->pos[VY] = SHORT(*((const int16_t*) (ptr+2)));
            t->pos[VZ] = SHORT(*((const int16_t*) (ptr+4)));
            t->angle = ANG45 * (SHORT(*((const int16_t*) (ptr+6))) / 45);
            t->doomEdNum = SHORT(*((const int16_t*) (ptr+8)));

            t->flags = SHORT(*((const int16_t*) (ptr+10)));
            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // DOOM64 format things spawn relative to the floor by default
            // unless their type-specific flags override.
            t->flags |= MTF_Z_FLOOR;

            t->d64TID = SHORT(*((const int16_t*) (ptr+12)));
        }

#undef MTF_EASY
#undef MTF_MEDIUM
#undef MTF_HARD
#undef MTF_DEAF
#undef MTF_NOTSINGLE
#undef MTF_DONTSPAWNATSTART
#undef MTF_SCRIPT_TOUCH
#undef MTF_SCRIPT_DEATH
#undef MTF_SECRET
#undef MTF_NOTARGET
#undef MTF_NOTDM
#undef MTF_NOTCOOP
#undef MASK_UNKNOWN_THING_FLAGS
        break;

    case MF_HEXEN:
/**
 * Hexen Thing flags:
 */
#define MTF_EASY            0x00000001
#define MTF_NORMAL          0x00000002
#define MTF_HARD            0x00000004
#define MTF_AMBUSH          0x00000008
#define MTF_DORMANT         0x00000010
#define MTF_FIGHTER         0x00000020
#define MTF_CLERIC          0x00000040
#define MTF_MAGE            0x00000080
#define MTF_GSINGLE         0x00000100
#define MTF_GCOOP           0x00000200
#define MTF_GDEATHMATCH     0x00000400
// The following are not currently used.
#define MTF_SHADOW          0x00000800 // (ZDOOM) Thing is 25% translucent.
#define MTF_INVISIBLE       0x00001000 // (ZDOOM) Makes the thing invisible.
#define MTF_FRIENDLY        0x00002000 // (ZDOOM) Friendly monster.
#define MTF_STILL           0x00004000 // (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).

#define MASK_UNKNOWN_THING_FLAGS (0xffffffff \
    ^ (MTF_EASY|MTF_NORMAL|MTF_HARD|MTF_AMBUSH|MTF_DORMANT|MTF_FIGHTER|MTF_CLERIC|MTF_MAGE|MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH|MTF_SHADOW|MTF_INVISIBLE|MTF_FRIENDLY|MTF_STILL))

        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            mthing_t*           t = &map->things[n];

            t->xTID = SHORT(*((const int16_t*) (ptr)));
            t->pos[VX] = SHORT(*((const int16_t*) (ptr+2)));
            t->pos[VY] = SHORT(*((const int16_t*) (ptr+4)));
            t->pos[VZ] = SHORT(*((const int16_t*) (ptr+6)));
            t->angle = SHORT(*((const int16_t*) (ptr+8)));
            t->doomEdNum = SHORT(*((const int16_t*) (ptr+10)));
            /**
             * For some reason, the Hexen format stores polyobject tags in
             * the angle field in THINGS. Thus, we cannot translate the
             * angle until we know whether it is a polyobject type or not.
             */
            if(t->doomEdNum != PO_ANCHOR_DOOMEDNUM &&
               t->doomEdNum != PO_SPAWN_DOOMEDNUM &&
               t->doomEdNum != PO_SPAWNCRUSH_DOOMEDNUM)
                t->angle = ANG45 * (t->angle / 45);
            t->flags = SHORT(*((const int16_t*) (ptr+12)));
            t->flags &= ~MASK_UNKNOWN_THING_FLAGS;

            /**
             * Translate flags:
             */
            // Game type logic is inverted.
            t->flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);

            // HEXEN format things spawn relative to the floor by default
            // unless their type-specific flags override.
            t->flags |= MTF_Z_FLOOR;

            t->xSpecial = *(ptr+14);
            t->xArgs[0] = *(ptr+15);
            t->xArgs[1] = *(ptr+16);
            t->xArgs[2] = *(ptr+17);
            t->xArgs[3] = *(ptr+18);
            t->xArgs[4] = *(ptr+19);
        }

#undef MTF_EASY
#undef MTF_NORMAL
#undef MTF_HARD
#undef MTF_AMBUSH
#undef MTF_DORMANT
#undef MTF_FIGHTER
#undef MTF_CLERIC
#undef MTF_MAGE
#undef MTF_GSINGLE
#undef MTF_GCOOP
#undef MTF_GDEATHMATCH
// The following are not currently used.
#undef MTF_SHADOW
#undef MTF_INVISIBLE
#undef MTF_FRIENDLY
#undef MTF_STILL
#undef MASK_UNKNOWN_THING_FLAGS
        break;
    }

    return true;

#undef MTF_Z_FLOOR
#undef MTF_Z_CEIL
#undef MTF_Z_RANDOM
}

static boolean loadLights(const byte* buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte*         ptr;

    VERBOSE(Con_Message("WadMapConverter::loadLights: Processing...\n"));

    elmSize = SIZEOF_LIGHT;
    num = len / elmSize;
    for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
    {
        surfacetint_t*           t = &map->lights[n];

        t->rgb[0] = (float) *(ptr) / 255;
        t->rgb[1] = (float) *(ptr+1) / 255;
        t->rgb[2] = (float) *(ptr+2) / 255;
        t->xx[0] = *(ptr+3);
        t->xx[1] = *(ptr+4);
        t->xx[2] = *(ptr+5);
    }

    return true;
}

static void bufferLump(int lumpNum, byte** buf, size_t* len, size_t* oldLen)
{
    *len = W_LumpLength(lumpNum);

    // Need to enlarge our buffer?
    if(*len > *oldLen)
    {
        *buf = realloc(*buf, *len);
        *oldLen = *len;
    }

    // Buffer the entire lump.
    W_ReadLump(lumpNum, *buf);
}

boolean LoadMap(const int* lumpList, int numLumps)
{
    int                 i;
    byte*               buf = NULL;
    size_t              oldLen = 0;

    // Allocate the data structure arrays.
    map->vertexes = malloc(map->numVertexes * 2 * sizeof(float));
    map->lines = malloc(map->numLines * sizeof(mline_t));
    map->sides = malloc(map->numSides * sizeof(mside_t));
    map->sectors = malloc(map->numSectors * sizeof(msector_t));
    map->things = malloc(map->numThings * sizeof(mthing_t));
    if(map->numLights)
        map->lights = malloc(map->numLights * sizeof(surfacetint_t));

    for(i = 0; i < numLumps; ++i)
    {
        size_t              len;
        lumptype_t          lumpType;

        lumpType = DataTypeForLumpName(W_LumpName(lumpList[i]));

        // Process it, transforming it into our local representation.
        switch(lumpType)
        {
        case ML_VERTEXES:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadVertexes(buf, len);
            break;

        case ML_LINEDEFS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadLinedefs(buf, len);
            break;

        case ML_SIDEDEFS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadSidedefs(buf, len);
            break;

        case ML_SECTORS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadSectors(buf, len);
            break;

        case ML_THINGS:
            if(map->numThings)
            {
                bufferLump(lumpList[i], &buf, &len, &oldLen);
                loadThings(buf, len);
            }
            break;

        case ML_LIGHTS:
            if(map->numLights)
            {
                bufferLump(lumpList[i], &buf, &len, &oldLen);
                loadLights(buf, len);
            }
            break;

        case ML_MACROS:
            //// \todo Write me!
            break;

        default:
            break;
        }
    }

    if(buf)
        free(buf);

    return true; // Read and converted successfully.
}

boolean TransferMap(void)
{
    uint                startTime = Sys_GetRealTime();

    uint                i;
    boolean             result;

    // Announce any bad material names we came across while loading the map.
    LogUnknownMaterials();

    VERBOSE(Con_Message("WadMapConverter::TransferMap...\n"));

    MPE_Begin(map->name);

    // Create all the data structures.
    VERBOSE(Con_Message("WadMapConverter::Transfering vertexes...\n"));
    MPE_CreateVertices(map->numVertexes, map->vertexes, NULL);

    VERBOSE(Con_Message("WadMapConverter::Transfering sectors...\n"));
    for(i = 0; i < map->numSectors; ++i)
    {
        msector_t* sec = &map->sectors[i];
        uint sectorIDX, floorIDX, ceilIDX;

        sectorIDX = MPE_CreateSector((float) sec->lightLevel / 255.0f, 1, 1, 1);

        floorIDX = MPE_CreatePlane(sec->floorHeight,
                                   sec->floorMaterial? sec->floorMaterial->material : 0,
                                   0, 0, 1, 1, 1, 1, 0, 0, 1);
        ceilIDX = MPE_CreatePlane(sec->ceilHeight,
                                  sec->ceilMaterial? sec->ceilMaterial->material : 0,
                                  0, 0, 1, 1, 1, 1, 0, 0, -1);

        MPE_SetSectorPlane(sectorIDX, 0, floorIDX);
        MPE_SetSectorPlane(sectorIDX, 1, ceilIDX);

        MPE_GameObjectRecordProperty("XSector", i, "ID", DDVT_INT, &i);
        MPE_GameObjectRecordProperty("XSector", i, "Tag", DDVT_SHORT, &sec->tag);
        MPE_GameObjectRecordProperty("XSector", i, "Type", DDVT_SHORT, &sec->type);

        if(map->format == MF_DOOM64)
        {
            MPE_GameObjectRecordProperty("XSector", i, "Flags", DDVT_SHORT, &sec->d64flags);
            MPE_GameObjectRecordProperty("XSector", i, "CeilingColor", DDVT_SHORT, &sec->d64ceilingColor);
            MPE_GameObjectRecordProperty("XSector", i, "FloorColor", DDVT_SHORT, &sec->d64floorColor);
            MPE_GameObjectRecordProperty("XSector", i, "UnknownColor", DDVT_SHORT, &sec->d64unknownColor);
            MPE_GameObjectRecordProperty("XSector", i, "WallTopColor", DDVT_SHORT, &sec->d64wallTopColor);
            MPE_GameObjectRecordProperty("XSector", i, "WallBottomColor", DDVT_SHORT, &sec->d64wallBottomColor);
        }
    }

    VERBOSE(Con_Message("WadMapConverter::Transfering linedefs...\n"));
    for(i = 0; i < map->numLines; ++i)
    {
        mline_t*            l = &map->lines[i];
        mside_t*            front, *back;
        uint                frontIdx = 0, backIdx = 0;

        front = (l->sides[RIGHT] != 0? &map->sides[l->sides[RIGHT]-1] : NULL);
        if(front)
        {
            frontIdx =
                MPE_CreateSideDef(front->sector,
                                  (map->format == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  front->topMaterial? front->topMaterial->material : 0,
                                  front->offset[VX], front->offset[VY], 1, 1, 1,
                                  front->middleMaterial? front->middleMaterial->material : 0,
                                  front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                  front->bottomMaterial? front->bottomMaterial->material : 0,
                                  front->offset[VX], front->offset[VY], 1, 1, 1);
        }

        back = (l->sides[LEFT] != 0? &map->sides[l->sides[LEFT]-1] : NULL);
        if(back)
        {
            backIdx =
                MPE_CreateSideDef(back->sector,
                                  (map->format == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  back->topMaterial? back->topMaterial->material : 0,
                                  back->offset[VX], back->offset[VY], 1, 1, 1,
                                  back->middleMaterial? back->middleMaterial->material : 0,
                                  back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                  back->bottomMaterial? back->bottomMaterial->material : 0,
                                  back->offset[VX], back->offset[VY], 1, 1, 1);
        }

        MPE_CreateLineDef(l->v[0], l->v[1], frontIdx, backIdx, 0);

        MPE_GameObjectRecordProperty("XLinedef", i, "ID", DDVT_INT, &i);
        MPE_GameObjectRecordProperty("XLinedef", i, "Flags", DDVT_SHORT, &l->flags);

        switch(map->format)
        {
        default:
        case MF_DOOM:
            MPE_GameObjectRecordProperty("XLinedef", i, "Type", DDVT_SHORT, &l->dType);
            MPE_GameObjectRecordProperty("XLinedef", i, "Tag", DDVT_SHORT, &l->dTag);
            break;

        case MF_DOOM64:
            MPE_GameObjectRecordProperty("XLinedef", i, "DrawFlags", DDVT_BYTE, &l->d64drawFlags);
            MPE_GameObjectRecordProperty("XLinedef", i, "TexFlags", DDVT_BYTE, &l->d64texFlags);
            MPE_GameObjectRecordProperty("XLinedef", i, "Type", DDVT_BYTE, &l->d64type);
            MPE_GameObjectRecordProperty("XLinedef", i, "UseType", DDVT_BYTE, &l->d64useType);
            MPE_GameObjectRecordProperty("XLinedef", i, "Tag", DDVT_SHORT, &l->d64tag);
            break;

        case MF_HEXEN:
            MPE_GameObjectRecordProperty("XLinedef", i, "Type", DDVT_BYTE, &l->xType);
            MPE_GameObjectRecordProperty("XLinedef", i, "Arg0", DDVT_BYTE, &l->xArgs[0]);
            MPE_GameObjectRecordProperty("XLinedef", i, "Arg1", DDVT_BYTE, &l->xArgs[1]);
            MPE_GameObjectRecordProperty("XLinedef", i, "Arg2", DDVT_BYTE, &l->xArgs[2]);
            MPE_GameObjectRecordProperty("XLinedef", i, "Arg3", DDVT_BYTE, &l->xArgs[3]);
            MPE_GameObjectRecordProperty("XLinedef", i, "Arg4", DDVT_BYTE, &l->xArgs[4]);
            break;
        }
    }

    VERBOSE(Con_Message("WadMapConverter::Transfering lights...\n"));
    for(i = 0; i < map->numLights; ++i)
    {
        surfacetint_t*           l = &map->lights[i];

        MPE_GameObjectRecordProperty("Light", i, "ColorR", DDVT_FLOAT, &l->rgb[0]);
        MPE_GameObjectRecordProperty("Light", i, "ColorG", DDVT_FLOAT, &l->rgb[1]);
        MPE_GameObjectRecordProperty("Light", i, "ColorB", DDVT_FLOAT, &l->rgb[2]);
        MPE_GameObjectRecordProperty("Light", i, "XX0", DDVT_BYTE, &l->xx[0]);
        MPE_GameObjectRecordProperty("Light", i, "XX1", DDVT_BYTE, &l->xx[1]);
        MPE_GameObjectRecordProperty("Light", i, "XX2", DDVT_BYTE, &l->xx[2]);
    }

    VERBOSE(Con_Message("WadMapConverter::Transfering polyobjs...\n"));
    for(i = 0; i < map->numPolyobjs; ++i)
    {
        mpolyobj_t*         po = map->polyobjs[i];
        uint                j, *lineList;

        lineList = malloc(sizeof(uint) * po->lineCount);
        for(j = 0; j < po->lineCount; ++j)
            lineList[j] = po->lineIndices[j] + 1;
        MPE_CreatePolyobj(lineList, po->lineCount, po->tag,
                          po->seqType, (float) po->anchor[VX],
                          (float) po->anchor[VY]);
        free(lineList);
    }

    VERBOSE(Con_Message("WadMapConverter::Transfering things...\n"));
    for(i = 0; i < map->numThings; ++i)
    {
        mthing_t*           th = &map->things[i];

        MPE_GameObjectRecordProperty("Thing", i, "X", DDVT_SHORT, &th->pos[VX]);
        MPE_GameObjectRecordProperty("Thing", i, "Y", DDVT_SHORT, &th->pos[VY]);
        MPE_GameObjectRecordProperty("Thing", i, "Z", DDVT_SHORT, &th->pos[VZ]);
        MPE_GameObjectRecordProperty("Thing", i, "Angle", DDVT_ANGLE, &th->angle);
        MPE_GameObjectRecordProperty("Thing", i, "DoomEdNum", DDVT_SHORT, &th->doomEdNum);
        MPE_GameObjectRecordProperty("Thing", i, "Flags", DDVT_INT, &th->flags);

        if(map->format == MF_DOOM64)
        {
            MPE_GameObjectRecordProperty("Thing", i, "ID", DDVT_SHORT, &th->d64TID);
        }
        else if(map->format == MF_HEXEN)
        {

            MPE_GameObjectRecordProperty("Thing", i, "Special", DDVT_BYTE, &th->xSpecial);
            MPE_GameObjectRecordProperty("Thing", i, "ID", DDVT_SHORT, &th->xTID);
            MPE_GameObjectRecordProperty("Thing", i, "Arg0", DDVT_BYTE, &th->xArgs[0]);
            MPE_GameObjectRecordProperty("Thing", i, "Arg1", DDVT_BYTE, &th->xArgs[1]);
            MPE_GameObjectRecordProperty("Thing", i, "Arg2", DDVT_BYTE, &th->xArgs[2]);
            MPE_GameObjectRecordProperty("Thing", i, "Arg3", DDVT_BYTE, &th->xArgs[3]);
            MPE_GameObjectRecordProperty("Thing", i, "Arg4", DDVT_BYTE, &th->xArgs[4]);
        }
    }

    // We've now finished with the original map data.
    freeMapData();

    // Let Doomsday know that we've finished with this map.
    result = MPE_End();

    VERBOSE(
    Con_Message("WadMapConverter::TransferMap: Done in %.2f seconds.\n",
                (Sys_GetRealTime() - startTime) / 1000.0f));

    return result;
}

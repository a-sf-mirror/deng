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

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <assert.h>

using namespace wadconverter;

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

#define SEQTYPE_NUMSEQ          (10)

// TYPES -------------------------------------------------------------------

/*typedef struct usecrecord_s {
    sector_t*           sec;
    double              nearPos[2];
} usecrecord_t;*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

/*bool         registerUnclosedSectorNear(sector_t* sec, double x, double y);
void            printUnclosedSectorList(void);*/

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static uint numUnclosedSectors;
static usecrecord_t* unclosedSectors;*/

// CODE --------------------------------------------------------------------

static int C_DECL compareMaterialNames(const void* a, const void* b)
{
    return stricmp((*(Map::materialref_t**)a)->name, (*(Map::materialref_t**)b)->name);
}

Map::materialref_t* Map::getMaterial(const char* regName, bool isFlat)
{
    materialref_t*** list = isFlat? &_flats : &_textures;
    size_t size = isFlat? _numFlats : _numTextures;
    int result;
    size_t bottomIdx, topIdx, pivot;
    Map::materialref_t* m;
    bool isDone;
    char name[9];

    if(size == 0)
        return NULL;

    if(_formatId == DOOM64)
    {
        int idx = *((int*) regName);
        dd_snprintf(name, 9, "UNK%05i", idx);
    }
    else
    {
        size_t len = MIN_OF(8, strlen(regName));
        strncpy(name, regName, len);
        name[len] = '\0';
    }

    bottomIdx = 0;
    topIdx = size-1;
    m = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        Map::materialref_t* cand;

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

static void addMaterialToList(Map::materialref_t* m, Map::materialref_t*** list, size_t* size)
{
    size_t i;

    // Enlarge the list.
    (*list) = (Map::materialref_t**) realloc((*list), sizeof(m) * ++(*size));

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
static __inline bool isUnknownMaterialRef(const Map::materialref_t* m, bool isFlat)
{
    return m->material == NULL ? true : false;
}

const Map::materialref_t* Map::RegisterMaterial(const char* name, bool isFlat)
{
    materialref_t* m;

    // When loading DOOM or Hexen format maps check for the special case "-"
    // texture name (no texture).
    if(!isFlat && (_formatId == Map::DOOM || _formatId == Map::HEXEN) &&
       !stricmp(name, "-"))
        return NULL;

    // Check if this material has already been registered.
    if((m = getMaterial(name, isFlat)) != NULL)
    {
        m->refCount++;
        return m;
    }

    /**
     * A new material.
     */
    m = (materialref_t*) malloc(sizeof(*m));
    memcpy(m->name, name, 8);
    m->name[8] = '\0';

    // First try the prefered namespace, then any.
    if(!(m->material = P_MaterialForName((isFlat? MN_FLATS : MN_TEXTURES), m->name)))
        m->material = P_MaterialForName(MN_ANY, m->name);

    // Add it to the material reference list.
    addMaterialToList(m, isFlat? &_flats : &_textures,
                      isFlat? &_numFlats : &_numTextures);
    m->refCount = 1;

    if(isUnknownMaterialRef(m, isFlat))
    {
        if(isFlat)
            ++_numUnknownFlats;
        else
            ++_numUnknownTextures;
    }

    return m;
}

const Map::materialref_t* Map::RegisterMaterial(int idx, bool isFlat)
{
    materialref_t* m;

    // Check if this material has already been registered.
    if((m = getMaterial((const char*) idx, isFlat)) != NULL)
    {
        m->refCount++;
        return m;
    }
    else
    {
        /**
         * A new material.
         */
        m = (materialref_t*) malloc(sizeof(*m));

        dd_snprintf(m->name, 9, "UNK%05i", idx);

        // First try the prefered namespace, then any.
        if(!(m->material = R_MaterialForTextureId((isFlat? MN_FLATS : MN_TEXTURES), idx)))
            m->material = R_MaterialForTextureId(MN_ANY, idx);

        // Add it to the material reference list.
        addMaterialToList(m, isFlat? &_flats : &_textures,
                          isFlat? &_numFlats : &_numTextures);
        m->refCount = 1;

        if(isUnknownMaterialRef(m, isFlat))
        {
            if(isFlat)
                ++_numUnknownFlats;
            else
                ++_numUnknownTextures;
        }

        return m;
    }
}

void Map::logUnknownMaterials(void)
{
    static const char* nameStr[] = { "name", "names" };

    if(_numUnknownFlats)
    {
        size_t i;

        Con_Message("WadMapConverter: Warning: Found %u bad flat %s:\n",
                    _numUnknownFlats,
                    nameStr[_numUnknownFlats? 1 : 0]);

        for(i = 0; i < _numFlats; ++i)
        {
            Map::materialref_t* m = _flats[i];

            if(isUnknownMaterialRef(m, true))
                Con_Message(" %4u x \"%s\"\n", m->refCount, m->name);
        }
    }

    if(_numUnknownTextures)
    {
        size_t i;

        Con_Message("WadMapConverter: Warning: Found %u bad texture %s:\n",
                    _numUnknownTextures,
                    nameStr[_numUnknownTextures? 1 : 0]);

        for(i = 0; i < _numTextures; ++i)
        {
            Map::materialref_t* m = _textures[i];

            if(isUnknownMaterialRef(m, false))
                Con_Message(" %4u x \"%s\"\n", m->refCount, m->name);
        }
    }
}

#if 0
/**
 * Register the specified sector in the list of unclosed _sectors.
 *
 * @param sec           Ptr to the sector to be registered.
 * @param x             Approximate X coordinate to the sector's origin.
 * @param y             Approximate Y coordinate to the sector's origin.
 *
 * @return              @c true, if sector was registered.
 */
static bool registerUnclosedSectorNear(sector_t* sec, double x, double y)
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
    usec->nearPos[0] = x;
    usec->nearPos[1] = y;

    return true;
}

/**
 * Print the list of unclosed _sectors.
 */
static void printUnclosedSectorList(void)
{
    uint i;

    if(!editMapInited)
        return;

    if(numUnclosedSectors)
    {
        Con_Printf("Warning, found %u unclosed _sectors:\n", numUnclosedSectors);

        for(i = 0; i < numUnclosedSectors; ++i)
        {
            usecrecord_t* usec = &unclosedSectors[i];

            Con_Printf("  #%d near [%1.1f, %1.1f]\n", usec->sec->buildData.index - 1,
                       usec->nearPos[0], usec->nearPos[1]);
        }
    }
}

/**
 * Free the list of unclosed _sectors.
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
static bool loadBlockmap(tempmap_t* map, maplumpinfo_t* maplump)
{
#define MAPBLOCKUNITS       128

    bool generateBMap = (createBMap == 2)? true : false;

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

        v[0] = (float) SHORT(blockmapLump[0]);
        v[1] = (float) SHORT(blockmapLump[1]);
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

        V2_Set(bounds[0], v[0], v[1]);
        v[0] += (float) (width * MAPBLOCKUNITS);
        v[1] += (float) (height * MAPBLOCKUNITS);
        V2_Set(bounds[1], v[0], v[1]);

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

                // Count the number of _lineDefs in this block.
                count = 0;
                while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    count++;

                if(count > 0)
                {
                    linedef_t** _lineDefs, **ptr;

                    // A NULL-terminated array of pointers to _lineDefs.
                    _lineDefs = Z_Malloc((count + 1) * sizeof(linedef_t *), PU_STATIC, NULL);

                    // Copy pointers to the array, delete the nodes.
                    ptr = _lineDefs;
                    count = 0;
                    while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    {
#if _DEBUG
if(idx < 0 || idx >= (long) map->_numLineDefs)
{
    Con_Error("loadBlockMap: Invalid linedef id %li\n!", idx);
}
#endif
                        *ptr++ = &map->_lineDefs[idx];
                        count++;
                    }
                    // Terminate.
                    *ptr = NULL;

                    // Link it into the BlockMap.
                    P_BlockmapSetBlock(blockmap, x, y, _lineDefs, NULL);
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
 * line-of-sight tests between _sectors. This is done with a matrix of sector
 * pairs i.e. if a monster in sector 4 can see the player in sector 2; the
 * inverse should be true.
 *
 * Note however, some PWADS have carefully constructed REJECT data to create
 * special effects. For example it is possible to make a player completely
 * invissible in certain _sectors.
 *
 * The format of the table is a simple matrix of bool values, a (true)
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
 *     ceiling(_numSectors^2)
 *
 * For now we only do very basic reject processing, limited to determining
 * all isolated sector groups (islands that are surrounded by void space).
 *
 * \note Algorithm:
 * Initially all _sectors are in individual groups. Next, we scan the linedef
 * list. For each 2-sectored line, merge the two sector groups into one.
 */
static void buildReject(gamemap_t* map)
{
/**
 * \todo We can do something much better now that we are building the BSP.
 */
    int i;
    int group;
    int* secGroups;
    int view, target;
    size_t rejectSize;
    byte* matrix;

    secGroups = M_Malloc(sizeof(int) * _numSectors);
    for(i = 0; i < _numSectors; ++i)
    {
        sector_t* sec = LookupSector(i);
        secGroups[i] = group++;
        sec->rejNext = sec->rejPrev = sec;
    }

    for(i = 0; i < numLinedefs; ++i)
    {
        linedef_t* line = LookupLinedef(i);
        sector_t* sec1, *sec2, *p;

        if(!line->sideDefs[FRONT] || !line->sideDefs[BACK])
            continue;

        sec1 = line->sideDefs[FRONT]->sector;
        sec2 = line->sideDefs[BACK]->sector;

        if(!sec1 || !sec2 || sec1 == sec2)
            continue;

        // Already in the same group?
        if(secGroups[sec1->index] == secGroups[sec2->index])
            continue;

        // Swap _sectors so that the smallest group is added to the biggest
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

    rejectSize = (_numSectors * _numSectors + 7) / 8;
    matrix = Z_Calloc(rejectSize, PU_STATIC, 0);

    for(view = 0; view < _numSectors; ++view)
        for(target = 0; target < view; ++target)
        {
            int p1, p2;

            if(secGroups[view] == secGroups[target])
                continue;

            // For symmetry, do two bits at a time.
            p1 = view * _numSectors + target;
            p2 = target * _numSectors + view;

            matrix[p1 >> 3] |= (1 << (p1 & 7));
            matrix[p2 >> 3] |= (1 << (p2 & 7));
        }

    M_Free(secGroups);
}
#endif

Map::lumptype_t Map::dataTypeForLumpName(const char* name)
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
        {ML_INVALID,    NULL}
    };
    size_t i;

    if(name && name[0])
    {
        for(i = 0; knownLumps[i].name; ++i)
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
int Map::isClosedPolygon(LineDef** lineList, size_t num)
{
    uint i;

    for(i = 0; i < num; ++i)
    {
        LineDef* line = lineList[i];
        LineDef* next = (i == num-1? lineList[0] : lineList[i+1]);

        if(!(line->v[1] == next->v[0]))
             return false;
    }

    // The polygon is closed.
    return true;
}

/**
 * Create a temporary polyobj (read from the original map data).
 */
void Map::createPolyobj(LineDef** lineDefs, uint lineDefCount, int tag,
    int sequenceType, int16_t anchorX, int16_t anchorY)
{
    assert(lineDefs);
    assert(lineDefCount > 0);

    // Ensure that lineList is a closed polygon.
    if(isClosedPolygon(lineDefs, lineDefCount) == 0)
    {   // Nope, perhaps it needs sorting?
        throw std::runtime_error("LineDefs do not form a closed polygon.");
    }

    uint* lineIndices = (uint*) malloc(sizeof(objectrecordid_t) * lineDefCount);
    for(uint i = 0; i < lineDefCount; ++i)
    {
        lineDefs[i]->polyobjOwned = true;

        /// \todo Ugly or what? Revise Polyobj construction interface.
        LineDefs::iterator iter;
        for(iter = _lineDefs.begin(); iter != _lineDefs.end(); ++i)
            if(&(*iter) == lineDefs[i])
            {
                lineIndices[i] = distance(iter, _lineDefs.begin());
                break;
            }
        if(iter == _lineDefs.end())
        {   // How'd that happen??
            free(lineIndices);
            throw std::runtime_error("Failed to find LineDef in global store.");
        }
    }

    _polyobjs.push_back(
        Polyobj::Polyobj(++_numPolyobjs, lineIndices, lineDefCount,
                tag, sequenceType, anchorX, anchorY));
}

/**
 * @param lineList      @c NULL, will cause IterFindPolyLines to count
 *                      the number of _lineDefs in the polyobj.
 */
bool Map::iterFindPolyLines(int16_t x, int16_t y, int16_t polyStart[2],
    uint* polyLineCount, LineDef** lineDefs)
{
    if(x == polyStart[0] && y == polyStart[1])
    {
        return true;
    }

    for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
    {
        int16_t v1[2], v2[2];

        v1[0] = (int16_t) _vertexCoords[(i->v[0] - 1) * 2];
        v1[1] = (int16_t) _vertexCoords[(i->v[0] - 1) * 2 + 1];
        v2[0] = (int16_t) _vertexCoords[(i->v[1] - 1) * 2];
        v2[1] = (int16_t) _vertexCoords[(i->v[1] - 1) * 2 + 1];

        if(v1[0] == x && v1[1] == y)
        {
            if(!lineDefs)
                *polyLineCount++;
            else
                *lineDefs++ = &(*i);

            iterFindPolyLines(v2[0], v2[1], polyStart, polyLineCount, lineDefs);
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
bool Map::findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
{
#define MAXPOLYLINES         32

    for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
    {
        if(i->xType == PO_LINE_START && i->xArgs[0] == tag)
        {
            byte seqType;
            LineDef** lineList;
            int16_t v1[2], v2[2];
            uint polyLineCount;
            int16_t polyStart[2];

            i->xType = 0;
            i->xArgs[0] = 0;
            polyLineCount = 1;

            v1[0] = (int16_t) _vertexCoords[(i->v[0]-1) * 2];
            v1[1] = (int16_t) _vertexCoords[(i->v[0]-1) * 2 + 1];
            v2[0] = (int16_t) _vertexCoords[(i->v[1]-1) * 2];
            v2[1] = (int16_t) _vertexCoords[(i->v[1]-1) * 2 + 1];
            polyStart[0] = v1[0];
            polyStart[1] = v1[1];
            if(!iterFindPolyLines(v2[0], v2[1], polyStart, &polyLineCount, NULL))
            {
                Con_Error("WadMapConverter::findAndCreatePolyobj: Found unclosed polyobj.\n");
            }

            lineList = (LineDef**) malloc((polyLineCount+1) * sizeof(LineDef*));

            lineList[0] = &(*i); // Insert the first line.
            iterFindPolyLines(v2[0], v2[1], polyStart, &polyLineCount, lineList + 1);
            lineList[polyLineCount] = 0; // Terminate.

            seqType = i->xArgs[2];
            if(seqType >= SEQTYPE_NUMSEQ)
                seqType = 0;

            createPolyobj(lineList, polyLineCount, tag, seqType, anchorX, anchorY);
            return true;
        }
    }

    /**
     * Didn't find a polyobj through PO_LINE_START.
     * We'll try another approach...
     */
    {
    LineDef* polyLineList[MAXPOLYLINES];
    uint lineCount = 0;
    uint psIndex, psIndexOld;

    psIndex = 0;
    for(uint j = 1; j < MAXPOLYLINES; ++j)
    {
        psIndexOld = psIndex;
        for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
        {
            if(i->xType == PO_LINE_EXPLICIT &&
               i->xArgs[0] == tag)
            {
                if(!i->xArgs[1])
                {
                    Con_Error
                        ("WadMapConverter::findAndCreatePolyobj: Explicit line missing order number "
                         "(probably %d) in poly %d.\n",
                         j + 1, tag);
                }

                if(i->xArgs[1] == j)
                {
                    // Add this line to the list.
                    polyLineList[psIndex] = &(*i);
                    lineCount++;
                    psIndex++;
                    if(psIndex > MAXPOLYLINES)
                    {
                        Con_Error
                            ("WadMapConverter::findAndCreatePolyobj: psIndex > MAXPOLYLINES\n");
                    }

                    // Clear out any special.
                    i->xType = 0;
                    i->xArgs[0] = 0;
                }
            }
        }

        if(psIndex == psIndexOld)
        {   // Check if an explicit line order has been skipped
            // A line has been skipped if there are any more explicit
            // _lineDefs with the current tag value
            for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
            {
                if(i->xType == PO_LINE_EXPLICIT &&
                   i->xArgs[0] == tag)
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
        LineDef* lineDef = polyLineList[0];
        // Set Polyobj's first LineDef to point to its mirror (if it exists).       
        lineDef->xArgs[1] = lineDef->xArgs[2];

        createPolyobj(polyLineList, lineCount, tag, lineDef->xArgs[3], anchorX, anchorY);
        return true;
    }
    }

    return false;

#undef MAXPOLYLINES
}

void Map::findPolyobjs(void)
{
    for(Things::iterator i = _things.begin(); i != _things.end(); ++i)
    {
        if(i->doomEdNum == Thing::PO_ANCHOR)
        {   // A polyobj anchor.
            findAndCreatePolyobj(i->angle, i->pos[0], i->pos[1]);
        }
    }
}

Map::FormatId Map::recognize(const LumpNums& lumpNums)
{
    FormatId formatId = UNKNOWN;

    // Lets first check for format specific lumps, as their prescense
    // determines the format of the map data.
    for(LumpNums::const_iterator i = lumpNums.begin(); i != lumpNums.end(); ++i)
    {
        const char* lumpName = W_LumpName(*i);

        if(!lumpName || !lumpName[0])
            continue;

        if(!strncmp(lumpName, "BEHAVIOR", 8))
        {
            formatId = HEXEN;
            break;
        }

        if(!strncmp(lumpName, "MACROS", 6) ||
           !strncmp(lumpName, "LIGHTS", 6) ||
           !strncmp(lumpName, "LEAFS", 5))
        {
            formatId = DOOM64;
            break;
        }
    }

    if(formatId == UNKNOWN)
        formatId = DOOM; // Assume DOOM format.

    return formatId;
}

/**
 * \note Map takes ownership of @a lumpNums.
 */
Map::Map(LumpNums& lumpNums)
    : _formatId(UNKNOWN),
      _numVertexes(0),
      _numSectors(0),
      _numLineDefs(0),
      _numSideDefs(0),
      _numPolyobjs(0),
      _numThings(0),
      _numSurfaceTints(0),
      _numFlats(0),
      _numUnknownFlats(0),
      _flats(NULL),
      _numTextures(0),
      _numUnknownTextures(0),
      _textures(NULL)
{
    _lumpNums.resize(lumpNums.size());
    std::copy(lumpNums.begin(), lumpNums.end(), _lumpNums.begin());

    _formatId = recognize(_lumpNums);

    /**
     * Determine the number of map data elements in each lump.
     */
    for(LumpNums::iterator i = _lumpNums.begin(); i != _lumpNums.end(); ++i)
    {
        lumptype_t lumpType = dataTypeForLumpName(W_LumpName(*i));
        size_t elmSize = 0; // Num of bytes.
        uint* ptr = NULL;

        switch(lumpType)
        {
        case ML_VERTEXES:
            ptr = &_numVertexes;
            elmSize = (_formatId == DOOM64? SIZEOF_64VERTEX : SIZEOF_VERTEX);
            break;

        case ML_THINGS:
            ptr = &_numThings;
            elmSize = (_formatId == DOOM64? SIZEOF_64THING :
                       _formatId == HEXEN? SIZEOF_XTHING : SIZEOF_THING);
            break;

        case ML_LINEDEFS:
            ptr = &_numLineDefs;
            elmSize = (_formatId == DOOM64? SIZEOF_64LINEDEF :
                       _formatId == HEXEN? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
            break;

        case ML_SIDEDEFS:
            ptr = &_numSideDefs;
            elmSize = (_formatId == DOOM64? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF);
            break;

        case ML_SECTORS:
            ptr = &_numSectors;
            elmSize = (_formatId == DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR);
            break;

        case ML_LIGHTS:
            ptr = &_numSurfaceTints;
            elmSize = SIZEOF_LIGHT;
            break;

        default:
            break;
        }

        if(ptr)
        {
            size_t lumpLength = W_LumpLength(*i);

            if(0 != lumpLength % elmSize)
            {
                _formatId = UNKNOWN; // What is this??
                return;
            }

            *ptr += lumpLength / elmSize;
        }
    }

    if(!(_numVertexes > 0 && _numLineDefs > 0 && _numSideDefs > 0 && _numSectors > 0))
        _formatId = UNKNOWN;
}

Map::~Map()
{
    clear();
}

void Map::clear(void)
{
    _lumpNums.clear();

    _lineDefs.clear();
    _numLineDefs = 0;

    _sideDefs.clear();
    _numSideDefs = 0;

    _sectors.clear();
    _numSectors = 0;

    _things.clear();
    _numThings = 0;

    for(Polyobjs::iterator i = _polyobjs.begin(); i != _polyobjs.end(); ++i)
        free(i->lineIndices);
    _polyobjs.clear();
    _numPolyobjs = 0;

    _surfaceTints.clear();
    _numSurfaceTints = 0;

    /*if(macros)
        free(macros);
    macros = NULL;*/

    if(_textures)
    {
        size_t i;
        for(i = 0; i < _numTextures; ++i)
        {
            materialref_t* m = _textures[i];
            free(m);
        }
        free(_textures);
    }
    _textures = NULL;

    if(_flats)
    {
        size_t i;
        for(i = 0; i < _numFlats; ++i)
        {
            materialref_t* m = _flats[i];
            free(m);
        }
        free(_flats);
    }
    _flats = NULL;
}

/**
 * Helper constructor. Given a map identifier search the Doomsday file system
 * for the associated lumps needed to instantiate Map.
 */
Map* Map::construct(const char* mapID)
{
    lumpnum_t startLump;

    if((startLump = W_CheckNumForName(mapID)) == -1)
        throw std::runtime_error("Failed to locate map data");

    LumpNums lumpNums;   
    // Add the marker lump to the list of lumps for this map.
    lumpNums.push_back(startLump);

    /**
     * Find the lumps associated with this map dataset and link them to the
     * archivedmap record.
     *
     * \note Some obscure PWADs have these lumps in a non-standard order,
     * so we need to go resort to finding them automatically.
     */
    for(lumpnum_t i = startLump + 1; i < W_NumLumps(); ++i)
    {
        const char* lumpName = W_LumpName(i);
        lumptype_t lumpType = dataTypeForLumpName(lumpName);

        /// \todo Do more validity checking.
        if(lumpType != ML_INVALID)
        {   // Its a known map lump.
            lumpNums.push_back(i);
            continue;
        }

        // Stop looking, we *should* have found them all.
        break;
    }

    if(recognize(lumpNums) == UNKNOWN)
    {
        lumpNums.clear();
        throw std::runtime_error("Unknown map data format.");
    }

    Map* map = new Map(lumpNums);
    lumpNums.clear();
    return map;
}

bool Map::loadVertexes(const byte* buf, size_t len)
{
    size_t elmSize = (_formatId == DOOM64? SIZEOF_64VERTEX : SIZEOF_VERTEX);
    uint n, num = len / elmSize;
    const byte* ptr;

    switch(_formatId)
    {
    default:
    case DOOM:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            _vertexCoords.push_back((float) SHORT(*((const int16_t*) (ptr))));
            _vertexCoords.push_back((float) SHORT(*((const int16_t*) (ptr+2))));
        }
        break;

    case DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            _vertexCoords.push_back(FIX2FLT(LONG(*((const int32_t*) (ptr)))));
            _vertexCoords.push_back(FIX2FLT(LONG(*((const int32_t*) (ptr+4)))));
        }
        break;
    }

    return true;
}

bool Map::loadLinedefs(const byte* buf, size_t len)
{
    uint n;
    const byte* ptr;

    switch(_formatId)
    {
    default:
    case DOOM:
        {uint num = len / SIZEOF_LINEDEF;
        for(n = 0, ptr = buf; n < num; ++n, ptr += SIZEOF_LINEDEF)
        {
            int vtx1Id = (int) USHORT(*((const uint16_t*) (ptr)));
            int vtx2Id = (int) USHORT(*((const uint16_t*) (ptr+2)));
            int frontSideId = (int) USHORT(*((const uint16_t*) (ptr+10)));
            int backSideId = (int) USHORT(*((const uint16_t*) (ptr+12)));

            _lineDefs.push_back(
                LineDef(vtx1Id == 0xFFFF? 0 : vtx1Id+1,
                        vtx2Id == 0xFFFF? 0 : vtx2Id+1,
                        frontSideId == 0xFFFF? 0 : frontSideId+1,
                        backSideId == 0xFFFF? 0 : backSideId+1,
                        SHORT(*((const int16_t*) (ptr+4))),
                        SHORT(*((const int16_t*) (ptr+6))),
                        SHORT(*((const int16_t*) (ptr+8))) ));
        }}
        break;

    case DOOM64:
        {uint num = len / SIZEOF_64LINEDEF;
        for(n = 0, ptr = buf; n < num; ++n, ptr += SIZEOF_64LINEDEF)
        {
            int vtx1Id = (int) USHORT(*((const uint16_t*) (ptr)));
            int vtx2Id = (int) USHORT(*((const uint16_t*) (ptr+2)));
            int frontSideId = (int) USHORT(*((const uint16_t*) (ptr+12)));
            int backSideId = (int) USHORT(*((const uint16_t*) (ptr+14)));

            _lineDefs.push_back(
                LineDef(vtx1Id == 0xFFFF? 0 : vtx1Id+1,
                        vtx2Id == 0xFFFF? 0 : vtx2Id+1,
                        frontSideId == 0xFFFF? 0 : frontSideId+1,
                        backSideId == 0xFFFF? 0 : backSideId+1,
                        SHORT(*((const int16_t*) (ptr+4))),
                        *((const byte*) (ptr + 6)),
                        *((const byte*) (ptr + 7)),
                        *((const byte*) (ptr + 8)),
                        *((const byte*) (ptr + 9)),
                        SHORT(*((const int16_t*) (ptr+10))) ));
        }}
        break;

    case HEXEN:
        {uint num = len / SIZEOF_XLINEDEF;
        for(n = 0, ptr = buf; n < num; ++n, ptr += SIZEOF_XLINEDEF)
        {
            int vtx1Id = (int) USHORT(*((const uint16_t*) (ptr)));
            int vtx2Id = (int) USHORT(*((const uint16_t*) (ptr+2)));
            int frontSideId = (int) USHORT(*((const uint16_t*) (ptr+12)));
            int backSideId = (int) USHORT(*((const uint16_t*) (ptr+14)));

            _lineDefs.push_back(
                LineDef(vtx1Id == 0xFFFF? 0 : vtx1Id+1,
                        vtx2Id == 0xFFFF? 0 : vtx2Id+1,
                        frontSideId == 0xFFFF? 0 : frontSideId+1,
                        backSideId == 0xFFFF? 0 : backSideId+1,
                        SHORT(*((const int16_t*) (ptr+4))),
                        *((const byte*) (ptr+6)),
                        *((const byte*) (ptr+7)),
                        *((const byte*) (ptr+8)),
                        *((const byte*) (ptr+9)),
                        *((const byte*) (ptr+10)),
                        *((const byte*) (ptr+11)),
                        false ));
        }}
        break;
    }

    return true;
}

bool Map::loadSidedefs(const byte* buf, size_t len)
{
    size_t elmSize = (_formatId == DOOM64? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF);
    uint n, num = len / elmSize;
    const byte* ptr;

    switch(_formatId)
    {
    default:
    case DOOM:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int sectorId = (int) USHORT(*((const uint16_t*) (ptr+28)));

            _sideDefs.push_back(
                SideDef(SHORT(*((const int16_t*) (ptr))),
                        SHORT(*((const int16_t*) (ptr+2))),
                        RegisterMaterial((const char*) (ptr+4), false),
                        RegisterMaterial((const char*) (ptr+12), false),
                        RegisterMaterial((const char*) (ptr+20), false),
                        sectorId == 0xFFFF? 0 : sectorId+1));
        }
        break;

    case DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int sectorId = (int) USHORT(*((const uint16_t*) (ptr+10)));

            _sideDefs.push_back(
                SideDef(SHORT(*((const int16_t*) (ptr))),
                        SHORT(*((const int16_t*) (ptr+2))),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+4))), false),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+6))), false),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+8))), false),
                        sectorId == 0xFFFF? 0 : sectorId+1));
        }
        break;
    }

    return true;
}

bool Map::loadSectors(const byte* buf, size_t len)
{
    size_t elmSize = (_formatId == DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR);
    uint n, num = len / elmSize;
    const byte* ptr;

    switch(_formatId)
    {
    default: // DOOM
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            _sectors.push_back(
                Sector(SHORT(*((const int16_t*) ptr)),
                       SHORT(*((const int16_t*) (ptr+2))),
                       RegisterMaterial((const char*) (ptr+4), true),
                       RegisterMaterial((const char*) (ptr+12), true),
                       SHORT(*((const int16_t*) (ptr+20))),
                       SHORT(*((const int16_t*) (ptr+22))),
                       SHORT(*((const int16_t*) (ptr+24)))));
        }
        break;

    case DOOM64:
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            _sectors.push_back(
                Sector(SHORT(*((const int16_t*) ptr)),
                       SHORT(*((const int16_t*) (ptr+2))),
                       RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+4))), false),
                       RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+6))), false),
                       160,
                       SHORT(*((const int16_t*) (ptr+18))),
                       SHORT(*((const int16_t*) (ptr+20))),
                       USHORT(*((const uint16_t*) (ptr+22))),
                       USHORT(*((const uint16_t*) (ptr+10))),
                       USHORT(*((const uint16_t*) (ptr+8))),
                       USHORT(*((const uint16_t*) (ptr+12))),
                       USHORT(*((const uint16_t*) (ptr+14))),
                       USHORT(*((const uint16_t*) (ptr+16)))));
        }
        break;
    }

    return true;
}

bool Map::loadThings(const byte* buf, size_t len)
{
// New flags: \todo get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

#define ANG45               0x20000000

    size_t elmSize = (_formatId == DOOM64? SIZEOF_64THING :
        _formatId == HEXEN? SIZEOF_XTHING : SIZEOF_THING);
    uint n, num = len / elmSize;
    const byte* ptr;

    switch(_formatId)
    {
    default:
    case DOOM:
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
            _things.push_back(
                Thing(SHORT(*((const int16_t*) (ptr))),
                      SHORT(*((const int16_t*) (ptr+2))),
                      0,
                      ANG45 * (SHORT(*((const int16_t*) (ptr+4))) / 45),
                      SHORT(*((const int16_t*) (ptr+6))),
                      (SHORT(*((const int16_t*) (ptr+8))) & ~MASK_UNKNOWN_THING_FLAGS) | MTF_Z_FLOOR));
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

    case DOOM64:
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
            _things.push_back(
                Thing(SHORT(*((const int16_t*) (ptr))),
                      SHORT(*((const int16_t*) (ptr+2))),
                      SHORT(*((const int16_t*) (ptr+4))),
                      ANG45 * (SHORT(*((const int16_t*) (ptr+6))) / 45),
                      SHORT(*((const int16_t*) (ptr+8))),
                      (SHORT(*((const int16_t*) (ptr+10))) & ~MASK_UNKNOWN_THING_FLAGS) | MTF_Z_FLOOR,
                      SHORT(*((const int16_t*) (ptr+12)))));
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

    case HEXEN:
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
            int16_t doomEdNum = SHORT(*((const int16_t*) (ptr+10)));
            angle_t angle = SHORT(*((const int16_t*) (ptr+8)));

            /**
             * Hexen format stores polyobject tags in the angle field in THINGS.
             * Thus, we cannot translate the angle until we know whether it is a
             * polyobject type or not.
             */
            if(doomEdNum != Thing::PO_ANCHOR &&
               doomEdNum != Thing::PO_SPAWN &&
               doomEdNum != Thing::PO_SPAWNCRUSH)
                angle = ANG45 * (angle / 45);

            // Translate flags:
            int32_t flags = SHORT(*((const int16_t*) (ptr+12)));
            flags &= ~MASK_UNKNOWN_THING_FLAGS;
            // Game type logic is inverted.
            flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);
            // HEXEN format things spawn relative to the floor by default
            // unless their type-specific flags override.
            flags |= MTF_Z_FLOOR;

            _things.push_back(
                Thing(SHORT(*((const int16_t*) (ptr+2))),
                      SHORT(*((const int16_t*) (ptr+4))),
                      SHORT(*((const int16_t*) (ptr+6))),
                      angle, doomEdNum, flags,
                      SHORT(*((const int16_t*) (ptr))),
                      *(ptr+14),
                      *(ptr+15), *(ptr+16), *(ptr+17), *(ptr+18), *(ptr+19)));
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

bool Map::loadLights(const byte* buf, size_t len)
{
    size_t elmSize = SIZEOF_LIGHT;
    uint n, num = len / elmSize;
    const byte* ptr;
    
    for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
    {
        _surfaceTints.push_back(
            SurfaceTint((float) *(ptr) / 255,
                        (float) *(ptr+1) / 255,
                        (float) *(ptr+2) / 255,
                        *(ptr+3), *(ptr+4), *(ptr+5)));
    }

    return true;
}

static size_t bufferLump(lumpnum_t lumpNum, byte** buf, size_t* oldLen)
{
    size_t lumpLength = W_LumpLength(lumpNum);

    // Need to enlarge our buffer?
    if(lumpLength > *oldLen)
    {
        *buf = (byte*) realloc(*buf, lumpLength);
        *oldLen = lumpLength;
    }

    // Buffer the entire lump.
    W_ReadLump(lumpNum, *buf);
    return lumpLength;
}

bool Map::load(void)
{
    // Allocate storage for the map data structures.
    _vertexCoords.reserve(_numVertexes * 2);
    _lineDefs.reserve(_numLineDefs);
    _sideDefs.reserve(_numSideDefs);
    _sectors.reserve(_numSectors);
    if(_numThings)
        _things.reserve(_numThings);
    if(_numSurfaceTints)
        _surfaceTints.reserve(_numSurfaceTints);

    byte* buf = NULL;
    size_t oldLen = 0;
    for(LumpNums::iterator i = _lumpNums.begin(); i != _lumpNums.end(); ++i)
    {
        lumptype_t lumpType = dataTypeForLumpName(W_LumpName(*i));
        size_t size;

        // Process it, transforming it into our local representation.
        switch(lumpType)
        {
        case ML_VERTEXES:
            size = bufferLump(*i, &buf, &oldLen);
            loadVertexes(buf, size);
            break;

        case ML_LINEDEFS:
            size = bufferLump(*i, &buf, &oldLen);
            loadLinedefs(buf, size);
            break;

        case ML_SIDEDEFS:
            size = bufferLump(*i, &buf, &oldLen);
            loadSidedefs(buf, size);
            break;

        case ML_SECTORS:
            size = bufferLump(*i, &buf, &oldLen);
            loadSectors(buf, size);
            break;

        case ML_THINGS:
            if(_numThings)
            {
                size = bufferLump(*i, &buf, &oldLen);
                loadThings(buf, size);
            }
            break;

        case ML_LIGHTS:
            if(_numSurfaceTints)
            {
                size = bufferLump(*i, &buf, &oldLen);
                loadLights(buf, size);
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

    if(_formatId == HEXEN)
        findPolyobjs();

    return true; // Read and converted successfully.
}

bool Map::transfer(void)
{
    uint startTime = Sys_GetRealTime();

    struct map_s* deMap = P_CurrentMap();
    bool result;

    // Announce any bad material names we came across while loading.
    logUnknownMaterials();

    Map_EditBegin(deMap);

    // Vertexes
    {
    /// @todo Map_CreateVertices only accepts an array so do the necessary.
    size_t numCoords = _vertexCoords.capacity();
    float* coords = new float[numCoords];
    copy(_vertexCoords.begin(), _vertexCoords.end(), coords);
    Map_CreateVertices(deMap, numCoords>>1, coords, NULL);
    delete coords;
    }

    // Sectors.
    {uint n = 0;
    for(Sectors::iterator i = _sectors.begin(); i != _sectors.end(); ++i, ++n)
    {
        uint sectorIDX, floorIDX, ceilIDX;

        sectorIDX = Map_CreateSector(deMap, (float) i->lightLevel / 255.0f, 1, 1, 1);

        floorIDX = Map_CreatePlane(deMap, i->floorHeight,
                                   i->floorMaterial? i->floorMaterial->material : 0,
                                   0, 0, 1, 1, 1, 1, 0, 0, 1);
        ceilIDX = Map_CreatePlane(deMap, i->ceilHeight,
                                  i->ceilMaterial? i->ceilMaterial->material : 0,
                                  0, 0, 1, 1, 1, 1, 0, 0, -1);

        Map_SetSectorPlane(deMap, sectorIDX, 0, floorIDX);
        Map_SetSectorPlane(deMap, sectorIDX, 1, ceilIDX);

        Map_GameObjectRecordProperty(deMap, "XSector", n, "ID", DDVT_INT, &n);
        Map_GameObjectRecordProperty(deMap, "XSector", n, "Tag", DDVT_SHORT, &i->tag);
        Map_GameObjectRecordProperty(deMap, "XSector", n, "Type", DDVT_SHORT, &i->type);

        if(_formatId == DOOM64)
        {
            Map_GameObjectRecordProperty(deMap, "XSector", n, "Flags", DDVT_SHORT, &i->d64flags);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "CeilingColor", DDVT_SHORT, &i->d64ceilColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "FloorColor", DDVT_SHORT, &i->d64floorColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "UnknownColor", DDVT_SHORT, &i->d64unknownColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "WallTopColor", DDVT_SHORT, &i->d64wallTopColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "WallBottomColor", DDVT_SHORT, &i->d64wallBottomColor);
        }
    }}

    {uint n = 0;
    for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i, ++n)
    {
        uint frontIdx = 0, backIdx = 0;

        if(i->sides[0])
        {
            const SideDef& s = _sideDefs[i->sides[0]-1];

            frontIdx =
                Map_CreateSideDef(deMap, s.sectorId,
                                  (_formatId == DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  s.topMaterial? s.topMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1,
                                  s.middleMaterial? s.middleMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1, 1,
                                  s.bottomMaterial? s.bottomMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1);
        }

        if(i->sides[1])
        {
            const SideDef& s = _sideDefs[i->sides[1]-1];

            backIdx =
                Map_CreateSideDef(deMap, s.sectorId,
                                  (_formatId == DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  s.topMaterial? s.topMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1,
                                  s.middleMaterial? s.middleMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1, 1,
                                  s.bottomMaterial? s.bottomMaterial->material : 0,
                                  s.offset[0], s.offset[1], 1, 1, 1);
        }

        Map_CreateLineDef(deMap, i->v[0], i->v[1], frontIdx, backIdx, 0);

        Map_GameObjectRecordProperty(deMap, "XLinedef", n, "ID", DDVT_INT, &n);
        Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Flags", DDVT_SHORT, &i->flags);

        switch(_formatId)
        {
        default:
        case DOOM:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_SHORT, &i->dType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Tag", DDVT_SHORT, &i->dTag);
            break;

        case DOOM64:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "DrawFlags", DDVT_BYTE, &i->d64drawFlags);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "TexFlags", DDVT_BYTE, &i->d64texFlags);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_BYTE, &i->d64type);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "UseType", DDVT_BYTE, &i->d64useType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Tag", DDVT_SHORT, &i->d64tag);
            break;

        case HEXEN:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_BYTE, &i->xType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg0", DDVT_BYTE, &i->xArgs[0]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg1", DDVT_BYTE, &i->xArgs[1]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg2", DDVT_BYTE, &i->xArgs[2]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg3", DDVT_BYTE, &i->xArgs[3]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg4", DDVT_BYTE, &i->xArgs[4]);
            break;
        }
    }}

    {uint n = 0;
    for(SurfaceTints::iterator i = _surfaceTints.begin();
        i != _surfaceTints.end(); ++i, ++n)
    {
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorR", DDVT_FLOAT, &i->rgb[0]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorG", DDVT_FLOAT, &i->rgb[1]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorB", DDVT_FLOAT, &i->rgb[2]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX0", DDVT_BYTE, &i->xx[0]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX1", DDVT_BYTE, &i->xx[1]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX2", DDVT_BYTE, &i->xx[2]);
    }}

    {uint n = 0;
    for(Polyobjs::iterator i = _polyobjs.begin();
        i != _polyobjs.end(); ++i, ++n)
    {
        objectrecordid_t* lineList;
        uint j;

        lineList = (objectrecordid_t*) malloc(sizeof(objectrecordid_t) * i->lineCount);
        for(j = 0; j < i->lineCount; ++j)
            lineList[j] = i->lineIndices[j] + 1;
        Map_CreatePolyobj(deMap, lineList, i->lineCount, i->tag,
                          i->seqType, (float) i->anchor[0],
                          (float) i->anchor[1]);
        free(lineList);
    }}

    {uint n = 0;
    for(Things::iterator i = _things.begin();
        i != _things.end(); ++i, ++n)
    {
        Map_GameObjectRecordProperty(deMap, "Thing", n, "X", DDVT_SHORT, &i->pos[0]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Y", DDVT_SHORT, &i->pos[1]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Z", DDVT_SHORT, &i->pos[2]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Angle", DDVT_ANGLE, &i->angle);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "DoomEdNum", DDVT_SHORT, &i->doomEdNum);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Flags", DDVT_INT, &i->flags);

        if(_formatId == DOOM64)
        {
            Map_GameObjectRecordProperty(deMap, "Thing", n, "ID", DDVT_SHORT, &i->d64TID);
        }
        else if(_formatId == HEXEN)
        {
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Special", DDVT_BYTE, &i->xSpecial);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "ID", DDVT_SHORT, &i->xTID);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg0", DDVT_BYTE, &i->xArgs[0]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg1", DDVT_BYTE, &i->xArgs[1]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg2", DDVT_BYTE, &i->xArgs[2]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg3", DDVT_BYTE, &i->xArgs[3]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg4", DDVT_BYTE, &i->xArgs[4]);
        }
    }}

    // Let Doomsday know that we've finished with this wadmap.
    result = Map_EditEnd(deMap)? true : false;

    if(ArgExists("-verbose"))
        Con_Message("WadMapConverter::TransferMap: Done in %.2f seconds.\n",
                    (Sys_GetRealTime() - startTime) / 1000.0f);

    return result;
}

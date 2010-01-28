/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include "doomsday.h"

#include "Map"

#include <cstring>
#include <algorithm>
#include <assert.h>

using namespace wadconverter;

/**
 * \note Map takes ownership of @a lumpNums.
 */
Map::Map(LumpNums* lumpNums)
    : _lumpNums(lumpNums),
      _numVertexes(0),
      _numSectors(0),
      _numLineDefs(0),
      _numSideDefs(0),
      _numPolyobjs(0),
      _numThings(0),
      _numSurfaceTints(0)
{
    assert(_lumpNums);
    assert(_lumpNums->size() > 0);

    /**
     * Determine the number of map data elements in each lump.
     */
    _formatId = recognize(*_lumpNums);
    for(LumpNums::iterator i = _lumpNums->begin(); i != _lumpNums->end(); ++i)
    {
        MapLumpId lumpType = dataTypeForLumpName(W_LumpName(*i));
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

Map::MaterialRefId Map::getMaterial(const char* _name)
{
    char name[9];
    const char *namePtr;

    if(_formatId == DOOM64)
    {
        int idx = *((int*) _name);
        dd_snprintf(name, 9, "UNK%05i", idx);
        namePtr = name;
    }
    else
    {
        namePtr = _name;
    }

    {MaterialRefId n = 0;
    for(MaterialRefs::iterator i = _materialRefs.begin();
        i != _materialRefs.end(); ++i, ++n)
        if(i->compare(namePtr) == 0)
            return n+1; // 1-based index.
    }
    return 0;
}

Map::MaterialRefId Map::RegisterMaterial(const char* _name, bool onPlane)
{
    char name[9];
    memcpy(name, _name, sizeof(name));
    name[8] = '\0';

    // When loading DOOM or Hexen format maps check for the special case "-"
    // texture name (no texture).
    if(!onPlane && (_formatId == Map::DOOM || _formatId == Map::HEXEN) &&
       !stricmp(name, "-"))
        return 0;

    // Check if this material has already been registered.
    MaterialRefId refId;
    if((refId = getMaterial(name)) > 0)
        return refId;

    // A new material.
    _materialRefs.push_back(std::string(name));
    return _materialRefs.size(); // 1-based index.
}

Map::MapLumpId Map::dataTypeForLumpName(const char* name)
{
    assert(name && name[0]);

    static const struct lumptype_s {
        MapLumpId       type;
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

    if(name && name[0])
    {
        for(size_t i = 0; knownLumps[i].name; ++i)
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
#define SEQTYPE_NUMSEQ       10
#define PO_LINE_START        1
#define PO_LINE_EXPLICIT     5

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
                Con_Error("WadConverter::findAndCreatePolyobj: Found unclosed polyobj.\n");
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
                        ("WadConverter::findAndCreatePolyobj: Explicit line missing order number "
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
                            ("WadConverter::findAndCreatePolyobj: psIndex > MAXPOLYLINES\n");
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
                        ("WadConverter::findAndCreatePolyobj: Missing explicit line %d for poly %d\n",
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

#undef PO_LINE_EXPLICIT
#undef PO_LINE_START
#undef SEQTYPE_NUMSEQ
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

void Map::clear(void)
{
    _lumpNums->clear();
    delete _lumpNums;

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

    _materialRefs.clear();
}

/**
 * Helper constructor. Given a map identifier search the Doomsday file system
 * for the associated lumps needed to instantiate Map.
 */
Map* Map::construct(const char* mapID)
{
    assert(mapID && mapID[0]);

    lumpnum_t startLump;
    if((startLump = W_CheckNumForName(mapID)) == -1)
        throw std::runtime_error("Failed to locate map data");

    LumpNums* lumpNums = new LumpNums;

    // Add the marker lump to the list of lumps for this map.
    lumpNums->push_back(startLump);

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
        MapLumpId lumpType = dataTypeForLumpName(lumpName);

        /// \todo Do more validity checking.
        if(lumpType != ML_INVALID)
        {   // Its a known map lump.
            lumpNums->push_back(i);
            continue;
        }

        // Stop looking, we *should* have found them all.
        break;
    }

    if(recognize(*lumpNums) == UNKNOWN)
    {
        lumpNums->clear();
        delete lumpNums;
        throw std::runtime_error("Unknown map data format.");
    }

    return new Map(lumpNums);
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
#pragma message( "Warning: Need to read DOOM64 format SideDefs" )
        /*for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int sectorId = (int) USHORT(*((const uint16_t*) (ptr+10)));

            _sideDefs.push_back(
                SideDef(SHORT(*((const int16_t*) (ptr))),
                        SHORT(*((const int16_t*) (ptr+2))),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+4))), false),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+6))), false),
                        RegisterMaterial((int) USHORT(*((const uint16_t*) (ptr+8))), false),
                        sectorId == 0xFFFF? 0 : sectorId+1));
        }*/
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
#pragma message( "Warning: Need to read DOOM64 format Sectors" )
        /*for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
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
        }*/
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
    for(LumpNums::iterator i = _lumpNums->begin(); i != _lumpNums->end(); ++i)
    {
        MapLumpId lumpType = dataTypeForLumpName(W_LumpName(*i));
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

material_t* Map::getMaterialForId(MaterialRefId id, bool onPlane)
{
    if(id == 0)
        return NULL;

    const std::string& name = _materialRefs[id-1];
    // First try the prefered namespace, then any.
    material_t* material = P_MaterialForName(onPlane? MN_FLATS : MN_TEXTURES, name.c_str());
    if(material) return material;
    return P_MaterialForName(MN_ANY, name.c_str());
}

bool Map::transfer(void)
{
    uint startTime = Sys_GetRealTime();

    struct map_s* deMap = P_CurrentMap();
    bool result;

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
        uint sectorIDX = Map_CreateSector(deMap, (float) i->lightLevel / 255.0f, 1, 1, 1);

        uint floorIDX = Map_CreatePlane(deMap, i->floorHeight,
                                   getMaterialForId(i->floorMaterial, true),
                                   0, 0, 1, 1, 1, 1, 0, 0, 1);
        uint ceilIDX = Map_CreatePlane(deMap, i->ceilHeight,
                                 getMaterialForId(i->ceilMaterial, true),
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
                                  getMaterialForId(s.topMaterial, false),
                                  s.offset[0], s.offset[1], 1, 1, 1,
                                  getMaterialForId(s.middleMaterial, false),
                                  s.offset[0], s.offset[1], 1, 1, 1, 1,
                                  getMaterialForId(s.bottomMaterial, false),
                                  s.offset[0], s.offset[1], 1, 1, 1);
        }

        if(i->sides[1])
        {
            const SideDef& s = _sideDefs[i->sides[1]-1];

            backIdx =
                Map_CreateSideDef(deMap, s.sectorId,
                                  (_formatId == DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  getMaterialForId(s.topMaterial, false),
                                  s.offset[0], s.offset[1], 1, 1, 1,
                                  getMaterialForId(s.middleMaterial, false),
                                  s.offset[0], s.offset[1], 1, 1, 1, 1,
                                  getMaterialForId(s.bottomMaterial, false),
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
        Con_Message("WadConverter::TransferMap: Done in %.2f seconds.\n",
                    (Sys_GetRealTime() - startTime) / 1000.0f);

    return result;
}

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

Map::MaterialRefId Map::readMaterialReference(de::Reader& from, bool onPlane)
{
#pragma message( "Warning: Need to read DOOM64 format material references" )
// DOOM64 material references are indexes into lump directory (for flats)
// or offsets in TEXTURES. Read as: (int) USHORT(*((const uint16_t*) (offset)))

    de::String name;
    de::FixedByteArray byteSeq(name, 0, 8);
    from >> byteSeq;
    // When loading DOOM or Hexen format maps check for the special case "-"
    // texture name (no texture).
    if(!onPlane && (_formatId == Map::DOOM || _formatId == Map::HEXEN) &&
       !name.compareWithCase("-"))
        return MaterialRefs::NONINDEX;
    return _materialRefs.insert(name);
}

material_t* Map::getMaterialForId(MaterialRefId id, bool onPlane)
{
    if(id == MaterialRefs::NONINDEX)
        return NULL;

    const std::string& name = _materialRefs.get(id);
    // First try the prefered namespace, then any.
    material_t* material = P_MaterialForName(onPlane? MN_FLATS : MN_TEXTURES, name.c_str());
    if(material) return material;
    return P_MaterialForName(MN_ANY, name.c_str());
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
            if((*iter) == lineDefs[i])
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
        LineDef* lineDef = (*i);
        int16_t v1[2], v2[2];
        v1[0] = (int16_t) _vertexCoords[(lineDef->v[0] - 1) * 2];
        v1[1] = (int16_t) _vertexCoords[(lineDef->v[0] - 1) * 2 + 1];
        v2[0] = (int16_t) _vertexCoords[(lineDef->v[1] - 1) * 2];
        v2[1] = (int16_t) _vertexCoords[(lineDef->v[1] - 1) * 2 + 1];

        if(v1[0] == x && v1[1] == y)
        {
            if(!lineDefs)
                *polyLineCount++;
            else
                *lineDefs++ = lineDef;

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
        LineDef* lineDef = (*i);
        if(lineDef->xType == PO_LINE_START && lineDef->xArgs[0] == tag)
        {
            byte seqType;
            LineDef** lineList;
            int16_t v1[2], v2[2];
            uint polyLineCount;
            int16_t polyStart[2];

            lineDef->xType = 0;
            lineDef->xArgs[0] = 0;
            polyLineCount = 1;

            v1[0] = (int16_t) _vertexCoords[(lineDef->v[0]-1) * 2];
            v1[1] = (int16_t) _vertexCoords[(lineDef->v[0]-1) * 2 + 1];
            v2[0] = (int16_t) _vertexCoords[(lineDef->v[1]-1) * 2];
            v2[1] = (int16_t) _vertexCoords[(lineDef->v[1]-1) * 2 + 1];
            polyStart[0] = v1[0];
            polyStart[1] = v1[1];
            if(!iterFindPolyLines(v2[0], v2[1], polyStart, &polyLineCount, NULL))
            {
                Con_Error("WadConverter::findAndCreatePolyobj: Found unclosed polyobj.\n");
            }

            lineList = (LineDef**) malloc((polyLineCount+1) * sizeof(LineDef*));

            lineList[0] = lineDef; // Insert the first line.
            iterFindPolyLines(v2[0], v2[1], polyStart, &polyLineCount, lineList + 1);
            lineList[polyLineCount] = 0; // Terminate.

            seqType = lineDef->xArgs[2];
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
            LineDef* lineDef = (*i);
            if(lineDef->xType == PO_LINE_EXPLICIT && lineDef->xArgs[0] == tag)
            {
                if(!lineDef->xArgs[1])
                {
                    Con_Error
                        ("WadConverter::findAndCreatePolyobj: Explicit line missing order number "
                         "(probably %d) in poly %d.\n",
                         j + 1, tag);
                }

                if(lineDef->xArgs[1] == j)
                {
                    // Add this line to the list.
                    polyLineList[psIndex] = lineDef;
                    lineCount++;
                    psIndex++;
                    if(psIndex > MAXPOLYLINES)
                    {
                        Con_Error
                            ("WadConverter::findAndCreatePolyobj: psIndex > MAXPOLYLINES\n");
                    }

                    // Clear out any special.
                    lineDef->xType = 0;
                    lineDef->xArgs[0] = 0;
                }
            }
        }

        if(psIndex == psIndexOld)
        {   // Check if an explicit line order has been skipped
            // A line has been skipped if there are any more explicit
            // _lineDefs with the current tag value
            for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
            {
                LineDef* lineDef = (*i);
                if(lineDef->xType == PO_LINE_EXPLICIT && lineDef->xArgs[0] == tag)
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
        Thing* thing = (*i);
        if(thing->doomEdNum == Thing::PO_ANCHOR)
        {   // A polyobj anchor.
            findAndCreatePolyobj(thing->angle, thing->pos[0], thing->pos[1]);
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
    _lumpNums = NULL;

    _numLineDefs = 0;
    _numSideDefs = 0;
    _numSectors = 0;
    _numThings = 0;
    _numSurfaceTints = 0;

    for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i)
        delete (*i);
    _lineDefs.clear();

    for(SideDefs::iterator i = _sideDefs.begin(); i != _sideDefs.end(); ++i)
        delete (*i);
    _sideDefs.clear();

    for(Sectors::iterator i = _sectors.begin(); i != _sectors.end(); ++i)
        delete (*i);
    _sectors.clear();

    for(Things::iterator i = _things.begin(); i != _things.end(); ++i)
        delete (*i);
    _things.clear();

    for(SurfaceTints::iterator i = _surfaceTints.begin(); i != _surfaceTints.end(); ++i)
        delete (*i);
    _surfaceTints.clear();

    for(Polyobjs::iterator i = _polyobjs.begin(); i != _polyobjs.end(); ++i)
        free(i->lineIndices);
    _polyobjs.clear();
    _numPolyobjs = 0;

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

Map::SideDef* Map::SideDef::constructFrom(de::Reader& from, Map* map)
{
    SideDef* s = new SideDef(0, 0, 0, 0, 0, 0);

    from >> s->offset[0]
         >> s->offset[1];

    s->topMaterial    = map->readMaterialReference(from, false);
    s->bottomMaterial = map->readMaterialReference(from, false);
    s->middleMaterial = map->readMaterialReference(from, false);

    de::dint16 secId;
    from >> secId; s->sectorId = secId == 0xFFFF? 0 : secId+1;
    return s;
}

Map::Sector* Map::Sector::constructFrom(de::Reader& from, Map* map)
{
    Sector* s = new Sector(0, 0, 0, 0, 0, 0, 0);

    from >> s->floorHeight
         >> s->ceilHeight;

    s->floorMaterial = map->readMaterialReference(from, true);
    s->ceilMaterial  = map->readMaterialReference(from, true);

    if(map->_formatId == DOOM64)
    {
        s->lightLevel = 160;
        from >> s->d64ceilColor
             >> s->d64floorColor
             >> s->d64unknownColor
             >> s->d64wallTopColor
             >> s->d64wallBottomColor;
    }
    else
        from >> s->lightLevel;

    from >> s->type
         >> s->tag;

    if(map->_formatId == DOOM64)
        from >> s->d64flags;

    return s;
}

Map::LineDef* Map::LineDef::constructFrom(de::Reader& from, Map* map)
{
    LineDef* l = new LineDef(0, 0, 0, 0, 0, 0, 0);

    de::dint16 idx;
    from >> idx; l->v[0] = idx == 0xFFFF? 0 : idx+1;
    from >> idx; l->v[1] = idx == 0xFFFF? 0 : idx+1;

    from >> l->flags;

    switch(map->_formatId)
    {
    case DOOM:
        from >> l->dType
             >> l->dTag;
        break;
    case DOOM64:
        from >> l->d64drawFlags
             >> l->d64texFlags
             >> l->d64type
             >> l->d64useType
             >> l->d64tag;
        break;
    case HEXEN:
        from >> l->xType
             >> l->xArgs[0]
             >> l->xArgs[1]
             >> l->xArgs[2]
             >> l->xArgs[3]
             >> l->xArgs[4];
        break;
    }

    from >> idx; l->sides[0] = idx == 0xFFFF? 0 : idx+1;
    from >> idx; l->sides[1] = idx == 0xFFFF? 0 : idx+1;

    return l;
}

Map::Thing* Map::Thing::constructFrom(de::Reader& from, Map* map)
{
// New flags: \todo get these from a game api header.
#define MTF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MTF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MTF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

#define ANG45               0x20000000

    Thing* t = new Thing(0, 0, 0, 0, 0, 0);

    switch(map->_formatId)
    {
    default:
    case DOOM:
        {
/**
 * DOOM Thing flags:
 */
#define MTF_EASY            0x0001 // Can be spawned in Easy skill modes.
#define MTF_MEDIUM          0x0002 // Can be spawned in Medium skill modes.
#define MTF_HARD            0x0004 // Can be spawned in Hard skill modes.
#define MTF_DEAF            0x0008 // Mobj will be deaf spawned deaf.
#define MTF_NOTSINGLE       0x0010 // (BOOM) Can not be spawned in single player gamemodes.
#define MTF_NOTDM           0x0020 // (BOOM) Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x0040 // (BOOM) Can not be spawned in the Co-op gameMode.
#define MTF_FRIENDLY        0x0080 // (BOOM) friendly monster.

#define MASK_UNKNOWN_THING_FLAGS (0xffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_NOTDM|MTF_NOTCOOP|MTF_FRIENDLY))

        from >> t->pos[0]
             >> t->pos[1];

        de::dint16 ang;
        from >> ang; t->angle = ANG45 * static_cast<angle_t>(ang / 45);

        from >> t->doomEdNum;

        de::dint16 flags;
        from >> flags; t->flags = static_cast<de::dint32>(flags & ~MASK_UNKNOWN_THING_FLAGS) | MTF_Z_FLOOR;

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
        }
    case DOOM64:
        {
/**
 * DOOM64 Thing flags:
 */
#define MTF_EASY            0x0001 // Appears in easy skill modes.
#define MTF_MEDIUM          0x0002 // Appears in medium skill modes.
#define MTF_HARD            0x0004 // Appears in hard skill modes.
#define MTF_DEAF            0x0008 // Thing is deaf.
#define MTF_NOTSINGLE       0x0010 // Appears in multiplayer game modes only.
#define MTF_DONTSPAWNATSTART 0x0020 // Do not spawn this thing at map start.
#define MTF_SCRIPT_TOUCH    0x0040 // Mobjs spawned from this spot will envoke a script when touched.
#define MTF_SCRIPT_DEATH    0x0080 // Mobjs spawned from this spot will envoke a script on death.
#define MTF_SECRET          0x0100 // A secret (bonus) item.
#define MTF_NOTARGET        0x0200 // Mobjs spawned from this spot will not target their attacker when hurt.
#define MTF_NOTDM           0x0400 // Can not be spawned in the Deathmatch gameMode.
#define MTF_NOTCOOP         0x0800 // Can not be spawned in the Co-op gameMode.

#define MASK_UNKNOWN_THING_FLAGS (0xffff \
    ^ (MTF_EASY|MTF_MEDIUM|MTF_HARD|MTF_DEAF|MTF_NOTSINGLE|MTF_DONTSPAWNATSTART|MTF_SCRIPT_TOUCH|MTF_SCRIPT_DEATH|MTF_SECRET|MTF_NOTARGET|MTF_NOTDM|MTF_NOTCOOP))

        from >> t->pos[0]
             >> t->pos[1]
             >> t->pos[2];

        de::dint16 ang;
        from >> ang; t->angle = ANG45 * static_cast<angle_t>(ang / 45);

        from >> t->doomEdNum;

        de::dint16 flags;
        from >> flags; t->flags = static_cast<de::dint32>(flags & ~MASK_UNKNOWN_THING_FLAGS) | MTF_Z_FLOOR;

        from >> t->d64TID;

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
        }
    case HEXEN:
        {
/**
 * Hexen Thing flags:
 */
#define MTF_EASY            0x0001
#define MTF_NORMAL          0x0002
#define MTF_HARD            0x0004
#define MTF_AMBUSH          0x0008
#define MTF_DORMANT         0x0010
#define MTF_FIGHTER         0x0020
#define MTF_CLERIC          0x0040
#define MTF_MAGE            0x0080
#define MTF_GSINGLE         0x0100
#define MTF_GCOOP           0x0200
#define MTF_GDEATHMATCH     0x0400
// The following are not currently used.
#define MTF_SHADOW          0x0800 // (ZDOOM) Thing is 25% translucent.
#define MTF_INVISIBLE       0x1000 // (ZDOOM) Makes the thing invisible.
#define MTF_FRIENDLY        0x2000 // (ZDOOM) Friendly monster.
#define MTF_STILL           0x4000 // (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).

#define MASK_UNKNOWN_THING_FLAGS (0xffff \
    ^ (MTF_EASY|MTF_NORMAL|MTF_HARD|MTF_AMBUSH|MTF_DORMANT|MTF_FIGHTER|MTF_CLERIC|MTF_MAGE|MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH|MTF_SHADOW|MTF_INVISIBLE|MTF_FRIENDLY|MTF_STILL))

        from >> t->xTID
             >> t->pos[0]
             >> t->pos[1]
             >> t->pos[2];

        de::dint16 ang;
        from >> ang;

        from >> t->doomEdNum;

        /**
         * Hexen format stores polyobject tags in the angle field in THINGS.
         * Thus, we cannot translate the angle until we know whether it is a
         * polyobject type or not.
         */
        if(t->doomEdNum != Thing::PO_ANCHOR &&
           t->doomEdNum != Thing::PO_SPAWN &&
           t->doomEdNum != Thing::PO_SPAWNCRUSH)
            t->angle = ANG45 * static_cast<angle_t>(ang / 45);
        else
            t->angle = static_cast<angle_t>(ang);

        // Translate flags:
        de::dint16 flags;
        from >> flags;
        flags &= ~MASK_UNKNOWN_THING_FLAGS;
        // Game type logic is inverted.
        flags ^= (MTF_GSINGLE|MTF_GCOOP|MTF_GDEATHMATCH);
        t->flags = static_cast<de::dint32>(flags) | MTF_Z_FLOOR;

        from >> t->xSpecial
             >> t->xArgs[0]
             >> t->xArgs[1]
             >> t->xArgs[2]
             >> t->xArgs[3]
             >> t->xArgs[4];

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
    }

    return t;

#undef MTF_Z_FLOOR
#undef MTF_Z_CEIL
#undef MTF_Z_RANDOM
}

Map::SurfaceTint* Map::SurfaceTint::constructFrom(de::Reader& from, Map* map)
{
    SurfaceTint* s = new SurfaceTint(0, 0, 0, 0, 0, 0);

    de::dbyte comp;
    from >> comp; s->rgb[0] = static_cast<de::dfloat>(comp) / 255;
    from >> comp; s->rgb[1] = static_cast<de::dfloat>(comp) / 255;
    from >> comp; s->rgb[2] = static_cast<de::dfloat>(comp) / 255;

    from >> s->xx[0]
         >> s->xx[1]
         >> s->xx[2];

    return s;
}

void Map::loadVertexes(de::Reader& from, de::dsize numElements)
{
    _vertexCoords.reserve(numElements * 2);
    for(de::dsize n = 0; n < numElements; ++n)
    {
        de::dfloat x, y;

        if(_formatId == DOOM64)
        {
            de::dint32 comp;
            from >> comp; x = static_cast<de::dfloat>(comp) / FRACUNIT;
            from >> comp; y = static_cast<de::dfloat>(comp) / FRACUNIT;
        }
        else
        {
            de::dint16 comp;
            from >> comp; x = static_cast<de::dfloat>(comp);
            from >> comp; y = static_cast<de::dfloat>(comp);
        }

        _vertexCoords.push_back(x);
        _vertexCoords.push_back(y);
    }
}

void Map::loadLinedefs(de::Reader& from, de::dsize numElements)
{
    _lineDefs.reserve(numElements);
    for(de::dsize n = 0; n < numElements; ++n)
        _lineDefs.push_back(LineDef::constructFrom(from, this));
}

void Map::loadSidedefs(de::Reader& from, de::dsize numElements)
{
    _sideDefs.reserve(numElements);
    for(de::dsize n = 0; n < numElements; ++n)
        _sideDefs.push_back(SideDef::constructFrom(from, this));
}

void Map::loadSectors(de::Reader& from, de::dsize numElements)
{
    _sectors.reserve(numElements);
    for(de::dsize n = 0; n < numElements; ++n)
        _sectors.push_back(Sector::constructFrom(from, this));
}

void Map::loadThings(de::Reader& from, de::dsize numElements)
{
    _things.reserve(numElements);
    for(de::dsize n = 0; n < numElements; ++n)
        _things.push_back(Thing::constructFrom(from, this));
}

void Map::loadLights(de::Reader& from, de::dsize numElements)
{
    _surfaceTints.reserve(numElements);
    for(de::dsize n = 0; n < numElements; ++n)
        _surfaceTints.push_back(SurfaceTint::constructFrom(from, this));
}

static const de::FixedByteArray* bufferLump(de::Block& buffer, lumpnum_t lumpNum)
{
    size_t lumpLength = W_LumpLength(lumpNum);
    byte* temp = new byte[lumpLength];
    W_ReadLump(lumpNum, temp);
    de::FixedByteArray* data = new de::FixedByteArray(buffer, 0, lumpLength);
    data->set(0, temp, lumpLength);
    delete [] temp;
    return data;
}

bool Map::load(void)
{
    de::Block buffer(0);
    for(LumpNums::iterator i = _lumpNums->begin(); i != _lumpNums->end(); ++i)
    {
        lumpnum_t lumpNum = (*i);
        MapLumpId lumpType = dataTypeForLumpName(W_LumpName(lumpNum));

        switch(lumpType)
        {
        case ML_VERTEXES:
            {
            const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
            loadVertexes(de::Reader(*cachedLump, de::littleEndianByteOrder),
                cachedLump->size() / (_formatId == DOOM64? SIZEOF_64VERTEX : SIZEOF_VERTEX));
            delete cachedLump;
            break;
            }
        case ML_LINEDEFS:
            {
            const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
            loadLinedefs(de::Reader(*cachedLump, de::littleEndianByteOrder),
                cachedLump->size() /
                (_formatId == DOOM64? SIZEOF_64LINEDEF :
                 _formatId == HEXEN? SIZEOF_XLINEDEF : SIZEOF_LINEDEF));
            delete cachedLump;
            break;
            }
        case ML_SIDEDEFS:
            {
            const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
            loadSidedefs(de::Reader(*cachedLump, de::littleEndianByteOrder),
                cachedLump->size() / (_formatId == DOOM64? SIZEOF_64SIDEDEF : SIZEOF_SIDEDEF));
            delete cachedLump;
            break;
            }
        case ML_SECTORS:
            {
            const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
            loadSectors(de::Reader(*cachedLump, de::littleEndianByteOrder),
                cachedLump->size() / (_formatId == DOOM64? SIZEOF_64SECTOR : SIZEOF_SECTOR));
            delete cachedLump;
            break;
            }
        case ML_THINGS:
            if(_numThings)
            {
                const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
                loadThings(de::Reader(*cachedLump, de::littleEndianByteOrder),
                    cachedLump->size() /
                    (_formatId == DOOM64? SIZEOF_64THING :
                     _formatId == HEXEN? SIZEOF_XTHING : SIZEOF_THING));
                delete cachedLump;
            }
            break;
        case ML_LIGHTS:
            if(_numSurfaceTints)
            {
                const de::FixedByteArray* cachedLump = bufferLump(buffer, lumpNum);
                loadLights(de::Reader(*cachedLump, de::littleEndianByteOrder),
                    cachedLump->size() / SIZEOF_LIGHT);
                delete cachedLump;
            }
            break;

        default:
            break;
        }
    }

    if(_formatId == HEXEN)
        findPolyobjs();

    return true; // Read and converted successfully.
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
        Sector* sector = (*i);
        uint sectorIDX = Map_CreateSector(deMap, (float) sector->lightLevel / 255.0f, 1, 1, 1);

        uint floorIDX = Map_CreatePlane(deMap, sector->floorHeight,
                                   getMaterialForId(sector->floorMaterial, true),
                                   0, 0, 1, 1, 1, 1, 0, 0, 1);
        uint ceilIDX = Map_CreatePlane(deMap, sector->ceilHeight,
                                 getMaterialForId(sector->ceilMaterial, true),
                                  0, 0, 1, 1, 1, 1, 0, 0, -1);

        Map_SetSectorPlane(deMap, sectorIDX, 0, floorIDX);
        Map_SetSectorPlane(deMap, sectorIDX, 1, ceilIDX);

        Map_GameObjectRecordProperty(deMap, "XSector", n, "ID", DDVT_INT, &n);
        Map_GameObjectRecordProperty(deMap, "XSector", n, "Tag", DDVT_SHORT, &sector->tag);
        Map_GameObjectRecordProperty(deMap, "XSector", n, "Type", DDVT_SHORT, &sector->type);

        if(_formatId == DOOM64)
        {
            Map_GameObjectRecordProperty(deMap, "XSector", n, "Flags", DDVT_SHORT, &sector->d64flags);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "CeilingColor", DDVT_SHORT, &sector->d64ceilColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "FloorColor", DDVT_SHORT, &sector->d64floorColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "UnknownColor", DDVT_SHORT, &sector->d64unknownColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "WallTopColor", DDVT_SHORT, &sector->d64wallTopColor);
            Map_GameObjectRecordProperty(deMap, "XSector", n, "WallBottomColor", DDVT_SHORT, &sector->d64wallBottomColor);
        }
    }}

    {uint n = 0;
    for(LineDefs::iterator i = _lineDefs.begin(); i != _lineDefs.end(); ++i, ++n)
    {
        LineDef* lineDef = (*i);
        uint frontIdx = 0, backIdx = 0;

        if(lineDef->sides[0])
        {
            const SideDef* s = _sideDefs[lineDef->sides[0]-1];

            frontIdx =
                Map_CreateSideDef(deMap, s->sectorId,
                                  (_formatId == DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  getMaterialForId(s->topMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1,
                                  getMaterialForId(s->middleMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1, 1,
                                  getMaterialForId(s->bottomMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1);
        }

        if(lineDef->sides[1])
        {
            const SideDef* s = _sideDefs[lineDef->sides[1]-1];

            backIdx =
                Map_CreateSideDef(deMap, s->sectorId,
                                  (_formatId == DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  getMaterialForId(s->topMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1,
                                  getMaterialForId(s->middleMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1, 1,
                                  getMaterialForId(s->bottomMaterial, false),
                                  s->offset[0], s->offset[1], 1, 1, 1);
        }

        Map_CreateLineDef(deMap, lineDef->v[0], lineDef->v[1], frontIdx, backIdx, 0);

        Map_GameObjectRecordProperty(deMap, "XLinedef", n, "ID", DDVT_INT, &n);
        Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Flags", DDVT_SHORT, &lineDef->flags);

        switch(_formatId)
        {
        default:
        case DOOM:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_SHORT, &lineDef->dType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Tag", DDVT_SHORT, &lineDef->dTag);
            break;

        case DOOM64:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "DrawFlags", DDVT_BYTE, &lineDef->d64drawFlags);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "TexFlags", DDVT_BYTE, &lineDef->d64texFlags);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_BYTE, &lineDef->d64type);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "UseType", DDVT_BYTE, &lineDef->d64useType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Tag", DDVT_SHORT, &lineDef->d64tag);
            break;

        case HEXEN:
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Type", DDVT_BYTE, &lineDef->xType);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg0", DDVT_BYTE, &lineDef->xArgs[0]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg1", DDVT_BYTE, &lineDef->xArgs[1]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg2", DDVT_BYTE, &lineDef->xArgs[2]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg3", DDVT_BYTE, &lineDef->xArgs[3]);
            Map_GameObjectRecordProperty(deMap, "XLinedef", n, "Arg4", DDVT_BYTE, &lineDef->xArgs[4]);
            break;
        }
    }}

    {uint n = 0;
    for(SurfaceTints::iterator i = _surfaceTints.begin(); i != _surfaceTints.end(); ++i, ++n)
    {
        SurfaceTint* surfaceTint = (*i);
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorR", DDVT_FLOAT, &surfaceTint->rgb[0]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorG", DDVT_FLOAT, &surfaceTint->rgb[1]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "ColorB", DDVT_FLOAT, &surfaceTint->rgb[2]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX0", DDVT_BYTE, &surfaceTint->xx[0]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX1", DDVT_BYTE, &surfaceTint->xx[1]);
        Map_GameObjectRecordProperty(deMap, "Light", n, "XX2", DDVT_BYTE, &surfaceTint->xx[2]);
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
    for(Things::iterator i = _things.begin(); i != _things.end(); ++i, ++n)
    {
        Thing* thing = (*i);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "X", DDVT_SHORT, &thing->pos[0]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Y", DDVT_SHORT, &thing->pos[1]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Z", DDVT_SHORT, &thing->pos[2]);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Angle", DDVT_ANGLE, &thing->angle);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "DoomEdNum", DDVT_SHORT, &thing->doomEdNum);
        Map_GameObjectRecordProperty(deMap, "Thing", n, "Flags", DDVT_INT, &thing->flags);

        if(_formatId == DOOM64)
        {
            Map_GameObjectRecordProperty(deMap, "Thing", n, "ID", DDVT_SHORT, &thing->d64TID);
        }
        else if(_formatId == HEXEN)
        {
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Special", DDVT_BYTE, &thing->xSpecial);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "ID", DDVT_SHORT, &thing->xTID);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg0", DDVT_BYTE, &thing->xArgs[0]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg1", DDVT_BYTE, &thing->xArgs[1]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg2", DDVT_BYTE, &thing->xArgs[2]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg3", DDVT_BYTE, &thing->xArgs[3]);
            Map_GameObjectRecordProperty(deMap, "Thing", n, "Arg4", DDVT_BYTE, &thing->xArgs[4]);
        }
    }}

    // Let Doomsday know that we've finished with this wadmap.
    result = Map_EditEnd(deMap)? true : false;

    if(ArgExists("-verbose"))
        Con_Message("WadConverter::TransferMap: Done in %.2f seconds.\n",
                    (Sys_GetRealTime() - startTime) / 1000.0f);

    return result;
}

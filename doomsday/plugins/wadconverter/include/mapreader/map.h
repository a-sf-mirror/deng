/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef WADCONVERTER_MAP_H
#define WADCONVERTER_MAP_H

#include <vector>

#include "StringTable"

namespace wadconverter
{
    /**
     * DOOM, Hexen and DOOM64 format map reader/interpreter class.
     * Encapsulates the process of reading maps in these formats and
     * recreating them in the Doomsday-internal format by way of the
     * public map editing interface.
     *
     * @ingroup mapreader
     */
    class Map
    {
    private:
        /// Identifier used to describe the determined map data format.
        typedef uint8_t FormatId;

        /**
         * @defgroup mapformats Map Formats
         *
         * Known map format identifiers.
         */
        /*{*/
        enum {
            UNKNOWN = 0,
            DOOM = 1,
            HEXEN = 2,
            DOOM64 = 3
        };
        /*}*/

        /**
         * @defgroup maplumpidentifiers Map Lump Identifiers.
         *
         * Known map data lump identifiers.
         */
        /*{*/
        typedef enum {
            ML_INVALID = -1,
            FIRST_LUMP_TYPE,
            ML_LABEL = FIRST_LUMP_TYPE, // A separator, name, ExMx or MAPxx
            ML_THINGS, // Monsters, items..
            ML_LINEDEFS, // LineDefs, from editing
            ML_SIDEDEFS, // SideDefs, from editing
            ML_VERTEXES, // Vertices, edited and BSP splits generated
            ML_SEGS, // LineSegs, from LineDefs split by BSP
            ML_SSECTORS, // SubSectors, list of LineSegs
            ML_NODES, // BSP nodes
            ML_SECTORS, // Sectors, from editing
            ML_REJECT, // LUT, sector-sector visibility
            ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
            ML_BEHAVIOR, // ACS Scripts (compiled).
            ML_SCRIPTS, // ACS Scripts (source).
            ML_LIGHTS, // Surface color tints.
            ML_MACROS, // DOOM64 format, macro scripts.
            ML_LEAFS, // DOOM64 format, segs (close subsectors).
            ML_GLVERT, // GL vertexes
            ML_GLSEGS, // GL segs
            ML_GLSSECT, // GL subsectors
            ML_GLNODES, // GL nodes
            ML_GLPVS, // GL PVS dataset
            NUM_LUMP_TYPES
        } MapLumpId;
        /*}*/

        typedef std::vector<lumpnum_t> LumpNums;

        // Size of the map data structures in bytes in the arrived WAD format.
        enum {
            SIZEOF_64VERTEX = (4 * 2),
            SIZEOF_VERTEX = (2 * 2),
            SIZEOF_64THING = (2 * 7),
            SIZEOF_XTHING = (2 * 7 + 1 * 6),
            SIZEOF_THING = (2 * 5),
            SIZEOF_XLINEDEF = (2 * 5 + 1 * 6),
            SIZEOF_64LINEDEF = (2 * 6 + 1 * 4),
            SIZEOF_LINEDEF = (2 * 7),
            SIZEOF_64SIDEDEF = (2 * 6),
            SIZEOF_SIDEDEF = (2 * 3 + 8 * 3),
            SIZEOF_64SECTOR = (2 * 12),
            SIZEOF_SECTOR = (2 * 5 + 8 * 2),
            SIZEOF_LIGHT = (1 * 6)
        };

        typedef de::StringTable MaterialRefs;
        typedef de::StringTable::StringId MaterialRefId;
        MaterialRefs _materialRefs;

        MaterialRefId registerMaterial(const char* name, bool onPlane);
        material_t* getMaterialForId(MaterialRefId id, bool onPlane);

    private:
        /**
         * Map constructor. Concrete instances cannot be constructed by the
         * caller, instead use the static construct methods (e.g., Map::construct)
         */
        Map(LumpNums* lumpNums);

    public:
        ~Map();

        void clear();

        /**
         * Loads a map.
         */
        bool load();

        /**
         * Transfers the map to Doomsday.
         */
        bool transfer();

    private:
        /// Identifier of the archived map format.
        FormatId _formatId;

        /**
         * Lump numbers (indices into the engine-owned lump directory) which
         * we have determined to contain map data in a format known to us.
         */
        LumpNums* _lumpNums;

        /// Number of map data elements in found lumps.
        uint _numVertexes;
        uint _numSectors;
        uint _numLineDefs;
        uint _numSideDefs;
        uint _numThings;
        uint _numSurfaceTints;

        /**
         * Vertex coords.
         */
        std::vector<float> _vertexCoords; // [v0 X, vo Y, v1 X, v1 Y...]

        /**
         * Sectors.
         */
        struct Sector {
            int16_t floorHeight;
            int16_t ceilHeight;
            int16_t lightLevel;
            int16_t type;
            int16_t tag;
            MaterialRefId floorMaterial;
            MaterialRefId ceilMaterial;

            // DOOM64 format members:
            int16_t d64flags;
            uint16_t d64floorColor;
            uint16_t d64ceilColor;
            uint16_t d64unknownColor;
            uint16_t d64wallTopColor;
            uint16_t d64wallBottomColor;

            // DOOM format constructor.
            Sector(int16_t floorHeight, int16_t ceilHeight,
                   MaterialRefId floorMat, MaterialRefId ceilMat,
                   int16_t lightLevel, int16_t type, int16_t tag)
                : floorHeight(floorHeight),
                  ceilHeight(ceilHeight),
                  floorMaterial(floorMat),
                  ceilMaterial(ceilMat),
                  lightLevel(lightLevel),
                  type(type),
                  tag(tag),
                  d64flags(0),
                  d64floorColor(0),
                  d64ceilColor(0),
                  d64unknownColor(0),
                  d64wallTopColor(0),
                  d64wallBottomColor(0) {}

            // DOOM64 format constructor.
            Sector(int16_t floorHeight, int16_t ceilHeight,
                   MaterialRefId floorMat, MaterialRefId ceilMat,
                   int16_t lightLevel, int16_t type, int16_t tag,
                   int16_t d64flags, uint16_t d64floorColor,
                   uint16_t d64ceilColor, uint16_t d64unknownColor,
                   uint16_t d64wallTopColor, uint16_t d64wallBottomColor)
                : floorHeight(floorHeight),
                  ceilHeight(ceilHeight),
                  floorMaterial(floorMat),
                  ceilMaterial(ceilMat),
                  lightLevel(lightLevel),
                  type(type),
                  tag(tag),
                  d64flags(d64flags),
                  d64floorColor(d64floorColor),
                  d64ceilColor(d64ceilColor),
                  d64unknownColor(d64unknownColor),
                  d64wallTopColor(d64wallTopColor),
                  d64wallBottomColor(d64wallBottomColor) {}
        };

        typedef std::vector<Sector> Sectors;
        Sectors _sectors;

        /**
         * LineDefs.
         */
        struct LineDef {
            uint v[2];
            uint sides[2];
            int16_t flags; // MF_* flags, read from the LINEDEFS, map data lump.

            bool polyobjOwned;

            // DOOM format members:
            int16_t dType;
            int16_t dTag;

            // Hexen format members:
            byte xType;
            byte xArgs[5];

            // DOOM64 format members:
            byte d64drawFlags;
            byte d64texFlags;
            byte d64type;
            byte d64useType;
            int16_t d64tag;

            // DOOM format constructor.
            LineDef(uint v1, uint v2, uint frontSide, uint backSide, int16_t flags,
                    int16_t dType, int16_t dTag)
                : flags(flags),
                  dType(dType),
                  dTag(dTag),
                  polyobjOwned(false),
                  xType(0),
                  d64drawFlags(0),
                  d64texFlags(0),
                  d64type(0),
                  d64useType(0),
                  d64tag(0)
                { v[0] = v1; v[1] = v2; sides[0] = frontSide; sides[1] = backSide; xArgs[0] = xArgs[1] = xArgs[2] = xArgs[3] = xArgs[4] = 0; }

            // DOOM64 format constructor.
            LineDef(uint v1, uint v2, uint frontSide, uint backSide, int16_t flags,
                    byte d64drawFlags, byte d64texFlags, byte d64type,
                    byte d64useType, int16_t d64tag)
                : flags(flags),
                  d64drawFlags(d64drawFlags),
                  d64texFlags(d64texFlags),
                  d64type(d64type),
                  d64useType(d64useType),
                  d64tag(d64tag),
                  polyobjOwned(false),
                  dType(0),
                  dTag(0),
                  xType(0)
                { v[0] = v1; v[1] = v2; sides[0] = frontSide; sides[1] = backSide; xArgs[0] = xArgs[1] = xArgs[2] = xArgs[3] = xArgs[4] = 0; }

            // Hexen format constructor.
            LineDef(uint v1, uint v2, uint frontSide, uint backSide, int16_t flags,
                    byte xType, byte arg1, byte arg2, byte arg3, byte arg4,
                    byte arg5, bool polyobjOwned)
                : flags(flags),
                  xType(xType),
                  polyobjOwned(polyobjOwned),
                  dType(0),
                  dTag(0),
                  d64drawFlags(0),
                  d64texFlags(0),
                  d64type(0),
                  d64useType(0),
                  d64tag(0)
                { v[0] = v1; v[1] = v2; sides[0] = frontSide; sides[1] = backSide; xArgs[0] = arg1; xArgs[1] = arg2; xArgs[2] = arg3; xArgs[3] = arg4; xArgs[4] = arg5; }
        };

        typedef std::vector<LineDef> LineDefs;
        LineDefs _lineDefs;

        /**
         * SideDefs.
         */
        struct SideDef {
            int16_t offset[2];
            MaterialRefId topMaterial;
            MaterialRefId bottomMaterial;
            MaterialRefId middleMaterial;
            uint sectorId;

            SideDef(int16_t offX, int16_t offY, MaterialRefId topMat,
                    MaterialRefId bottomMat, MaterialRefId midMat,
                    uint sectorId)
                : topMaterial(topMat),
                  bottomMaterial(bottomMat),
                  middleMaterial(midMat),
                  sectorId(sectorId)
                { offset[0] = offX; offset[1] = offY; }
        };

        typedef std::vector<SideDef> SideDefs;
        SideDefs _sideDefs;

        /**
         * Things.
         */
        struct Thing {
            /**
             * @defgroup ThingEdNums Thing EdNums
             */
            /*{*/
            enum {
                PO_ANCHOR = 3000,
                PO_SPAWN = 3001,
                PO_SPAWNCRUSH = 3002
            };
            /*}*/

            int16_t pos[3];
            angle_t angle;
            int16_t doomEdNum; // @see ThingEdNums.
            int32_t flags;

            // Hexen format members:
            int16_t xTID;
            byte xSpecial;
            byte xArgs[5];

            // DOOM64 format members:
            int16_t d64TID;

            // DOOM format constructor
            Thing(int16_t x, int16_t y, int16_t z, angle_t angle, int16_t doomEdNum, int32_t flags)
                : angle(angle),
                  doomEdNum(doomEdNum),
                  flags(flags),
                  xTID(0),
                  xSpecial(0),
                  d64TID(0)
                { pos[0] = x; pos[1] = y; pos[2] = z; xArgs[0] = xArgs[1] = xArgs[2] = xArgs[3] = xArgs[4] = 0; }

            // DOOM64 format constructor
            Thing(int16_t x, int16_t y, int16_t z, angle_t angle, int16_t doomEdNum, int32_t flags, int16_t tid)
                : angle(angle),
                  doomEdNum(doomEdNum),
                  flags(flags),
                  xTID(0),
                  xSpecial(0),
                  d64TID(tid)
                { pos[0] = x; pos[1] = y; pos[2] = z; xArgs[0] = xArgs[1] = xArgs[2] = xArgs[3] = xArgs[4] = 0; }

            // Hexen format constructor
            Thing(int16_t x, int16_t y, int16_t z, angle_t angle, int16_t doomEdNum, int32_t flags, int16_t tid, byte special, byte arg1, byte arg2, byte arg3, byte arg4, byte arg5)
                : angle(angle),
                  doomEdNum(doomEdNum),
                  flags(flags),
                  xTID(tid),
                  d64TID(0)
                { pos[0] = x; pos[1] = y; pos[2] = z; xArgs[0] = arg1; xArgs[1] = arg2; xArgs[2] = arg3; xArgs[3] = arg4; xArgs[4] = arg5; }
        };

        typedef std::vector<Thing> Things;
        Things _things;

        /**
         * Polyobjs (Hexen only).
         */
        struct Polyobj {
            uint idx; // Idx of polyobject
            uint* lineIndices;
            uint lineCount;
            int tag; // Reference tag assigned in HereticEd
            int seqType;
            int16_t anchor[2];

            Polyobj(uint idx, uint* lineIndices, uint lineCount, int tag, int seqType, int16_t anchorX, int16_t anchorY)
                : idx(idx),
                  lineIndices(lineIndices),
                  lineCount(lineCount),
                  tag(tag),
                  seqType(seqType)
                { anchor[0] = anchorX; anchor[1] = anchorY; }
        };

        typedef std::vector<Polyobj> Polyobjs;
        Polyobjs _polyobjs;
        uint _numPolyobjs;

        /**
         * Surface color tints (DOOM64 only).
         */
        struct SurfaceTint {
            float rgb[3];
            byte xx[3];

            SurfaceTint(float red, float green, float blue, byte x1, byte x2, byte x3)
                { rgb[0] = red; rgb[1] = green, rgb[2] = blue; xx[0] = x1; xx[1] = x2; xx[2] = x3; }
        };

        typedef std::vector<SurfaceTint> SurfaceTints;
        SurfaceTints _surfaceTints;

        bool loadVertexes(const byte* buf, size_t len);
        bool loadLinedefs(const byte* buf, size_t len);
        bool loadSidedefs(const byte* buf, size_t len);
        bool loadSectors(const byte* buf, size_t len);
        bool loadThings(const byte* buf, size_t len);
        bool loadLights(const byte* buf, size_t len);

        void findPolyobjs();
        bool findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY);
        bool iterFindPolyLines(int16_t x, int16_t y, int16_t polyStart[2], uint* polyLineCount, LineDef** lineList);
        void createPolyobj(LineDef** lineList, uint num, int tag, int sequenceType, int16_t anchorX, int16_t anchorY);

    public:
        static Map* construct(const char* mapID);

        /**
         * Given a lump name return the expected data identifier.
         *
         * @param name  Name of the lump to lookup.
         */
        static MapLumpId dataTypeForLumpName(const char* name);

    private:
        static FormatId recognize(const LumpNums& lumpNums);
        static int isClosedPolygon(LineDef** lineList, size_t num);
    };
}

#endif /* WADCONVERTER_MAP_H */

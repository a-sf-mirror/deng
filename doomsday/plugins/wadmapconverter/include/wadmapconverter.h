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
 * wadmapconverter.h: Doomsday plugin for converting DOOM-like format maps.
 *
 * The purpose of a mapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 */

#ifndef __MAPCONVERTER_H__
#define __MAPCONVERTER_H__

#include "dd_types.h"

// Vertex indices.
enum { VX, VY, VZ };

// Line sides.
#define RIGHT                   0
#define LEFT                    1

#define VERBOSE(code)   { if(verbose) { code; } }

typedef struct materialref_s {
    char            name[9];
    material_t*     material; // Doomsday's unique identifier for this.
    uint            refCount;
} materialref_t;

typedef struct mside_s {
    int16_t         offset[2];
    const materialref_t* topMaterial;
    const materialref_t* bottomMaterial;
    const materialref_t* middleMaterial;
    uint            sector;
} mside_t;

// Line flags
#define LAF_POLYOBJ             0x1 // Line is from a polyobject.

typedef struct mline_s {
    uint            v[2];
    uint            sides[2];
    int16_t         flags; // MF_* flags, read from the LINEDEFS, map data lump.

    // Analysis data:
    int16_t         aFlags;

    // DOOM format members:
    int16_t         dType;
    int16_t         dTag;

    // Hexen format members:
    byte            xType;
    byte            xArgs[5];

    // DOOM64 format members:
    byte            d64drawFlags;
    byte            d64texFlags;
    byte            d64type;
    byte            d64useType;
    int16_t         d64tag;
} mline_t;

typedef struct msector_s {
    int16_t         floorHeight;
    int16_t         ceilHeight;
    int16_t         lightLevel;
    int16_t         type;
    int16_t         tag;
    const materialref_t* floorMaterial;
    const materialref_t* ceilMaterial;

    // DOOM64 format members:
    int16_t         d64flags;
    uint16_t        d64floorColor;
    uint16_t        d64ceilingColor;
    uint16_t        d64unknownColor;
    uint16_t        d64wallTopColor;
    uint16_t        d64wallBottomColor;
} msector_t;

typedef struct mthing_s {
    int16_t         pos[3];
    angle_t         angle;
    int16_t         doomEdNum;
    int32_t         flags;

    // Hexen format members:
    int16_t         xTID;
    byte            xSpecial;
    byte            xArgs[5];

    // DOOM64 format members:
    int16_t         d64TID;
} mthing_t;

// Hexen only (at present):
typedef struct mpolyobj_s {
    uint            idx; // Idx of polyobject
    uint            lineCount;
    uint*           lineIndices;
    int             tag; // Reference tag assigned in HereticEd
    int             seqType;
    int16_t         anchor[2];
} mpolyobj_t;

// DOOM64 only (at present):
typedef struct mlight_s {
    float           rgb[3];
    byte            xx[3];
} surfacetint_t;

typedef enum {
    MF_DOOM = 0,
    MF_HEXEN,
    MF_DOOM64
} mapformatid_t;

typedef struct wadmap_s {
    char            name[9];
    uint            numVertexes;
    uint            numSectors;
    uint            numLines;
    uint            numSides;
    uint            numPolyobjs;
    uint            numThings;
    uint            numLights;

    float*          vertexes; // Array of vertex coords [v0 X, vo Y, v1 X, v1 Y...]
    msector_t*      sectors;
    mline_t*        lines;
    mside_t*        sides;
    mthing_t*       things;
    mpolyobj_t**    polyobjs;
    surfacetint_t*  lights;

    size_t          numFlats, numUnknownFlats;
    materialref_t** flats;
    size_t          numTextures, numUnknownTextures;
    materialref_t** textures;

    mapformatid_t   format;

    byte*           rejectMatrix;
    void*           blockMap;
} wadmap_t;

extern wadmap_t* wadmap;
extern boolean verbose;

boolean         IsSupportedFormat(const int* lumpList, int numLumps);

boolean         LoadMap(const int* lumpList, int numLumps);
void            AnalyzeMap(void);
boolean         TransferMap();

void            LoadANIMATED(void);
void            LoadANIMDEFS(void);

extern char* sc_String;
extern int sc_Number;
extern int sc_LineNumber;
extern char sc_ScriptName[];

void SC_Open(const char* name);
void SC_OpenLump(lumpnum_t lump);
void SC_OpenFile(const char* name);
void SC_OpenFileCLib(const char* name);
void SC_Close(void);
boolean SC_GetString(void);
void SC_MustGetString(void);
void SC_MustGetStringName(char* name);
boolean SC_GetNumber(void);
void SC_MustGetNumber(void);
void SC_UnGet(void);
void SC_SkipToStartOfNextLine(void);
void SC_ScriptError(char* message);
boolean SC_Compare(char* text);

#endif

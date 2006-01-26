/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *                    Daniel Swanson <danij@users.sourceforge.net>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_arch.h: Doomsday Archived Map (DAM) reader
 *
 * Engine-internal header for DAM.
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_H__
#define __DOOMSDAY_ARCHIVED_MAP_H__

// number of map data lumps for a level
#define NUM_MAPLUMPS 11

// well, there is GL_PVIS too but we arn't interested in that
#define NUM_GLLUMPS 4

enum {
    mlThings,
    mlLineDefs,
    mlSideDefs,
    mlVertexes,
    mlSegs,
    mlSSectors,
    mlNodes,
    mlSectors,
    mlReject,
    mlBlockMap,
    mlBehavior,
    glVerts,
    glSegs,
    glSSects,
    glNodes,
    NUM_LUMPCLASSES
};

//
// Types used in map data handling
//
typedef struct {
    int property; // DAM property to map the data to
    int gameprop; // if > 0 is a specific data item (passed to the game)
    int flags;
    int size;   // num of bytes
    int offset;
} datatype_t;

typedef struct mapdatalumpformat_s {
    int   version;
    char *magicid;
    size_t elmSize;
    int   numValues;
    datatype_t *values;
} mapdatalumpformat_t;

typedef struct mapdataformat_s {
    char *vername;
    mapdatalumpformat_t verInfo[NUM_MAPLUMPS];
    boolean supported;
} mapdataformat_t;

typedef struct glnodeformat_s {
    char *vername;
    mapdatalumpformat_t verInfo[NUM_GLLUMPS];
    boolean supported;
} glnodeformat_t;

typedef struct {
    char *lumpname;
    void (*func) (int startIndex, int dataType, const byte *buffer,
                  size_t elmSize, int elements, int version, int values,
                  const datatype_t *types);
    int  mdLump;
    int  glLump;
    int  dataType;
    int  lumpclass;
    boolean required;
    boolean precache;
} maplumpinfo_t;

void        P_InitMapDataFormats(void);
boolean     P_LoadMapData(char *levelId);
#endif

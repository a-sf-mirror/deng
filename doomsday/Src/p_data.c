/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * p_data.c: Playsim Data Structures, Macros and Constants
 *
 * The Map Update API is implemented here.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

#include "rend_bias.h"

// MACROS ------------------------------------------------------------------

// Maximum values in a vector.
#define MAX_VALUES 4

// TYPES -------------------------------------------------------------------

enum // Value types.
{
    VT_BOOL,
    VT_BYTE,
    VT_SHORT,
    VT_INT,    // 32 or 64
    VT_FIXED,
    VT_ANGLE,
    VT_FLOAT,
    VT_PTR,
    VT_FLAT_INDEX
};

typedef struct setargs_s {
    int type;
    int prop;
    int valueType;
    boolean* booleanValues;
    byte* byteValues;
    int* intValues;
    fixed_t* fixedValues;
    float* floatValues;
    angle_t* angleValues;
    void* ptrValue;
} setargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     numvertexes;
byte   *vertexes;

int     numsegs;
byte   *segs;

int     numsectors;
byte   *sectors;

int     numsubsectors;
byte   *subsectors;

int     numnodes;
byte   *nodes;

int     numlines;
byte   *lines;

int     numsides;
byte   *sides;

short  *blockmaplump;			// offsets in blockmap are from here
short  *blockmap;
int     bmapwidth, bmapheight;	// in mapblocks
fixed_t bmaporgx, bmaporgy;		// origin of block map
linkmobj_t *blockrings;			// for thing rings
byte   *rejectmatrix;			// for fast sight rejection
polyblock_t **polyblockmap;		// polyobj blockmap
nodepile_t thingnodes, linenodes;	// all kinds of wacky links

ded_mapinfo_t *mapinfo = 0;		// Current mapinfo.
fixed_t mapgravity;				// Gravity for the current map.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Convert pointer to index. 
 */
int P_ToIndex(int type, void* ptr)
{
    switch(type)
    {
    case DMU_VERTEX:
        return GET_VERTEX_IDX(ptr);
        
    case DMU_SEG:
        return GET_SEG_IDX(ptr);
        
    case DMU_LINE:  
        return GET_LINE_IDX(ptr);
        
    case DMU_SIDE:
        return GET_SIDE_IDX(ptr);

    case DMU_SUBSECTOR:
        return GET_SUBSECTOR_IDX(ptr);
        
    case DMU_SECTOR:
        return GET_SECTOR_IDX(ptr);
        
    case DMU_POLYOBJ:
        return GET_POLYOBJ_IDX(ptr);
        
    case DMU_NODE:
        return GET_NODE_IDX(ptr);

    default:
        Con_Error("P_ToIndex: unknown type %i.\n", type);
    }
    return -1;
}

/*
 * Convert index to pointer.
 */
void* P_ToPtr(int type, int index)
{
 switch(type)
    {
    case DMU_VERTEX:
        return VERTEX_PTR(index);
        
    case DMU_SEG:
        return SEG_PTR(index);
        
    case DMU_LINE:  
        return LINE_PTR(index);
        
    case DMU_SIDE:
        return SIDE_PTR(index);

    case DMU_SUBSECTOR:
        return SUBSECTOR_PTR(index);
        
    case DMU_SECTOR:
        return SECTOR_PTR(index);
        
    case DMU_POLYOBJ:
        return PO_PTR(index);
        
    case DMU_NODE:
        return NODE_PTR(index);

    default:
        Con_Error("P_ToPtr: unknown type %i.\n", type);
    }
    return NULL;
}

/*
 * Call a callback function on a selection of map data objects. The selected
 * objects will be specified by 'type' and 'index'. 'context' is passed to the
 * callback function along with a pointer to the data object. P_Callback 
 * returns true if all the calls to the callback function return true. False
 * is returned when the callback function returns false; in this case, the 
 * iteration is aborted immediately when the callback function returns false.
 */
int P_Callback(int type, int index, void* context, int (*callback)(void* p, void* ctx))
{
    int i;

    switch(type)
    {
    case DMU_VERTEX:
        if(index >= 0 && index < numvertexes)
        {
            return callback(VERTEX_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numvertexes; ++i)
                if(!callback(VERTEX_PTR(i), context)) return false;
        }
        break;
        
    case DMU_SEG:
        if(index >= 0 && index < numsegs)
        {
            return callback(SEG_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsegs; ++i)
                if(!callback(SEG_PTR(i), context)) return false;
        }
        break;
        
    case DMU_LINE:
        if(index >= 0 && index < numlines)
        {
            return callback(LINE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numlines; ++i)
                if(!callback(LINE_PTR(i), context)) return false;
        }
        break;
        
    case DMU_SIDE:
        if(index >= 0 && index < numsides)
        {
            return callback(SIDE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsides; ++i)
                if(!callback(SIDE_PTR(i), context)) return false;
        }
        break;

    case DMU_NODE:
        if(index >= 0 && index < numnodes)
        {
            return callback(NODE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numnodes; ++i)
                if(!callback(NODE_PTR(i), context)) return false;
        }
        break;

    case DMU_SUBSECTOR:
        if(index >= 0 && index < numsubsectors)
        {
            return callback(SUBSECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsubsectors; ++i)
                if(!callback(SUBSECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_SECTOR:
        if(index >= 0 && index < numsectors)
        {
            return callback(SECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsectors; ++i)
                if(!callback(SECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_LINE_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINE_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG:
        Con_Error("P_Callback: Type %i not implemented yet.\n", type);
        /*
        for(i = 0; i < numlines; ++i)
        {
            if(!callback(LINE_PTR(i), context)) return false;
        }
        */
        break;

    default:
        Con_Error("P_Callback: Type %i unknown (index %i).\n", type, index);
    }
    return true;
}

/*
 * Another version of callback iteration. The set of selected objects is 
 * determined by 'type' and 'ptr'. Otherwise works like P_Callback.
 */
int P_Callbackp(int type, void* ptr, void* context, int (*callback)(void* p, void* ctx))
{
    return true;
}

/*
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void SetValue(int valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == VT_FIXED)
    {
        fixed_t* d = dst;
        
        switch(args->valueType)
        {
        case VT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case VT_INT:
            *d = (args->intValues[index] << FRACBITS); 
            break;
        case VT_FIXED:
            *d = args->fixedValues[index];
            break;
        case VT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: VT_FIXED incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_FLOAT)
    {
        float* d = dst;
        
        switch(args->valueType)
        {
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: VT_FLOAT incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_BOOL)
    {
        boolean* d = dst;
        
        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: VT_BOOL incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_BYTE)
    {
        byte* d = dst;
        
        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: VT_BYTE incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_INT)
    {
        int* d = dst;
        
        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        case VT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: VT_INT incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_SHORT || valueType == VT_FLAT_INDEX)
    {
        short* d = dst;
        
        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        case VT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: VT_SHORT incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_ANGLE)
    {
        angle_t* d = dst;
        
        switch(args->valueType)
        {
        case VT_ANGLE:
            *d = (angle_t) args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: VT_ANGLE incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else if(valueType == VT_PTR)
    {
        void** d = dst;
    
        switch(args->valueType)
        {
        case VT_PTR:
            *d = args->ptrValue;
            break;
        default:
            Con_Error("SetValue: VT_PTR incompatible with value type %i.\n", 
                      args->valueType);
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %i.\n", valueType);
    }
}

/*
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a 
 * fatal error.
 */
static int SetProperty(void* ptr, void* context)
{
    setargs_t* args = (setargs_t*) context;
    
    switch(args->type)
    {
    case DMU_VERTEX:
        {
        // Vertices are not writable through DMU.
        /*        
        vertex_t* p = ptr;
        switch(args->prop)
        {
        case DMU_X:
            SetValue(VT_FIXED, &p->x, args, 0);
            break;
        case DMU_Y:
            SetValue(VT_FIXED, &p->y, args, 0);
            break;
        case DMU_XY:
            SetValue(VT_FIXED, &p->x, args, 0);
            SetValue(VT_FIXED, &p->y, args, 1);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_VERTEX.\n", 
                      args->prop);
        }
        */
        Con_Error("SetProperty: DMU_VERTEX is not writable.\n");
        break;
        }
        
    case DMU_SEG:
        {
        seg_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_BYTE, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_SEG.\n",
                      args->prop);
        }
        break;
        }
        
    case DMU_LINE:  
        {
        line_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_SHORT, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_LINE.\n",
                      args->prop);
        }
        break;
        }
        
    case DMU_SIDE:
        {
        side_t* p = ptr;
        switch(args->prop)
        {
        case DMU_TEXTURE_OFFSET_X:
            SetValue(VT_FIXED, &p->textureoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_Y:
            SetValue(VT_FIXED, &p->rowoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_XY:
            SetValue(VT_FIXED, &p->textureoffset, args, 0);
            SetValue(VT_FIXED, &p->rowoffset, args, 1);
            break;
        case DMU_TOP_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->toptexture, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->midtexture, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->bottomtexture, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_SUBSECTOR.\n",
                      args->prop);
        }
        break;
        }

    case DMU_SUBSECTOR:
        {
        seg_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_BYTE, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_SUBSECTOR.\n",
                      args->prop);
        }
        break;
        }
        
    case DMU_SECTOR:
        {
        sector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_COLOR:
            SetValue(VT_BYTE, &p->rgb[0], args, 0);
            SetValue(VT_BYTE, &p->rgb[1], args, 1);
            SetValue(VT_BYTE, &p->rgb[2], args, 2);
            break;
        case DMU_LIGHT_LEVEL:
            SetValue(VT_SHORT, &p->lightlevel, args, 0);
            break;
        case DMU_FLOOR_HEIGHT:
            SetValue(VT_FIXED, &p->floorheight, args, 0);
            break;
        case DMU_FLOOR_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->floorpic, args, 0);
            break;
        case DMU_FLOOR_OFFSET_X:
            SetValue(VT_FLOAT, &p->flooroffx, args, 0);
            break;
        case DMU_FLOOR_OFFSET_Y:
            SetValue(VT_FLOAT, &p->flooroffy, args, 0);
            break;
        case DMU_FLOOR_OFFSET_XY:
            SetValue(VT_FLOAT, &p->flooroffx, args, 0);
            SetValue(VT_FLOAT, &p->flooroffy, args, 1);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_X:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_Y:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[1], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_XY:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[1], args, 1);
            break;
        case DMU_FLOOR_TARGET:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].target, args, 0);
            break;
        case DMU_FLOOR_SPEED:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].speed, args, 0);
            break;
        case DMU_CEILING_HEIGHT:
            SetValue(VT_FIXED, &p->ceilingheight, args, 0);
            break;
        case DMU_CEILING_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->ceilingpic, args, 0);
            break;
        case DMU_CEILING_OFFSET_X:
            SetValue(VT_FLOAT, &p->ceiloffx, args, 0);
            break;
        case DMU_CEILING_OFFSET_Y:
            SetValue(VT_FLOAT, &p->ceiloffy, args, 0);
            break;
        case DMU_CEILING_OFFSET_XY:
            SetValue(VT_FLOAT, &p->ceiloffx, args, 0);
            SetValue(VT_FLOAT, &p->ceiloffy, args, 1);
            break;
        case DMU_CEILING_TEXTURE_MOVE_X:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[0], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_Y:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[1], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_XY:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[0], args, 0);
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[1], args, 1);
            break;
        case DMU_CEILING_TARGET:
            SetValue(VT_INT, &p->planes[PLN_CEILING].target, args, 0);
            break;
        case DMU_CEILING_SPEED:
            SetValue(VT_INT, &p->planes[PLN_CEILING].speed, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %i is not writable in DMU_SEG.\n",
                      args->prop);
        }
        // TODO: Notify the relevant subsystems.
        break;
        }
        
    case DMU_POLYOBJ:
        Con_Error("SetProperty: Property %i is not writable in DMU_POLYOBJ.\n",
                  args->prop);
        break;
        
    case DMU_NODE:
        Con_Error("SetProperty: Property %i is not writable in DMU_NODE.\n",
                  args->prop);
        break;

    default:
        Con_Error("SetProperty: Type %i unknown.\n", args->type);
    }
    // Continue iteration.
    return true;
}

void P_SetBool(int type, int index, int prop, boolean param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetByte(int type, int index, int prop, byte param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BYTE;
    args.byteValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetInt(int type, int index, int prop, int param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_INT;
    args.intValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixed(int type, int index, int prop, fixed_t param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FIXED;
    args.fixedValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetAngle(int type, int index, int prop, angle_t param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_ANGLE;
    args.angleValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloat(int type, int index, int prop, float param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FLOAT;
    args.floatValues = &param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtr(int type, int index, int prop, void* param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_PTR;
    args.ptrValue = param;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BOOL;
    args.booleanValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BYTE;
    args.byteValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_INT;
    args.intValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FIXED;
    args.fixedValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_ANGLE;
    args.angleValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FLOAT;
    args.floatValues = params;

    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_PTR;
    args.ptrValue = params;

    P_Callback(type, index, &args, SetProperty);
}

/* pointer-based write functions */
    
void P_SetBoolp(int type, void* ptr, int prop, boolean param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytep(int type, void* ptr, int prop, byte param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BYTE;
    args.byteValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntp(int type, void* ptr, int prop, int param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_INT;
    args.intValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedp(int type, void* ptr, int prop, fixed_t param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FIXED;
    args.fixedValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglep(int type, void* ptr, int prop, angle_t param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_ANGLE;
    args.angleValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatp(int type, void* ptr, int prop, float param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FLOAT;
    args.floatValues = &param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrp(int type, void* ptr, int prop, void* param)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_PTR;
    args.ptrValue = param;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBoolpv(int type, void* ptr, int prop, boolean* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BOOL;
    args.booleanValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytepv(int type, void* ptr, int prop, byte* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_BYTE;
    args.byteValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntpv(int type, void* ptr, int prop, int* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_INT;
    args.intValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedpv(int type, void* ptr, int prop, fixed_t* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FIXED;
    args.fixedValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglepv(int type, void* ptr, int prop, angle_t* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_ANGLE;
    args.angleValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatpv(int type, void* ptr, int prop, float* params)
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_FLOAT;
    args.floatValues = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrpv(int type, void* ptr, int prop, void* params)    
{
    setargs_t args;
    args.type = type;
    args.prop = prop;
    args.valueType = VT_PTR;
    args.ptrValue = params;

    P_Callbackp(type, ptr, &args, SetProperty);
}

    /* index-based read functions */

    boolean         P_GetBool(int type, int index, int prop);
    byte            P_GetByte(int type, int index, int prop);
    int             P_GetInt(int type, int index, int prop);
    fixed_t         P_GetFixed(int type, int index, int prop);
    angle_t         P_GetAngle(int type, int index, int prop);
    float           P_GetFloat(int type, int index, int prop);
    void*           P_GetPtr(int type, int index, int prop);

    void            P_GetBoolv(int type, int index, int prop, boolean* params);
    void            P_GetBytev(int type, int index, int prop, byte* params);
    void            P_GetIntv(int type, int index, int prop, int* params);
    void            P_GetFixedv(int type, int index, int prop, fixed_t* params);
    void            P_GetAnglev(int type, int index, int prop, angle_t* params);
    void            P_GetFloatv(int type, int index, int prop, float* params);
    void            P_GetPtrv(int type, int index, int prop, void* params);
    
    /* pointer-based read functions */
    boolean         P_GetBoolp(int type, void* ptr, int prop);
    byte            P_GetBytep(int type, void* ptr, int prop);
    int             P_GetIntp(int type, void* ptr, int prop);
    fixed_t         P_GetFixedp(int type, void* ptr, int prop);
    angle_t         P_GetAnglep(int type, void* ptr, int prop);
    float           P_GetFloatp(int type, void* ptr, int prop);
    void*           P_GetPtrp(int type, void* ptr, int prop);

    void            P_GetBoolpv(int type, void* ptr, int prop, boolean* params);
    void            P_GetBytepv(int type, void* ptr, int prop, byte* params);
    void            P_GetIntpv(int type, void* ptr, int prop, int* params);
    void            P_GetFixedpv(int type, void* ptr, int prop, fixed_t* params);
    void            P_GetAnglepv(int type, void* ptr, int prop, angle_t* params);
    void            P_GetFloatpv(int type, void* ptr, int prop, float* params);
    void            P_GetPtrpv(int type, void* ptr, int prop, void* params);


//===========================================================================
// P_ValidateLevel
//  Make sure all texture references in the level data are good.
//===========================================================================
void P_ValidateLevel(void)
{
	int     i;

	for(i = 0; i < numsides; i++)
	{
		side_t *side = SIDE_PTR(i);

		if(side->toptexture > numtextures - 1)
			side->toptexture = 0;
		if(side->midtexture > numtextures - 1)
			side->midtexture = 0;
		if(side->bottomtexture > numtextures - 1)
			side->bottomtexture = 0;
	}
}

//==========================================================================
// P_LoadBlockMap
//==========================================================================
void P_LoadBlockMap(int lump)
{
	int     i, count;

	/*
	   // Disabled for now: Plutonia MAP28.
	   if(W_LumpLength(lump) > 0x7fff)  // From GMJ.
	   Con_Error("Invalid map - Try using this with Boomsday.\n");
	 */

	blockmaplump = W_CacheLumpNum(lump, PU_LEVEL);
	blockmap = blockmaplump + 4;
	count = W_LumpLength(lump) / 2;
	for(i = 0; i < count; i++)
		blockmaplump[i] = SHORT(blockmaplump[i]);

	bmaporgx = blockmaplump[0] << FRACBITS;
	bmaporgy = blockmaplump[1] << FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

	// Clear out mobj rings.
	count = sizeof(*blockrings) * bmapwidth * bmapheight;
	blockrings = Z_Malloc(count, PU_LEVEL, 0);
	memset(blockrings, 0, count);
	for(i = 0; i < bmapwidth * bmapheight; i++)
		blockrings[i].next = blockrings[i].prev = (mobj_t *) &blockrings[i];
}

/* 
   //==========================================================================
   // P_LoadBlockMap
   //   Modified for long blockmap/blockmaplump defs - GMJ Sep 2001
   //   I want to think about this a bit more... -jk
   //==========================================================================
   void P_LoadBlockMap(int lump)
   {
   long i, count = W_LumpLength(lump)/2;
   short *wadblockmaplump;

   if (count >= 0x10000)
   Con_Error("Map exceeds limits of +/- 32767 map units");

   wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
   blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

   // killough 3/1/98: Expand wad blockmap into larger internal one,
   // by treating all offsets except -1 as unsigned and zero-extending
   // them. This potentially doubles the size of blockmaps allowed,
   // because Doom originally considered the offsets as always signed.

   blockmaplump[0] = SHORT(wadblockmaplump[0]);
   blockmaplump[1] = SHORT(wadblockmaplump[1]);
   blockmaplump[2] = (long)(SHORT(wadblockmaplump[2])) & 0xffff;
   blockmaplump[3] = (long)(SHORT(wadblockmaplump[3])) & 0xffff;

   for (i=4 ; i<count ; i++)
   {
   short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
   blockmaplump[i] = t == -1 ? -1l : (long) t & 0xffff;
   }

   Z_Free(wadblockmaplump);

   bmaporgx = blockmaplump[0]<<FRACBITS;
   bmaporgy = blockmaplump[1]<<FRACBITS;
   bmapwidth = blockmaplump[2];
   bmapheight = blockmaplump[3];

   // Clear out mobj rings.
   count = sizeof(*blockrings) * bmapwidth * bmapheight;
   blockrings = Z_Malloc(count, PU_LEVEL, 0);
   memset(blockrings, 0, count);
   blockmap = blockmaplump + 4;
   for(i = 0; i < bmapwidth * bmapheight; i++)
   blockrings[i].next = blockrings[i].prev = (mobj_t*) &blockrings[i];
   } */

/*
 * Load the REJECT data lump.
 */
void P_LoadReject(int lump)
{
	rejectmatrix = W_CacheLumpNum(lump, PU_LEVEL);

	// If no reject matrix is found, issue a warning.
	if(rejectmatrix == NULL)
	{
		Con_Message("P_LoadReject: No REJECT data found.\n");
	}
}

void P_PlaneChanged(sector_t *sector, boolean theCeiling)
{
    int i, k;
    subsector_t *sub;
    seg_t *seg;
    
    // FIXME: Find a better way to find the subsectors of a sector.
    
    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);

        // Only the subsectors of the changed sector.
        if(sub->sector != sector)
            continue;

        for(k = 0; k < sub->linecount; ++k)
        {
            seg = SEG_PTR(k + sub->firstline);

            // Inform the shadow bias of changed geometry.
            SB_SegHasMoved(seg);
        }

        // Inform the shadow bias of changed geometry.
        SB_PlaneHasMoved(sub, theCeiling);
    }
}

/*
 * When a change is made, this must be called to inform the engine of
 * it.  Repercussions include notifications to the renderer, network...
 */
void P_FloorChanged(sector_t *sector)
{
    P_PlaneChanged(sector, false);
}

void P_CeilingChanged(sector_t *sector)
{
    P_PlaneChanged(sector, true);
}

void P_PolyobjChanged(polyobj_t *po)
{
    seg_t **seg = po->segs;
    int i;

    for(i = 0; i < po->numsegs; ++i, ++seg)
    {
        // Shadow bias must be told.
        SB_SegHasMoved(*seg);
    }
}

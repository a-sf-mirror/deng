/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Doomsday Map Update API
 *
 * The Map Update API is used for accessing and making changes to map data
 * during gameplay. From here, the relevant engine's subsystems will be
 * notified of changes in the map data they use, thus allowing them to
 * update their status whenever needed.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct dummyline_s {
    linedef_t       line; // Line data.
    void*           extraData; // Pointer to user data.
    boolean         inUse; // true, if the dummy is being used.
} dummyline_t;

typedef struct dummysector_s {
    sector_t        sector; // Sector data.
    void           *extraData; // Pointer to user data.
    boolean         inUse; // true, if the dummy is being used.
} dummysector_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint dummyCount = 8; // Number of dummies to allocate (per type).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dummyline_t* dummyLines;
static dummysector_t* dummySectors;

static int usingDMUAPIver; // Version of the DMU API the game expects.

// CODE --------------------------------------------------------------------

/**
 * Convert DMU enum constant into a string for error/debug messages.
 */
const char* DMU_Str(uint prop)
{
    static char         propStr[40];

    struct prop_s {
        uint prop;
        const char* str;
    } props[] =
    {
        { DMU_NONE, "(invalid)" },
        { DMU_VERTEX, "DMU_VERTEX" },
        { DMU_HEDGE, "DMU_HEDGE" },
        { DMU_LINEDEF, "DMU_LINEDEF" },
        { DMU_SIDEDEF, "DMU_SIDEDEF" },
        { DMU_NODE, "DMU_NODE" },
        { DMU_SUBSECTOR, "DMU_SUBSECTOR" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_PLANE, "DMU_PLANE" },
        { DMU_MATERIAL, "DMU_MATERIAL" },
        { DMU_LINEDEF_BY_TAG, "DMU_LINEDEF_BY_TAG" },
        { DMU_SECTOR_BY_TAG, "DMU_SECTOR_BY_TAG" },
        { DMU_LINEDEF_BY_ACT_TAG, "DMU_LINEDEF_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_X, "DMU_X" },
        { DMU_Y, "DMU_Y" },
        { DMU_XY, "DMU_XY" },
        { DMU_NORMAL_X, "DMU_NORMAL_X" },
        { DMU_NORMAL_Y, "DMU_NORMAL_Y" },
        { DMU_NORMAL_Z, "DMU_NORMAL_Z" },
        { DMU_NORMAL_XYZ, "DMU_NORMAL_XYZ" },
        { DMU_VERTEX0, "DMU_VERTEX0" },
        { DMU_VERTEX1, "DMU_VERTEX1" },
        { DMU_FRONT_SECTOR, "DMU_FRONT_SECTOR" },
        { DMU_BACK_SECTOR, "DMU_BACK_SECTOR" },
        { DMU_SIDEDEF0, "DMU_SIDEDEF0" },
        { DMU_SIDEDEF1, "DMU_SIDEDEF1" },
        { DMU_FLAGS, "DMU_FLAGS" },
        { DMU_DX, "DMU_DX" },
        { DMU_DY, "DMU_DY" },
        { DMU_DXY, "DMU_DXY" },
        { DMU_LENGTH, "DMU_LENGTH" },
        { DMU_SLOPE_TYPE, "DMU_SLOPE_TYPE" },
        { DMU_ANGLE, "DMU_ANGLE" },
        { DMU_OFFSET, "DMU_OFFSET" },
        { DMU_OFFSET_X, "DMU_OFFSET_X" },
        { DMU_OFFSET_Y, "DMU_OFFSET_Y" },
        { DMU_OFFSET_XY, "DMU_OFFSET_XY" },
        { DMU_BLENDMODE, "DMU_BLENDMODE" },
        { DMU_VALID_COUNT, "DMU_VALID_COUNT" },
        { DMU_LINEDEF_COUNT, "DMU_LINEDEF_COUNT" },
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_COLOR_RED, "DMU_COLOR_RED" },
        { DMU_COLOR_GREEN, "DMU_COLOR_GREEN" },
        { DMU_COLOR_BLUE, "DMU_COLOR_BLUE" },
        { DMU_ALPHA, "DMU_ALPHA" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMT_MOBJS, "DMT_MOBJS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_SOUND_ORIGIN, "DMU_SOUND_ORIGIN" },
        { DMU_WIDTH, "DMU_WIDTH" },
        { DMU_HEIGHT, "DMU_HEIGHT" },
        { DMU_TARGET_HEIGHT, "DMU_TARGET_HEIGHT" },
        { DMU_SEG_COUNT, "DMU_SEG_COUNT" },
        { DMU_SPEED, "DMU_SPEED" },
        { DMU_NAMESPACE, "DMU_NAMESPACE" },
        { 0, NULL }
    };
    uint                i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

/**
 * Determines the type of the map data object.
 *
 * @param ptr  Pointer to a map data object.
 */
static int DMU_GetType(const void* ptr)
{
    int                 type;

    type = P_DummyType((void*)ptr);
    if(type != DMU_NONE)
        return type;

    type = ((const runtime_mapdata_header_t*)ptr)->type;

    // Make sure it's valid.
    switch(type)
    {
        case DMU_VERTEX:
        case DMU_HEDGE:
        case DMU_LINEDEF:
        case DMU_SIDEDEF:
        case DMU_SUBSECTOR:
        case DMU_SECTOR:
        case DMU_PLANE:
        case DMU_NODE:
        case DMU_MATERIAL:
            return type;

        default:
            // Unknown.
            break;
    }
    return DMU_NONE;
}

/**
 * Initializes a setargs struct.
 *
 * @param type          Type of the map data object.
 * @param args          Ptr to setargs struct to be initialized.
 * @param prop          Property of the map data object.
 */
static void initArgs(setargs_t* args, int type, uint prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop & ~DMU_FLAG_MASK;
    args->modifiers = prop & DMU_FLAG_MASK;
}

/**
 * Initializes the dummy arrays with a fixed number of dummies.
 */
void P_InitMapUpdate(void)
{
    // Request the DMU API version the game is expecting.
    usingDMUAPIver = gx.GetInteger(DD_GAME_DMUAPI_VER);
    if(!usingDMUAPIver)
        Con_Error("P_InitMapUpdate: Game dll is not compatible with "
                  "Doomsday " DOOMSDAY_VERSION_TEXT ".");

    if(usingDMUAPIver > DMUAPI_VER)
        Con_Error("P_InitMapUpdate: Game dll expects a latter version of the\n"
                  "DMU API then that defined by Doomsday " DOOMSDAY_VERSION_TEXT ".\n"
                  "This game is for a newer version of Doomsday.");

    // A fixed number of dummies is allocated because:
    // - The number of dummies is mostly dependent on recursive depth of
    //   game functions.
    // - To test whether a pointer refers to a dummy is based on pointer
    //   comparisons; if the array is reallocated, its address may change
    //   and all existing dummies are invalidated.
    dummyLines = Z_Calloc(dummyCount * sizeof(dummyline_t), PU_STATIC, NULL);
    dummySectors = Z_Calloc(dummyCount * sizeof(dummysector_t), PU_STATIC, NULL);
}

/**
 * Allocates a new dummy object.
 *
 * @param type          DMU type of the dummy object.
 * @param extraData     Extra data pointer of the dummy. Points to
 *                      caller-allocated memory area of extra data for the
 *                      dummy.
 */
void* P_AllocDummy(int type, void* extraData)
{
    uint                i;

    switch(type)
    {
    case DMU_LINEDEF:
        for(i = 0; i < dummyCount; ++i)
        {
            if(!dummyLines[i].inUse)
            {
                dummyLines[i].inUse = true;
                dummyLines[i].extraData = extraData;
                dummyLines[i].line.header.type = DMU_LINEDEF;
                dummyLines[i].line.L_frontside =
                    dummyLines[i].line.L_backside = NULL;
                return &dummyLines[i];
            }
        }
        break;

    case DMU_SECTOR:
        for(i = 0; i < dummyCount; ++i)
        {
            if(!dummySectors[i].inUse)
            {
                dummySectors[i].inUse = true;
                dummySectors[i].extraData = extraData;
                dummySectors[i].sector.header.type = DMU_SECTOR;
                return &dummySectors[i];
            }
        }
        break;

    default:
        Con_Error("P_AllocDummy: Dummies of type %s not supported.\n",
                  DMU_Str(type));
    }

    Con_Error("P_AllocDummy: Out of dummies of type %s.\n", DMU_Str(type));
    return 0;
}

/**
 * Frees a dummy object.
 */
void P_FreeDummy(void* dummy)
{
    int                 type = P_DummyType(dummy);

    switch(type)
    {
    case DMU_LINEDEF:
        ((dummyline_t*)dummy)->inUse = false;
        break;

    case DMU_SECTOR:
        ((dummysector_t*)dummy)->inUse = false;
        break;

    default:
        Con_Error("P_FreeDummy: Dummy is of unknown type.\n");
        break;
    }
}

/**
 * Determines the type of a dummy object. For extra safety (in a debug build)
 * it would be possible to look through the dummy arrays and make sure the
 * pointer refers to a real dummy.
 */
int P_DummyType(void* dummy)
{
    // Is it a line?
    if(dummy >= (void*) &dummyLines[0] &&
       dummy <= (void*) &dummyLines[dummyCount - 1])
    {
        return DMU_LINEDEF;
    }

    // A sector?
    if(dummy >= (void*) &dummySectors[0] &&
       dummy <= (void*) &dummySectors[dummyCount - 1])
    {
        return DMU_SECTOR;
    }

    // Unknown.
    return DMU_NONE;
}

/**
 * Determines if a map data object is a dummy.
 */
boolean P_IsDummy(void* dummy)
{
    return P_DummyType(dummy) != DMU_NONE;
}

/**
 * Returns the extra data pointer of the dummy, or NULL if the object is not
 * a dummy object.
 */
void* P_DummyExtraData(void* dummy)
{
    switch(P_DummyType(dummy))
    {
    case DMU_LINEDEF:
        return ((dummyline_t*)dummy)->extraData;

    case DMU_SECTOR:
        return ((dummysector_t*)dummy)->extraData;

    default:
        break;
    }
    return NULL;
}

void* P_GetVariable(int value)
{
    switch(value)
    {
    case DMU_SECTOR_COUNT:
        return &numSectors;

    case DMU_LINE_COUNT:
        return &numLineDefs;

    case DMU_SIDE_COUNT:
        return &numSideDefs;

    case DMU_VERTEX_COUNT:
        return &numVertexes;

    case DMU_POLYOBJ_COUNT:
        return &numPolyObjs;

    case DMU_HEDGE_COUNT:
        return &numHEdges;

    case DMU_SUBSECTOR_COUNT:
        return &numSSectors;

    case DMU_NODE_COUNT:
        return &numNodes;

    default:
        break;
    }

    return 0;
}

void P_SetVariable(int value, void* data)
{
    // Stub.
}

/**
 * Convert pointer to index.
 */
uint P_ToIndex(const void* ptr)
{
    if(!ptr)
    {
        return 0;
    }

    switch(DMU_GetType(ptr))
    {
    case DMU_VERTEX:
        return GET_VERTEX_IDX((vertex_t*) ptr);

    case DMU_HEDGE:
        return GET_HEDGE_IDX((hedge_t*) ptr);

    case DMU_LINEDEF:
        return GET_LINE_IDX((linedef_t*) ptr);

    case DMU_SIDEDEF:
        return GET_SIDE_IDX((sidedef_t*) ptr);

    case DMU_SUBSECTOR:
        return GET_SUBSECTOR_IDX((subsector_t*) ptr);

    case DMU_SECTOR:
        return GET_SECTOR_IDX((sector_t*) ptr);

    case DMU_NODE:
        return GET_NODE_IDX((node_t*) ptr);

    case DMU_PLANE:
        return GET_PLANE_IDX((plane_t*) ptr);

    case DMU_MATERIAL:
        return P_ToMaterialNum((material_t*) ptr);

    default:
        Con_Error("P_ToIndex: Unknown type %s.\n", DMU_Str(DMU_GetType(ptr)));
    }
    return 0;
}

/**
 * Convert index to pointer.
 */
void* P_ToPtr(int type, uint index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return VERTEX_PTR(index);

    case DMU_HEDGE:
        return HEDGE_PTR(index);

    case DMU_LINEDEF:
        return LINE_PTR(index);

    case DMU_SIDEDEF:
        return SIDE_PTR(index);

    case DMU_SUBSECTOR:
        return SUBSECTOR_PTR(index);

    case DMU_SECTOR:
        return SECTOR_PTR(index);

    case DMU_NODE:
        return NODE_PTR(index);

    case DMU_PLANE:
        Con_Error("P_ToPtr: Cannot convert %s to a ptr (sector is unknown).\n",
                  DMU_Str(type));
        break;

    case DMU_MATERIAL:
        return P_ToMaterial(index);

    default:
        Con_Error("P_ToPtr: unknown type %s.\n", DMU_Str(type));
    }
    return NULL;
}

int P_Iteratep(void *ptr, uint prop, void* context,
               int (*callback) (void* p, void* ctx))
{
    int                 type = DMU_GetType(ptr);

    switch(type)
    {
    case DMU_SECTOR:
        switch(prop)
        {
        case DMU_LINEDEF:
            {
            sector_t*           sec = (sector_t*) ptr;
            int                 result = 1;

            if(sec->lineDefs)
            {
                linedef_t**         linePtr = sec->lineDefs;

                while(*linePtr && (result = callback(*linePtr, context)) != 0)
                    *linePtr++;
            }

            return result;
            }

        case DMU_PLANE:
            {
            sector_t*           sec = (sector_t*) ptr;
            int                 result = 1;

            if(sec->planes)
            {
                plane_t**           planePtr = sec->planes;

                while(*planePtr && (result = callback(*planePtr, context)) != 0)
                    *planePtr++;
            }

            return result;
            }

        case DMU_SUBSECTOR:
            {
            sector_t*           sec = (sector_t*) ptr;
            int                 result = 1;

            if(sec->ssectors)
            {
                subsector_t**       ssecPtr = sec->ssectors;

                while(*ssecPtr && (result = callback(*ssecPtr, context)) != 0)
                    *ssecPtr++;
            }

            return result;
            }

        default:
            Con_Error("P_Iteratep: Property %s unknown/not vector.\n",
                      DMU_Str(prop));
        }
        break;

    case DMU_SUBSECTOR:
        switch(prop)
        {
        case DMU_HEDGE:
            {
            subsector_t*        ssec = (subsector_t*) ptr;
            int                 result = 1;
            hedge_t*            hEdge;

            if((hEdge = ssec->hEdge))
            {
                do
                {
                    if((result = callback(hEdge, context)) == 0)
                        break;
                } while((hEdge = hEdge->next) != ssec->hEdge);
            }

            return result;
            }

        default:
            Con_Error("P_Iteratep: Property %s unknown/not vector.\n",
                      DMU_Str(prop));
        }
        break;

    default:
        Con_Error("P_Iteratep: Type %s unknown.\n", DMU_Str(type));
        break;
    }

    return true; // Successfully completed.
}

/**
 * Call a callback function on a selection of map data objects. The
 * selected objects will be specified by 'type' and 'index'.
 *
 * @param context       Is passed to the callback function.
 *
 * @return              @c true if all the calls to the callback function
 *                      return @c true.
 *                      @c false is returned when the callback function
 *                      returns @c false; in this case, the iteration is
 *                      aborted immediately when the callback function
 *                      returns @c false.
 */
int P_Callback(int type, uint index, void* context,
               int (*callback)(void* p, void* ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
        if(index < numVertexes)
            return callback(VERTEX_PTR(index), context);
        break;

    case DMU_HEDGE:
        if(index < numHEdges)
            return callback(HEDGE_PTR(index), context);
        break;

    case DMU_LINEDEF:
        if(index < numLineDefs)
            return callback(LINE_PTR(index), context);
        break;

    case DMU_SIDEDEF:
        if(index < numSideDefs)
            return callback(SIDE_PTR(index), context);
        break;

    case DMU_NODE:
        if(index < numNodes)
            return callback(NODE_PTR(index), context);
        break;

    case DMU_SUBSECTOR:
        if(index < numSSectors)
            return callback(SUBSECTOR_PTR(index), context);
        break;

    case DMU_SECTOR:
        if(index < numSectors)
            return callback(SECTOR_PTR(index), context);
        break;

    case DMU_PLANE:
        Con_Error("P_Callback: %s cannot be referenced by id alone (sector is unknown).\n",
                  DMU_Str(type));
        break;

    case DMU_MATERIAL:
        if(index < numMaterialBinds)
            return callback(P_ToMaterial(index), context);
        break;

    case DMU_LINEDEF_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINEDEF_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG:
        Con_Error("P_Callback: Type %s not implemented yet.\n", DMU_Str(type));
        /*
        for(i = 0; i < numLineDefs; ++i)
        {
            if(!callback(LINE_PTR(i), context)) return false;
        }
        */
        break;

    default:
        Con_Error("P_Callback: Type %s unknown (index %i).\n", DMU_Str(type), index);
    }

    // Successfully completed.
    return true;
}

/**
 * Another version of callback iteration. The set of selected objects is
 * determined by 'type' and 'ptr'. Otherwise works like P_Callback.
 */
int P_Callbackp(int type, void* ptr, void* context,
                int (*callback)(void* p, void* ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
    case DMU_HEDGE:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_NODE:
    case DMU_SUBSECTOR:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_MATERIAL:
        // Only do the callback if the type is the same as the object's.
        if(type == DMU_GetType(ptr))
        {
            return callback(ptr, context);
        }
#if _DEBUG
        else
        {
            Con_Message("P_Callbackp: Type mismatch %s != %s\n",
                        DMU_Str(type), DMU_Str(DMU_GetType(ptr)));
        }
#endif
        break;

    // \todo If necessary, add special types for accessing multiple objects.

    default:
        Con_Error("P_Callbackp: Type %s unknown.\n", DMU_Str(type));
    }
    return true;
}

/**
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
void DMU_SetValue(valuetype_t valueType, void* dst, const setargs_t* args,
                  uint index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = args->fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
                Con_Error("SetValue: %d is not a valid value for DDVT_BLENDMODE.\n",
                          args->intValues[index]);

            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", valueType);
    }
}

/**
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a
 * fatal error.
 *
 * When a property changes, the relevant subsystems are notified of the change
 * so that they can update their state accordingly.
 */
static int setProperty(void* obj, void* context)
{
    setargs_t*          args = (setargs_t*) context;
    sector_t*           updateSector1 = NULL, *updateSector2 = NULL;
    plane_t*            updatePlane = NULL;
    linedef_t*          updateLinedef = NULL;
    sidedef_t*          updateSidedef = NULL;
    surface_t*          updateSurface = NULL;
    // subsector_t*        updateSubSector = NULL;

    /**
     * \algorithm:
     *
     * When setting a property, reference resolution is done hierarchically so
     * that we can update all owner's of the objects being manipulated should
     * the DMU object's Set routine suggests that a change occured which other
     * DMU objects may wish/need to respond to.
     *
     * 1) Collect references to all current owners of the object.
     * 2) Pass the change delta on to the object.
     * 3) Object responds:
     *      @c true = update owners, ELSE @c false.
     * 4) If num collected references > 0
     *        recurse, Object = owners[n]
     */

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_SUBSECTOR)
    {
        // updateSubSector = (subsector_t*) obj;

        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
        }
    }

    if(args->type == DMU_SECTOR)
    {
        updateSector1 = (sector_t*) obj;

        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            sector_t           *sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            sector_t           *sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        updateLinedef = (linedef_t*) obj;

        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            obj = ((linedef_t*) obj)->L_frontside;
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            linedef_t          *li = ((linedef_t*) obj);
            if(!li->L_backside)
                Con_Error("DMU_setProperty: Linedef %i has no back side.\n",
                          P_ToIndex(li));

            obj = li->L_backside;
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        updateSidedef = (sidedef_t*) obj;

        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_topsurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_middlesurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_bottomsurface;
            args->type = DMU_SURFACE;
        }
    }

    if(args->type == DMU_PLANE)
    {
        updatePlane = (plane_t*) obj;

        switch(args->prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
            obj = &((plane_t*) obj)->surface;
            args->type = DMU_SURFACE;
            break;

        default:
            break;
        }
    }

    if(args->type == DMU_SURFACE)
    {
        updateSurface = (surface_t*) obj;
/*
        // Resolve implicit references to properties of the surface's material.
        switch(args->prop)
        {
        case UNKNOWN1:
            obj = &((surface_t*) obj)->material;
            args->type = DMU_MATERIAL;
            break;

        default:
            break;
        }*/
    }

    switch(args->type)
    {
    case DMU_SURFACE:
        Surface_SetProperty(obj, args);
        break;

    case DMU_PLANE:
        Plane_SetProperty(obj, args);
        break;

    case DMU_VERTEX:
        Vertex_SetProperty(obj, args);
        break;

    case DMU_HEDGE:
        Seg_SetProperty(obj, args);
        break;

    case DMU_LINEDEF:
        Linedef_SetProperty(obj, args);
        break;

    case DMU_SIDEDEF:
        Sidedef_SetProperty(obj, args);
        break;

    case DMU_SUBSECTOR:
        Subsector_SetProperty(obj, args);
        break;

    case DMU_SECTOR:
        Sector_SetProperty(obj, args);
        break;

    case DMU_MATERIAL:
        Material_SetProperty(obj, args);
        break;

    case DMU_NODE:
        Con_Error("SetProperty: Property %s is not writable in DMU_NODE.\n",
                  DMU_Str(args->prop));
        break;

    default:
        Con_Error("SetProperty: Type %s not writable.\n", DMU_Str(args->type));
    }

    if(updateSurface)
    {
        if(R_UpdateSurface(updateSurface, false))
        {
            switch(DMU_GetType(updateSurface->owner))
            {
            case DMU_SIDEDEF:
                updateSidedef = updateSurface->owner;
                break;

            case DMU_PLANE:
                updatePlane = updateSurface->owner;
                break;

            default:
                Con_Error("SetPropert: Internal error, surface owner unknown.\n");
            }
        }
    }

    if(updateSidedef)
    {
        if(R_UpdateSidedef(updateSidedef, false))
            updateLinedef = updateSidedef->line;
    }

    if(updateLinedef)
    {
        if(R_UpdateLinedef(updateLinedef, false))
        {
            updateSector1 = updateLinedef->L_frontside->sector;
            updateSector2 = updateLinedef->L_backside->sector;
        }
    }

    if(updatePlane)
    {
        if(R_UpdatePlane(updatePlane, false))
            updateSector1 = updatePlane->sector;
    }

    if(updateSector1)
    {
        R_UpdateSector(updateSector1, false);
    }

    if(updateSector2)
    {
        R_UpdateSector(updateSector2, false);
    }

/*  if(updateSubSector)
    {
        R_UpdateSubSector(updateSubSector, false);
    } */

    return true; // Continue iteration.
}

/**
 * Gets a value. Does some basic type checking so that incompatible types
 * are not assigned. Simple conversions are also done, e.g., float to
 * fixed.
 */
void DMU_GetValue(valuetype_t valueType, const void* src, setargs_t* args,
                  uint index)
{
    if(valueType == DDVT_FIXED)
    {
        const fixed_t* s = src;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_INT:
            args->intValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = FIX2FLT(*s);
            break;
        default:
            Con_Error("GetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        const float* s = src;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = (int) *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = FLT2FIX(*s);
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        const boolean* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        const byte* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        const int* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT)
    {
        const short* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        const angle_t* s = src;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            args->angleValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        const blendmode_t* s = src;

        switch(args->valueType)
        {
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        const void* const* s = src;

        switch(args->valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map data objects. Failure leads into a fatal error.
            args->intValues[index] = P_ToIndex(*s);
            break;
        case DDVT_PTR:
            args->ptrValues[index] = (void*) *s;
            break;
        default:
            Con_Error("GetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("GetValue: unknown value type %d.\n", valueType);
    }
}

static int getProperty(void* obj, void* context)
{
    setargs_t*          args = (setargs_t*) context;

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_SUBSECTOR)
    {
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
        }
    }

    if(args->type == DMU_SECTOR)
    {
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            sector_t           *sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            sector_t           *sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            obj = ((linedef_t*) obj)->L_frontside;
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            linedef_t          *li = ((linedef_t*) obj);
            if(!li->L_backside)
                Con_Error("DMU_setProperty: Linedef %i has no back side.\n",
                          P_ToIndex(li));

            obj = li->L_backside;
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_topsurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_middlesurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            obj = &((sidedef_t*) obj)->SW_bottomsurface;
            args->type = DMU_SURFACE;
        }
    }

    if(args->type == DMU_PLANE)
    {
        switch(args->prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
            obj = &((plane_t*) obj)->surface;
            args->type = DMU_SURFACE;
            break;

        default:
            break;
        }
    }

/*
    if(args->type == DMU_SURFACE)
    {
        // Resolve implicit references to properties of the surface's material.
        switch(args->prop)
        {
        case UNKNOWN1:
            obj = &((surface_t*) obj)->material;
            args->type = DMU_MATERIAL;
            break;

        default:
            break;
        }
    }
*/

    switch(args->type)
    {
    case DMU_VERTEX:
        Vertex_GetProperty(obj, args);
        break;

    case DMU_HEDGE:
        Seg_GetProperty(obj, args);
        break;

    case DMU_LINEDEF:
        Linedef_GetProperty(obj, args);
        break;

    case DMU_SURFACE:
        Surface_GetProperty(obj, args);
        break;

    case DMU_PLANE:
        Plane_GetProperty(obj, args);
        break;

    case DMU_SECTOR:
        Sector_GetProperty(obj, args);
        break;

    case DMU_SIDEDEF:
        Sidedef_GetProperty(obj, args);
        break;

    case DMU_SUBSECTOR:
        Subsector_GetProperty(obj, args);
        break;

    case DMU_MATERIAL:
        Material_GetProperty(obj, args);
        break;

    default:
        Con_Error("GetProperty: Type %s not readable.\n", DMU_Str(args->type));
    }

    // Currently no aggregate values are collected.
    return false;
}

void P_SetBool(int type, uint index, uint prop, boolean param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetByte(int type, uint index, uint prop, byte param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetInt(int type, uint index, uint prop, int param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFixed(int type, uint index, uint prop, fixed_t param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetAngle(int type, uint index, uint prop, angle_t param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFloat(int type, uint index, uint prop, float param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetPtr(int type, uint index, uint prop, void* param)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetBoolv(int type, uint index, uint prop, boolean* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetBytev(int type, uint index, uint prop, byte* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetIntv(int type, uint index, uint prop, int* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFixedv(int type, uint index, uint prop, fixed_t* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetAnglev(int type, uint index, uint prop, angle_t* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFloatv(int type, uint index, uint prop, float* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetPtrv(int type, uint index, uint prop, void* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, setProperty);
}

/* pointer-based write functions */

void P_SetBoolp(void* ptr, uint prop, boolean param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBytep(void* ptr, uint prop, byte param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetIntp(void* ptr, uint prop, int param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFixedp(void* ptr, uint prop, fixed_t param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetAnglep(void* ptr, uint prop, angle_t param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFloatp(void* ptr, uint prop, float param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetPtrp(void* ptr, uint prop, void* param)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBoolpv(void* ptr, uint prop, boolean* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBytepv(void* ptr, uint prop, byte* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetIntpv(void* ptr, uint prop, int* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetAnglepv(void* ptr, uint prop, angle_t* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFloatpv(void* ptr, uint prop, float* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetPtrpv(void* ptr, uint prop, void* params)
{
    setargs_t           args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

/* index-based read functions */

boolean P_GetBool(int type, uint index, uint prop)
{
    setargs_t           args;
    boolean             returnValue = false;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

byte P_GetByte(int type, uint index, uint prop)
{
    setargs_t           args;
    byte                returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

int P_GetInt(int type, uint index, uint prop)
{
    setargs_t           args;
    int                 returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

fixed_t P_GetFixed(int type, uint index, uint prop)
{
    setargs_t           args;
    fixed_t             returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

angle_t P_GetAngle(int type, uint index, uint prop)
{
    setargs_t           args;
    angle_t             returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

float P_GetFloat(int type, uint index, uint prop)
{
    setargs_t           args;
    float               returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

void* P_GetPtr(int type, uint index, uint prop)
{
    setargs_t           args;
    void               *returnValue = NULL;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

void P_GetBoolv(int type, uint index, uint prop, boolean* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetBytev(int type, uint index, uint prop, byte* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetIntv(int type, uint index, uint prop, int* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetFixedv(int type, uint index, uint prop, fixed_t* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetAnglev(int type, uint index, uint prop, angle_t* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetFloatv(int type, uint index, uint prop, float* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetPtrv(int type, uint index, uint prop, void* params)
{
    setargs_t           args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, getProperty);
}

/* pointer-based read functions */

boolean P_GetBoolp(void* ptr, uint prop)
{
    setargs_t           args;
    boolean             returnValue = false;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

byte P_GetBytep(void* ptr, uint prop)
{
    setargs_t           args;
    byte                returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

int P_GetIntp(void* ptr, uint prop)
{
    setargs_t           args;
    int                 returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

fixed_t P_GetFixedp(void* ptr, uint prop)
{
    setargs_t           args;
    fixed_t             returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

angle_t P_GetAnglep(void* ptr, uint prop)
{
    setargs_t           args;
    angle_t             returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

float P_GetFloatp(void* ptr, uint prop)
{
    setargs_t           args;
    float               returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

void* P_GetPtrp(void* ptr, uint prop)
{
    setargs_t           args;
    void               *returnValue = NULL;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

void P_GetBoolpv(void* ptr, uint prop, boolean* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetBytepv(void* ptr, uint prop, byte* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetIntpv(void* ptr, uint prop, int* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetAnglepv(void* ptr, uint prop, angle_t* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetFloatpv(void* ptr, uint prop, float* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetPtrpv(void* ptr, uint prop, void* params)
{
    setargs_t           args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

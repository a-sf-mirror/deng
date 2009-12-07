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
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    objectrecord_t  sentinel;
    objectrecordid_t num;
    objectrecordid_t* ids;
} objectrecord_namespace_t;

typedef struct dummyline_s {
    runtime_mapdata_header_t header;
    linedef_t       line; // Line data.
    void*           extraData; // Pointer to user data.
    boolean         inUse; // true, if the dummy is being used.
} dummyline_t;

typedef struct dummysector_s {
    runtime_mapdata_header_t header;
    sector_t        sector; // Sector data.
    void*           extraData; // Pointer to user data.
    boolean         inUse; // true, if the dummy is being used.
} dummysector_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint dummyCount = 8; // Number of dummies to allocate (per type).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int usingDMUAPIver; // Version of the DMU API the game expects.

static objectrecordid_t numRecords;
static objectrecord_t* records = NULL;

static int numObjectRecordNamespaces;
static objectrecord_namespace_t** objectRecordNamespaces = NULL;

static dummyline_t* dummyLines;
static dummysector_t* dummySectors;

static map_t* currentMap = NULL;

/**
 * Game-specific, object type definitions.
 * @todo Utilize DED reader 2.0:
 */
static uint numGameObjectDefs;
static def_gameobject_t* gameObjectDefs;

// CODE --------------------------------------------------------------------

static objectrecordid_t addObjectRecord(objectrecord_namespace_t* s, objectrecordid_t id)
{
    s->ids = M_Realloc(s->ids, sizeof(objectrecordid_t) * ++s->num);
    s->ids[s->num - 1] = id;

    return s->num - 1;
}

static void destroyRecordNamespace(int idx)
{
    objectrecord_namespace_t* s = objectRecordNamespaces[idx];

    M_Free(s);

    if(numObjectRecordNamespaces > 1 && idx != numObjectRecordNamespaces - 1)
        memmove(objectRecordNamespaces[idx], objectRecordNamespaces[idx+1],
                sizeof(objectrecord_namespace_t*) * (numObjectRecordNamespaces - 1 - idx));
    numObjectRecordNamespaces--;
}

static int findNamespaceForRecordType(int type)
{
    int i;

    for(i = 0; i < numObjectRecordNamespaces; ++i)
    {
        if(objectRecordNamespaces[i]->sentinel.header.type == type)
        {
            return i;
        }
    }

    return -1; // Not found.
}

static objectrecord_t* findRecordInNamespace(const objectrecord_namespace_t* s, void* obj)
{
    objectrecordid_t i;

    for(i = 1; i < s->num; ++i)
    {
        if(records[s->ids[i]].obj == obj)
            return &records[s->ids[i]];
    }

    return NULL;
}

static objectrecord_namespace_t* createObjectRecordNamespace(int type)
{
    objectrecord_namespace_t* s;

    objectRecordNamespaces = M_Realloc(objectRecordNamespaces, sizeof(s) * ++numObjectRecordNamespaces);
    objectRecordNamespaces[numObjectRecordNamespaces-1] = s = M_Malloc(sizeof(*s));

    s->num = 0;
    s->ids = NULL;

    // Add the sentinel for this record set.
    s->sentinel.dummy = -667;
    s->sentinel.header.type = type;
    s->sentinel.obj = NULL;
    s->sentinel.id = -1;
    addObjectRecord(s, -1);

    return s;
}

static int iterateRecords(int type, int (*callback) (void*, void*),
                          void* context, boolean retObjectRecord)
{
    int result = true; // Successfully completed.

    if(type != DMU_NONE)
    {
        int idx;

        if((idx = findNamespaceForRecordType(type)) != -1)
        {
            objectrecord_namespace_t* s = objectRecordNamespaces[idx];
            objectrecordid_t i;

            for(i = 1; i < s->num; ++i)
            {
                objectrecord_t* r = &records[s->ids[i]];
                void* obj = (retObjectRecord? r : r->obj);

                if((result = callback(obj, context)) == 0)
                    break;
            }
        }
    }
    else
    {
        int j;

        for(j = 0; j < numObjectRecordNamespaces; ++j)
        {
            objectrecord_namespace_t* s = objectRecordNamespaces[j];
            objectrecordid_t i;

            for(i = 1; i < s->num; ++i)
            {
                objectrecord_t* r = &records[s->ids[i]];
                void* obj = (retObjectRecord? r : r->obj);

                if((result = callback(obj, context)) == 0)
                    break;
            }
        }
    }

    return result;
}

void P_DestroyObjectRecordsByType(int type)
{
    int idx;

    if(type < DMU_FIRST_TYPE)
        return;

    if((idx = findNamespaceForRecordType(type)) != -1)
    {
        objectrecord_namespace_t* s = objectRecordNamespaces[idx];
        objectrecordid_t i;

        for(i = 1; i < s->num; ++i)
        {
            objectrecord_t* r = &records[s->ids[i]];

            // Deallocation of records is lazy.
            r->header.type = DMU_NONE;
            r->obj = NULL;
            r->id = -1;
        }

        s->ids = M_Realloc(s->ids, sizeof(objectrecordid_t));
        s->num = 1;
    }
}

objectrecordid_t P_CreateObjectRecord(int type, void* p)
{
    int idx;
    objectrecord_t* r;
    objectrecord_namespace_t* s;

    if(type < DMU_FIRST_TYPE)
        Con_Error("P_CreateObjectRecord: Invalid type %i.", type);

    if((idx = findNamespaceForRecordType(type)) != -1)
        s = objectRecordNamespaces[idx];
    else
        s = createObjectRecordNamespace(type);

    if(!records)
        records = M_Realloc(records, sizeof(objectrecord_t) * 80000); //++numRecords);
    numRecords++;

    r = &records[numRecords - 1];
    r->dummy = -666;
    r->header.type = type;
    r->obj = p;
    r->id = addObjectRecord(s, numRecords - 1);

    return r->id;
}

void P_DestroyObjectRecord(int type, void* p)
{
    objectrecord_t* r = NULL;
    int idx;

    // First, remove the object record from any record sets.
    if((idx = findNamespaceForRecordType(type)) != -1)
    {
        objectrecord_namespace_t*  s = objectRecordNamespaces[idx];
        objectrecordid_t    i;

        for(i = 1; i < s->num; ++i)
        {
            if(records[s->ids[i]].obj != p)
                continue;

            r = &records[s->ids[i]];

            if(i < s->num - 1)
                memmove(&s->ids[i], &s->ids[i+1],
                        sizeof(*s->ids) * (s->num - i - 1));

            s->ids[s->num - 1] = -1;
            --s->num;
            break;
        }
    }

    // Next, remove the object record.
    if(r)
    {
        // Deallocation of records is lazy.
        r->header.type = DMU_NONE;
        r->obj = NULL;
        r->id = -1;
    }
}

objectrecord_t* P_ObjectRecord(int type, void* p)
{
    int idx;

    if(p && (idx = findNamespaceForRecordType(type)) != -1)
    {
        return findRecordInNamespace(objectRecordNamespaces[idx], p);
    }

    return NULL;
}

/**
 * @return              Ptr to the current map.
 */
map_t* P_CurrentMap(void)
{
    return currentMap;
}

void P_SetCurrentMap(map_t* map)
{
    currentMap = map;
}

/**
 * Look up a mapobj definition.
 *
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
 */
def_gameobject_t* P_GameObjectDef(int identifier, const char* objName,
                                  boolean canCreate)
{
    uint i;
    size_t len;
    def_gameobject_t* def;

    if(objName)
        len = strlen(objName);

    // Is this a known game object?
    for(i = 0; i < numGameObjectDefs; ++i)
    {
        def = &gameObjectDefs[i];

        if(objName && objName[0])
        {
            if(!strnicmp(objName, def->name, len))
            {   // Found it!
                return def;
            }
        }
        else
        {
            if(identifier == def->identifier)
            {   // Found it!
                return def;
            }
        }
    }

    if(!canCreate)
        return NULL; // Not a known game map object.

    if(identifier == 0)
        return NULL; // Not a valid indentifier.

    if(!objName || !objName[0])
        return NULL; // Must have a name.

    // Ensure the name is unique.
    for(i = 0; i < numGameObjectDefs; ++i)
    {
        def = &gameObjectDefs[i];
        if(!strnicmp(objName, def->name, len))
        {   // Oh dear, a duplicate.
            return NULL;
        }
    }

    gameObjectDefs =
        M_Realloc(gameObjectDefs, ++numGameObjectDefs * sizeof(*gameObjectDefs));

    def = &gameObjectDefs[numGameObjectDefs - 1];
    def->identifier = identifier;
    def->name = M_Malloc(len+1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProperties = 0;
    def->properties = NULL;

    return def;
}

def_gameobject_t* P_GetGameObjectDef(uint idx)
{
    if(idx < numGameObjectDefs)
        return &gameObjectDefs[idx];

    Con_Error("P_GetGameObjectDef: Error, no definition exists for id %u.", idx);
    return NULL; // Unreachable.
}

/**
 * Called by the game to register the map object types it wishes us to make
 * public via the MPE interface.
 */
boolean P_CreateObjectDef(int identifier, const char* name)
{
    if(P_GameObjectDef(identifier, name, true))
        return true; // Success.

    return false;
}

/**
 * Called by the game to add a new property to a previously registered
 * map object type definition.
 */
boolean P_AddPropertyToObjectDef(int identifier, int propIdentifier,
                                 const char* propName, valuetype_t type)
{
    uint i;
    size_t len;
    mapobjprop_t* prop;
    def_gameobject_t* def = P_GameObjectDef(identifier, NULL, false);

    if(!def) // Not a valid identifier.
    {
        Con_Error("P_AddPropertyToObjectDef: Unknown mapobj identifier %i.",
                  identifier);
    }

    if(propIdentifier == 0) // Not a valid identifier.
        Con_Error("P_AddPropertyToObjectDef: 0 not valid for propIdentifier.");

    if(!propName || !propName[0]) // Must have a name.
        Con_Error("P_AddPropertyToObjectDef: Cannot register without name.");

    // Screen out value types we don't currently support for gmos.
    switch(type)
    {
    case DDVT_BYTE:
    case DDVT_SHORT:
    case DDVT_INT:
    case DDVT_FIXED:
    case DDVT_ANGLE:
    case DDVT_FLOAT:
        break;

    default:
        Con_Error("P_AddPropertyToObjectDef: Unknown/not supported value type %i.",
                  type);
    }

    // Next, make sure propIdentifer and propName are unique.
    len = strlen(propName);
    for(i = 0; i < def->numProperties; ++i)
    {
        prop = &def->properties[i];

        if(prop->identifier == propIdentifier)
            Con_Error("P_AddPropertyToObjectDef: propIdentifier %i not unique for %s.",
                      propIdentifier, def->name);

        if(!strnicmp(propName, prop->name, len))
            Con_Error("P_AddPropertyToObjectDef: propName \"%s\" not unique for %s.",
                      propName, def->name);
    }

    // Looks good! Add it to the list of properties.
    def->properties = M_Realloc(def->properties, ++def->numProperties * sizeof(*def->properties));

    prop = &def->properties[def->numProperties - 1];
    prop->identifier = propIdentifier;
    prop->name = M_Malloc(len + 1);
    strncpy(prop->name, propName, len);
    prop->name[len] = '\0';
    prop->type = type;

    return true; // Success!
}

/**
 * Called during init to initialize the map obj defs.
 */
void P_InitGameObjectDefs(void)
{
    gameObjectDefs = NULL;
    numGameObjectDefs = 0;
}

/**
 * Called at shutdown to free all memory allocated for the map obj defs.
 */
void P_DestroyGameObjectDefs(void)
{
    uint i, j;

    if(gameObjectDefs)
    {
        for(i = 0; i < numGameObjectDefs; ++i)
        {
            def_gameobject_t* def = &gameObjectDefs[i];

            for(j = 0; j < def->numProperties; ++j)
            {
                mapobjprop_t* prop = &def->properties[j];

                M_Free(prop->name);
            }
            M_Free(def->properties);

            M_Free(def->name);
        }

        M_Free(gameObjectDefs);
    }

    gameObjectDefs = NULL;
    numGameObjectDefs = 0;
}

uint P_NumGameObjectDefs(void)
{
    return numGameObjectDefs;
}

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
        { DMU_SEG, "DMU_SEG" },
        { DMU_LINEDEF, "DMU_LINEDEF" },
        { DMU_SIDEDEF, "DMU_SIDEDEF" },
        { DMU_NODE, "DMU_NODE" },
        { DMU_SUBSECTOR, "DMU_SUBSECTOR" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_PLANE, "DMU_PLANE" },
        { DMU_MATERIAL, "DMU_MATERIAL" },
        { DMU_SKY, "DMU_SKY" },
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
        { DMU_MOBJS, "DMU_MOBJS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_SOUND_ORIGIN, "DMU_SOUND_ORIGIN" },
        { DMU_WIDTH, "DMU_WIDTH" },
        { DMU_HEIGHT, "DMU_HEIGHT" },
        { DMU_TARGET_HEIGHT, "DMU_TARGET_HEIGHT" },
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

    type = ((const objectrecord_t*)ptr)->header.type;

    // Make sure it's valid.
    switch(type)
    {
    case DMU_VERTEX:
    case DMU_SEG:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_SUBSECTOR:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_NODE:
    case DMU_MATERIAL:
    case DMU_SKY:
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

    // Initialize the obj record database.
    records = NULL;
    numRecords = 0;

    objectRecordNamespaces = NULL;
    numObjectRecordNamespaces = 0;
}

void P_ShutdownMapUpdate(void)
{
    P_DestroyGameObjectDefs();

    if(objectRecordNamespaces)
    {
        int                 i;

        for(i = 0; i < numObjectRecordNamespaces; ++i)
        {
            objectrecord_namespace_t*  s = objectRecordNamespaces[i];

            if(s->ids)
                M_Free(s->ids);

            M_Free(s);
        }

        M_Free(objectRecordNamespaces);
    }

    objectRecordNamespaces = NULL;
    numObjectRecordNamespaces = 0;

    if(records)
        M_Free(records);
    records = NULL;
    numRecords = 0;
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
                dummyLines[i].header.type = DMU_LINEDEF;
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
                dummySectors[i].header.type = DMU_SECTOR;
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
    static uint count = 0;

    switch(value)
    {
    case DMU_SECTOR_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numSectors;
        return &count;
        }
    case DMU_LINE_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numLineDefs;
        return &count;
        }
    case DMU_SIDE_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numSideDefs;
        return &count;
        }
    case DMU_VERTEX_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = HalfEdgeDS_NumVertices(Map_HalfEdgeDS(map));
        return &count;
        }
    case DMU_POLYOBJ_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numPolyObjs;
        return &count;
        }
    case DMU_SEG_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numSegs;
        return &count;
        }
    case DMU_SUBSECTOR_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numSubsectors;
        return NULL;
        }
    case DMU_NODE_COUNT:
        {
        map_t* map = P_CurrentMap();
        if(map)
            count = map->numNodes;
        return &count;
        }
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
objectrecordid_t P_ToIndex(const void* ptr)
{
    int type;

    if(!ptr)
    {
        return 0;
    }

    type = DMU_GetType(ptr);
    if(type < DMU_FIRST_TYPE)
        Con_Error("P_ToIndex: Cannot convert %s to an index.\n", DMU_Str(type));

    switch(type)
    {
    case DMU_SECTOR:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_VERTEX:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_SEG:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_LINEDEF:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_SIDEDEF:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_SUBSECTOR:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_NODE:
        return ((objectrecord_t*) ptr)->id - 1;
    case DMU_PLANE:
        return ((plane_t*) (((objectrecord_t*) ptr)->obj))->planeID;

    case DMU_MATERIAL:
        return Materials_ToIndex(((objectrecord_t*) ptr)->obj);

    case DMU_SKY:
        return 0;

    default:
        Con_Error("P_ToIndex: Unknown type %s.\n", DMU_Str(type));
    }
    return 0;
}

/**
 * Convert index to pointer.
 */
void* P_ToPtr(int type, objectrecordid_t index)
{
    int                 idx;

    // Handle special radixes.
    switch(type)
    {
    case DMU_MATERIAL:
        if(index == 0) // Zero means "no reference".
            return NULL;
        index -= 1; // Zero-based internally.
        break;

    case DMU_SKY:
        index = 0; // Only one.
        break;

    default:
        break;
    }

    if((idx = findNamespaceForRecordType(type)) != -1)
        return &records[objectRecordNamespaces[idx]->ids[1 + index]];

    Con_Error("P_ToPtr: Invalid type+object index (t=%s, i=%u).\n",
              DMU_Str(type), index);

    return NULL; // Unreachable.
}

int P_Iteratep(void* ptr, uint prop, int (*callback) (void*, void*),
               void* context)
{
    int type = DMU_GetType(ptr);

    switch(type)
    {
    case DMU_SECTOR:
        switch(prop)
        {
        case DMU_MOBJS:
            return P_SectorTouchingMobjsIterator(((objectrecord_t*) ptr)->obj,
                                                 callback, context);
        case DMU_LINEDEF:
            {
            const objectrecord_namespace_t* s = objectRecordNamespaces[findNamespaceForRecordType(DMU_LINEDEF)];
            sector_t* sec = (sector_t*) ((objectrecord_t*) ptr)->obj;
            int result = 1;

            if(sec->lineDefs)
            {
                linedef_t** linePtr = sec->lineDefs;

                while(*linePtr && (result =
                      callback(findRecordInNamespace(s, *linePtr), context)) != 0)
                    *linePtr++;
            }

            return result;
            }

        case DMU_PLANE:
            {
            const objectrecord_namespace_t* s = objectRecordNamespaces[findNamespaceForRecordType(DMU_PLANE)];
            sector_t* sec = (sector_t*) ((objectrecord_t*) ptr)->obj;
            int result = 1;

            if(sec->planes)
            {
                plane_t** planePtr = sec->planes;

                while(*planePtr && (result =
                      callback(findRecordInNamespace(s, *planePtr), context)) != 0)
                    *planePtr++;
            }

            return result;
            }

        case DMU_SUBSECTOR:
            {
            const objectrecord_namespace_t* s = objectRecordNamespaces[findNamespaceForRecordType(DMU_SUBSECTOR)];
            sector_t* sec = (sector_t*) ((objectrecord_t*) ptr)->obj;
            int result = 1;

            if(sec->subsectors)
            {
                subsector_t** subsectorPtr = sec->subsectors;

                while(*subsectorPtr && (result =
                      callback(findRecordInNamespace(s, *subsectorPtr), context)) != 0)
                    *subsectorPtr++;
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
        case DMU_SEG:
            {
            const objectrecord_namespace_t* s = objectRecordNamespaces[findNamespaceForRecordType(DMU_SEG)];
            subsector_t* subsector = (subsector_t*) ((objectrecord_t*) ptr)->obj;
            int result = 1;
            hedge_t* hEdge;

            if((hEdge = subsector->face->hEdge))
            {
                do
                {
                    objectrecord_t* r = findRecordInNamespace(s, (seg_t*) hEdge->data);
                    if((result = callback(r, context)) == 0)
                        break;
                } while((hEdge = hEdge->next) != subsector->face->hEdge);
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
int P_Callback(int type, objectrecordid_t index, int (*callback)(void* p, void* ctx),
               void* context)
{
    switch(type)
    {
    case DMU_VERTEX:
        {
        map_t* map = P_CurrentMap();
        if(index < HalfEdgeDS_NumVertices(Map_HalfEdgeDS(map)))
            return callback(P_ObjectRecord(DMU_VERTEX, Map_HalfEdgeDS(map)->vertices[index]), context);
        break;
        }
    case DMU_SEG:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numSegs)
            return callback(P_ObjectRecord(DMU_SEG, map->segs[index]), context);
        break;
        }
    case DMU_LINEDEF:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numLineDefs)
            return callback(P_ObjectRecord(DMU_LINEDEF, map->lineDefs[index]), context);
        break;
        }
    case DMU_SIDEDEF:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numSideDefs)
            return callback(P_ObjectRecord(DMU_SIDEDEF, map->sideDefs[index]), context);
        break;
        }
    case DMU_NODE:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numNodes)
            return callback(P_ObjectRecord(DMU_NODE, map->nodes[index]), context);
        break;
        }
    case DMU_SUBSECTOR:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numSubsectors)
            return callback(P_ObjectRecord(DMU_SUBSECTOR, map->subsectors[index]), context);
        break;
        }
    case DMU_SECTOR:
        {
        map_t* map = P_CurrentMap();
        if(index < map->numSectors)
            return callback(P_ObjectRecord(DMU_SECTOR, map->sectors[index]), context);
        break;
        }
    case DMU_PLANE:
        Con_Error("P_Callback: %s cannot be referenced by id alone (sector is unknown).\n",
                  DMU_Str(type));
        break;

    case DMU_MATERIAL:
        if(index < numMaterialBinds)
            return callback(Materials_ToMaterial(index), context);
        break;

    case DMU_SKY:
        //if(index < numSkies)
            return callback(P_ObjectRecord(DMU_SKY, theSky /*skies + index*/), context);
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
int P_Callbackp(int type, void* ptr, int (*callback)(void*, void*),
                void* context)
{
    objectrecord_t*     r = (objectrecord_t*) ptr;

    switch(type)
    {
    case DMU_VERTEX:
    case DMU_SEG:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_NODE:
    case DMU_SUBSECTOR:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_MATERIAL:
    case DMU_SKY:
        // Only do the callback if the type is the same as the object's.
        // \todo If necessary, add special types for accessing multiple objects.
        if(type == DMU_GetType(r))
        {
            return callback(r, context);
        }
#if _DEBUG
        else
        {
            Con_Message("P_Callbackp: Type mismatch %s != %s\n",
                        DMU_Str(type), DMU_Str(DMU_GetType(r)));
        }
#endif

        return true;

    default:
        break;
    }

    Con_Error("P_Callbackp: Type %s unknown.\n", DMU_Str(type));
    return false; // Unreachable.
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
static int setProperty(void* ptr, void* context)
{
    objectrecord_t* r = (objectrecord_t*) ptr;
    setargs_t* args = (setargs_t*) context;
    void* obj = r->obj;
    sector_t* updateSector1 = NULL, *updateSector2 = NULL;
    plane_t* updatePlane = NULL;
    linedef_t* updateLineDef = NULL;
    sidedef_t* updateSideDef = NULL;
    surface_t* updateSurface = NULL;
    // subsector_t* updateSubsector = NULL;

    /**
     * \algorithm:
     *
     * When setting a property, reference resolution is done hierarchically so
     * that we can update all listeners of the objects being manipulated should
     * the DMU object's Set routine suggests that a change occured which other
     * DMU objects may wish/need to respond to.
     *
     * 1) Collect references to all current listeners of the object.
     * 2) Pass the change delta on to the object.
     * 3) Object responds:
     *      @c true = update owners, ELSE @c false.
     * 4) If num collected references > 0
     *        recurse, Object = owners[n]
     */

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_SUBSECTOR)
    {
        // updateSubSector = (face_t*) obj;

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
            sector_t* sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            sector_t* sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        updateLineDef = (linedef_t*) obj;

        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            obj = LINE_FRONTSIDE((linedef_t*) obj);
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            sidedef_t* si = LINE_BACKSIDE((linedef_t*) obj);

            if(!si)
                Con_Error("DMU_setProperty: LineDef %i has no back side.\n",
                          P_ToIndex(obj));

            obj = si;
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        updateSideDef = (sidedef_t*) obj;

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

    if(args->type == DMU_SUBSECTOR)
    {
        switch(args->prop)
        {
        case DMU_LIGHT_LEVEL:
        case DMU_MOBJS:
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
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

    case DMU_SEG:
        Seg_SetProperty(obj, args);
        break;

    case DMU_LINEDEF:
        LineDef_SetProperty(obj, args);
        break;

    case DMU_SIDEDEF:
        SideDef_SetProperty(obj, args);
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

    case DMU_SKY:
        Sky_SetProperty(obj, args);
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
                updateSideDef = updateSurface->owner;
                break;

            case DMU_PLANE:
                updatePlane = updateSurface->owner;
                break;

            default:
                Con_Error("SetPropert: Internal error, surface owner unknown.\n");
            }
        }
    }

    if(updateSideDef)
    {
        if(R_UpdateSideDef(updateSideDef, false))
            updateLineDef = updateSideDef->lineDef;
    }

    if(updateLineDef)
    {
        if(R_UpdateLineDef(updateLineDef, false))
        {
            updateSector1 = LINE_FRONTSECTOR(updateLineDef);
            updateSector2 = LINE_BACKSECTOR(updateLineDef);
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

static int getProperty(void* ptr, void* context)
{
    objectrecord_t* r = (objectrecord_t*) ptr;
    setargs_t* args = (setargs_t*) context;
    void* obj = r->obj;

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
            sector_t* sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            sector_t* sec = (sector_t*) obj;
            obj = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            obj = LINE_FRONTSIDE((linedef_t*) obj);
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            sidedef_t* si = LINE_BACKSIDE((linedef_t*) obj);

            if(!si)
                Con_Error("DMU_setProperty: LineDef %i has no back side.\n",
                          P_ToIndex(obj));

            obj = si;
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

    if(args->type == DMU_SUBSECTOR)
    {
        switch(args->prop)
        {
        case DMU_LIGHT_LEVEL:
        case DMU_MOBJS:
            obj = ((subsector_t*) obj)->sector;
            args->type = DMU_SECTOR;
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

    case DMU_SEG:
        Seg_GetProperty(obj, args);
        break;

    case DMU_LINEDEF:
        LineDef_GetProperty(obj, args);
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
        SideDef_GetProperty(obj, args);
        break;

    case DMU_SUBSECTOR:
        Subsector_GetProperty(obj, args);
        break;

    case DMU_MATERIAL:
        Material_GetProperty(obj, args);
        break;

    case DMU_SKY:
        Sky_GetProperty(obj, args);
        break;

    default:
        Con_Error("GetProperty: Type %s not readable.\n", DMU_Str(args->type));
    }

    // Currently no aggregate values are collected.
    return false;
}

void P_SetBool(int type, objectrecordid_t index, uint prop, boolean param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetByte(int type, objectrecordid_t index, uint prop, byte param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetInt(int type, objectrecordid_t index, uint prop, int param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetFixed(int type, objectrecordid_t index, uint prop, fixed_t param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetAngle(int type, objectrecordid_t index, uint prop, angle_t param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetFloat(int type, objectrecordid_t index, uint prop, float param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetPtr(int type, objectrecordid_t index, uint prop, void* param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, setProperty, &args);
}

void P_SetBoolv(int type, objectrecordid_t index, uint prop, boolean* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetBytev(int type, objectrecordid_t index, uint prop, byte* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetIntv(int type, objectrecordid_t index, uint prop, int* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetFixedv(int type, objectrecordid_t index, uint prop, fixed_t* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetAnglev(int type, objectrecordid_t index, uint prop, angle_t* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetFloatv(int type, objectrecordid_t index, uint prop, float* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, setProperty, &args);
}

void P_SetPtrv(int type, objectrecordid_t index, uint prop, void* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, setProperty, &args);
}

/* pointer-based write functions */

void P_SetBoolp(void* ptr, uint prop, boolean param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetBytep(void* ptr, uint prop, byte param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetIntp(void* ptr, uint prop, int param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetFixedp(void* ptr, uint prop, fixed_t param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetAnglep(void* ptr, uint prop, angle_t param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetFloatp(void* ptr, uint prop, float param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetPtrp(void* ptr, uint prop, void* param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetBoolpv(void* ptr, uint prop, boolean* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetBytepv(void* ptr, uint prop, byte* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetIntpv(void* ptr, uint prop, int* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetAnglepv(void* ptr, uint prop, angle_t* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetFloatpv(void* ptr, uint prop, float* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

void P_SetPtrpv(void* ptr, uint prop, void* params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(args.type, ptr, setProperty, &args);
}

boolean P_GetBool(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    boolean returnValue = false;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

byte P_GetByte(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    byte returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

int P_GetInt(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    int returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

fixed_t P_GetFixed(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

angle_t P_GetAngle(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

float P_GetFloat(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    float returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

void* P_GetPtr(int type, objectrecordid_t index, uint prop)
{
    setargs_t args;
    void* returnValue = NULL;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, getProperty, &args);
    return returnValue;
}

void P_GetBoolv(int type, objectrecordid_t index, uint prop, boolean* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetBytev(int type, objectrecordid_t index, uint prop, byte* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetIntv(int type, objectrecordid_t index, uint prop, int* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetFixedv(int type, objectrecordid_t index, uint prop, fixed_t* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetAnglev(int type, objectrecordid_t index, uint prop, angle_t* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetFloatv(int type, objectrecordid_t index, uint prop, float* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, getProperty, &args);
}

void P_GetPtrv(int type, objectrecordid_t index, uint prop, void* params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, getProperty, &args);
}

boolean P_GetBoolp(void* ptr, uint prop)
{
    setargs_t args;
    boolean returnValue = false;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

byte P_GetBytep(void* ptr, uint prop)
{
    setargs_t args;
    byte returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

int P_GetIntp(void* ptr, uint prop)
{
    setargs_t args;
    int returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

fixed_t P_GetFixedp(void* ptr, uint prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

angle_t P_GetAnglep(void* ptr, uint prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

float P_GetFloatp(void* ptr, uint prop)
{
    setargs_t args;
    float returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

void* P_GetPtrp(void* ptr, uint prop)
{
    setargs_t args;
    void* returnValue = NULL;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = &returnValue;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }

    return returnValue;
}

void P_GetBoolpv(void* ptr, uint prop, boolean* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetBytepv(void* ptr, uint prop, byte* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetIntpv(void* ptr, uint prop, int* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetAnglepv(void* ptr, uint prop, angle_t* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetFloatpv(void* ptr, uint prop, float* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

void P_GetPtrpv(void* ptr, uint prop, void* params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = params;
        P_Callbackp(args.type, ptr, getProperty, &args);
    }
}

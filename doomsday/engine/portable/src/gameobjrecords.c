/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * objrecords.c: Object Records.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"

#include "gameobjrecords.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    uint            idx;
    valuetype_t     type;
    uint            valueIdx;
} gameobjrecord_property_t;

typedef struct {
    uint            elmIdx;
    uint            numProperties;
    gameobjrecord_property_t* properties;
} gameobjrecord_t;

typedef struct gameobjrecord_namespace_s {
    struct def_gameobject_s* def;
    uint            numRecords;
    gameobjrecord_t** records;
} gameobjrecord_namespace_t;

typedef struct valuetable_s {
    valuetype_t     type;
    uint            numElements;
    void*           data;
} valuetable_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static valuetable_t* getDBTable(valuedb_t* db, valuetype_t type, boolean canCreate)
{
    uint i;
    valuetable_t* tbl;

    if(!db)
        return NULL;

    for(i = 0; i < db->num; ++i)
    {
        tbl = db->tables[i];
        if(tbl->type == type)
        {   // Found it!
            return tbl;
        }
    }

    if(!canCreate)
        return NULL;

    // We need to add a new value table to the db.
    db->tables = Z_Realloc(db->tables, ++db->num * sizeof(*db->tables), PU_STATIC);
    tbl = db->tables[db->num - 1] = Z_Malloc(sizeof(valuetable_t), PU_STATIC, 0);

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElements = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t* db, valuetype_t type, void* data)
{
    valuetable_t* tbl = getDBTable(db, type, true);

    // Insert the new value.
    switch(type)
    {
    case DDVT_BYTE:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements, PU_STATIC);
        ((byte*) tbl->data)[tbl->numElements - 1] = *((byte*) data);
        break;

    case DDVT_SHORT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(short), PU_STATIC);
        ((short*) tbl->data)[tbl->numElements - 1] = *((short*) data);
        break;

    case DDVT_INT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(int), PU_STATIC);
        ((int*) tbl->data)[tbl->numElements - 1] = *((int*) data);
        break;

    case DDVT_FIXED:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(fixed_t), PU_STATIC);
        ((fixed_t*) tbl->data)[tbl->numElements - 1] = *((fixed_t*) data);
        break;

    case DDVT_ANGLE:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(angle_t), PU_STATIC);
        ((angle_t*) tbl->data)[tbl->numElements - 1] = *((angle_t*) data);
        break;

    case DDVT_FLOAT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(float), PU_STATIC);
        ((float*) tbl->data)[tbl->numElements - 1] = *((float*) data);
        break;

    default:
        Con_Error("insetIntoDB: Unknown value type %d.", type);
    }

    return tbl->numElements - 1;
}

static void* getPtrToDBElm(valuedb_t* db, valuetype_t type, uint elmIdx)
{
    valuetable_t* tbl = getDBTable(db, type, false);

    if(!tbl)
        Con_Error("getPtrToDBElm: Table for type %i not found.", (int) type);

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx < 0 || elmIdx >= tbl->numElements)
        Con_Error("P_GetObjectRecordByte: valueIdx out of bounds.");

    switch(tbl->type)
    {
    case DDVT_BYTE:
        return &(((byte*) tbl->data)[elmIdx]);

    case DDVT_SHORT:
        return &(((short*)tbl->data)[elmIdx]);

    case DDVT_INT:
        return &(((int*) tbl->data)[elmIdx]);

    case DDVT_FIXED:
        return &(((fixed_t*) tbl->data)[elmIdx]);

    case DDVT_ANGLE:
        return &(((angle_t*) tbl->data)[elmIdx]);

    case DDVT_FLOAT:
        return &(((float*) tbl->data)[elmIdx]);

    default:
        Con_Error("P_GetObjectRecordByte: Invalid table type %i.", tbl->type);
    }

    // Should never reach here.
    return NULL;
}

static gameobjrecord_namespace_t*
getGameObjectRecordNamespace(gameobjrecords_t* records, def_gameobject_t* def)
{
    uint i;

    for(i = 0; i < records->numNamespaces; ++i)
        if(records->namespaces[i].def == def)
            return &records->namespaces[i];

    return NULL;
}

static gameobjrecord_t* findRecord(gameobjrecords_t* records,
                                   def_gameobject_t* def, uint elmIdx,
                                   boolean canCreate)
{
    gameobjrecord_namespace_t* rnamespace;
    gameobjrecord_t* record;
    uint i;

    assert(records);

    if(!records->numNamespaces)
    {   // We haven't yet created the lists.
        records->numNamespaces = P_NumGameObjectDefs();
        records->namespaces = Z_Malloc(sizeof(*rnamespace) * records->numNamespaces, PU_STATIC, 0);
        for(i = 0; i < records->numNamespaces; ++i)
        {
            rnamespace = &records->namespaces[i];

            rnamespace->def = P_GetGameObjectDef(i);
            rnamespace->records = NULL;
            rnamespace->numRecords = 0;
        }
    }

    rnamespace = getGameObjectRecordNamespace(records, def);
    assert(rnamespace);

    // Have we already created this record?
    for(i = 0; i < rnamespace->numRecords; ++i)
    {
        record = rnamespace->records[i];
        if(record->elmIdx == elmIdx)
            return record; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new record.
    rnamespace->records = Z_Realloc(rnamespace->records,
        ++rnamespace->numRecords * sizeof(gameobjrecord_t*), PU_STATIC);

    record = (gameobjrecord_t*) Z_Malloc(sizeof(*record), PU_STATIC, 0);
    record->elmIdx = elmIdx;
    record->numProperties = 0;
    record->properties = NULL;

    rnamespace->records[rnamespace->numRecords - 1] = record;

    return record;
}

static uint numGameObjectRecords(gameobjrecords_t* records, int identifier)
{
    if(records)
    {
        uint i;
        for(i = 0; i < records->numNamespaces; ++i)
        {
            gameobjrecord_namespace_t* rnamespace = &records->namespaces[i];

            if(rnamespace->def->identifier == identifier)
                return rnamespace->numRecords;
        }
    }

    return 0;
}

static void* getValueForGameObjectRecordProperty(gameobjrecords_t* records, int identifier, uint elmIdx,
                                                 int propIdentifier, valuetype_t* type)
{
    uint i;
    def_gameobject_t* def;
    gameobjrecord_t* record;

    if((def = P_GameObjectDef(identifier, NULL, false)) == NULL)
        Con_Error("P_GetObjectRecordByte: Invalid identifier %i.", identifier);

    if((record = findRecord(records, def, elmIdx, false)) == NULL)
        Con_Error("P_GetObjectRecordByte: There is no element %i of type %s.", elmIdx,
                  def->name);

    // Find the requested property.
    for(i = 0; i < record->numProperties; ++i)
    {
        gameobjrecord_property_t* rproperty = &record->properties[i];

        if(def->properties[rproperty->idx].identifier == propIdentifier)
        {
            void* ptr;

            if(NULL ==
               (ptr = getPtrToDBElm(&records->values, rproperty->type, rproperty->valueIdx)))
                Con_Error("P_GetObjectRecordByte: Failed db look up.");

            if(type)
                *type = rproperty->type;

            return ptr;
        }
    }

    return NULL;
}

/**
 * Handle some basic type conversions.
 */
static void setValue(void* dst, valuetype_t dstType, void* src, valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = (*((byte*) src) << FRACBITS);
            break;
        case DDVT_INT:
            *d = (*((int*) src) << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = *((fixed_t*) src);
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(*((float*) src));
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(*((fixed_t*) src));
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_FLOAT:
            *d = (byte) *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", dstType);
    }
}

gameobjrecords_t* P_CreateGameObjectRecords(void)
{
    gameobjrecords_t* records = Z_Calloc(sizeof(*records), PU_STATIC, 0);
    return records;
}

void P_DestroyGameObjectRecords(gameobjrecords_t* records)
{
    assert(records);

    if(records->namespaces)
    {
        uint i;
        for(i = 0; i < records->numNamespaces; ++i)
        {
            gameobjrecord_namespace_t* rnamespace = &records->namespaces[i];
            uint j;

            for(j = 0; j < rnamespace->numRecords; ++j)
            {
                gameobjrecord_t* record = rnamespace->records[j];

                if(record->properties)
                    Z_Free(record->properties);

                Z_Free(record);
            }
        }

        Z_Free(records->namespaces);
    }
    records->namespaces = NULL;

    if(records->values.tables)
    {
        uint i;
        for(i = 0; i < records->values.num; ++i)
        {
            valuetable_t* tbl = records->values.tables[i];

            if(tbl->data)
                Z_Free(tbl->data);

            Z_Free(tbl);
        }

        Z_Free(records->values.tables);
    }
    records->values.tables = NULL;
    records->values.num = 0;
}

uint GameObjRecords_Num(gameobjrecords_t* records, int identifier)
{
    assert(records);
    return numGameObjectRecords(records, identifier);
}

void GameObjRecords_Update(gameobjrecords_t* records, def_gameobject_t* def,
                           uint propIdx, uint elmIdx, valuetype_t type,
                           void* data)
{
    gameobjrecord_property_t* prop;
    gameobjrecord_t* record;
    uint i;

    assert(records);

    record = findRecord(records, def, elmIdx, true);

    if(!record)
        Con_Error("GameObjRecords_GetFloat: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < record->numProperties; ++i)
    {
        if(record->properties[i].idx == propIdx)
        {   // We are updating.
            Con_Error("addGameMapObj: Value type change not currently supported.");
            return;
        }
    }

    // Its a new property value.
    record->properties = Z_Realloc(record->properties,
        ++record->numProperties * sizeof(*record->properties), PU_STATIC);

    prop = &record->properties[record->numProperties - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&records->values, type, data);
}

byte GameObjRecords_GetByte(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    byte returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_BYTE, ptr, type);

    return returnVal;
}

short GameObjRecords_GetShort(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    short returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_SHORT, ptr, type);

    return returnVal;
}

int GameObjRecords_GetInt(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    int returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_INT, ptr, type);

    return returnVal;
}

fixed_t GameObjRecords_GetFixed(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    fixed_t returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_FIXED, ptr, type);

    return returnVal;
}

angle_t GameObjRecords_GetAngle(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    angle_t returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_ANGLE, ptr, type);

    return returnVal;
}

float GameObjRecords_GetFloat(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    float returnVal = 0;

    assert(records);

    if((ptr = getValueForGameObjectRecordProperty(records, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_FLOAT, ptr, type);

    return returnVal;
}

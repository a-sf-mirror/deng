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
 * p_data.c: Playsim Data Structures, Macros and Constants
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_dam.h"
#include "de_edit.h"
#include "de_defs.h"

#include "rend_bias.h"
#include "m_bams.h"

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/**
 * These map data arrays are internal to the engine.
 */
char mapID[9]; // Name by which the game referred to the current map.
uint numVertexes = 0;
vertex_t** vertexes = NULL;

uint numHEdges = 0;
hedge_t** hEdges = NULL;

uint numSectors = 0;
sector_t** sectors = NULL;

uint numFaces = 0;
face_t** faces = NULL;

uint numNodes = 0;
node_t** nodes = NULL;

uint numLineDefs = 0;
linedef_t** lineDefs = NULL;

uint numSideDefs = 0;
sidedef_t** sideDefs = NULL;

watchedplanelist_t* watchedPlaneList = NULL;
surfacelist_t* movingSurfaceList = NULL;
surfacelist_t* decoratedSurfaceList = NULL;

blockmap_t* BlockMap = NULL;
blockmap_t* SubSectorBlockMap = NULL;

nodepile_t* mobjNodes = NULL, *lineNodes = NULL; // All kinds of wacky links.

float mapGravity;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gamemap_t* currentMap = NULL;

// Game-specific, map object type definitions.
static uint numGameMapObjDefs;
static gamemapobjdef_t* gameMapObjDefs;

// CODE --------------------------------------------------------------------

void P_PolyobjChanged(polyobj_t* po)
{
    uint                i;
    gamemap_t*          map = P_GetCurrentMap();

    // Shadow bias must be told.
    for(i = 0; i < po->numSegs; ++i)
    {
        linedef_t*          line = ((dmuobjrecord_t*) po->lineDefs[i])->obj;
        poseg_t*            seg = &po->segs[i];

        if(seg->bsuf)
            SB_SurfaceMoved(map, seg->bsuf);
    }
}

/**
 * Generate a 'unique' identifier for the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char* P_GenerateUniqueMapID(const char* mapID)
{
    static char         uid[255];
    filename_t          base;
    int                 lump = W_GetNumForName(mapID);

    M_ExtractFileBase(base, W_LumpSourceFile(lump), FILENAME_T_MAXLEN);

    dd_snprintf(uid, 255, "%s|%s|%s|%s", mapID,
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            (char *) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/**
 * @return              Ptr to the current map.
 */
gamemap_t *P_GetCurrentMap(void)
{
    return currentMap;
}

void P_SetCurrentMap(gamemap_t* map)
{
    strncpy(mapID, map->mapID, sizeof(mapID));

    numVertexes = map->numVertexes;
    vertexes = map->vertexes;

    numHEdges = map->numHEdges;
    hEdges = map->hEdges;

    numSectors = map->numSectors;
    sectors = map->sectors;

    numFaces = map->numFaces;
    faces = map->faces;

    numNodes = map->numNodes;
    nodes = map->nodes;

    numLineDefs = map->numLineDefs;
    lineDefs = map->lineDefs;

    numSideDefs = map->numSideDefs;
    sideDefs = map->sideDefs;

    watchedPlaneList = &map->watchedPlaneList;
    movingSurfaceList = &map->movingSurfaceList;
    decoratedSurfaceList = &map->decoratedSurfaceList;

    numPolyObjs = map->numPolyObjs;
    polyObjs = map->polyObjs;

    mobjNodes = &map->mobjNodes;
    lineNodes = &map->lineNodes;
    linelinks = map->lineLinks;

    BlockMap = map->blockMap;
    SubSectorBlockMap = map->subSectorBlockMap;

    mapGravity = map->globalGravity;

    currentMap = map;
}

void P_DestroyMap(gamemap_t* map)
{
    biassurface_t* bsuf;

    if(!map)
        return;

    DL_DestroyDynlights(map);

    /**
     * @todo handle vertexillum allocation/destruction along with the surfaces,
     * utilizing global storage.
     */
    for(bsuf = map->bias.surfaces; bsuf; bsuf = bsuf->next)
    {
        if(bsuf->illum)
            Z_Free(bsuf->illum);
    }

    SB_DestroySurfaces(map);

    if(map->vertexes)
    {
        uint i;

        for(i = 0; i < map->numVertexes; ++i)
        {
            vertex_t* vertex = map->vertexes[i];
            Z_Free(vertex);
        }
        Z_Free(map->vertexes);
    }
    map->vertexes = NULL;
    map->numVertexes = 0;

    if(map->sideDefs)
    {
        uint i;

        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t* sideDef = map->sideDefs[i];
            Z_Free(sideDef);
        }
        Z_Free(map->sideDefs);
    }
    map->sideDefs = NULL;
    map->numSideDefs = 0;

    if(map->lineDefs)
    {
        uint i;
        for(i = 0; i < map->numLineDefs; ++i)
        {
            linedef_t* line = map->lineDefs[i];
            Z_Free(line);
        }
        Z_Free(map->lineDefs);
    }
    map->lineDefs = NULL;
    map->numLineDefs = 0;

    if(map->sectors)
    {
        uint i;
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sector = map->sectors[i];
            uint j;

            if(sector->planes)
            {
                for(j = 0; j < sector->planeCount; ++j)
                {
                    Z_Free(sector->planes[j]);
                }
                Z_Free(sector->planes);
            }

            if(sector->faces)
                Z_Free(sector->faces);
            if(sector->reverbFaces)
                Z_Free(sector->reverbFaces);
            if(sector->blocks)
                Z_Free(sector->blocks);
            if(sector->lineDefs)
                Z_Free(sector->lineDefs);

            Z_Free(sector);
        }

        Z_Free(map->sectors);
    }
    map->sectors = NULL;
    map->numSectors = 0;

    if(map->faces)
    {
        uint i;

        for(i = 0; i < map->numFaces; ++i)
        {
            face_t* face = map->faces[i];
            subsector_t* subsector = (subsector_t*) face->data;

            if(subsector)
            {
                /*shadowlink_t* slink;
                while((slink = subSector->shadows))
                {
                    subSector->shadows = slink->next;
                    Z_Free(slink);
                }*/
                
                Z_Free(subsector);
            }

            Z_Free(face);
        }

        Z_Free(map->faces);
    }
    map->faces = NULL;
    map->numFaces = 0;

    if(map->hEdges)
    {
        uint i;

        for(i = 0; i < map->numHEdges; ++i)
        {
            hedge_t* hEdge = map->hEdges[i];

            if(hEdge->data)
               Z_Free(hEdge->data);

            Z_Free(hEdge);
        }

        Z_Free(map->hEdges);
    }
    map->hEdges = NULL;
    map->numHEdges = 0;

    if(map->nodes)
    {
        uint i;

        for(i = 0; i < map->numNodes; ++i)
        {
            node_t* node = map->nodes[i];
            Z_Free(node);
        }
        Z_Free(map->nodes);
    }
    map->nodes = NULL;
    map->numNodes = 0;
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char* P_GetMapID(gamemap_t* map)
{
    if(!map)
        return NULL;

    return map->mapID;
}

/**
 * @return              The 'unique' identifier of the map.
 */
const char* P_GetUniqueMapID(gamemap_t* map)
{
    if(!map)
        return NULL;

    return map->uniqueID;
}

void P_GetMapBounds(gamemap_t* map, float* min, float* max)
{
    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int P_GetMapAmbientLightLevel(gamemap_t* map)
{
    if(!map)
        return 0;

    return map->ambientLightLevel;
}

extern gamemap_t* DAM_LoadMap(const char* mapID);

/**
 * Begin the process of loading a new map.
 * Can be accessed by the games via the public API.
 *
 * @param levelId       Identifier of the map to be loaded (eg "E1M1").
 *
 * @return              @c true, if the map was loaded successfully.
 */
boolean P_LoadMap(const char* mapID)
{
    uint                i;
    gamemap_t*          map = NULL;

    if(!mapID || !mapID[0])
        return false; // Yeah, ok... :P

    Con_Message("P_LoadMap: \"%s\"\n", mapID);

    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t*           plr = &ddPlayers[i];

            if(!(plr->shared.flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    if(DAM_TryMapConversion(mapID))
    {
        ddstring_t*         s = DAM_ComposeArchiveMapFilepath(mapID);

        map = MPE_GetLastBuiltMap();

        //DAM_MapWrite(map, Str_Text(s));

        Str_Delete(s);
    }
    else
        return false;

    if(1)//(map = DAM_LoadMap(mapID)))
    {
        ded_sky_t*          skyDef = NULL;
        ded_mapinfo_t*      mapInfo;

        SBE_InitForMap(map);

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        R_InitLinks(map);

        R_BuildSectorLinks(map);

        // Init blockmap for searching subsectors.
        P_BuildSubSectorBlockMap(map);

        // Init the watched object lists.
        memset(&map->watchedPlaneList, 0, sizeof(map->watchedPlaneList));
        memset(&map->movingSurfaceList, 0, sizeof(map->movingSurfaceList));
        memset(&map->decoratedSurfaceList, 0, sizeof(map->decoratedSurfaceList));

        strncpy(map->mapID, mapID, 8);
        strncpy(map->uniqueID, P_GenerateUniqueMapID(mapID),
                sizeof(map->uniqueID));

        // See what mapinfo says about this map.
        mapInfo = Def_GetMapInfo(map->mapID);
        if(!mapInfo)
            mapInfo = Def_GetMapInfo("*");

        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef)
                skyDef = &mapInfo->sky;
        }

        R_SetupSky(theSky, skyDef);

        // Setup accordingly.
        if(mapInfo)
        {
            map->globalGravity = mapInfo->gravity;
            map->ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found, so set some basic stuff.
            map->globalGravity = 1.0f;
            map->ambientLightLevel = 0;
        }

        // \todo should be called from P_LoadMap() but R_InitMap requires the
        // currentMap to be set first.
        P_SetCurrentMap(map);

        R_InitSectorShadows();

        {
        uint                startTime = Sys_GetRealTime();

        R_InitSkyFix();

        // How much time did we spend?
        VERBOSE(Con_Message
                ("  InitialSkyFix: Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - startTime) / 1000.0f));
        }
        }
        {
        uint                i;
        gamemap_t*          map = P_GetCurrentMap();

        // Init the thinker lists (public and private).
        P_InitThinkerLists(0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(map);

        Cl_Reset();
        RL_DeleteLists();
        R_CalcLightModRange(NULL);

        // Invalidate old cmds and init player values.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t           *plr = &ddPlayers[i];

            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        Materials_RewindAnimationGroups();

        R_InitObjLinksForMap();
        LO_InitForMap(); // Lumobj management.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        // Init Particle Generator links.
        P_PtcInitForMap();

        // Initialize the lighting grid.
        LG_Init();

        if(!isDedicated)
            R_InitRendVerticesPool();
        return true;
    }

    return false;
}

/**
 * Look up a mapobj definition.
 *
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
 */
gamemapobjdef_t* P_GetGameMapObjDef(int identifier, const char *objName,
                                    boolean canCreate)
{
    uint                i;
    size_t              len;
    gamemapobjdef_t*    def;

    if(objName)
        len = strlen(objName);

    // Is this a known game object?
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];

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
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];
        if(!strnicmp(objName, def->name, len))
        {   // Oh dear, a duplicate.
            return NULL;
        }
    }

    gameMapObjDefs =
        M_Realloc(gameMapObjDefs, ++numGameMapObjDefs * sizeof(*gameMapObjDefs));

    def = &gameMapObjDefs[numGameMapObjDefs - 1];
    def->identifier = identifier;
    def->name = M_Malloc(len+1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProps = 0;
    def->props = NULL;

    return def;
}

/**
 * Called by the game to register the map object types it wishes us to make
 * public via the MPE interface.
 */
boolean P_RegisterMapObj(int identifier, const char *name)
{
    if(P_GetGameMapObjDef(identifier, name, true))
        return true; // Success.

    return false;
}

/**
 * Called by the game to add a new property to a previously registered
 * map object type definition.
 */
boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                 const char *propName, valuetype_t type)
{
    uint                    i;
    size_t                  len;
    mapobjprop_t           *prop;
    gamemapobjdef_t        *def = P_GetGameMapObjDef(identifier, NULL, false);

    if(!def) // Not a valid identifier.
    {
        Con_Error("P_RegisterMapObjProperty: Unknown mapobj identifier %i.",
                  identifier);
    }

    if(propIdentifier == 0) // Not a valid identifier.
        Con_Error("P_RegisterMapObjProperty: 0 not valid for propIdentifier.");

    if(!propName || !propName[0]) // Must have a name.
        Con_Error("P_RegisterMapObjProperty: Cannot register without name.");

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
        Con_Error("P_RegisterMapObjProperty: Unknown/not supported value type %i.",
                  type);
    }

    // Next, make sure propIdentifer and propName are unique.
    len = strlen(propName);
    for(i = 0; i < def->numProps; ++i)
    {
        prop = &def->props[i];

        if(prop->identifier == propIdentifier)
            Con_Error("P_RegisterMapObjProperty: propIdentifier %i not unique for %s.",
                      propIdentifier, def->name);

        if(!strnicmp(propName, prop->name, len))
            Con_Error("P_RegisterMapObjProperty: propName \"%s\" not unique for %s.",
                      propName, def->name);
    }

    // Looks good! Add it to the list of properties.
    def->props = M_Realloc(def->props, ++def->numProps * sizeof(*def->props));

    prop = &def->props[def->numProps - 1];
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
void P_InitGameMapObjDefs(void)
{
    gameMapObjDefs = NULL;
    numGameMapObjDefs = 0;
}

/**
 * Called at shutdown to free all memory allocated for the map obj defs.
 */
void P_ShutdownGameMapObjDefs(void)
{
    uint                i, j;

    if(gameMapObjDefs)
    {
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjdef_t    *def = &gameMapObjDefs[i];

            for(j = 0; j < def->numProps; ++j)
            {
                mapobjprop_t       *prop = &def->props[j];

                M_Free(prop->name);
            }

            M_Free(def->name);
            M_Free(def->props);
        }

        M_Free(gameMapObjDefs);
    }

    gameMapObjDefs = NULL;
    numGameMapObjDefs = 0;
}

static valuetable_t *getDBTable(valuedb_t *db, valuetype_t type,
                                boolean canCreate)
{
    uint                i;
    valuetable_t       *tbl;

    if(!db)
        return NULL;

    for(i = 0; i < db->numTables; ++i)
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
    db->tables = M_Realloc(db->tables, ++db->numTables * sizeof(*db->tables));
    tbl = db->tables[db->numTables - 1] = M_Malloc(sizeof(valuetable_t));

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElms = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t *db, valuetype_t type, void *data)
{
    valuetable_t       *tbl = getDBTable(db, type, true);

    // Insert the new value.
    switch(type)
    {
    case DDVT_BYTE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms);
        ((byte*) tbl->data)[tbl->numElms - 1] = *((byte*) data);
        break;

    case DDVT_SHORT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(short));
        ((short*) tbl->data)[tbl->numElms - 1] = *((short*) data);
        break;

    case DDVT_INT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(int));
        ((int*) tbl->data)[tbl->numElms - 1] = *((int*) data);
        break;

    case DDVT_FIXED:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(fixed_t));
        ((fixed_t*) tbl->data)[tbl->numElms - 1] = *((fixed_t*) data);
        break;

    case DDVT_ANGLE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(angle_t));
        ((angle_t*) tbl->data)[tbl->numElms - 1] = *((angle_t*) data);
        break;

    case DDVT_FLOAT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(float));
        ((float*) tbl->data)[tbl->numElms - 1] = *((float*) data);
        break;

    default:
        Con_Error("insetIntoDB: Unknown value type %d.", type);
    }

    return tbl->numElms - 1;
}

static void* getPtrToDBElm(valuedb_t *db, valuetype_t type, uint elmIdx)
{
    valuetable_t       *tbl = getDBTable(db, type, false);

    if(!tbl)
        Con_Error("getPtrToDBElm: Table for type %i not found.", (int) type);

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx < 0 || elmIdx >= tbl->numElms)
        Con_Error("P_GetGMOByte: valueIdx out of bounds.");

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
        Con_Error("P_GetGMOByte: Invalid table type %i.", tbl->type);
    }

    // Should never reach here.
    return NULL;
}

/**
 * Destroy the given game map obj database.
 */
void P_DestroyGameMapObjDB(gameobjdata_t *moData)
{
    uint                i, j;

    if(moData->objLists)
    {
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t*       objList = &moData->objLists[i];

            for(j = 0; j < objList->num; ++j)
            {
                gamemapobj_t       *gmo = objList->objs[j];

                if(gmo->props)
                    M_Free(gmo->props);

                M_Free(gmo);
            }
        }

        M_Free(moData->objLists);
    }
    moData->objLists = NULL;

    if(moData->db.tables)
    {
        for(i = 0; i < moData->db.numTables; ++i)
        {
            valuetable_t       *tbl = moData->db.tables[i];

            if(tbl->data)
                M_Free(tbl->data);

            M_Free(tbl);
        }

        M_Free(moData->db.tables);
    }
    moData->db.tables = NULL;
    moData->db.numTables = 0;
}

static uint countGameMapObjs(gameobjdata_t *moData, int identifier)
{
    if(moData)
    {
        uint                i;

        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t*   objList = &moData->objLists[i];

            if(objList->def->identifier == identifier)
                return objList->num;
        }
    }

    return 0;
}

uint P_CountGameMapObjs(int identifier)
{
    gamemap_t*          map = P_GetCurrentMap();

    return countGameMapObjs(&map->gameObjData, identifier);
}

static gamemapobjlist_t* getMapObjList(gameobjdata_t* moData,
                                       gamemapobjdef_t* def)
{
    uint                i;

    for(i = 0; i < numGameMapObjDefs; ++i)
        if(moData->objLists[i].def == def)
            return &moData->objLists[i];

    return NULL;
}

gamemapobj_t* P_GetGameMapObj(gameobjdata_t *moData, gamemapobjdef_t *def,
                              uint elmIdx, boolean canCreate)
{
    uint                i;
    gamemapobj_t*       gmo;
    gamemapobjlist_t*   objList;

    if(!moData->objLists)
    {   // We haven't yet created the lists.
        moData->objLists = M_Malloc(sizeof(*objList) * numGameMapObjDefs);
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            objList = &moData->objLists[i];

            objList->def = &gameMapObjDefs[i];
            objList->objs = NULL;
            objList->num = 0;
        }
    }

    objList = getMapObjList(moData, def);
    assert(objList);

    // Have we already created this gmo?
    for(i = 0; i < objList->num; ++i)
    {
        gmo = objList->objs[i];
        if(gmo->elmIdx == elmIdx)
            return gmo; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new gamemapobj.
    objList->objs =
        M_Realloc(objList->objs, ++objList->num * sizeof(gamemapobj_t*));

    gmo = objList->objs[objList->num - 1] = M_Malloc(sizeof(*gmo));
    gmo->elmIdx = elmIdx;
    gmo->numProps = 0;
    gmo->props = NULL;

    return gmo;
}

void P_AddGameMapObjValue(gameobjdata_t *moData, gamemapobjdef_t *gmoDef,
                          uint propIdx, uint elmIdx, valuetype_t type,
                          void *data)
{
    uint                i;
    customproperty_t   *prop;
    gamemapobj_t       *gmo = P_GetGameMapObj(moData, gmoDef, elmIdx, true);

    if(!gmo)
        Con_Error("addGameMapObj: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < gmo->numProps; ++i)
    {
        if(gmo->props[i].idx == propIdx)
        {   // We are updating.
            //if(gmo->props[i].type == type)
            //    updateInDB(map->values, type, gmo->props[i].valueIdx, data);
            //else
                Con_Error("addGameMapObj: Value type change not currently supported.");
            return;
        }
    }

    // Its a new value.
    gmo->props = M_Realloc(gmo->props, ++gmo->numProps * sizeof(*gmo->props));

    prop = &gmo->props[gmo->numProps - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&moData->db, type, data);
}

static void* getGMOPropValue(gameobjdata_t *data, int identifier,
                             uint elmIdx, int propIdentifier,
                             valuetype_t *type)
{
    uint                i;
    gamemapobjdef_t    *def;
    gamemapobj_t       *gmo;

    if((def = P_GetGameMapObjDef(identifier, NULL, false)) == NULL)
        Con_Error("P_GetGMOByte: Invalid identifier %i.", identifier);

    if((gmo = P_GetGameMapObj(data, def, elmIdx, false)) == NULL)
        Con_Error("P_GetGMOByte: There is no element %i of type %s.", elmIdx,
                  def->name);

    // Find the requested property.
    for(i = 0; i < gmo->numProps; ++i)
    {
        customproperty_t   *prop = &gmo->props[i];

        if(def->props[prop->idx].identifier == propIdentifier)
        {
            void               *ptr;

            if(NULL ==
               (ptr = getPtrToDBElm(&data->db, prop->type, prop->valueIdx)))
                Con_Error("P_GetGMOByte: Failed db look up.");

            if(type)
                *type = prop->type;

            return ptr;
        }
    }

    return NULL;
}

/**
 * Handle some basic type conversions.
 */
static void setValue(void *dst, valuetype_t dstType, void *src,
                     valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t        *d = dst;

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
        float          *d = dst;

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
        byte           *d = dst;

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
        int            *d = dst;

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
        short          *d = dst;

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
        angle_t        *d = dst;

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

byte P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    byte                returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_BYTE, ptr, type);

    return returnVal;
}

short P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    short               returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_SHORT, ptr, type);

    return returnVal;
}

int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    int                 returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type)))
        setValue(&returnVal, DDVT_INT, ptr, type);

    return returnVal;
}

fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    fixed_t             returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_FIXED, ptr, type);

    return returnVal;
}

angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    angle_t             returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_ANGLE, ptr, type);

    return returnVal;
}

float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    float               returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_FLOAT, ptr, type);

    return returnVal;
}

/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/App"
#include "de/Library"
#include "de/Writer"
#include "de/Reader"
#include "de/Object"
#include "de/User"
#include "de/Polyobj"
#include "de/Map"
#include "de/NodeBuilder"
#include "de/HalfEdgeInfo"

using namespace de;

namespace
{
Vertex* rootVtx; // Used when sorting vertex line owners.

/// \todo Should these be in de::World?
Map::ownernode_t* unusedNodeList = NULL;

struct losdata_t {
    dint flags; // LS_* flags @see lineSightFlags
    Line2i trace;
    dfloat startZ; // Eye z of looker.
    dfloat topSlope; // Slope to top of target.
    dfloat bottomSlope; // Slope to bottom of target.
    MapRectanglef aaBounds;
    Vector3f to;
};

/*zblockset_t *shadowLinksBlockSet;
struct shadowlinkerparms_t {
    LineDef* lineDef;
    dbyte side;
};*/

bool updateVertexShadowOffsets(Vertex* vertex, void* paramaters)
{
    reinterpret_cast<MVertex*>(vertex->data)->updateShadowOffsets();
    return true; // Continue iteration.
}

#if 0
/**
 * Link a seg to an arbitary subsector for the purposes of shadowing.
 */
void linkShadowLineDefToSubsector(LineDef* line, dbyte side, Subsector_t* subsector)
{
    shadowlink_t* link;

#ifdef _DEBUG
// Check the links for dupes!
for(shadowlink_t* i = subsector->shadows; i; i = i->next)
    if(i->lineDef == line && i->side == side)
        LOG_ERROR("linkShadowLineDefToSubsector: Already here!!");
#endif

    // We'll need to allocate a new link.
    link = Z_BlockNewElement(shadowLinksBlockSet);

    // The links are stored into a linked list.
    link->next = subsector->shadows;
    subsector->shadows = link;
    link->lineDef = line;
    link->side = side;
}

/**
 * If the shadow polygon (parm) contacts the subsector, link the poly
 * to the subsector's shadow list.
 */
bool RIT_ShadowSubsectorLinker(Subsector* subsector, void* paramaters)
{
    shadowlinkerparms_t* data = reinterpret_cast<shadowlinkerparms_t*>(paramaters);
    linkShadowLineDefToSubsector(data->lineDef, data->side, subsector);
    return true;
}
#endif
}

Map::Map()
  : _thinkersFrozen(0),
    _editActive(true)
    /*_bias.editGrabbedID(-1)*/
{
    _halfEdgeDS = new HalfEdgeDS();
    //_lightGrid = P_CreateLightGrid();
    //_dlights.linkList = Z_Calloc(sizeof(dynlist_t), PU_STATIC, 0);
}

static bool destroyLinkedSubsector(Face* face, void* paramaters)
{
    Subsector* subsector = reinterpret_cast<Subsector*>(face->data);
    if(subsector) delete subsector;
    face->data = NULL;
    return true; // Continue iteration.
}

static bool destroyLinkedSeg(HalfEdge* halfEdge, void* paramaters)
{
    Seg* seg = reinterpret_cast<Seg*>(halfEdge->data);
    if(seg) delete seg;
    halfEdge->data = NULL;
    return true; // Continue iteration.
}

static bool destroyLinkedVertexInfo(Vertex* vertex, void* paramaters)
{
    MVertex* vinfo = reinterpret_cast<MVertex*>(vertex->data);
    if(vinfo) delete vinfo;
    vertex->data = NULL;
    return true; // Continue iteration.
}

static void destroyHalfEdgeDS(HalfEdgeDS* halfEdgeDS)
{
    halfEdgeDS->iterateFaces(destroyLinkedSubsector);
    halfEdgeDS->iterateHalfEdges(destroyLinkedSeg);
    halfEdgeDS->iterateVertices(destroyLinkedVertexInfo);

    delete halfEdgeDS;
}

static bool C_DECL freeNode(BinaryTree<void*>* tree, void* paramaters)
{
    if(!tree->isLeaf())
    {
        Node* node;
        if((node = reinterpret_cast<Node*>(tree->data())))
            delete node;
    }
    tree->setData(NULL);
    return true; // Continue iteration.
}

Map::~Map()
{
    clear();

#if 0
    DL_DestroyDynlights(_dlights.linkList);
    Z_Free(_dlights.linkList);
    _dlights.linkList = NULL;

    /**
     * @todo handle vertexillum allocation/destruction along with the surfaces,
     * utilizing global storage.
     */
    {
    biassurface_t* bsuf;
    for(bsuf = map->_bias.surfaces; bsuf; bsuf = bsuf->next)
    {
        if(bsuf->illum)
            Z_Free(bsuf->illum);
    }
    }

    SB_DestroySurfaces(map);
    if(map->_bias.sources)
        SB_DestroySourceList(map->_bias.sources);
    map->_bias.sources = NULL;

    if(map->_lightGrid)
        P_DestroyLightGrid(map->_lightGrid);
    map->_lightGrid = NULL;
#endif

    _watchedPlanes.clear();
    _movingSurfaces.clear();
    _decoratedSurfaces.clear();

    if(_thingBlockmap)
        delete _thingBlockmap; _thingBlockmap = NULL;

    if(_lineDefBlockmap)
        delete _lineDefBlockmap; _lineDefBlockmap = NULL;

    if(_subsectorBlockmap)
        delete _subsectorBlockmap; _subsectorBlockmap = NULL;

    if(_particleBlockmap)
        delete _particleBlockmap; _particleBlockmap = NULL;

    if(_lumobjBlockmap)
        delete _lumobjBlockmap; _lumobjBlockmap = NULL;

    if(_subsectorContacts)
        delete _subsectorContacts; _subsectorContacts = NULL;

    if(thingNodes)
        delete thingNodes; thingNodes = NULL;

    if(lineDefNodes)
        delete lineDefNodes; lineDefNodes = NULL;

    if(lineDefLinks)
        std::free(lineDefLinks); lineDefLinks = NULL;

    FOR_EACH(i, _sideDefs, SideDefs::iterator)
        delete *i;
    _sideDefs.clear();

    FOR_EACH(i, _lineDefs, LineDefs::iterator)
        delete *i;
    _lineDefs.clear();

    if(lineOwners)
        std::free(lineOwners);
    lineOwners = NULL;

    FOR_EACH(i, _polyobjs, Polyobjs::iterator)
        delete *i;
    _polyobjs.clear();

    FOR_EACH(i, _sectors, Sectors::iterator)
        delete *i;
    _sectors.clear();

    FOR_EACH(i, _planes, Planes::iterator)
        delete *i;
    _planes.clear();

    _segs.clear();
    _subsectors.clear();
    _nodes.clear();

    destroyHalfEdgeDS(_halfEdgeDS);
    _halfEdgeDS = NULL;

    // Free the BSP tree.
    if(_rootNode)
    {
        _rootNode->postOrder(freeNode);
        delete _rootNode;
    }

    if(&App::currentMap() == this)
        App::setCurrentMap(NULL);
}

void Map::load(const String& name)
{
    LOG_VERBOSE("Map::load: Attempt ") << name;

    // It would be very cool if  loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the (s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

#if 0
    if(isServer)
    {
        // Whenever the  changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(dint i = 0; i < DDMAXPLAYERS; ++i)
        {
            User* plr = &ddPlayers[i];

            if(!(plr->shared.flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }
#endif

    if(!DAM_TryMapConversion(name))
        return;

    ded_sky_t* skyDef = NULL;
    ded_mapinfo_t* mapInfo;

    // Initialize The Logical Sound Manager.
    S_MapChange();

    // Call the game's setup routines.
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DMU_LINEDEF);
        gx.SetupForMapData(DMU_SIDEDEF);
        gx.SetupForMapData(DMU_SECTOR);
    }

    // Must be called before any mobjs are spawned.
    initLinks();

    findDominantLightSources();

    buildThingBlockmap();
    buildSubsectorBlockmap();
    buildParticleBlockmap();
    buildLumobjBlockmap();

    initSubsectorContacts();

    // See what mapinfo says about this .
    mapInfo = Def_GetMapInfo(name);
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
        _gravity = mapInfo->gravity;
        _lightIntensity = mapInfo->ambient * 255;
    }
    else
    {
        // No info found, so set some basic stuff.
        _gravity = 1.0f;
        _lightIntensity = 0;
    }

    halfEdgeDS().iterateVertices(updateVertexShadowOffsets);

    R_InitSectorShadows();

    initSkyFix();

    // Tell shadow bias to initialize the bias light sources.
    SB_InitForMap();

    Cl_Reset();
    RL_DeleteLists();
    R_CalcLightModRange(NULL);

    // Invalidate old cmds and init player values.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        User* user = &ddPlayers[i];

        if(isServer && user->shared.inGame)
            clients[i].runTime = SECONDS_TO_TICKS(gameTime);

        user->extraLight = user->targetExtraLight = 0;
        user->extraLightCounter = 0;
    }

    // Make sure that the next frame doesn't use a filtered viewer.
    R_ResetViewer();

    // Texture animations should begin from their first step.
    Materials_RewindAnimationGroups();

    LO_InitForMap(); // Lumobj management.
    VL_InitForMap(); // Converted vlights (from lumobjs) management.

    initGenerators();

    // Initialize the lighting grid.
    LightGrid_Init();

    /// Success!
    _name = name;
}

bool Map::isVoid() const
{
    return _name.empty();
}

Object& Map::newObject()
{
    return addAs<Object>(GAME_SYMBOL(deng_NewObject)());
}

/*static int destroyGenerator(generator_t* gen, void* paramaters)
{
    thinker_t* th = (thinker_t*) gen;
    Map_RemoveThinker(Thinker_Map(th), th);
    P_DestroyGenerator(gen);
    return true; // Continue iteration.
}*/

void Map::clear()
{
    _name.clear();
    _info.clear();
    _thinkerEnum.reset();

    /**
     * Destroy Generators. This is only necessary because the particles of
     * each generator use storage linked only to the generator.
     */
    //iterate(GENERATOR, destroyGenerator);

    // Clear thinkers.
    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        delete i->second;
    }
    _thinkers.clear();
    
    // Ownership of the thinkers to add was given to us.
    FOR_EACH(i, _thinkersToAdd, PendingThinkers::iterator)
    {
        delete *i;
    }
    _thinkersToDestroy.clear();
}
    
Thinker* Map::thinker(const Id& id) const
{
    Thinkers::const_iterator found = _thinkers.find(id);
    if(found != _thinkers.end())
    {
        if(markedForDestruction(found->second))
        {
            // No longer exists officially.
            return NULL;
        }
        return found->second;
    }
    return NULL;
}

Object* Map::object(const Id& id) const
{
    Thinkers::const_iterator found = _thinkers.find(id);
    if(found != _thinkers.end())
    {
        if(markedForDestruction(found->second))
        {
            // No longer exists officially.
            return NULL;
        }
        return dynamic_cast<Object*>(found->second);
    }
    return NULL;
}

Id Map::findUniqueThinkerId()
{
    Id id = _thinkerEnum.get();
    while(_thinkerEnum.overflown() && thinker(id))
    {
        // This is in use, get the next one.
        id = _thinkerEnum.get();
    }
    return id;
}

Thinker& Map::add(Thinker* thinker)
{
    // Give the thinker a new id.
    thinker->setId(findUniqueThinkerId());
    addThinker(thinker);
    return *thinker;
}

void Map::addThinker(Thinker* t)
{
    t->setMap(this);
    
    if(_thinkersFrozen)
    {
        // Add it to the list of pending tasks.
        _thinkersToAdd.push_back(t);
        return;
    }
    
    _thinkers[t->id()] = t;
}

void Map::destroy(Thinker* t)
{
    if(thinker(t->id()))
    {
        if(_thinkersFrozen)
        {
            // Add it to the list of pending tasks.
            _thinkersToDestroy.push_back(t);
            return;
        }
        
        _thinkers.erase(t->id());
        t->setMap(0);
        delete t;
    }
    /// @throw NotFoundError  Thinker @a thinker was not found in the map.
    throw NotFoundError("Map::destroy", "Thinker " + t->id().asText() + " not found");
}

void Map::freezeThinkerList(bool freeze)
{
    if(freeze)
    {
        _thinkersFrozen++;
    }
    else
    {
        _thinkersFrozen--;
        assert(_thinkersFrozen >= 0);
        
        if(!_thinkersFrozen)
        {
            // Perform the pending tasks.
            FOR_EACH(i, _thinkersToAdd, PendingThinkers::iterator)
            {
                add(const_cast<Thinker*>(*i));
            }
            FOR_EACH(i, _thinkersToDestroy, PendingThinkers::iterator)
            {
                destroy(const_cast<Thinker*>(*i));
            }
            _thinkersToAdd.clear();
            _thinkersToDestroy.clear();
        }
    }
}

bool Map::markedForDestruction(const Thinker* thinker) const
{
    FOR_EACH(i, _thinkersToDestroy, PendingThinkers::const_iterator)
    {
        if(*i == thinker)
        {
            return true;
        }
    }
    return false;
}

bool Map::iterate(Thinker::SerialId serialId, bool (*callback)(Thinker*, void*), void* parameters)
{
    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        if(i->second->serialId() == serialId && !markedForDestruction(i->second))
        {
            if(!callback(i->second, parameters))
            {
                freezeThinkerList(false);
                return false;
            }
        }
    }

    freezeThinkerList(false);
    return true;
}

bool Map::iterateObjects(bool (*callback)(Object*, void*), void* parameters)
{
    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        Object* object = dynamic_cast<Object*>(i->second);
        if(object)
        {
            if(!callback(object, parameters) && !markedForDestruction(i->second))
            {
                freezeThinkerList(false);
                return false;
            }
        }
    }

    freezeThinkerList(false);
    return true;    
}

void Map::think(const Time::Delta& elapsed)
{
    // New ptcgens for planes?
    checkPtcPlanes();

    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        if(i->second->isAlive() && !markedForDestruction(i->second))
        {
            i->second->think(elapsed);
        }
    }    

    freezeThinkerList(false);
}

void Map::operator >> (Writer& to) const
{
    to << _name << _info;
    
    // Thinkers.
    to << duint32(_thinkers.size());
    FOR_EACH(i, _thinkers, Thinkers::const_iterator)
    {
        to << *i->second;
    }
}

void Map::operator << (Reader& from)
{
    clear();
    
    from >> _name >> _info;
    
    // Thinkers.
    duint32 count;
    from >> count;
    while(count--)
    {
        Thinker* thinker = Thinker::constructFrom(from);
        addThinker(thinker);
        _thinkerEnum.claim(thinker->id());
    }
}

LineDef* Map::createLineDef2(Vertex* vtx1, Vertex* vtx2, SideDef* front, SideDef* back)
{
    _lineDefs.push_back(new LineDef(vtx1, vtx2, front, back));
    LineDef* lineDef = _lineDefs[_lineDefs.size()-1];
    lineDef->buildData.index = _lineDefs.size(); // 1-based index.
    return lineDef;
}

SideDef* Map::createSideDef2(Sector* sector, dshort flags,
    Material* middleMaterial, const Vector2f& middleOffset,
    const Vector3f& middleTintColor, dfloat middleOpacity,
    Material* topMaterial, const Vector2f& topOffset,
    const Vector3f& topTintColor, Material* bottomMaterial,
    const Vector2f& bottomOffset, const Vector3f& bottomTintColor)
{
    _sideDefs.push_back(
        new SideDef(sector, flags, middleMaterial, middleOffset,
                    middleTintColor, middleOpacity,
                    topMaterial, topOffset, topTintColor,
                    bottomMaterial, bottomOffset, bottomTintColor));
    SideDef* sideDef = _sideDefs[_sideDefs.size()-1];
    sideDef->buildData.index = _sideDefs.size(); // 1-based index.
    return sideDef;
}

Sector* Map::createSector2(dfloat lightIntensity, const Vector3f& lightColor)
{
    _sectors.push_back(new Sector(lightIntensity, lightColor));
    Sector* sector = _sectors[_sectors.size()-1];
    sector->buildData.index = _sectors.size(); // 1-based index.
    return sector;
}

Plane* Map::createPlane2(dfloat height, const Vector3f& normal,
    Material* material, const Vector2f& materialOffset,
    dfloat opacity, Blendmode blendmode, const Vector3f& tintColor,
    dfloat glowIntensity, const Vector3f& glowColor)
{
    _planes.push_back(
        new Plane(height, normal, material, materialOffset,
                  opacity, blendmode, tintColor, glowIntensity, glowColor));
    Plane* plane = _planes[_planes.size()-1];
    plane->buildData.index = _planes.size(); // 1-based index.
    return plane;
}

Polyobj* Map::createPolyobj2(Polyobj::LineDefs lineDefs,
    dint tag, dint sequenceType, dfloat anchorX, dfloat anchorY)
{
    _polyobjs.push_back(
        new Polyobj(lineDefs, tag, sequenceType, anchorX, anchorY));
    Polyobj* polyobj = _polyobjs[_polyobjs.size()-1];
    polyobj->idx = _polyobjs.size()-1; // 0-based index.
    polyobj->buildData.index = _polyobjs.size(); // 1-based index, 0 = NIL.
    return polyobj;
}

void Map::initSubsectorContacts()
{
    _subsectorContacts = reinterpret_cast<objcontactlist_t*>(std::calloc(1, sizeof(*_subsectorContacts) * numSubsectors()));
}

void Map::clearSubsectorContacts()
{
    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(_subsectorContacts)
        memset(_subsectorContacts, 0, numSubsectors() * sizeof(*_subsectorContacts));
}

/**
 * Link the given objcontact node to list.
 */
static __inline void linkContact(Map::objcontact_t** list, Map::objcontact_t* con, duint index)
{
    con->next = list[index];
    list[index] = con;
}

void Map::linkContactToSubsector(duint subsectorIdx, objcontacttype_t type, objcontact_t* node)
{
    linkContact(&_subsectorContacts[subsectorIdx].head[type], node, 0);
}

Map::objcontact_t* Map::allocObjContact(void)
{
    objcontact_t* con;

    if(contCursor == NULL)
    {
        con = Z_Malloc(sizeof(*con), PU_STATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }

    con->obj = NULL;
    return con;
}

/**
 * The given LineDef might cross the Thing. If necessary, link the Thing into
 * the LineDef's Thing link ring.
 */
static bool PIT_LinkToLineDef(LineDef* lineDef, void* paramaters)
{
    Map::linelinker_data_t* data = reinterpret_cast<Map::linelinker_data_t*>(paramaters);

    if(!data->aaBounds.intersects(lineDef->aaBounds()))
        return true; // Bounding boxes do not overlap.

    if(lineDef->boxOnSide(data->aaBounds) != -1)
        return true; // Line does not cross the mobj's bounding box.

    /// One-sided LineDefs will not be linked to because a Thing can not
    /// legally cross one.
    if(!lineDef->hasFront() || !lineDef->hasBack())
        return true;

    // Add a node to the Thing's ring.
    NodePile::Index nix;
    data->map->thingNodes->link(nix = data->map->thingNodes->newIndex(lineDef), data->thing->lineRoot);

    // Add a node to the LineDef's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    data->map->thingNodes->nodes[nix].data = data->map->lineDefNodes->newIndex(data->thing);
    data->map->lineDefNodes->link(data->map->thingNodes->nodes[nix].data,
            data->map->lineDefLinks[P_ObjectRecord(DMU_LINEDEF, lineDef)->id - 1]);

    return true;
}

void Map::linkToLineDefs(Thing* thing)
{
    assert(thing);

    linelinker_data_t data;

    // Get a new root node.
    thing->lineRoot = thingNodes->newIndex(NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    data.thing = thing;
    data.aaBounds = thing->aaBounds();
    data.map = this;

    validCount++;
    iterateLineDefs(data.aaBounds, PIT_LinkToLineDef, &data);
}

bool Map::unlinkFromLineDefs(Thing* thing)
{
    assert(thing);

    NodePile::LinkNode* tn;
    NodePile::Index nix;

    tn = thingNodes->nodes;

    // Try unlinking from lines.
    if(!thing->lineRoot)
        return false; // A zero index means it's not linked.

    // Unlink from each line.
    for(nix = tn[thing->lineRoot].next; nix != thing->lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        lineDefNodes->unlink(tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        lineDefNodes->dismiss(tn[nix].data);
        thingNodes->dismiss(nix);
    }

    // The mobj no longer has a line ring.
    thingNodes->dismiss(thing->lineRoot);
    thing->lineRoot = 0;

    return true;
}

bool Map::unlinkFromSector(Thing* thing)
{
    assert(thing);

    if(thing->sPrev == NULL)
        return false;

    if((*thing->sPrev = thing->sNext))
        thing->sNext->sPrev = thing->sPrev;

    // Not linked any more.
    thing->sNext = NULL;
    thing->sPrev = NULL;

    return true;
}

void Map::link(Thing* thing, Thing::LinkFlags flags)
{
    assert(thing);

    Subsector& subsector = pointInSubsector(thing->origin);

    // Link into the sector.
    thing->subsector = reinterpret_cast<Subsector*>(P_ObjectRecord(DMU_SUBSECTOR, &subsector));

    if(flags[Thing::LINK_SECTOR])
    {
        // Unlink from the current sector, if any.
        if(thing->sPrev)
            unlinkFromSector(thing);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((thing->sNext = subsector.sector().thingList))
            thing->sNext->sPrev = &thing->sNext;

        *(thing->sPrev = &subsector.sector().thingList) = thing;
    }

    // Link into blockmap?
    if(flags[Thing::LINK_BLOCKMAP])
    {
        // Unlink from the old block, if any.
        _thingBlockmap->unlink(thing);
        _thingBlockmap->link(thing);
    }

    // Link into lines.
    if(!(flags[Thing::LINK_NOLINEDEF]))
    {
        unlinkFromLineDefs(thing);
        linkToLineDefs(thing);
    }

    // If this is a user - perform addtional tests to see if they have
    // entered or exited the void.
    if(thing->object().user())
    {
        User* user = thing->object().user();

        user->inVoid = true;
        if(subsector.sector().pointInside2(thing->origin.x, thing->origin.y) &&
           (thing->origin.z < subsector.ceiling().visHeight - 4 &&
            thing->origin.z >= subsector.floor().visHeight))
            user->inVoid = false;
    }
}

Thing::LinkFlags Map::unlink(Thing* thing)
{
    assert(thing);

    Thing::LinkFlags links = 0;
    if(unlinkFromSector(thing))
        links[Thing::LINK_SECTOR] = true;
    if(_thingBlockmap->unlink(thing))
        links[Thing::LINK_BLOCKMAP] = true;
    if(!unlinkFromLineDefs(thing))
        links[Thing::LINK_NOLINEDEF] = true;
    return links;
}

Subsector& Map::pointInSubsector(dfloat x, dfloat y) const
{
    if(numNodes() == 0) // Single subsector is a special case.
        return *_subsectors[0];

    BinaryTree<void*>* tree = _rootNode;
    while(!tree->isLeaf())
    {
        const Node* node = reinterpret_cast<Node*>(tree->data());
        tree = tree->child(node->partition.side(x, y)<=0);
    }

    Face* face = reinterpret_cast<Face*>(tree->data());
    return *(reinterpret_cast<Subsector*>(face->data));
}

Sector* Map::sectorForOrigin(const void* ddMobjBase) const
{
    assert(ddMobjBase);

    FOR_EACH(i, _sectors, Sectors::const_iterator)
    {
        if(ddMobjBase == &((*i)->soundOrg))
            return *i;
    }
    return NULL;
}

Polyobj* Map::polyobjForOrigin(const void* ddMobjBase) const
{
    assert(ddMobjBase);

    FOR_EACH(i, _polyobjs, Polyobjs::const_iterator)
    {
        if(ddMobjBase == reinterpret_cast<ddmobj_base_t*>(*i))
            return *i;
    }
    return NULL;
}

#if 0
static void spawnMapParticleGens()
{
    if(isDedicated || !useParticles)
        return;

    ded_generator_t* def;
    for(dint i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        if(!def->map[0] || stricmp(def->map, map->mapID))
            continue;

        if(def->spawnAge > 0 && ddMapTime > def->spawnAge)
            continue; // No longer spawning this generator.

        P_SpawnMapParticleGen(map, def);
    }
}

/**
 * Spawns all type-triggered particle generators, regardless of whether
 * the type of mobj exists in the map or not (mobjs might be dynamically
 * created).
 */
static void spawnTypeParticleGens(map_t* map)
{
    ded_generator_t* def;
    int i;

    if(isDedicated || !useParticles)
        return;

    for(i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        if(def->typeNum < 0)
            continue;

        P_SpawnTypeParticleGen(map, def);
    }
}

typedef struct {
    sector_t*       sector;
    uint            planeID;
} findplanegeneratorparams_t;

static int findPlaneGenerator(void* ptr, void* context)
{
    generator_t* gen = (generator_t*) ptr;
    findplanegeneratorparams_t* params = (findplanegeneratorparams_t*) context;

    if(gen->thinker.function && gen->sector == params->sector &&
       gen->planeID == params->planeID)
        return false; // Stop iteration, we've found one!
    return true; // Continue iteration.
}

/**
 * Returns true iff there is an active ptcgen for the given plane.
 */
static boolean P_HasActiveGenerator(sector_t* sector, uint planeID)
{
    findplanegeneratorparams_t params;

    params.sector = sector;
    params.planeID = planeID;

    // @todo Sector should return the map its linked to.
    return !Map_IterateThinkers2(P_CurrentMap(), (think_t) P_GeneratorThinker, ITF_PRIVATE,
                                findPlaneGenerator, &params);
}

/**
 * Spawns new ptcgens for planes, if necessary.
 */
static void checkPtcPlanes(map_t* map)
{
    uint i, p;

    if(isDedicated || !useParticles)
        return;

    // There is no need to do this on every tic.
    if(SECONDS_TO_TICKS(gameTime) % 4)
        return;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        for(p = 0; p < 2; ++p)
        {
            uint plane = p;
            Material* mat = sector->SP_planematerial(plane);
            const ded_generator_t* def = Material_GetGenerator(mat);

            if(!def)
                continue;

            if(def->flags & PGF_CEILING_SPAWN)
                plane = PLN_CEILING;
            if(def->flags & PGF_FLOOR_SPAWN)
                plane = PLN_FLOOR;

            if(!P_HasActiveGenerator(sector, plane))
            {
                // Spawn it!
                P_SpawnPlaneParticleGen(sector, plane, def);
            }
        }
    }
}

static void initGenerators(map_t* map)
{
    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    spawnTypeParticleGens(map);
    spawnMapParticleGens(map);
}

static int updateGenerator(void* ptr, void* context)
{
    generator_t* gen = (generator_t*) ptr;

    if(gen->thinker.function)
    {
        int i;
        ded_generator_t* def;
        boolean found;

        // Map-static generators cannot be updated (we have no means to reliably
        // identify them), so destroy them.
        // Flat generators will be spawned automatically within a few tics so we'll
        // just destroy those too.
        if((gen->flags & PGF_UNTRIGGERED) || gen->sector)
        {
            destroyGenerator(gen, NULL);
            return true; // Continue iteration.
        }

        // Search for a suitable definition.
        i = 0;
        def = defs.generators;
        found = false;
        while(i < defs.count.generators.num && !found)
        {
            // A type generator?
            if(def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                found = true;
            }
            // A damage generator?
            else if(gen->source && gen->source->type == def->damageNum)
            {
                found = true;
            }
            // A state generator?
            else if(gen->source && def->state[0] &&
                    gen->source->state - states == Def_GetStateNum(def->state))
            {
                found = true;
            }
            else
            {
                i++;
                def++;
            }
        }

        if(found)
        {   // Update the generator using the new definition.
            gen->def = def;
        }
        else
        {   // Nothing else we can do, destroy it.
            destroyGenerator(gen, NULL);
        }
    }

    return true; // Continue iteration.
}
#endif

void Map::update()
{
#if 0
    // Defs might've changed, so update the generators.
    iterate((think_t) P_GeneratorThinker, updateGenerator);

    // Re-spawn map generators.
    spawnMapParticleGens(map);
#endif

    // Update all world surfaces.
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sector = *i;
        sector->floor().surface().update();
        sector->ceiling().surface().update();
    }
    FOR_EACH(i, _sideDefs, SideDefs::iterator)
    {
        SideDef* sideDef = *i;
        sideDef->middle().update();
        sideDef->bottom().update();
        sideDef->top().update();
    }
    FOR_EACH(i, _polyobjs, Polyobjs::iterator)
    {
        Polyobj* polyobj = *i;
        FOR_EACH(lineDefIter, polyobj->lineDefs(), Polyobj::LineDefs::const_iterator)
        {
            LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*lineDefIter)->obj);
            lineDef->front().middle().update();
        }
    }
}

bool Map::updatePlaneHeightTracking(Plane* plane)
{
    plane->updateHeightTracking();
    return true; // Continue iteration.
}

bool Map::resetPlaneHeightTracking(Plane* plane)
{
    plane->resetHeightTracking();
    _watchedPlanes.erase(plane);

    /// @fixme Use Observer.
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sector = *i;

        // We can early out after the first match.
        if(&sector->floor() == plane)
        {
            sector->markDependantSurfacesForDecorationUpdate();
            return true; // Continue iteration.
        }

        if(&sector->ceiling() == plane)
        {
            sector->markDependantSurfacesForDecorationUpdate();
            return true; // Continue iteration.
        }
    }

    return true; // Continue iteration.
}

bool Map::interpolatePlaneHeight(Plane* plane, dfloat frameTimePos)
{
    plane->interpolateHeight(frameTimePos);

    // Has this plane reached its destination?
    if(plane->visHeight == plane->height)
        _watchedPlanes.erase(plane);

    /// @fixme Use Observer.
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sector = *i;

        // We can early out after the first match.
        if(&sector->floor() == plane)
        {
            sector->markDependantSurfacesForDecorationUpdate();
            return true; // Continue iteration.
        }

        if(&sector->ceiling() == plane)
        {
            sector->markDependantSurfacesForDecorationUpdate();
            return true; // Continue iteration.
        }
    }

    return true; // Continue iteration.
}

void Map::updateWatchedPlanes()
{
    FOR_EACH(i, _watchedPlanes, PlaneSet::iterator)
        updatePlaneHeightTracking(*i);
}

void Map::interpolateWatchedPlanes(dfloat frameTimePos)
{
    if(frameTimePos < 0)
    {
        // Reset the plane height trackers.
        FOR_EACH(i, _watchedPlanes, PlaneSet::iterator)
            resetPlaneHeightTracking(*i);
        return;
    }

    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    //if(!clientPaused)
    {
        // Set the visible offsets.
        FOR_EACH(i, _watchedPlanes, PlaneSet::iterator)
            interpolatePlaneHeight(*i, frameTimePos);
    }
}

void Map::interpolateMovingSurfaces(dfloat frameTimePos)
{
    if(frameTimePos < 0)
    {
        // Reset the material offset trackers.
        FOR_EACH(i, _movingSurfaces, SurfaceSet::iterator)
        {
            MSurface* suf = *i;
            suf->resetScroll();
            _movingSurfaces.erase(suf);
        }
        return;
    }

    // While the game is paused there is no need to calculate any
    // visual material offsets.
    //if(!clientPaused)
    {
        // Set the visible material offsets.
        FOR_EACH(i, _movingSurfaces, SurfaceSet::iterator)
        {
            MSurface* suf = *i;
            suf->interpolateScroll(frameTimePos);
            /// Has this material reached its destination? If so remove it from the
            /// set of moving Surfaces.
            if(suf->visOffset == suf->offset)
                _movingSurfaces.erase(suf);
        }
    }
}

void Map::updateMovingSurfaces()
{
    FOR_EACH(i, _movingSurfaces, SurfaceSet::iterator)
        (*i)->updateScroll();
}

#if 0
typedef struct {
    arvec2_t        bounds;
    boolean         inited;
} findaabbforvertices_params_t;

static int addToAABB(vertex_t* vertex, void* context)
{
    findaabbforvertices_params_t* params = (findaabbforvertices_params_t*) context;
    vec2_t point;

    V2_Set(point, vertex->pos[0], vertex->pos[1]);
    if(!params->inited)
    {
        V2_InitBox(params->bounds, point);
        params->inited = true;
    }
    else
        V2_AddToBox(params->bounds, point);
    return true; // Continue iteration.
}

static void findAABBForVertices(map_t* map, arvec2_t bounds)
{
    findaabbforvertices_params_t params;

    params.bounds = bounds;
    params.inited = false;
    HalfEdgeDS_IterateVertices(Map_HalfEdgeDS(map), addToAABB, &params);
}

static void buildLineDefBlockmap(map_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint i;
    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    uint numBlocks; // Number of cells = nrows*ncols.
    vec2_t bounds[2], dims;
    linedefblockmap_t* blockmap;

    // Scan for map limits, which the blockmap must enclose.
    findAABBForVertices(map, bounds);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][0] - BLKMARGIN, bounds[0][1] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][0] + BLKMARGIN, bounds[1][1] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[0] <= blockSize[0])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[0] / blockSize[0]);

    if(dims[1] <= blockSize[1])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[1] / blockSize[1]);
    numBlocks = bMapWidth * bMapHeight;

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][0] + bMapWidth  * blockSize[0],
                      bounds[0][1] + bMapHeight * blockSize[1]);

    blockmap = P_CreateLineDefBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];
        if(lineDef->inFlags & LF_POLYOBJ)
            continue;
        LineDefBlockmap_Link(blockmap, lineDef);
    }

    map->_lineDefBlockmap = blockmap;

#undef BLKMARGIN
#undef MAPBLOCKUNITS
}

static void buildSubsectorBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint i, subMapWidth, subMapHeight;
    vec2_t bounds[2], blockSize, dims;
    subsectorblockmap_t* blockmap;

    // @fixme why not use map->bBox?
    bounds[0][0] = bounds[0][1] = DDMAXFLOAT;
    bounds[1][0] = bounds[1][1] = DDMINFLOAT;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* ssec = map->subsectors[i];

        if(ssec->bBox[0][0] < bounds[0][0])
            bounds[0][0] = ssec->bBox[0][0];
        if(ssec->bBox[0][1] < bounds[0][1])
            bounds[0][1] = ssec->bBox[0][1];
        if(ssec->bBox[1][0] > bounds[1][0])
            bounds[1][0] = ssec->bBox[1][0];
        if(ssec->bBox[1][1] > bounds[1][1])
            bounds[1][1] = ssec->bBox[1][1];
    }
    bounds[0][0] -= BLKMARGIN;
    bounds[0][1] -= BLKMARGIN;
    bounds[1][0] += BLKMARGIN;
    bounds[1][1] += BLKMARGIN;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    /*V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);*/

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[0] <= blockSize[0])
        subMapWidth = 1;
    else
        subMapWidth = ceil(dims[0] / blockSize[0]);

    if(dims[1] <= blockSize[1])
        subMapHeight = 1;
    else
        subMapHeight = ceil(dims[1] / blockSize[1]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][0] + subMapWidth  * blockSize[0],
                      bounds[0][1] + subMapHeight * blockSize[1]);

    blockmap = P_CreateSubsectorBlockmap(bounds[0], bounds[1], subMapWidth, subMapHeight);

    // Process all the subsectors in the map.
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];
        SubsectorBlockmap_Link(blockmap, subsector);
    }

    map->_subsectorBlockmap = blockmap;

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

static void buildMobjBlockmap(map_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    vec2_t bounds[2], dims;

    // Scan for map limits, which the blockmap must enclose.
    findAABBForVertices(map, bounds);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][0] - BLKMARGIN, bounds[0][1] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][0] + BLKMARGIN, bounds[1][1] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[0] <= blockSize[0])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[0] / blockSize[0]);

    if(dims[1] <= blockSize[1])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[1] / blockSize[1]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][0] + bMapWidth  * blockSize[0],
                      bounds[0][1] + bMapHeight * blockSize[1]);

    map->_thingBlockmap = P_CreateMobjBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

#undef BLKMARGIN
#undef MAPBLOCKUNITS
}

static void buildParticleBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint mapWidth, mapHeight;
    vec2_t bounds[2], blockSize, dims;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[0] <= blockSize[0])
        mapWidth = 1;
    else
        mapWidth = ceil(dims[0] / blockSize[0]);

    if(dims[1] <= blockSize[1])
        mapHeight = 1;
    else
        mapHeight = ceil(dims[1] / blockSize[1]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][0] + mapWidth  * blockSize[0],
                      bounds[0][1] + mapHeight * blockSize[1]);

    map->_particleBlockmap = P_CreateParticleBlockmap(bounds[0], bounds[1], mapWidth, mapHeight);

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

static void buildLumobjBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint mapWidth, mapHeight;
    vec2_t bounds[2], blockSize, dims;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[0] <= blockSize[0])
        mapWidth = 1;
    else
        mapWidth = ceil(dims[0] / blockSize[0]);

    if(dims[1] <= blockSize[1])
        mapHeight = 1;
    else
        mapHeight = ceil(dims[1] / blockSize[1]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][0] + mapWidth  * blockSize[0],
                      bounds[0][1] + mapHeight * blockSize[1]);

    map->_lumobjBlockmap = P_CreateLumobjBlockmap(bounds[0], bounds[1], mapWidth, mapHeight);

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}
#endif

static dint C_DECL vertexCompare(const void* p1, const void* p2)
{
    const Vertex* a = reinterpret_cast<const Vertex*>(*((const void**) p1));
    const Vertex* b = reinterpret_cast<const Vertex*>(*((const void**) p2));

    if(a == b)
        return 0;
    if(dint(a->pos.x) != dint(b->pos.x))
        return dint(a->pos.x) - dint(b->pos.x);
    return dint(a->pos.y) - dint(b->pos.y);
}

static void detectDuplicateVertices(HalfEdgeDS& halfEdgeDS)
{
    dsize numVertices = halfEdgeDS.numVertices();
    Vertex** hits = reinterpret_cast<Vertex**>(std::malloc(numVertices * sizeof(Vertex*)));
    // Sort array of ptrs.
    for(dsize i = 0; i < numVertices; ++i)
        hits[i] = halfEdgeDS.vertices[i];
    qsort(hits, numVertices, sizeof(Vertex*), vertexCompare);

    // Now mark them off.
    for(dsize i = 0; i < numVertices - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            Vertex* a = hits[i];
            Vertex* b = hits[i + 1];

            /// @fixme Looks like a bug to me, if not equivalent, then should be NULL surely?
            ((MVertex*) b->data)->equiv =
                (((MVertex*) a->data)->equiv ? ((MVertex*) a->data)->equiv : a);
        }
    }

    std::free(hits);
}

void Map::findEquivalentVertexes()
{
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = *i;

        // Handle duplicated vertices.
        while(reinterpret_cast<MVertex*>(lineDef->buildData.v[0]->data)->equiv)
        {
            reinterpret_cast<MVertex*>(lineDef->buildData.v[0]->data)->refCount--;
            lineDef->buildData.v[0] = reinterpret_cast<MVertex*>(lineDef->buildData.v[0]->data)->equiv;
            reinterpret_cast<MVertex*>(lineDef->buildData.v[0]->data)->refCount++;
        }

        while(reinterpret_cast<MVertex*>(lineDef->buildData.v[1]->data)->equiv)
        {
            reinterpret_cast<MVertex*>(lineDef->buildData.v[1]->data)->refCount--;
            lineDef->buildData.v[1] = reinterpret_cast<MVertex*>(lineDef->buildData.v[1]->data)->equiv;
            reinterpret_cast<MVertex*>(lineDef->buildData.v[1]->data)->refCount++;
        }
    }
}

void Map::pruneLineDefs()
{
    LineDefs::size_type newIndex = 0, unused = 0;
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* l = *i;

        if(!l->buildData.sideDefs[LineDef::FRONT] && !l->buildData.sideDefs[LineDef::BACK])
        {
            unused++;
            delete l;
            continue;
        }

        l->buildData.index = newIndex + 1;
        _lineDefs[newIndex++] = l;
    }

    if(newIndex != _lineDefs.size())
    {
        if(unused > 0)
            LOG_MESSAGE("  Pruned %d unused linedefs.") << unused;

        _lineDefs.resize(newIndex);
    }
}

void Map::pruneVertexes()
{
    HalfEdgeDS::Vertices::size_type numVertices = halfEdgeDS().numVertices();
    HalfEdgeDS::Vertices::size_type newIndex = 0, unused = 0;
    for(dsize i = 0; i < numVertices; ++i)
    {
        Vertex* v = halfEdgeDS().vertices[i];

        if(reinterpret_cast<MVertex*>(v->data)->refCount == 0)
        {
            if(reinterpret_cast<MVertex*>(v->data)->equiv == NULL)
                unused++;
            std::free(v->data);
            delete v;
            continue;
        }

        reinterpret_cast<MVertex*>(v->data)->index = newIndex + 1;
        halfEdgeDS().vertices[newIndex++] = v;
    }

    if(newIndex != numVertices)
    {
        SideDefs::size_type dupNum = numVertices - newIndex - unused;

        if(unused > 0)
            LOG_MESSAGE("  Pruned %d unused vertices.") << unused;

        if(dupNum > 0)
            LOG_MESSAGE("  Pruned %d duplicate vertices.") << dupNum;

        halfEdgeDS().vertices.resize(newIndex);
    }
}

void Map::pruneSideDefs()
{
    SideDefs::size_type newIndex = 0, unused = 0;
    FOR_EACH(i, _sideDefs, SideDefs::iterator)
    {
        SideDef* s = *i;

        if(s->buildData.refCount == 0)
        {
            unused++;
            delete s;
            continue;
        }

        s->buildData.index = newIndex + 1;
        _sideDefs[newIndex++] = s;
    }

    if(newIndex != _sideDefs.size())
    {
        SideDefs::size_type dupNum = _sideDefs.size() - newIndex - unused;

        if(unused > 0)
            LOG_MESSAGE("  Pruned %d unused sidedefs.") << unused;

        if(dupNum > 0)
            LOG_MESSAGE("  Pruned %d duplicate sidedefs.") << dupNum;

        _sideDefs.resize(newIndex);
    }
}

void Map::pruneSectors()
{
    FOR_EACH(i, _sideDefs, SideDefs::iterator)
    {
        SideDef* s = *i;
        if(!s->hasSector())
            continue;
        s->sector().buildData.refCount++;
    }

    Sectors::size_type newIndex = 0;
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* s = *i;

        if(s->buildData.refCount == 0)
        {
            delete s;
            continue;
        }

        s->buildData.index = newIndex + 1;
        _sectors[newIndex++] = s;
    }

    if(newIndex != _sectors.size())
    {
        LOG_MESSAGE("  Pruned %d unused sectors.") << _sectors.size() - newIndex;
        _sectors.resize(newIndex);
    }
}

void Map::pruneUnusedObjects(dint flags)
{
    /**
     * @fixme Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     *
     * @note Order here is critical!
     */
    findEquivalentVertexes();

/*    if(flags & PRUNE_LINEDEFS)
        pruneLineDefs();*/

    if(flags & PRUNE_VERTEXES)
        pruneVertexes();

/*    if(flags & PRUNE_SIDEDEFS)
        pruneSideDefs();

    if(flags & PRUNE_SECTORS)
        pruneSectors();
    
    if(flags & PRUNE_PLANES)
        prunePlanes();
*/
}

void Map::buildSectorLineDefSets()
{
    typedef struct linelink_s {
        LineDef* lineDef;
        struct linelink_s *next;
    } linelink_t;

    zblockset_t* lineLinksBlockSet;
    linelink_t** sectorLineLinks;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = reinterpret_cast<linelink_t**>(std::calloc(1, sizeof(linelink_t*) * _sectors.size()));

    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = *i;

        if(lineDef->polyobjOwned)
            continue;

        if(lineDef->hasFront())
        {
            linelink_t* link = Z_BlockNewElement(lineLinksBlockSet);

            duint secIDX = P_ObjectRecord(DMU_SECTOR, lineDef->frontSector())->id - 1;
            link->lineDef = lineDef;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
        }

        if(lineDef->hasBack() && &lineDef->backSector() != &lineDef->frontSector())
        {
            linelink_t* link = Z_BlockNewElement(lineLinksBlockSet);

            duint secIDX = P_ObjectRecord(DMU_SECTOR, lineDef->backSector())->id - 1;
            link->lineDef = lineDef;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
        }
    }

    // Harden the sector line links into arrays.
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sector = *i;

        if(sectorLineLinks[i])
        {
            linelink_t* link = sectorLineLinks[i];

            /**
             * The behaviour of some algorithms used in original DOOM are
             * dependant upon the order of these lists (e.g., EV_DoFloor
             * and EV_BuildStairs). Lets be helpful and use the same order.
             *
             * Sort: LineDef index ascending (zero based).
             */
            duint numLineDefs = 0;
            while(link)
            {
                numLineDefs++;
                link = link->next;
            }

            sector->lineDefs = Z_Malloc((numLineDefs + 1) * sizeof(LineDef*), PU_STATIC, 0);

            duint j = numLineDefs - 1;
            link = sectorLineLinks[i];
            while(link)
            {
                sector->lineDefs[j--] = link->lineDef;
                link = link->next;
            }

            sector->lineDefs[numLineDefs] = NULL; // terminate.
            sector->lineDefCount = numLineDefs;
        }
        else
        {
            sector->lineDefs = NULL;
            sector->lineDefCount = 0;
        }
    }

    // Free temporary storage.
    Z_BlockDestroy(lineLinksBlockSet);
    std::free(sectorLineLinks);
}

void Map::finishSectors2()
{
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sec = *i;

        sec->updateAABounds();

        // Set the degenmobj_t to the middle of the bounding box.
        sec->soundOrg.pos = sec->aaBounds().middle();
        sec->soundOrg.pos.z = 0;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        sec->floor().target = sec->floor().height;
        sec->ceiling().target = sec->ceiling().height;
    }
}

void Map::updateAABounds()
{
    if(_sectors.size() == 0)
    {
        _aaBounds = MapRectangled(Vector2d(0, 0), Vector2d(0, 0));
        return;
    }

    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sec = *i;

        if(i == _sectors.begin())
        {
            // The first sector is used as is.
            _aaBounds = sec->aaBounds();
        }
        else
        {
            // Expand the bounding box.
            _aaBounds.include(sec->aaBounds());
        }
    }
}

void Map::finishLineDefs2()
{
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* ld = *i;

        if(!ld->halfEdges[0])
            continue;

        ld->direction = ld->vtx2().pos - ld->vtx1().pos;

        // Calculate the accurate length of each line.
        ld->length = dfloat(ld->direction.length());
        ld->angle = bamsAtan2(dint(ld->direction.y), dint(ld->direction.x)) << FRACBITS;

        if(fequal(ld->direction.x, 0))
            ld->slopeType = LineDef::ST_VERTICAL;
        else if(fequal(ld->direction.y, 0))
            ld->slopeType = LineDef::ST_HORIZONTAL;
        else
        {
            if(ld->direction.y / ld->direction.x > 0)
                ld->slopeType = LineDef::ST_POSITIVE;
            else
                ld->slopeType = LineDef::ST_NEGATIVE;
        }

        const Vector2d bottomLeft = ld->vtx1().pos.min(ld->vtx2().pos);
        const Vector2d topRight = ld->vtx1().pos.max(ld->vtx2().pos);
        ld->_aaBounds = MapRectangled(bottomLeft, topRight);
    }
}

void Map::findSubsectorMidPoints()
{
    FOR_EACH(i, _subsectors, Subsectors::iterator)
        (*i)->updateMidPoint();
}

/**
 * Compares the angles of two linelefs that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static dint C_DECL lineAngleSorter(const void* a, const void* b)
{
    lineowner_t* own[2];
    own[0] = reinterpret_cast<lineowner_t*>(const_cast<void*>(a));
    own[1] = reinterpret_cast<lineowner_t*>(const_cast<void*>(b));

    dbinangle angles[2];
    for(duint i = 0; i < 2; ++i)
    {
        if(own[i]->link[0]) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            LineDef* lineDef = own[i]->lineDef;

            Vertex* otherVtx = lineDef->buildData.v[lineDef->buildData.v[0] == rootVtx? 1:0];

            Vector2d delta = otherVtx->pos - rootVtx->pos;
            own[i]->angle = angles[i] = bamsAtan2(-100 * dint(delta.x), 100 * dint(delta.y));

            // Mark as having a cached angle.
            own[i]->link[0] = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static lineowner_t* mergeLineOwners(lineowner_t* left, lineowner_t* right,
                                    dint (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.link[1] = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->link[1] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->link[1] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->link[1] = left;
    if(right)
        np->link[1] = right;

    // Is the list empty?
    if(&tmp.next() == &tmp)
        return NULL;

    return &tmp.next();
}

static lineowner_t* splitLineOwners(lineowner_t* list)
{
    lineowner_t* lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = &listb->next();
        lista = &lista->next();
        if(lista != NULL)
            lista = &lista->next();
    } while(lista);

    listc->link[1] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t* sortLineOwners(lineowner_t* list,
                                   dint (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t* p;

    if(list && list->link[1])
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Map::vertexinfo_t* vInfo, LineDef* lineDef, dbyte vertex,
                               lineowner_t** storage)
{
    lineowner_t* newOwner;

    // Has this line already been registered with this vertex?
    if(vInfo->numLineOwners != 0)
    {
        lineowner_t* owner = vInfo->lineOwners;

        do
        {
            if(owner->lineDef == lineDef)
                return; // Yes, we can exit.

            owner = &owner->next();
        } while(owner);
    }

    //Add a new owner.
    vInfo->numLineOwners++;

    newOwner = (*storage)++;
    newOwner->lineDef = lineDef;
    newOwner->link[0] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->link[1] = vInfo->lineOwners;
    vInfo->lineOwners = newOwner;

    // Link the line to its respective owner node.
    lineDef->vo[vertex] = newOwner;
}

#if _DEBUG
static void checkVertexOwnerRings(Map::vertexinfo_t* vertexInfo, duint num)
{
    for(duint i = 0; i < num; ++i)
    {
        Map::vertexinfo_t* vInfo = &vertexInfo[i];

        validCount++;

        if(vInfo->numLineOwners)
        {
            lineowner_t* base, *owner;

            owner = base = vInfo->lineOwners;
            do
            {
                /// LineDef linked multiple times in owner link ring?
                assert(owner->lineDef->validCount != validCount);
                owner->lineDef->validCount = validCount;

                /// Invalid ring?
                assert(&owner->prev().next() == owner);
                assert(&owner->next().prev() == owner);

                owner = &owner->next();
            } while(owner != base);
        }
    }
}
#endif

void Map::buildVertexOwnerRings(vertexinfo_t* vertexInfo)
{
    // We know how many vertex line owners we need (numLineDefs * 2).

    HalfEdgeDS::Vertices::size_type numVertices = _halfEdgeDS->numVertices();

    lineOwners = reinterpret_cast<lineowner_t*>(std::calloc(1, sizeof(lineowner_t) * _lineDefs.size() * 2));

    lineowner_t* allocator = lineOwners;
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = *i;

        for(duint j = 0; j < 2; ++j)
        {
            vertexinfo_t* vInfo =
                &vertexInfo[reinterpret_cast<MVertex*>(lineDef->buildData.v[j]->data)->index - 1];

            setVertexLineOwner(vInfo, lineDef, j, &allocator);
        }
    }

    // Sort line owners and then finish the rings.
    for(HalfEdgeDS::Vertices::size_type i = 0; i < numVertices; ++i)
    {
        vertexinfo_t* vInfo = &vertexInfo[i];

        // Line owners:
        if(vInfo->numLineOwners != 0)
        {
            lineowner_t* owner, *last;
            dbinangle firstAngle;

            // Redirect the linedef links to the hardened map.
            owner = vInfo->lineOwners;
            while(owner)
            {
                owner->lineDef = _lineDefs[owner->lineDef->buildData.index - 1];
                owner = &owner->next();
            }

            // Sort them; ordered clockwise by angle.
            rootVtx = _halfEdgeDS->vertices[i];
            vInfo->lineOwners = sortLineOwners(vInfo->lineOwners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            firstAngle = vInfo->lineOwners->angle;
            last = vInfo->lineOwners;
            owner = &last->next();
            while(owner)
            {
                owner->link[0] = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - owner->angle;

                last = owner;
                owner = &owner->next();
            }
            last->link[1] = vInfo->lineOwners;
            vInfo->lineOwners->link[0] = last;

            // Set the angle of the last owner.
            last->angle = last->angle - firstAngle;
        }
    }
}

static bool C_DECL buildSeg(HalfEdge* halfEdge, void* paramaters)
{
    Map* map = reinterpret_cast<Map*>(paramaters);
    HalfEdgeInfo* info = reinterpret_cast<HalfEdgeInfo*>(halfEdge->data);

    halfEdge->data = NULL;

    if(!(info->lineDef &&
        (!info->sector || (info->back && info->lineDef->buildData.windowEffect))))
    {
        map->createSeg(*halfEdge, info->lineDef, info->back);
    }

    return true; // Continue iteration.
}

static Sector* pickSectorFromHEdges(const HalfEdge* firstHEdge, bool allowSelfRef)
{
    const HalfEdge* halfEdge;
    Sector* sector = NULL;

    halfEdge = firstHEdge;
    do
    {
        if(!allowSelfRef && halfEdge->twin &&
           reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->sector ==
           reinterpret_cast<HalfEdgeInfo*>(halfEdge->twin->data)->sector)
            continue;

        if(reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->lineDef &&
           reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->sector)
        {
            LineDef* lineDef = reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->lineDef;

            if(lineDef->buildData.windowEffect && reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->back)
                sector = lineDef->buildData.windowEffect;
            else
                sector = &lineDef->buildData.sideDefs[
                    reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->back? 1 : 0]->sector();
        }
    } while(!sector && (halfEdge = halfEdge->next) != firstHEdge);

    return sector;
}

static bool C_DECL buildSubsector(BinaryTree<void*>* tree, void* paramaters)
{
    if(!tree->isLeaf())
    {
        Node* node = reinterpret_cast<Node*>(tree->data());
        BinaryTree<void*>* child;

        child = tree->right();
        if(child && child->isLeaf())
        {
            Face* face = reinterpret_cast<Face*>(child->data());
            Sector* sector;

            /**
             * Determine which sector this subsector belongs to.
             * On the first pass, we are picky; do not consider half-edges from
             * self-referencing linedefs. If that fails, take whatever we can find.
             */
            sector = pickSectorFromHEdges(face->halfEdge, false);
            if(!sector)
                sector = pickSectorFromHEdges(face->halfEdge, true);

            reinterpret_cast<Map*>(paramaters)->createSubsector(*face, sector);
        }

        child = tree->left();
        if(child && child->isLeaf())
        {
            Face* face = reinterpret_cast<Face*>(child->data());
            Sector* sector;

            /**
             * Determine which sector this subsector belongs to.
             * On the first pass, we are picky; do not consider half-edges from
             * self-referencing linedefs. If that fails, take whatever we can find.
             */
            sector = pickSectorFromHEdges(face->halfEdge, false);
            if(!sector)
                sector = pickSectorFromHEdges(face->halfEdge, true);

            reinterpret_cast<Map*>(paramaters)->createSubsector(*face, sector);
        }
    }

    return true; // Continue iteration.
}

bool Map::buildNodes()
{
    /// Does not yet support a live rebuild.
    assert(_rootNode == 0);

    /// @todo Do this in busy mode.
    NodeBuilder nodeBuilder = NodeBuilder(this, bspFactor);

    nodeBuilder.build();
    _rootNode = nodeBuilder.bspTree;

    if(_rootNode)
    {   // Build subsectors and segs.
        _rootNode->postOrder(buildSubsector, this);
        halfEdgeDS().iterateHalfEdges(buildSeg, this);

        LOG_VERBOSE("  Built ")
            << numNodes() << " Nodes, " << numSubsectors() << " Subsectors, "
            << numSegs() << " Segs.";

        if(!_rootNode->isLeaf())
            LOG_VERBOSE("  Balance: ")
                << (_rootNode->right()? _rootNode->right()->height() : 0) << " | "
                << (_rootNode->left()? _rootNode->left()->height() : 0) << ".";
    }

    return _rootNode != NULL;
}

#if 0 /* Currently unused. */
/**
 * @return              The "lowest" vertex (normally the left-most, but if
 *                      the line is vertical, then the bottom-most).
 *                      @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const linedef_t* l)
{
    return (((int) l->v[0]->buildData.pos[0] < (int) l->v[1]->buildData.pos[0] ||
             ((int) l->v[0]->buildData.pos[0] == (int) l->v[1]->buildData.pos[0] &&
              (int) l->v[0]->buildData.pos[1] < (int) l->v[1]->buildData.pos[1]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[0] != (int) d->buildData.pos[0])
        return (int) c->buildData.pos[0] - (int) d->buildData.pos[0];

    return (int) c->buildData.pos[1] - (int) d->buildData.pos[1];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[0] != (int) d->buildData.pos[0])
        return (int) c->buildData.pos[0] - (int) d->buildData.pos[0];

    return (int) c->buildData.pos[1] - (int) d->buildData.pos[1];
}

size_t numOverlaps;

boolean testOverlaps(linedef_t* b, void* data)
{
    linedef_t*          a = (linedef_t*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {   // Found an overlap!
                b->buildData.overlap =
                    (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return true; // Continue iteration.
}

typedef struct {
    blockmap_t*         blockMap;
    uint                block[2];
} findoverlaps_params_t;

boolean findOverlapsForLineDef(linedef_t* l, void* data)
{
    findoverlaps_params_t* params = (findoverlaps_params_t*) data;

    LineDefBlockmap_Iterate(params->blockMap, params->block, testOverlaps, l, false);
    return true; // Continue iteration.
}

/**
 * \note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(map_t* map)
{
    uint                x, y, bmapDimensions[2];
    findoverlaps_params_t params;

    params.blockMap = map->blockMap;
    numOverlaps = 0;

    Blockmap_Dimensions(map->blockMap, bmapDimensions);

    for(y = 0; y < bmapDimensions[1]; ++y)
        for(x = 0; x < bmapDimensions[0]; ++x)
        {
            params.block[0] = x;
            params.block[1] = y;

            LineDefBlockmap_Iterate(map->blockMap, params.block,
                                    findOverlapsForLineDef, &params, false);
        }

    if(numOverlaps > 0)
        VERBOSE(Con_Message("Detected %lu overlapped linedefs\n",
                            (unsigned long) numOverlaps));
}
#endif

static Map::ownernode_t* newOwnerNode(void)
{
    Map::ownernode_t* node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = reinterpret_cast<Map::ownernode_t*>(std::malloc(sizeof(Map::ownernode_t)));
    }

    return node;
}

static void setSectorOwner(Map::ownerlist_t* ownerList, Subsector* subsector)
{
    if(!subsector)
        return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    Map::ownernode_t* node = newOwnerNode();
    node->data = subsector;
    node->next = ownerList->head;
    ownerList->head = node;
}

void Map::findSubsectorsAffectingSector(duint secIDX)
{
    ownerlist_t subsectorOwnerList;
    memset(&subsectorOwnerList, 0, sizeof(subsectorOwnerList));

    Sector* sec = _sectors[secIDX];

    MapRectangled affectBounds(sec->aaBounds().bottomLeft() - Vector2d(128, 128),
                               sec->aaBounds().topRight()   + Vector2d(128, 128));

    FOR_EACH(i, _subsectors, Subsectors::iterator)
    {
        Subsector* subsector = *i;

        // Is this subsector close enough?
        if(&subsector->sector() == sec || // subsector is IN this sector
           affectBounds.contains(subsector->midPoint))
        {
            // It will contribute to the reverb settings of this sector.
            setSectorOwner(&subsectorOwnerList, subsector);
        }
    }

    // Now harden the list.
    if(subsectorOwnerList.count)
    {
        ownernode_t* node = subsectorOwnerList.head;
        for(duint i = 0; i < subsectorOwnerList.count; ++i)
        {
            ownernode_t* next = node->next;

            Subsector* subsector = reinterpret_cast<Subsector*>(node->data);
            sec->reverbSubsectors.insert(subsector);

            if(i < numSectors() - 1)
            {   // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {   // No further use for the nodes.
                std::free(node);
            }
            node = next;
        }
    }
}

void Map::initSoundEnvironment()
{
    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        findSubsectorsAffectingSector(i - _sectors.begin());
    }

    // Free any nodes left in the unused list.
    ownernode_t* node = unusedNodeList;
    ownernode_t* p;
    while(node)
    {
        p = node->next;
        std::free(node);
        node = p;
    }
    unusedNodeList = NULL;
}

bool Map::editBegin()
{
    _editActive = true;
    return true;
}

bool Map::editEnd()
{
    if(!_editActive)
        return true;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    detectDuplicateVertices(*_halfEdgeDS);
    pruneUnusedObjects(PRUNE_ALL);

    buildLineDefBlockmap(this);

    /*builtOK =*/ buildNodes();

    FOR_EACH(i, _polyobjs, Polyobjs::iterator)
    {
        Polyobj* po = *i;

        po->_originalPts.reserve(po->lineDefs.size());
        po->_prevPts.reserve(po->lineDefs.size());

        FOR_EACH(lineDefIter, po->lineDefs, Polyobj::LineDefs::iterator)
        {
            LineDef* lineDef = *lineDefIter;

            HalfEdge* fHEdge = new HalfEdge();
            fHEdge->face = NULL;
            fHEdge->vertex = _halfEdgeDS->vertices[
                reinterpret_cast<MVertex*>(lineDef->buildData.v[0]->data)->index - 1];
            fHEdge->vertex->halfEdge = fHEdge;

            HalfEdge* bHEdge = new HalfEdge();
            bHEdge->face = NULL;
            bHEdge->vertex = _halfEdgeDS->vertices[
                reinterpret_cast<MVertex*>(lineDef->buildData.v[1]->data)->index - 1];
            bHEdge->vertex->halfEdge = bHEdge;

            fHEdge->twin = bHEdge;
            bHEdge->twin = fHEdge;

            lineDef->halfEdges[0] = lineDef->halfEdges[1] = fHEdge;

            // The original Pts are based off the anchor Pt, and are unique
            // to each linedef.
            Vector2f point = lineDef->vtx1().pos - po->origin;
            po->_originalPts.push_back(point);
            po->_prevPts.push_back(point);
        }

        for(duint j = 0; j < po->lineDefs.size(); ++j)
        {
            po->lineDefs[j] = reinterpret_cast<LineDef*>(P_ObjectRecord(DMU_LINEDEF, po->lineDefs[j]));
        }

        po->createSegs();
    }

    buildSectorSubsectorLists();
    buildSectorLineLists();
    finishLineDefs2();
    finishSectors2();
    updateMapBounds();

    initSoundEnvironment();
    findSubsectorMidPoints();

    vertexinfo_t* vertexInfo = reinterpret_cast<vertexinfo_t*>(std::calloc(1, sizeof(vertexinfo_t) * halfEdgeDS().numVertices()));

    buildVertexOwnerRings(vertexInfo);

#if _DEBUG
checkVertexOwnerRings(vertexInfo, halfEdgeDS().numVertices());
#endif

    // Build the vertex lineowner info.
    // @todo now redundant, rewire fakeradio to use HalfEdgeDS.
    FOR_EACH(i, halfEdgeDS().vertices, HalfEdgeDS::Vertices::iterator)
    {
        Vertex* vertex = *i;
        vertexinfo_t* vInfo = &vertexInfo[i - halfEdgeDS().vertices.begin()];

        reinterpret_cast<MVertex*>(vertex->data)->lineOwners = vInfo->lineOwners;
        reinterpret_cast<MVertex*>(vertex->data)->numLineOwners = vInfo->numLineOwners;
    }
    std::free(vertexInfo);

    _editActive = false;
    return true;
}

void Map::beginFrame(bool resetNextViewer)
{
    FOR_EACH(i, _sectors, Sectors::iterator)
        (*i)->clearFrameFlags();

    interpolateWatchedPlanes(resetNextViewer);
    interpolateMovingSurfaces(resetNextViewer);

    if(!freezeRLs)
    {
        clearSubsectorContacts();

        LightGrid_Update(this->_lightGrid);
        SB_BeginFrame(this);
        LO_ClearForFrame(this);
        Rend_InitDecorationsForFrame(this);

        _particleBlockmap->empty();
        R_CreateParticleLinks(this);

        _lumobjBlockmap->empty();
        Rend_AddLuminousDecorations(this);
        LO_AddLuminousMobjs(this);
    }
}

void Map::endFrame()
{
    if(!freezeRLs)
    {
        // Wrap up with Source, Bias lights.
        SB_EndFrame(this);

        // Update the bias light editor.
        SBE_EndFrame(this);
    }
}

void Map::markAllSectorsForLightGridUpdate()
{
    if(_lightGrid.inited)
    {
        // Mark all blocks and contributors.
        FOR_EACH(i, _sectors, Sectors::iterator)
        {
            Sector* sec = *i;
            LightGrid_MarkForUpdate(_lightGrid, sec->blocks, sec->changedBlockCount, sec->blockCount);
        }
    }
}

Seg* Map::createSeg(HalfEdge& halfEdge, LineDef* lineDef, bool back)
{
    SideDef* sideDef = (lineDef && lineDef->buildData.sideDefs[back])?
        _sideDefs[lineDef->buildData.sideDefs[back]->buildData.index - 1] : 0;

    _segs.push_back(new Seg(halfEdge, sideDef, back));
    Seg* seg = _segs[_segs.size()-1];
    halfEdge.data = reinterpret_cast<void*>(seg);
    return seg;
}

Subsector* Map::createSubsector(Face& face, Sector* sector)
{
    _subsectors.push_back(new Subsector(face, sector));
    Subsector* subsector = _subsectors[_subsectors.size()-1];

    if(subsector->hasSector())
    {
        sector->subsectors.insert(subsector);
    }
    else
    {
        LOG_WARNING("Orphan subsector #") << _subsectors.size()-1 << ".";
    }
    face.data = reinterpret_cast<void*>(subsector);

    return subsector;
}

Node* Map::createNode(const Node::Partition& partition, const MapRectangled& rightAABB, const MapRectangled& leftAABB)
{
    _nodes.push_back(new Node(partition, rightAABB, leftAABB));
    return _nodes[_nodes.size()-1];
}

void Map::initLinks()
{
    thingNodes = new NodePile(256); // Allocate a small pile.
    lineDefNodes = new NodePile(numLineDefs() + 1000);

    lineDefLinks = reinterpret_cast<NodePile::Index*>(std::malloc(sizeof(*lineDefLinks) * numLineDefs()));
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
        lineDefLinks[i - _lineDefs.begin()] = lineDefNodes->newIndex(NP_ROOT_NODE);
}

void Map::initSkyFix()
{
    skyFixFloor = MAXFLOAT;
    skyFixCeiling = MINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    FOR_EACH(i, _sectors, Sectors::iterator)
        updateSkyFixForSector(i - _sectors.begin());
}

void Map::updateSkyFixForSector(duint sectorIndex)
{
    assert(sectorIndex < numSectors());

    const Sector* sector = _sectors[sectorIndex];
    bool skyFloor = sector->floor().material().isSky();
    bool skyCeil = sector->ceiling().material().isSky();

    if(!skyFloor && !skyCeil)
        return;

    if(skyCeil)
    {
        // Adjust for the plane height.
        if(sector->ceiling().visHeight > skyFixCeiling)
        {   // Must raise the skyfix ceiling.
            skyFixCeiling = sector->ceiling().visHeight;
        }

        // Check that all the mobjs in the sector fit in.
        for(Thing* thing = sector->thingList; thing; thing = thing->sNext)
        {
            dfloat extent = thing->origin.z + thing->height;

            if(extent > skyFixCeiling)
            {   // Must raise the skyfix ceiling.
                skyFixCeiling = extent;
            }
        }
    }

    if(skyFloor)
    {
        // Adjust for the plane height.
        if(sector->floor().visHeight < skyFixFloor)
        {   // Must lower the skyfix floor.
            skyFixFloor = sector->floor().visHeight;
        }
    }

    // Update for middle textures on two sided linedefs which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    FOR_EACH(i, sector->lineDefs, Sector::LineDefSet::const_iterator)
    {
        const LineDef* lineDef = *i;

        // Must be twosided.
        if(lineDef->hasFront() && lineDef->hasBack())
        {
            SideDef& sideDef = &lineDef->frontSector() == sector? lineDef->front() : lineDef->back();

            if(sideDef.middle().material() != NullMaterial)
            {
                if(skyCeil)
                {
                    dfloat top = sector->ceiling().visHeight + sideDef.middle().visOffset[1];
                    if(top > skyFixCeiling)// Must raise the skyfix ceiling.
                        skyFixCeiling = top;
                }

                if(skyFloor)
                {
                    dfloat bottom = sector->floor().visHeight + sideDef.middle().visOffset[1] - sideDef.middle().material->height;
                    if(bottom < skyFixFloor) // Must lower the skyfix floor.
                        skyFixFloor = bottom;
                }
            }
        }
    }
}

Vertex* Map::createVertex2(dfloat x, dfloat y)
{
    Vertex* vtx = &halfEdgeDS().createVertex();
    vtx->pos.x = x;
    vtx->pos.y = y;
    return vtx;
}

Map::objectrecordid_t Map::createVertex(dfloat x, dfloat y)
{
    if(!_editActive)
        return NULL;
    Vertex* v = createVertex2(x, y);
    return v ? reinterpret_cast<MVertex*>(v->data)->index : 0;
}

bool Map::createVertices(dsize num, dfloat* values, objectrecordid_t* indices)
{
    if(!num || !values)
        return false;

    if(!_editActive)
        return false;

    // Create many vertexes.
    for(duint n = 0; n < num; ++n)
    {
        Vertex* v = createVertex2(values[n * 2], values[n * 2 + 1]);
        if(indices)
            indices[n] = reinterpret_cast<MVertex*>(v->data)->index;
    }
    return true;
}

Map::objectrecordid_t Map::createLineDef(objectrecordid_t v1,
    objectrecordid_t v2, duint frontSide, duint backSide, dint flags)
{
    if(!_editActive)
        return 0;
    if(frontSide > numSideDefs())
        return 0;
    if(backSide > numSideDefs())
        return 0;
    if(v1 == 0 || v1 > numVertexes())
        return 0;
    if(v2 == 0 || v2 > numVertexes())
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide && _sideDefs[frontSide - 1]->buildData.refCount)
        return 0; // Already in use.
    if(backSide && _sideDefs[backSide - 1]->buildData.refCount)
        return 0; // Already in use.

    // Next, check the length is not zero.
    Vertex* vtx1 = halfEdgeDS().vertices[v1 - 1];
    Vertex* vtx2 = halfEdgeDS().vertices[v2 - 1];

    Vector2d direction = vtx2->pos - vtx1->pos;
    if(!(direction.length() > 0))
        return 0;

    SideDef* front = NULL, *back = NULL;
    if(frontSide > 0)
        front = _sideDefs[frontSide - 1];
    if(backSide > 0)
        back = _sideDefs[backSide - 1];

    LineDef* l = createLineDef2(vtx1, vtx2, front, back);
    return l->buildData.index;
}

Map::objectrecordid_t Map::createSideDef(objectrecordid_t sector,
    dshort flags, Material* topMaterial, dfloat topOffsetX, dfloat topOffsetY,
    dfloat topRed, dfloat topGreen, dfloat topBlue, Material* middleMaterial,
    dfloat middleOffsetX, dfloat middleOffsetY, dfloat middleRed, dfloat middleGreen,
    dfloat middleBlue, dfloat middleAlpha, Material* bottomMaterial,
    dfloat bottomOffsetX, dfloat bottomOffsetY, dfloat bottomRed, dfloat bottomGreen,
    dfloat bottomBlue)
{
    if(!_editActive)
        return 0;
    if(sector > numSectors())
        return 0;

    SideDef* s = createSideDef2((sector == 0? NULL: _sectors[sector-1]), flags,
                      topMaterial? reinterpret_cast<Material*>(reinterpret_cast<objectrecord_t*>(topMaterial)->obj) : NULL,
                      topOffsetX, topOffsetY, topRed, topGreen, topBlue,
                      middleMaterial? reinterpret_cast<Material*>(reinterpret_cast<objectrecord_t*>(middleMaterial)->obj) : NULL,
                      middleOffsetX, middleOffsetY, middleRed, middleGreen, middleBlue,
                      middleAlpha, bottomMaterial? reinterpret_cast<Material*>(reinterpret_cast<objectrecord_t*>(bottomMaterial)->obj) : NULL,
                      bottomOffsetX, bottomOffsetY, bottomRed, bottomGreen, bottomBlue);

    return s ? s->buildData.index : 0;
}

Map::objectrecordid_t Map::createSector(dfloat lightLevel, dfloat red, dfloat green, dfloat blue)
{
    if(!_editActive)
        return 0;
    Sector* s = createSector2(lightLevel, Vector3f(red, green, blue));
    return s ? s->buildData.index : 0;
}

Map::objectrecordid_t Map::createPlane(dfloat height, Material* material,
    dfloat matOffsetX, dfloat matOffsetY, dfloat r, dfloat g, dfloat b,
    dfloat a, dfloat normalX, dfloat normalY, dfloat normalZ)
{
    if(!_editActive)
        return 0;

    Plane* pln = createPlane2(height, Vector3f(normalX, normalY, normalZ),
        material? reinterpret_cast<objectrecord_t*>(material)->obj : NULL,
        Vector2f(matOffsetX, matOffsetY), opactiy, BM_NORMAL,
        Vector3f(r, g, b), 0, Vector3f(1, 1, 1));
    return pln ? pln->buildData.index : 0;
}

Map::objectrecordid_t Map::createPolyobj(objectrecordid_t* lines,
    duint lineCount, dint tag, dint sequenceType, dfloat anchorX, dfloat anchorY)
{
    if(!_editActive)
        return 0;
    if(!lineCount || !lines)
        return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(duint i = 0; i < lineCount; ++i)
    {
        LineDef* lineDef;

        if(lines[i] == 0 || lines[i] > numLineDefs())
            return 0;

        lineDef = _lineDefs[lines[i] - 1];
        if(lineDef->polyobjOwned)
            return 0;
    }

    Polyobj::LineDefs poLineDefs;
    poLineDefs.reserve(lineCount);
    for(duint i = 0; i < lineCount; ++i)
    {
        LineDef* lineDef = _lineDefs[lines[i] - 1];

        // This line is part of a polyobj.
        lineDef->polyobjOwned = true;

        poLineDefs.push_back(lineDef);
    }

    Polyobj* po = createPolyobj2(poLineDefs, tag, sequenceType, anchorX, anchorY);
    return po ? po->buildData.index : 0;
}

Polyobj* Map::polyobj(duint num)
{
    if(num & 0x80000000)
    {
        duint idx = num & 0x7fffffff;

        if(idx < numPolyobjs())
            return _polyobjs[idx];
        return NULL;
    }

    for(duint i = 0; i < numPolyobjs(); ++i)
    {
        Polyobj* po = _polyobjs[i];

        if(duint(po->tag) == num)
        {
            return po;
        }
    }
    return NULL;
}

void Map::findDominantLightSources()
{
#define DOMINANT_SIZE   1000*1000

    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sec = *i;

        if(sec->lineDefs.size() == 0)
            continue;
        if(sec->lightSource)
            continue;

        // Is this sector large enough to be a dominant light source?
        if(sec->approxArea > DOMINANT_SIZE && sec->hasSkySurface())
        {
            // All sectors touching this one will be affected.
            FOR_EACH(lineDefIter, sec->lineDefs, Sector::LineDefSet::iterator)
            {
                LineDef* lineDef = *lineDefIter;

                if(lineDef->hasFrontSector() && &lineDef->frontSector() != sec)
                    lineDef->frontSector().lightSource = sec;

                if(lineDef->hasBackSector() && &lineDef->backSector() != sec)
                    lineDef->backSector().lightSource = sec;
            }
        }
    }

#undef DOMINANT_SIZE
}

static bool PIT_LinkObjToSubsector(duint subsectorIdx, void* paramaters)
{
    linkobjtosubsectorparams_t* params = reinterpret_cast<linkobjtosubsectorparams_t*>(paramaters);
    objcontact_t* con = allocObjContact();

    con->obj = params->obj;

    // Link the contact list for this face.
    linkContactToSubsector(params->map, subsectorIdx, params->type, con);

    return true; // Continue iteration.
}

void Map::addSubsectorContact(duint subsectorIdx, objcontacttype_t type, void* obj)
{
    assert(subsectorIdx < numSubsectors());
    assert(type >= OCT_FIRST && type < NUM_OBJCONTACT_TYPES);
    assert(obj);

    linkobjtosubsectorparams_t params;
    params.map = map;
    params.obj = obj;
    params.type = type;
    PIT_LinkObjToSubsector(subsectorIdx, &params);
}

bool Map::iterateSubsectorContacts(duint subsectorIdx, objcontacttype_t type,
    bool (*callback) (void*, void*), void* paramaters)
{
    objcontact_t* con;

    assert(subsectorIdx < numSubsectors());
    assert(callback);

    con = _subsectorContacts[subsectorIdx].head[type];
    while(con)
    {
        if(!callback(con->obj, paramaters))
            return false;
        con = con->next;
    }
    return true;
}

void Map::initSectorShadows()
{
    /**
     * The algorithm:
     *
     * 1. Use the subsector blockmap to look for all the blocks that are
     *    within the linedef's shadow bounding box.
     *
     * 2. Check the subsectors whose sector is the same as the linedef.
     *
     * 3. If any of the shadow points are in the subsector, or any of the
     *    shadow edges cross one of the subsector's edges (not parallel),
     *    link the linedef to the subsector.
     */
    shadowLinksBlockSet = Z_BlockCreate(sizeof(shadowlink_t), 1024, PU_MAP);

    FOR_EACH(i, _sideDefs, SideDefs::const_iterator)
    {
        const SideDef* sideDef = *i;

        if(!sideDef->hasLineDef())
            continue;

        const LineDef& = sideDef->lineDef();
        if(!lineDef.isShadowing())
            continue;

        MapRectanglef aaBounds = lineDef.aaBounds();
        dbyte side = &lineDef.back() == sideDef? LineDef::BACK : LineDef::FRONT;
        // Use the extended points, they are wider than inoffsets.
        aaBounds.include(lineDef.vtx(side  ).pos + lineDef.vo[side  ]->next().shadowOffsets.extended);
        aaBounds.include(lineDef.vtx(side^1).pos + lineDef.vo[side^1]->prev().shadowOffsets.extended);

        shadowlinkerparms_t params;
        params.lineDef = &sideDef->lineDef();
        params.side = side;

        iterateSubsectors(aaBounds, &side->sector(), RIT_ShadowSubsectorLinker, &params, false);
    }
}

boolean Map_MobjsBoxIterator(map_t* map, const float box[4],
                             boolean (*func) (mobj_t*, void*), void* data)
{
    assert(map);
    {
    vec2_t bounds[2];

    bounds[0][0] = box[BOXLEFT];
    bounds[0][1] = box[BOXBOTTOM];
    bounds[1][0] = box[BOXRIGHT];
    bounds[1][1] = box[BOXTOP];

    return Map_MobjsBoxIteratorv(map, bounds, func, data);
    }
}

boolean Map_MobjsBoxIteratorv(map_t* map, const arvec2_t box,
                              boolean (*func) (mobj_t*, void*), void* data)
{
    assert(map);
    {
    uint blockBox[4];
    MobjBlockmap_BoxToBlocks(map->_thingBlockmap, blockBox, box);
    return MobjBlockmap_BoxIterate(map->_thingBlockmap, blockBox, func, data);
    }
}

static boolean linesBoxIteratorv(map_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*),
                                 void* data, boolean retObjRecord)
{
    uint blockBox[4];
    LineDefBlockmap_BoxToBlocks(map->_lineDefBlockmap, blockBox, box);
    return LineDefBlockmap_BoxIterate(map->_lineDefBlockmap, blockBox, func, data, retObjRecord);
}

boolean Map_LineDefsBoxIteratorv(map_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*), void* data,
                                 boolean retObjRecord)
{
    assert(map);
    return linesBoxIteratorv(map, box, func, data, retObjRecord);
}

/**
 * @return              @c false, if the iterator func returns @c false.
 */
boolean Map_SubsectorsBoxIterator2(map_t* map, const float box[4], sector_t* sector,
                                  boolean (*func) (subsector_t*, void*),
                                  void* parm, boolean retObjRecord)
{
    assert(map);
    {
    vec2_t bounds[2];

    bounds[0][0] = box[BOXLEFT];
    bounds[0][1] = box[BOXBOTTOM];
    bounds[1][0] = box[BOXRIGHT];
    bounds[1][1] = box[BOXTOP];

    return Map_SubsectorsBoxIteratorv(map, bounds, sector, func, parm, retObjRecord);
    }
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean Map_SubsectorsBoxIteratorv(map_t* map, const arvec2_t box, sector_t* sector,
                                   boolean (*func) (subsector_t*, void*),
                                   void* data, boolean retObjRecord)
{
    assert(map);
    {
    static int localValidCount = 0;
    uint blockBox[4];

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    SubsectorBlockmap_BoxToBlocks(map->_subsectorBlockmap, blockBox, box);
    return SubsectorBlockmap_BoxIterate(map->_subsectorBlockmap, blockBox, sector,
                                       box, localValidCount, func, data,
                                       retObjRecord);
    }
}

void Map::setSectorPlane(objectrecordid_t sector, uint type, objectrecordid_t plane)
{
    sector_t* sec;
    plane_t* pln;
    uint i;

    if(!map->editActive)
        return;
    if(sector == 0 || sector > Map_NumSectors(map))
        return;
    if(plane == 0 || plane > Map_NumPlanes(map))
        return;

    sec = map->sectors[sector-1];

    // First check whether sector is already linked with this plane.
    for(i = 0; i < sec->planeCount; ++i)
    {
        if(sec->planes[i]->buildData.index == plane)
            return;
    }

    pln = map->planes[plane-1];
    sec->planes = Z_Realloc(sec->planes, sizeof(plane_t*) * (++sec->planeCount + 1), PU_STATIC);
    sec->planes[type > PLN_CEILING? sec->planeCount-1 : type] = pln;
    sec->planes[sec->planeCount] = NULL; // Terminate.
}

static boolean interceptLineDef(const linedef_t* li, losdata_t* los,
                                divline_t* dl)
{
    divline_t localDL, *dlPtr;

    // Try a quick, bounding-box rejection.
    if(li->bBox[BOXLEFT]   > los->bBox[BOXRIGHT] ||
       li->bBox[BOXRIGHT]  < los->bBox[BOXLEFT] ||
       li->bBox[BOXBOTTOM] > los->bBox[BOXTOP] ||
       li->bBox[BOXTOP]    < los->bBox[BOXBOTTOM])
        return false;

    if(P_PointOnDivlineSide(li->L_v1->pos[VX], li->L_v1->pos[VY], &los->trace) ==
       P_PointOnDivlineSide(li->L_v2->pos[VX], li->L_v2->pos[VY], &los->trace))
        return false; // Not crossed.

    if(dl)
        dlPtr = dl;
    else
        dlPtr = &localDL;

    LineDef_ConstructDivline(li, dlPtr);

    if(P_PointOnDivlineSide(fix2flt(los->trace.pos[VX]),
                            fix2flt(los->trace.pos[VY]), dlPtr) ==
       P_PointOnDivlineSide(los->to[VX], los->to[VY], dlPtr))
        return false; // Not crossed.

    return true; // Crossed.
}

static boolean crossLineDef(const linedef_t* li, byte side, losdata_t* los)
{
#define RTOP            0x1
#define RBOTTOM         0x2
#define RMIDDLE         0x4

    float frac;
    byte ranges = 0;
    divline_t dl;
    const sector_t* fsec, *bsec;
    boolean noBack;

    if(!interceptLineDef(li, los, &dl))
        return true; // Ray does not intercept seg on the X/Y plane.

    if(!LINE_SIDE(li, side))
        return true; // Seg is on the back side of a one-sided window.

    fsec = LINE_SECTOR(li, side);
    bsec  = (LINE_BACKSIDE(li)? LINE_SECTOR(li, side^1) : NULL);
    noBack = LINE_BACKSIDE(li)? false : true;

    if(!noBack && !(los->flags & LS_PASSLEFT) &&
       (!(bsec->SP_floorheight < fsec->SP_ceilheight) ||
        !(fsec->SP_floorheight < bsec->SP_ceilheight)))
        noBack = true;

    if(noBack)
    {
        if((los->flags & LS_PASSLEFT) &&
           side != LineDef_PointOnSide(li, fix2flt(los->trace.pos[VX]),
                                           fix2flt(los->trace.pos[VY])))
            return true; // Ray does not intercept seg from left to right.

        if(!(los->flags & (LS_PASSOVER | LS_PASSUNDER)))
            return false; // Stop iteration.
    }

    // Handle the case of a zero height backside in the top range.
    if(noBack)
    {
        ranges |= RTOP;
    }
    else
    {
        if(bsec->SP_floorheight > fsec->SP_floorheight)
            ranges |= RBOTTOM;
        if(bsec->SP_ceilheight < fsec->SP_ceilheight)
            ranges |= RTOP;
        if(!(los->flags & LS_PASSMIDDLE))
            ranges |= RMIDDLE;
    }

    if(!ranges)
        return true;

    frac = P_InterceptVector(&los->trace, &dl);

    if(ranges & RMIDDLE)
    {
        surface_t* surface = &LINE_FRONTSIDE(li)->SW_middlesurface;

        if(surface->material && !(surface->blendMode > 0) &&
           !(surface->rgba[CA] < 1.0f) &&
           (!(los->flags & LS_PASSLEFT) ||
            side == LineDef_PointOnSide(li, fix2flt(los->trace.pos[VX]),
                                        fix2flt(los->trace.pos[VY]))))
        {
            material_snapshot_t ms;

            Materials_Prepare(surface->material, MPF_SMOOTH, NULL, &ms);
            if(ms.isOpaque)
            {
                HalfEdge* halfEdge = li->halfEdges[0];
                plane_t* ffloor, *fceil, *bfloor, *bceil;
                float bottom, top;

                R_PickPlanesForSegExtrusion(halfEdge, R_UseSectorsFromFrontSideDef(halfEdge, SEG_MIDDLE),
                                            &ffloor, &fceil, &bfloor, &bceil);
                if(R_FindBottomTopOfHEdgeSection(halfEdge, SEG_MIDDLE, ffloor, fceil, bfloor, bceil,
                                                 &bottom, &top, NULL, NULL))
                {
                    if(!(los->bottomSlope > (top - los->startZ) / frac) ||
                         los->topSlope < (bottom - los->startZ) / frac)
                        return false; // Stop iteration.
                }
            }
        }
    }

    if((ranges & ~RMIDDLE) == 0)
        return true;

    if(!noBack &&
       (((los->flags & LS_PASSOVER_SKY) &&
         IS_SKYSURFACE(&fsec->SP_ceilsurface) &&
         IS_SKYSURFACE(&bsec->SP_ceilsurface) &&
         los->bottomSlope > (bsec->SP_ceilheight - los->startZ) / frac) ||
        ((los->flags & LS_PASSUNDER_SKY) &&
         IS_SKYSURFACE(&fsec->SP_floorsurface) &&
         IS_SKYSURFACE(&bsec->SP_floorsurface) &&
         los->topSlope < (bsec->SP_floorheight - los->startZ) / frac)))
        return true;

    if((los->flags & LS_PASSOVER) &&
       los->bottomSlope > (fsec->SP_ceilheight - los->startZ) / frac)
        return true;

    if((los->flags & LS_PASSUNDER) &&
       los->topSlope < (fsec->SP_floorheight - los->startZ) / frac)
        return true;

    if(ranges & RTOP)
    {
        float slope, top;

        top = (noBack? fsec->SP_ceilheight : bsec->SP_ceilheight);
        slope = (top - los->startZ) / frac;

        if((los->topSlope > slope) ^ (noBack && !(los->flags & LS_PASSOVER)) ||
           (noBack && los->topSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->topSlope = slope;
        if((los->bottomSlope > slope) ^ (noBack && !(los->flags & LS_PASSUNDER)) ||
           (noBack && los->bottomSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->bottomSlope = slope;
    }

    if(ranges & RBOTTOM)
    {
        float bottom = (noBack? fsec->SP_floorheight : bsec->SP_floorheight);
        float slope = (bottom - los->startZ) / frac;

        if(los->bottomSlope < slope)
            los->bottomSlope = slope;
        if(los->topSlope < slope)
            los->topSlope = slope;
    }

    if(los->topSlope <= los->bottomSlope)
        return false; // Stop iteration.

    return true;

#undef RTOP
#undef RBOTTOM
}

/**
 * @return              @c true iff trace crosses the given subsector.
 */
static boolean crossSubsector(subsector_t* subsector, losdata_t* los)
{
    if(subsector->polyObj)
    {   // Check polyobj lines.
        polyobj_t* po = subsector->polyObj;
        uint i;

        for(i = 0; i < po->numLineDefs; ++i)
        {
            linedef_t* line = ((objectrecord_t*) po->lineDefs[i])->obj;

            if(line->validCount != validCount)
            {
                line->validCount = validCount;

                if(!crossLineDef(line, FRONT, los))
                    return false; // Stop iteration.
            }
        }
    }

    {
    // Check lines.
    HalfEdge* halfEdge;

    if((halfEdge = subsector->face->halfEdge))
    {
        do
        {
            const seg_t* seg = (seg_t*) (halfEdge)->data;

            if(seg && seg->sideDef && seg->sideDef->lineDef->validCount != validCount)
            {
                linedef_t* li = seg->sideDef->lineDef;

                li->validCount = validCount;

                if(!crossLineDef(li, seg->side, los))
                    return false;
            }
        } while((halfEdge = halfEdge->next) != subsector->face->halfEdge);
    }
    }

    return true; // Continue iteration.
}

/**
 * @return              @c true iff trace crosses the node.
 */
static boolean crossBSPNode(map_t* map, binarytree_t* tree, losdata_t* los)
{
    face_t* face;

    while(!BinaryTree_IsLeaf(tree))
    {
        const node_t* node = BinaryTree_GetData(tree);
        byte side = R_PointOnSide(fix2flt(los->trace.pos[VX]),
                                  fix2flt(los->trace.pos[VY]),
                                  &node->partition);

        // Would the trace completely cross this partition?
        if(side == R_PointOnSide(los->to[VX], los->to[VY],
                                 &node->partition))
        {   // Yes, decend!
            tree = BinaryTree_GetChild(tree, side);
        }
        else
        {   // No.
            if(!crossBSPNode(map, BinaryTree_GetChild(tree, side), los))
                return 0; // Cross the starting side.
            else
                tree = BinaryTree_GetChild(tree, side^1); // Cross the ending side.
        }
    }

    face = (face_t*) BinaryTree_GetData(tree);
    return crossSubsector((subsector_t*)face->data, los);
}

/**
 * Traces a line of sight.
 *
 * @param from          World position, trace origin coordinates.
 * @param to            World position, trace target coordinates.
 * @param flags         Line Sight Flags (LS_*) @see lineSightFlags
 *
 * @return              @c true if the traverser function returns @c true
 *                      for all visited lines.
 */
boolean Map_CheckLineSight(map_t* map, const float from[3], const float to[3],
                           float bottomSlope, float topSlope, int flags)
{
    assert(map);
    {
    losdata_t los;

    los.flags = flags;
    los.startZ = from[VZ];
    los.topSlope = to[VZ] + topSlope - los.startZ;
    los.bottomSlope = to[VZ] + bottomSlope - los.startZ;
    los.trace.pos[VX] = flt2fix(from[VX]);
    los.trace.pos[VY] = flt2fix(from[VY]);
    los.trace.dX = flt2fix(to[VX] - from[VX]);
    los.trace.dY = flt2fix(to[VY] - from[VY]);
    los.to[VX] = to[VX];
    los.to[VY] = to[VY];
    los.to[VZ] = to[VZ];

    if(from[VX] > to[VX])
    {
        los.bBox[BOXRIGHT]  = from[VX];
        los.bBox[BOXLEFT]   = to[VX];
    }
    else
    {
        los.bBox[BOXRIGHT]  = to[VX];
        los.bBox[BOXLEFT]   = from[VX];
    }

    if(from[VY] > to[VY])
    {
        los.bBox[BOXTOP]    = from[VY];
        los.bBox[BOXBOTTOM] = to[VY];
    }
    else
    {
        los.bBox[BOXTOP]    = to[VY];
        los.bBox[BOXBOTTOM] = from[VY];
    }

    validCount++;
    return crossBSPNode(map, map->_rootNode, &los);
    }
}

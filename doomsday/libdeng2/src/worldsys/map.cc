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
    dfloat bBox[4];
    dfloat to[3];
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

    if(segs) std::free(segs);

    if(subsectors) std::free(subsectors);

    if(nodes) std::free(nodes);

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

LineDef* Map::createLineDef2()
{
    _lineDefs.push_back(new LineDef());
    LineDef* lineDef = _lineDefs[_lineDefs.size()-1];
    lineDef->buildData.index = _lineDefs.size(); // 1-based index.
    return lineDef;
}

SideDef* Map::createSideDef2()
{
    _sideDefs.push_back(new SideDef());
    SideDef* sideDef = _sideDefs[_sideDefs.size()-1];
    sideDef->buildData.index = _sideDefs.size(); // 1-based index.
    return sideDef;
}

Sector* Map::createSector2()
{
    _sectors.push_back(new Sector());
    Sector* sector = _sectors[_sectors.size()-1];
    sector->buildData.index = _sectors.size(); // 1-based index.
    return sector;
}

Plane* Map::createPlane2()
{
    _planes.push_back(new Plane());
    Plane* plane = _planes[_planes.size()-1];
    plane->buildData.index = _planes.size(); // 1-based index.
    return plane;
}

Polyobj* Map::createPolyobj2()
{
    _polyobjs.push_back(new Polyobj());
    Polyobj* polyobj = _polyobjs[_polyobjs.size()-1];
    polyobj->buildData.index = _polyobjs.size(); // 1-based index, 0 = NIL.
    return polyobj;
}

void Map::initSubsectorContacts()
{
    _subsectorContacts = reinterpret_cast<objcontactlist_t*>(std::calloc(1, sizeof(*_subsectorContacts) * _numSubsectors));
}

void Map::clearSubsectorContacts()
{
    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(_subsectorContacts)
        memset(_subsectorContacts, 0, _numSubsectors * sizeof(*_subsectorContacts));
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

    if(data->aaBounds[1][0] <= lineDef->aaBounds().left ||
       data->aaBounds[0][0] >= lineDef->aaBounds().right ||
       data->aaBounds[1][1] <= lineDef->aaBounds().bottom ||
       data->aaBounds[0][1] >= lineDef->aaBounds().top)
        // Bounding boxes do not overlap.
        return true;

    if(lineDef->boxOnSide(data->aaBounds) != -1)
        // Line does not cross the mobj's bounding box.
        return true;

    // One sided lines will not be linked to because a mobj
    // can't legally cross one.
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
    if(thing->hasUser())
    {
        User& user = thing->user();

        user.inVoid = true;
        if(subsector.sector().pointInside2(thing->origin.x, thing->origin.y) &&
           (thing->origin.z < subsector.ceiling().visHeight - 4 &&
            thing->origin.z >= subsector.floor().visHeight))
            user.inVoid = false;
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
    if(!_numNodes) // Single subsector is a special case.
        return *subsectors[0];

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
        for(duint j = 0; j < polyobj->_numLineDefs; ++j)
        {
            LineDef* lineDef = reinterpret_cast<LineDef*>((reinterpret_cast<objectrecord_t*>(polyobj->lineDefs[j]))->obj);
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
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
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
    }
    // While the game is paused there is no need to calculate any
    // visual material offsets.
    else //if(!clientPaused)
    {
        // Set the visible material offsets.
        FOR_EACH(i, _movingSurfaces, SurfaceSet::iterator)
        {
            MSurface* suf = *i;
            suf->interpolateScroll(frameTimePos);
            /// Has this material reached its destination? If so remove it from the
            /// set of moving Surfaces.
            if(fequal(suf->visOffset[0], suf->offset[0]) &&
               fequal(suf->visOffset[1], suf->offset[1]))
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

void Map::buildSectorSubsectorSets()
{
    FOR_EACH(i, _sectors, Sectors::iterator)
        (*i)->buildSubsectorSet();
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
    sectorLineLinks = reinterpret_cast<linelink_t*>(std::calloc(1, sizeof(linelink_t*) * _sectors.size()));

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
        aaBounds = MapRectangled(Vector2d(0, 0), Vector2d(0, 0));
        return;
    }

    FOR_EACH(i, _sectors, Sectors::iterator)
    {
        Sector* sec = *i;

        if(i == _sectors.begin())
        {
            // The first sector is used as is.
            aaBounds = sec->aaBounds();
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(aaBounds, sec->aaBounds());
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
        ld->length = ld->direction.length();
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

        if(ld->vtx1().pos.x < ld->vtx2().pos.x)
        {
            ld->aaBounds.left   = ld->vtx1().pos.x;
            ld->aaBounds.right  = ld->vtx2().pos.x;
        }
        else
        {
            ld->aaBounds.left   = ld->vtx2().pos.x;
            ld->aaBounds.right  = ld->vtx1().pos.x;
        }

        if(ld->vtx1().pos.y < ld->vtx2().pos.y)
        {
            ld->aaBounds.bottom = ld->vtx1().pos.y;
            ld->aaBounds.top    = ld->vtx2().pos.y;
        }
        else
        {
            ld->aaBounds.bottom = ld->vtx2().pos.y;
            ld->aaBounds.top    = ld->vtx1().pos.y;
        }
    }
}

void Map::findSubsectorMidPoints()
{
    for(duint i = 0; i < _numSubsectors; ++i)
    {
        Subsector* subsector = subsectors[i];

        subsector->updateMidPoint();
    }
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
        map->createSeg(info->lineDef, info->back, halfEdge);
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

            reinterpret_cast<Map*>(paramaters)->createSubsector(face, sector);
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

            reinterpret_cast<Map*>(paramaters)->createSubsector(face, sector);
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
            << _numNodes << " Nodes, " << _numSubsectors << " Subsectors, "
            << _numSegs << " Segs.";

        if(!_rootNode->isLeaf())
            LOG_VERBOSE("  Balance: r")
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

void Map::findSubSectorsAffectingSector(duint secIDX)
{
    ownerlist_t subsectorOwnerList;
    memset(&subsectorOwnerList, 0, sizeof(subsectorOwnerList));

    Sector* sec = _sectors[secIDX];
    MapRectangled affectBounds = sec->aaBounds();
    affectBounds.left   -= 128;
    affectBounds.right  += 128;
    affectBounds.top    += 128;
    affectBounds.bottom -= 128;

    for(duint i = 0; i < _numSubsectors; ++i)
    {
        Subsector* subsector =  subsectors[i];

        // Is this subsector close enough?
        if(&subsector->sector() == sec || // subsector is IN this sector
           (subsector->midPoint.x > affectBounds.left &&
            subsector->midPoint.x < affectBounds.right &&
            subsector->midPoint.y < affectBounds.top &&
            subsector->midPoint.y > affectBounds.bottom))
        {
            // It will contribute to the reverb settings of this sector.
            setSectorOwner(&subsectorOwnerList, subsector);
        }
    }

    // Now harden the list.
    sec->numReverbSubsectorAttributors = subsectorOwnerList.count;
    if(sec->numReverbSubsectorAttributors)
    {
        ownernode_t* node, *p;
        Subsector** ptr;

        sec->reverbSubsectors =
            Z_Malloc((sec->numReverbSubsectorAttributors + 1) * sizeof(Subsector*),
                     PU_STATIC, 0);

        ptr = sec->reverbSubsectors;
        node = subsectorOwnerList.head;
        for(duint i = 0; i < sec->numReverbSubsectorAttributors; ++i, ptr++)
        {
            p = node->next;
            *ptr = reinterpret_cast<Subsector*>(node->data);

            if(i < numSectors() - 1)
            {   // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {   // No further use for the nodes.
                std::free(node);
            }
            node = p;
        }
        *ptr = NULL; // terminate.
    }
}

void Map::initSoundEnvironment()
{
    for(duint i = 0; i < numSectors(); ++i)
    {
        findSubSectorsAffectingSector(i);
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

    FOR_EACH(i, polyobjs, Polyobjs::iterator)
    {
        Polyobj* po = *i;

        po->originalPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);
        po->prevPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);

        for(duint j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* lineDef = po->lineDefs[j];
            HalfEdge* fHEdge, *bHEdge;

            fHEdge = Z_Calloc(sizeof(*fHEdge), PU_STATIC, 0);
            fHEdge->face = NULL;
            fHEdge->vertex = map->_halfEdgeDS->vertices[
                ((mvertex_t*) lineDef->buildData.v[0]->data)->index - 1];
            fHEdge->vertex->halfEdge = fHEdge;

            bHEdge = Z_Calloc(sizeof(*bHEdge), PU_STATIC, 0);
            bHEdge->face = NULL;
            bHEdge->vertex = map->_halfEdgeDS->vertices[
                ((mvertex_t*) lineDef->buildData.v[1]->data)->index - 1];
            bHEdge->vertex->halfEdge = bHEdge;

            fHEdge->twin = bHEdge;
            bHEdge->twin = fHEdge;

            lineDef->halfEdges[0] = lineDef->halfEdges[1] = fHEdge;

            // The original Pts are based off the anchor Pt, and are unique
            // to each linedef.
            po->originalPts[j].pos[0] = lineDef->L_v1->pos[0] - po->pos[0];
            po->originalPts[j].pos[1] = lineDef->L_v1->pos[1] - po->pos[1];

            po->lineDefs[j] = (linedef_t*) P_ObjectRecord(DMU_LINEDEF, lineDef);
        }

        // Temporary: Create a seg for each line of this polyobj.
        po->numSegs = po->numLineDefs;
        po->segs = Z_Calloc(sizeof(seg_t) * po->numSegs, PU_STATIC, 0);

        for(duint j = 0; j < po->numSegs; ++j)
        {
            linedef_t* lineDef = ((objectrecord_t*) po->lineDefs[j])->obj;
            seg_t* seg = &po->segs[j];

            lineDef->halfEdges[0]->data = seg;
            seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[FRONT]->buildData.index - 1];
        }
    }

    buildSectorSubsectorLists();
    buildSectorLineLists();
    finishLineDefs2();
    finishSectors2();
    updateMapBounds();

    initSoundEnvironment();
    findSubsectorMidPoints();

    vertexinfo_t* vertexInfo = std::calloc(1, sizeof(vertexinfo_t) * numVertices());

    buildVertexOwnerRings(vertexInfo);

#if _DEBUG
checkVertexOwnerRings(vertexInfo, numVertices());
#endif

    // Build the vertex lineowner info.
    // @todo now redundant, rewire fakeradio to use HalfEdgeDS.
    for(duint i = 0; i < numVertices(); ++i)
    {
        Vertex* vertex = _halfEdgeDS->vertices[i];
        vertexinfo_t* vInfo = &vertexInfo[i];

        reinterpret_cast<MVertex*>(vertex->data)->lineOwners = vInfo->lineOwners;
        reinterpret_cast<MVertex*>(vertex->data)->numLineOwners = vInfo->numLineOwners;
    }
    std::free(vertexInfo);

    _editActive = false;
    return true;
}

void Map_BeginFrame(map_t* map, boolean resetNextViewer)
{
    assert(map);

    clearSectorFlags(map);
    if(map->_watchedPlaneList)
        interpolateWatchedPlanes(map, resetNextViewer);
    if(map->_movingSurfaceList)
        interpolateMovingSurfaces(map, resetNextViewer);

    if(!freezeRLs)
    {
        clearSubsectorContacts(map);

        LightGrid_Update(map->_lightGrid);
        SB_BeginFrame(map);
        LO_ClearForFrame(map);
        Rend_InitDecorationsForFrame(map);

        ParticleBlockmap_Empty(map->_particleBlockmap);
        R_CreateParticleLinks(map);

        LumobjBlockmap_Empty(map->_lumobjBlockmap);
        Rend_AddLuminousDecorations(map);
        LO_AddLuminousMobjs(map);
    }
}

void Map_EndFrame(map_t* map)
{
    assert(map);
    if(!freezeRLs)
    {
        // Wrap up with Source, Bias lights.
        SB_EndFrame(map);

        // Update the bias light editor.
        SBE_EndFrame(map);
    }
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char* Map_ID(map_t* map)
{
    assert(map);
    return map->mapID;
}

/**
 * @return              The 'unique' identifier of the map.
 */
const char* Map_UniqueName(map_t* map)
{
    assert(map);
    return map->uniqueID;
}

void Map_Bounds(map_t* map, float* min, float* max)
{
    assert(map);
    min[0] = map->bBox[BOXLEFT];
    min[1] = map->bBox[BOXBOTTOM];

    max[0] = map->bBox[BOXRIGHT];
    max[1] = map->bBox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int Map_AmbientLightLevel(map_t* map)
{
    assert(map);
    return map->ambientLightLevel;
}

uint Map_NumSectors(map_t* map)
{
    assert(map);
    return map->numSectors;
}

uint Map_NumLineDefs(map_t* map)
{
    assert(map);
    return map->numLineDefs;
}

uint Map_NumSideDefs(map_t* map)
{
    assert(map);
    return map->numSideDefs;
}

uint Map_NumVertexes(map_t* map)
{
    assert(map);
    return HalfEdgeDS_NumVertices(Map_HalfEdgeDS(map));
}

uint Map_NumPolyobjs(map_t* map)
{
    assert(map);
    return map->numPolyObjs;
}

uint Map_NumSegs(map_t* map)
{
    assert(map);
    return map->numSegs;
}

uint Map_NumSubsectors(map_t* map)
{
    assert(map);
    return map->numSubsectors;
}

uint Map_NumNodes(map_t* map)
{
    assert(map);
    return map->numNodes;
}

uint Map_NumPlanes(map_t* map)
{
    assert(map);
    return map->numPlanes;
}

thinkers_t* Map_Thinkers(map_t* map)
{
    assert(map);
    return map->_thinkers;
}

halfedgeds_t* Map_HalfEdgeDS(map_t* map)
{
    assert(map);
    return map->_halfEdgeDS;
}

mobjblockmap_t* Map_MobjBlockmap(map_t* map)
{
    assert(map);
    return map->_thingBlockmap;
}

linedefblockmap_t* Map_LineDefBlockmap(map_t* map)
{
    assert(map);
    return map->_lineDefBlockmap;
}

subsectorblockmap_t* Map_SubsectorBlockmap(map_t* map)
{
    assert(map);
    return map->_subsectorBlockmap;
}

particleblockmap_t* Map_ParticleBlockmap(map_t* map)
{
    assert(map);
    return map->_particleBlockmap;
}

lumobjblockmap_t* Map_LumobjBlockmap(map_t* map)
{
    assert(map);
    return map->_lumobjBlockmap;
}

lightgrid_t* Map_LightGrid(map_t* map)
{
    assert(map);
    return map->_lightGrid;
}

void Map_MarkAllSectorsForLightGridUpdate(map_t* map)
{
    assert(map);
    {
    lightgrid_t* lg = map->_lightGrid;

    if(lg->inited)
    {
        uint i;

        // Mark all blocks and contributors.
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];
            LightGrid_MarkForUpdate(map->_lightGrid, sec->blocks, sec->changedBlockCount, sec->blockCount);
        }
    }
    }
}

seg_t* Map_CreateSeg(map_t* map, linedef_t* lineDef, byte side, HalfEdge* halfEdge)
{
    assert(map);
    assert(halfEdge);
    {
    seg_t* seg = P_CreateSeg();

    seg->halfEdge = halfEdge;
    seg->side = side;
    seg->sideDef = NULL;
    if(lineDef && lineDef->buildData.sideDefs[seg->side])
        seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[seg->side]->buildData.index - 1];

    if(seg->sideDef)
    {
        linedef_t* ldef = seg->sideDef->lineDef;
        vertex_t* vtx = ldef->buildData.v[seg->side];

        seg->offset = P_AccurateDistance(halfEdge->HE_v1->pos[0] - vtx->pos[0],
                                         halfEdge->HE_v1->pos[1] - vtx->pos[1]);
    }

    seg->angle =
        bamsAtan2((int) (halfEdge->twin->vertex->pos[1] - halfEdge->vertex->pos[1]),
                  (int) (halfEdge->twin->vertex->pos[0] - halfEdge->vertex->pos[0])) << FRACBITS;

    // Calculate the length of the segment. We need this for
    // the texture coordinates. -jk
    seg->length = P_AccurateDistance(halfEdge->HE_v2->pos[0] - halfEdge->HE_v1->pos[0],
                                     halfEdge->HE_v2->pos[1] - halfEdge->HE_v1->pos[1]);

    if(seg->length == 0)
        seg->length = 0.01f; // Hmm...

    // Calculate the surface normals
    // Front first
    if(seg->sideDef)
    {
        surface_t* surface = &seg->sideDef->SW_topsurface;

        surface->normal[1] = (halfEdge->HE_v1->pos[0] - halfEdge->HE_v2->pos[0]) / seg->length;
        surface->normal[0] = (halfEdge->HE_v2->pos[1] - halfEdge->HE_v1->pos[1]) / seg->length;
        surface->normal[VZ] = 0;

        // All surfaces of a sidedef have the same normal.
        memcpy(seg->sideDef->SW_middlenormal, surface->normal, sizeof(surface->normal));
        memcpy(seg->sideDef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
    }

    halfEdge->data = seg;

    P_CreateObjectRecord(DMU_SEG, seg);
    map->segs = Z_Realloc(map->segs, ++map->numSegs * sizeof(seg_t*), PU_STATIC);
    map->segs[map->numSegs-1] = seg;
    return seg;
    }
}

subsector_t* Map_CreateSubsector(map_t* map, face_t* face, sector_t* sector)
{
    assert(map);
    assert(face);
    {
    subsector_t* subsector = P_CreateSubsector();
    size_t hEdgeCount;
    HalfEdge* halfEdge;

    hEdgeCount = 0;
    halfEdge = face->halfEdge;
    do
    {
        hEdgeCount++;
    } while((halfEdge = halfEdge->next) != face->halfEdge);

    subsector->face = face;
    subsector->hEdgeCount = (uint) hEdgeCount;

    subsector->sector = sector;
    if(!subsector->sector)
        Con_Message("Map_CreateSubsector: Warning orphan subsector %p (%i half-edges).\n",
                    subsector, subsector->hEdgeCount);

    face->data = (void*) subsector;

    P_CreateObjectRecord(DMU_SUBSECTOR, subsector);
    map->subsectors = Z_Realloc(map->subsectors, ++map->numSubsectors * sizeof(subsector_t*), PU_STATIC);
    map->subsectors[map->numSubsectors-1] = subsector;

    return subsector;
    }
}

Node* Map::createNode(const Partition& partition, const MapRectangled& rightAABB, const MapRectangle& leftAABB)
{
    Node* node = Node(x, y, dX, dY, rightAABB, leftAABB);

    P_CreateObjectRecord(DMU_NODE, node);
    nodes = reinterpret_cast<Node*>std::realloc(nodes, ++_numNodes * sizeof(Node*)));
    nodes[_numNodes-1] = node;
    return node;
}

void Map::initLinks()
{
    thingNodes = NodePile(256); // Allocate a small pile.
    lineDefNodes = NodePile(_numLineDefs + 1000);

    lineLinks = reinterpret_cast<NodePile::Index>(std::malloc(sizeof(*lineLinks) * _numLineDefs));
    for(duint i = 0; i < _numLineDefs; ++i)
        lineLinks[i] = lineDefNodes->newIndex(NP_ROOT_NODE);
}

void Map::initSkyFix()
{
    skyFixFloor = MAXFLOAT;
    skyFixCeiling = MINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    for(duint i = 0; i < _numSectors; ++i)
        updateSkyFixForSector(i);
}

void Map::updateSkyFixForSector(duint sectorIndex)
{
    assert(sectorIndex < _numSectors);

    const Sector& sector = *sectors[sectorIndex];
    bool skyFloor = sector.floor().surface.isSky();
    bool skyCeil = sector.ceiling().surface.isSky();

    if(!skyFloor && !skyCeil)
        return;

    if(skyCeil)
    {
        Thing* thing;

        // Adjust for the plane height.
        if(sector.ceiling().visHeight > skyFixCeiling)
        {   // Must raise the skyfix ceiling.
            skyFixCeiling = sector.ceiling().visHeight;
        }

        // Check that all the mobjs in the sector fit in.
        for(Thing* thing = sector.ThingList; thing; thing = thing->sNext)
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
        if(sector.floor().visHeight < skyFixFloor)
        {   // Must lower the skyfix floor.
            skyFixFloor = sector.floor().visHeight;
        }
    }

    // Update for middle textures on two sided linedefs which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    if(sector.lineDefs)
    {
        LineDef** linePtr = sector.lineDefs;

        while(*linePtr)
        {
            const LineDef& lineDef = **linePtr;

            // Must be twosided.
            if(lineDef.hasFront() && lineDef.hasBack())
            {
                const SideDef& sideDef = &lineDef.frontSector() == sector? lineDef.front() : lineDef.back();

                if(sideDef.middle().material)
                {
                    if(skyCeil)
                    {
                        dfloat top = sector.ceiling().visHeight + sideDef.middle().visOffset[1];
                        if(top > skyFixCeiling)// Must raise the skyfix ceiling.
                            skyFixCeiling = top;
                    }

                    if(skyFloor)
                    {
                        dfloat bottom = sector.floor().visHeight + sideDef.middle().visOffset[1] - sideDef.middle().material->height;
                        if(bottom < skyFixFloor) // Must lower the skyfix floor.
                            skyFixFloor = bottom;
                    }
                }
            }
            *linePtr++;
        }
    }
}

Vertex* Map::createVertex2(dfloat x, dfloat y)
{
    Vertex* vtx;
    if(!editActive)
        return NULL;
    vtx = halfEdgeDS().createVertex();
    vtx->pos.x = x;
    vtx->pos.y = y;
    return vtx;
}

/**
 * Create a new vertex in currently loaded editable map.
 *
 * @param x             X coordinate of the new vertex.
 * @param y             Y coordinate of the new vertex.
 *
 * @return              Index number of the newly created vertex else 0 if
 *                      the vertex could not be created for some reason.
 */
objectrecordid_t Map_CreateVertex(map_t* map, float x, float y)
{
    assert(map);
    {
    vertex_t* v;
    if(!map->editActive)
        return 0;
    v = createVertex2(map, x, y);
    return v ? ((mvertex_t*) v->data)->index : 0;
    }
}

/**
 * Create many new vertexs in the currently loaded editable map.
 *
 * @param num           Number of vertexes to be created.
 * @param values        Ptr to an array containing the coordinates for the
 *                      vertexs to create [v0 X, vo Y, v1 X, v1 Y...]
 * @param indices       If not @c NULL, the indices of the newly created
 *                      vertexes will be written back here.
 */
boolean Map_CreateVertices(map_t* map, size_t num, float* values, objectrecordid_t* indices)
{
    assert(map);
    {
    uint n;

    if(!num || !values)
        return false;

    if(!map->editActive)
        return false;

    // Create many vertexes.
    for(n = 0; n < num; ++n)
    {
        vertex_t* v = createVertex(map, values[n * 2], values[n * 2 + 1]);

        if(indices)
            indices[n] = ((mvertex_t*) v->data)->index;
    }

    return true;
    }
}

static linedef_t* createLineDef(map_t* map, vertex_t* vtx1, vertex_t* vtx2,
    sidedef_t* front, sidedef_t* back)
{
    linedef_t* l;

    assert(map);

    if(!map->editActive)
        return NULL;

    l = createLineDef2(map);

    l->buildData.v[0] = vtx1;
    l->buildData.v[1] = vtx2;

    ((mvertex_t*) l->buildData.v[0]->data)->refCount++;
    ((mvertex_t*) l->buildData.v[1]->data)->refCount++;

    l->dX = vtx2->pos[0] - vtx1->pos[0];
    l->dY = vtx2->pos[1] - vtx1->pos[1];
    l->length = P_AccurateDistance(l->dX, l->dY);

    l->angle =
        bamsAtan2((int) (l->buildData.v[1]->pos[1] - l->buildData.v[0]->pos[1]),
                  (int) (l->buildData.v[1]->pos[0] - l->buildData.v[0]->pos[0])) << FRACBITS;

    if(l->dX == 0)
        l->slopeType = ST_VERTICAL;
    else if(l->dY == 0)
        l->slopeType = ST_HORIZONTAL;
    else
    {
        if(l->dY / l->dX > 0)
            l->slopeType = ST_POSITIVE;
        else
            l->slopeType = ST_NEGATIVE;
    }

    if(l->buildData.v[0]->pos[0] < l->buildData.v[1]->pos[0])
    {
        l->bBox[BOXLEFT]   = l->buildData.v[0]->pos[0];
        l->bBox[BOXRIGHT]  = l->buildData.v[1]->pos[0];
    }
    else
    {
        l->bBox[BOXLEFT]   = l->buildData.v[1]->pos[0];
        l->bBox[BOXRIGHT]  = l->buildData.v[0]->pos[0];
    }

    if(l->buildData.v[0]->pos[1] < l->buildData.v[1]->pos[1])
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[1];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[1];
    }
    else
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[1];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[1];
    }

    // Remember the number of unique references.
    if(front)
    {
        front->lineDef = l;
        front->buildData.refCount++;
    }

    if(back)
    {
        back->lineDef = l;
        back->buildData.refCount++;
    }

    l->buildData.sideDefs[FRONT] = front;
    l->buildData.sideDefs[BACK] = back;

    l->inFlags = 0;

    // Determine the default linedef flags.
    l->flags = 0;
    if(!front || !back)
        l->flags |= DDLF_BLOCKING;

    return l;
}

/**
 * Create a new linedef in the editable map.
 *
 * @param v1            Idx of the start vertex.
 * @param v2            Idx of the end vertex.
 * @param frontSide     Idx of the front sidedef.
 * @param backSide      Idx of the back sidedef.
 * @param flags         Currently unused.
 *
 * @return              Idx of the newly created linedef else @c 0 if there
 *                      was an error.
 */
objectrecordid_t Map_CreateLineDef(map_t* map, objectrecordid_t v1,
    objectrecordid_t v2, uint frontSide, uint backSide, int flags)
{
    assert(map);
    {
    linedef_t* l;
    sidedef_t* front = NULL, *back = NULL;
    vertex_t* vtx1, *vtx2;
    float length, dx, dy;

    if(!map->editActive)
        return 0;
    if(frontSide > map->numSideDefs)
        return 0;
    if(backSide > map->numSideDefs)
        return 0;
    if(v1 == 0 || v1 > Map_NumVertexes(map))
        return 0;
    if(v2 == 0 || v2 > Map_NumVertexes(map))
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide && map->sideDefs[frontSide - 1]->buildData.refCount)
        return 0; // Already in use.
    if(backSide && map->sideDefs[backSide - 1]->buildData.refCount)
        return 0; // Already in use.

    // Next, check the length is not zero.
    vtx1 = Map_HalfEdgeDS(map)->vertices[v1 - 1];
    vtx2 = Map_HalfEdgeDS(map)->vertices[v2 - 1];
    dx = vtx2->pos[0] - vtx1->pos[0];
    dy = vtx2->pos[1] - vtx1->pos[1];
    length = P_AccurateDistance(dx, dy);
    if(!(length > 0))
        return 0;

    if(frontSide > 0)
        front = map->sideDefs[frontSide - 1];
    if(backSide > 0)
        back = map->sideDefs[backSide - 1];

    l = createLineDef(map, vtx1, vtx2, front, back);

    return l->buildData.index;
    }
}

static sidedef_t* createSideDef(map_t* map, sector_t* sector, short flags,
    Material* topMaterial, float topOffsetX, float topOffsetY, float topRed,
    float topGreen, float topBlue, Material* middleMaterial, float middleOffsetX,
    float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
    float middleAlpha, Material* bottomMaterial, float bottomOffsetX,
    float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue)
{
    sidedef_t* s = NULL;

    assert(map);

    if(map->editActive)
    {
        s = createSideDef2(map);
        s->flags = flags;
        s->sector = sector;

        Surface_SetMaterial(&s->SW_topsurface, topMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_topsurface, topOffsetX, topOffsetY);
        Surface_SetColorRGBA(&s->SW_topsurface, topRed, topGreen, topBlue, 1);

        Surface_SetMaterial(&s->SW_middlesurface, middleMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_middlesurface, middleOffsetX, middleOffsetY);
        Surface_SetColorRGBA(&s->SW_middlesurface, middleRed, middleGreen, middleBlue, middleAlpha);

        Surface_SetMaterial(&s->SW_bottomsurface, bottomMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_bottomsurface, bottomOffsetX, bottomOffsetY);
        Surface_SetColorRGBA(&s->SW_bottomsurface, bottomRed, bottomGreen, bottomBlue, 1);
    }

    return s;
}

objectrecordid_t Map_CreateSideDef(map_t* map, objectrecordid_t sector,
    short flags, Material* topMaterial, float topOffsetX, float topOffsetY,
    float topRed, float topGreen, float topBlue, Material* middleMaterial,
    float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen,
    float middleBlue, float middleAlpha, Material* bottomMaterial,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue)
{
    assert(map);
    {
    sidedef_t* s;
    if(!map->editActive)
        return 0;
    if(sector > Map_NumSectors(map))
        return 0;

    s = createSideDef(map, (sector == 0? NULL: map->sectors[sector-1]), flags,
                      topMaterial? ((objectrecord_t*) topMaterial)->obj : NULL,
                      topOffsetX, topOffsetY, topRed, topGreen, topBlue,
                      middleMaterial? ((objectrecord_t*) middleMaterial)->obj : NULL,
                      middleOffsetX, middleOffsetY, middleRed, middleGreen, middleBlue,
                      middleAlpha, bottomMaterial? ((objectrecord_t*) bottomMaterial)->obj : NULL,
                      bottomOffsetX, bottomOffsetY, bottomRed, bottomGreen, bottomBlue);

    return s ? s->buildData.index : 0;
    }
}

static sector_t* createSector(map_t* map, float lightLevel, float red, float green, float blue)
{
    sector_t* s;

    assert(map);

    if(!map->editActive)
        return NULL;

    s = createSector2(map);

    s->planeCount = 0;
    s->planes = NULL;
    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightLevel = MINMAX_OF(0, lightLevel, 1);

    return s;
}

objectrecordid_t Map_CreateSector(map_t* map, float lightLevel, float red, float green, float blue)
{
    assert(map);
    {
    sector_t* s;
    if(!map->editActive)
        return 0;
    s = createSector(map, lightLevel, red, green, blue);
    return s ? s->buildData.index : 0;
    }
}

static plane_t* createPlane(map_t* map, float height, Material* material,
                            float matOffsetX, float matOffsetY, float r, float g,
                            float b, float a, float normalX, float normalY,
                            float normalZ)
{
    plane_t* pln;

    assert(map);

    if(!map->editActive)
        return NULL;

    pln = createPlane2(map);

    pln->height = pln->visHeight = pln->oldHeight[0] =
        pln->oldHeight[1] = height;
    pln->visHeightDelta = 0;
    pln->speed = 0;
    pln->target = 0;

    pln->glowRGB[CR] = pln->glowRGB[CG] = pln->glowRGB[CB] = 1;
    pln->glow = 0;

    Surface_SetMaterial(&pln->surface, material? ((objectrecord_t*) material)->obj : NULL, false);
    Surface_SetColorRGBA(&pln->surface, r, g, b, a);
    Surface_SetBlendMode(&pln->surface, BM_NORMAL);
    Surface_SetMaterialOffsetXY(&pln->surface, matOffsetX, matOffsetY);

    pln->PS_normal[0] = normalX;
    pln->PS_normal[1] = normalY;
    pln->PS_normal[VZ] = normalZ;
    /*if(pln->PS_normal[VZ] < 0)
        pln->type = PLN_CEILING;
    else
        pln->type = PLN_FLOOR;*/
    M_Normalize(pln->PS_normal);
    return pln;
}

objectrecordid_t Map_CreatePlane(map_t* map, float height, Material* material,
    float matOffsetX, float matOffsetY, float r, float g, float b, float a,
    float normalX, float normalY, float normalZ)
{
    assert(map);
    {
    plane_t* p;

    if(!map->editActive)
        return 0;

    p = createPlane(map, height, material, matOffsetX, matOffsetY, r, g, b, a, normalX, normalY, normalZ);
    return p ? p->buildData.index : 0;
    }
}

static Polyobj* createPolyobj(map_t* map, objectrecordid_t* lines, uint lineCount,
    int tag, int sequenceType, float anchorX, float anchorY)
{
    Polyobj* po;
    uint i;

    if(!map->editActive)
        return NULL;

    po = createPolyobj2(map);

    po->idx = map->numPolyObjs - 1; // 0-based index.
    po->lineDefs = Z_Calloc(sizeof(linedef_t*) * (lineCount+1), PU_STATIC, 0);
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line = map->lineDefs[lines[i] - 1];

        // This line is part of a polyobj.
        line->inFlags |= LF_POLYOBJ;

        po->lineDefs[i] = line;
    }
    po->lineDefs[i] = NULL;
    po->numLineDefs = lineCount;

    po->tag = tag;
    po->seqType = sequenceType;
    po->pos[0] = anchorX;
    po->pos[1] = anchorY;

    return po;
}

objectrecordid_t Map_CreatePolyobj(map_t* map, objectrecordid_t* lines, uint lineCount, int tag,
                                   int sequenceType, float anchorX, float anchorY)
{
    assert(map);
    {
    uint i;
    Polyobj* po;

    if(!map->editActive)
        return 0;
    if(!lineCount || !lines)
        return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line;

        if(lines[i] == 0 || lines[i] > Map_NumLineDefs(map))
            return 0;

        line = map->lineDefs[lines[i] - 1];
        if(line->inFlags & LF_POLYOBJ)
            return 0;
    }

    po = createPolyobj(map, lines, lineCount, tag, sequenceType, anchorX, anchorY);

    return po ? po->buildData.index : 0;
    }
}

Polyobj* Map::polyobj(duint num)
{
    if(num & 0x80000000)
    {
        duint idx = num & 0x7fffffff;

        if(idx < _numPolyObjs)
            return polyobjs[idx];
        return NULL;
    }

    for(duint i = 0; i < _numPolyObjs; ++i)
    {
        Polyobj* po = polyobjs[i];

        if(duint(po->tag) == num)
        {
            return po;
        }
    }
    return NULL;
}

static void findDominantLightSources(map_t* map)
{
#define DOMINANT_SIZE   1000

    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];

        if(!sec->lineDefCount)
            continue;

        // Is this sector large enough to be a dominant light source?
        if(sec->lightSource == NULL && sec->planeCount > 0 &&
           sec->bBox[BOXRIGHT] - sec->bBox[BOXLEFT]   > DOMINANT_SIZE &&
           sec->bBox[BOXTOP]   - sec->bBox[BOXBOTTOM] > DOMINANT_SIZE)
        {
            if(R_SectorContainsSkySurfaces(sec))
            {
                uint k;

                // All sectors touching this one will be affected.
                for(k = 0; k < sec->lineDefCount; ++k)
                {
                    linedef_t* lin = sec->lineDefs[k];
                    sector_t* other;

                    other = LINE_FRONTSECTOR(lin);
                    if(other == sec)
                    {
                        if(LINE_BACKSIDE(lin))
                            other = LINE_BACKSECTOR(lin);
                    }

                    if(other && other != sec)
                        other->lightSource = sec;
                }
            }
        }
    }

#undef DOMINANT_SIZE
}

static boolean PIT_LinkObjToSubsector(uint subsectorIdx, void* data)
{
    linkobjtosubsectorparams_t* params = (linkobjtosubsectorparams_t*) data;
    objcontact_t* con = allocObjContact();

    con->obj = params->obj;

    // Link the contact list for this face.
    linkContactToSubsector(params->map, subsectorIdx, params->type, con);

    return true; // Continue iteration.
}

void Map_AddSubsectorContact(map_t* map, uint subsectorIdx, objcontacttype_t type,
                             void* obj)
{
    linkobjtosubsectorparams_t params;

    assert(map);
    assert(subsectorIdx < map->numSubsectors);
    assert(type >= OCT_FIRST && type < NUM_OBJCONTACT_TYPES);
    assert(obj);

    params.map = map;
    params.obj = obj;
    params.type = type;

    PIT_LinkObjToSubsector(subsectorIdx, &params);
}

boolean Map_IterateSubsectorContacts(map_t* map, uint subsectorIdx, objcontacttype_t type,
                                     boolean (*callback) (void*, void*),
                                     void* data)
{
    objcontact_t* con;

    assert(map);
    assert(subsectorIdx < map->numSubsectors);
    assert(callback);

    con = map->_subsectorContacts[subsectorIdx].head[type];
    while(con)
    {
        if(!callback(con->obj, data))
            return false;

        con = con->next;
    }

    return true;
}

void Map::initSectorShadows()
{
    duint startTime = Sys_GetRealTime();

    duint i;
    vec2_t bounds[2], point;
    vertex_t* vtx0, *vtx1;
    lineowner_t* vo0, *vo1;
    shadowlinkerparms_t data;

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

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* side = map->sideDefs[i];
        byte sid;

        if(!R_IsShadowingLineDef(side->lineDef))
            continue;

        sid = LINE_BACKSIDE(side->lineDef) == side? BACK : FRONT;

        vtx0 = LINE_VERTEX(side->lineDef, sid);
        vtx1 = LINE_VERTEX(side->lineDef, sid^1);
        vo0 = side->lineDef->L_vo(sid)->next();
        vo1 = side->lineDef->L_vo(sid^1)->prev();

        // Use the extended points, they are wider than inoffsets.
        V2_Set(point, vtx0->pos[VX], vtx0->pos[VY]);
        V2_InitBox(bounds, point);

        V2_Sum(point, point, vo0->shadowOffsets.extended);
        V2_AddToBox(bounds, point);

        V2_Set(point, vtx1->pos[VX], vtx1->pos[VY]);
        V2_AddToBox(bounds, point);

        V2_Sum(point, point, vo1->shadowOffsets.extended);
        V2_AddToBox(bounds, point);

        data.lineDef = side->lineDef;
        data.side = sid;

        Map_SubsectorsBoxIteratorv(map, bounds, side->sector,
                                   RIT_ShadowSubsectorLinker, &data, false);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitSectorShadows: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Begin the process of loading a new map.
 *
 * @param levelId       Identifier of the map to be loaded (eg "E1M1").
 *
 * @return              @c true, if the map was loaded successfully.
 */
boolean Map_Load(map_t* map)
{
    assert(map);

    Con_Message("Map_Load: \"%s\"\n", map->mapID);

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
        int i;
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < DDMAXPLAYERS; ++i)
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

    if(!DAM_TryMapConversion(map->mapID))
        return false;

    if(1)//(map = DAM_LoadMap(mapID)))
    {
        ded_sky_t* skyDef = NULL;
        ded_mapinfo_t* mapInfo;

        // Initialize The Logical Sound Manager.
        S_MapChange();

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(map, DMU_LINEDEF);
            gx.SetupForMapData(map, DMU_SIDEDEF);
            gx.SetupForMapData(map, DMU_SECTOR);
        }

        {
        uint starttime = Sys_GetRealTime();

        // Must be called before any mobjs are spawned.
        initLinks(map);

        // How much time did we spend?
        VERBOSE(Con_Message
                ("initLinks: Allocating line link rings. Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - starttime) / 1000.0f));
        }

        findDominantLightSources(map);

        buildMobjBlockmap(map);
        buildSubsectorBlockmap(map);
        buildParticleBlockmap(map);
        buildLumobjBlockmap(map);

        initSubsectorContacts(map);

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
            map->gravity = mapInfo->gravity;
            map->ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found, so set some basic stuff.
            map->gravity = 1.0f;
            map->ambientLightLevel = 0;
        }

        map->_halfEdgeDS.iterateVertices(updateVertexShadowOffsets);

        R_InitSectorShadows(map);

        {
        uint startTime = Sys_GetRealTime();

        Map_InitSkyFix(map);

        // How much time did we spend?
        VERBOSE(Con_Message("  InitialSkyFix: Done in %.2f seconds.\n",
                            (Sys_GetRealTime() - startTime) / 1000.0f));
        }

        // Init the thinker lists (public and private).
        Thinkers_Init(map->_thinkers, 0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(map);

        Cl_Reset();
        RL_DeleteLists();
        R_CalcLightModRange(NULL);

        // Invalidate old cmds and init player values.
        {
        int i;
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            User* plr = &ddPlayers[i];

            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        Materials_RewindAnimationGroups();

        LO_InitForMap(map); // Lumobj management.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        initGenerators(map);

        // Initialize the lighting grid.
        LightGrid_Init(map);

        return true;
    }

    return false;
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

    if(P_PointOnDivlineSide(FIX2FLT(los->trace.pos[VX]),
                            FIX2FLT(los->trace.pos[VY]), dlPtr) ==
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
           side != LineDef_PointOnSide(li, FIX2FLT(los->trace.pos[VX]),
                                           FIX2FLT(los->trace.pos[VY])))
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
            side == LineDef_PointOnSide(li, FIX2FLT(los->trace.pos[VX]),
                                        FIX2FLT(los->trace.pos[VY]))))
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
        byte side = R_PointOnSide(FIX2FLT(los->trace.pos[VX]),
                                  FIX2FLT(los->trace.pos[VY]),
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
    los.trace.pos[VX] = FLT2FIX(from[VX]);
    los.trace.pos[VY] = FLT2FIX(from[VY]);
    los.trace.dX = FLT2FIX(to[VX] - from[VX]);
    los.trace.dY = FLT2FIX(to[VY] - from[VY]);
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

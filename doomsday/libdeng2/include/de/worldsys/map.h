/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_MAP_H
#define LIBDENG2_MAP_H

#include "../ISerializable"
#include "../Enumerator"
#include "../Record"
#include "../String"
#include "../Id"
#include "../HalfEdgeDS"
#include "../BinaryTree"
#include "../MobjBlockmap"
#include "../LineDefBlockmap"
#include "../SubsectorBlockmap"
#include "../LumObjBlockmap"
#include "../ParticleBlockmap"
#include "../GameObjRecords"
#include "../Thinker"
#include "../Vertex"
#include "../SideDef"
#include "../LineDef"
#include "../Sector"
#include "../Seg"
#include "../Subsector"
#include "../Node"

#include <map>

namespace de
{
    class Object;
    
    /**
     * Contains everything that makes a map work: sectors, lines, scripts, 
     * objects, etc. The game plugin is responsible for creating concrete
     * instances of Map. The game plugin can extend this with whatever 
     * information it needs.
     *
     * @ingroup world
     */
    class LIBDENG2_API Map : public ISerializable
    {
    public:
        /// Requested type casting was impossible. @ingroup errors
        DEFINE_ERROR(TypeError);
        
        /// The thinker that was searched for could not be found. @ingroup errors
        DEFINE_ERROR(NotFoundError);
        
        typedef std::map<Id, Thinker*> Thinkers;
                
    public:
        /**
         * Constructs an empty map.
         */
        Map();
        
        virtual ~Map();

        void clear();
        
        /**
         * Loads a map.
         *
         * @param name  Identifier of the map. The resources of the map are
         *              located based on this identifier.
         */
        virtual void load(const String& name);

        const String& name() const { return _name; }

        const Record& info() const { return _info; }

        Record& info() { return _info; }

        /**
         * Determines whether the map is void. A map is void when no map data 
         * has been loaded.
         */
        bool isVoid() const;

        /**
         * Returns a new unique thinker id.
         */
        Id findUniqueThinkerId();

        /**
         * Creates a new object in the map.
         *
         * @return  The new object. Map keeps ownership.
         */
        Object& newObject();

        /**
         * Adds a thinker to the map. The thinker will be assigned a new unique
         * id from the map's Enumerator.
         *
         * @param thinker  Thinker to add. Map takes ownership.
         */
        Thinker& add(Thinker* thinker);

        template <typename Type>
        Type& addAs(Type* thinker) {
            add(thinker);
            return *thinker;
        }
        
        /**
         * Removes and deletes a thinker in the map.
         *
         * @param thinker  Thinker to remove and delete.
         */
        void destroy(Thinker* thinker);

        /**
         * Returns all thinkers of the map.
         */
        const Thinkers& thinkers() const { return _thinkers; }

        /**
         * Returns a thinker with the specified id, or @c NULL if it doesn't exist.
         * Does not return objects.
         */
        Thinker* thinker(const Id& id) const;
        
        /**
         * Returns an object with the specified id, or @c NULL if it doesn't exist.
         */
        Object* object(const Id& id) const;

        /**
         * Finds any thinker (regular or object) with the specified id.
         *
         * @param id  Thinker/object id.
         *
         * @return  Thinker casted to @a Type.
         */
        template <typename Type>
        Type& anyThinker(const Id& id) const {
            Type* t = dynamic_cast<Type*>(thinker(id));
            if(t)
            {
                return *t;
            }
            /// @throw TypeError  Thinker's type was different than expected.
            throw TypeError("Map::anyThinker", "Thinker not found, or has unexpected type");
        }

        /**
         * Iterates through thinkers of a specific type.
         *
         * @param serialId    Type of thinker to iterate.
         * @param callback    Callback function that gets called on each thinker. Iteration
         *                    continues if the callback function returns @c true. Iteration
         *                    is aborted if @c false is returned.
         * @param parameters  Extra parameter passed to the callback function.
         *
         * @return  @c true, if all calls to the callback function returned @c true. 
         *          Otherwise, @c false.
         */
        bool iterate(Thinker::SerialId serialId, bool (*callback)(Thinker*, void*), void* parameters = 0);

        /**
         * Iterates through objects.
         *
         * @param callback    Callback function that gets called on each object. Iteration
         *                    continues if the callback function returns @c true. Iteration
         *                    is aborted if @c false is returned.
         * @param parameters  Extra parameter passed to the callback function.
         *
         * @return  @c true, if all calls to the callback function returned @c true. 
         *          Otherwise, @c false.
         */
        bool iterateObjects(bool (*callback)(Object*, void*), void* parameters = 0);

        /**
         * Performs thinking for all thinkers.
         *
         * @param elapsed  Amount of time elapsed since previous thinking.
         */
        void think(const Time::Delta& elapsed);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    protected:
        void addThinker(Thinker* thinker);
        void freezeThinkerList(bool freeze);
        bool markedForDestruction(const Thinker* thinker) const;
        
    private:
        /// Name of the map.
        String _name;
        
        /// Map-specific information. Lost when the map changes.
        Record _info;
        
        /// Generates ids for thinkers (objects, too).
        Enumerator _thinkerEnum;
        
        /// All thinkers of the map.
        Thinkers _thinkers;
        
        /// Is the addition and removal of thinkers currently allowed?
        dint _thinkersFrozen;

        /// While frozen, thinkers to add and remove will be stored here.
        typedef std::list<const Thinker*> PendingThinkers;
        PendingThinkers _thinkersToAdd;
        PendingThinkers _thinkersToDestroy;

    public:
        typedef dint objectrecordid_t;

        // Runtime map data objects, such as vertices, sectors, and subsectors all
        // have this header as their first member. This makes it possible to treat
        // an unknown map data pointer as a runtime_mapdata_header_t* and determine
        // its type. Note that this information is internal to the engine.
        typedef struct runtime_mapdata_header_s {
            dint            type; // One of the DMU type constants.
        } runtime_mapdata_header_t;

        typedef struct objrecord_s {
            dint            dummy;
            runtime_mapdata_header_t header;
            objectrecordid_t id;
            void*           obj;
        } objectrecord_t;

        typedef enum {
            OCT_FIRST = 0,
            OCT_MOBJ = OCT_FIRST,
            OCT_LUMOBJ,
            OCT_PARTICLE,
            NUM_OBJCONTACT_TYPES
        } objcontacttype_t;

        //char mapID[9];
        //char uniqueID[256];
        bool editActive;

        //struct thinkers_s* _thinkers;
        struct gameobjrecords_s* _gameObjectRecords;

        HalfEdgeDS* _halfEdgeDS;
        BinaryTree<Node>* _rootNode;

        MobjBlockmap* _mobjBlockmap;
        LineDefBlockmap* _lineDefBlockmap;
        SubsectorBlockmap* _subsectorBlockmap;

        /// Following blockmaps are emptied each render frame.
        ParticleBlockmap* _particleBlockmap;
        LumObjBlockmap* _lumobjBlockmap;

        //struct objcontactlist_s* _subsectorContacts; // List of obj contacts for each subsector.
        //struct lightgrid_s* _lightGrid;

        dfloat bBox[4];

        duint _numSectors;
        Sector** sectors;

        duint _numLineDefs;
        LineDef** lineDefs;

        duint _numSideDefs;
        struct sidedef_s** sideDefs;

        duint _numPlanes;
        struct plane_s** planes;

        duint _numNodes;
        Node** nodes;

        duint _numSubsectors;
        Subsector** subsectors;

        duint _numSegs;
        Seg** segs;

        //duint _numPolyObjs;
        //struct polyobj_s** polyObjs;

        struct lineowner_s* lineOwners;

        //struct planelist_s* _watchedPlaneList;
        //struct surfacelist_s* _movingSurfaceList;
        //struct surfacelist_s* _decoratedSurfaceList;

        //struct nodepile_s* mobjNodes, *lineNodes; // All kinds of wacky links.
        //nodeindex_t* lineLinks; // Indices to roots.

        dfloat gravity;
        dint _ambientLightLevel;

        dfloat skyFixCeiling;
        dfloat skyFixFloor;

        /*struct {
            struct dynlist_s* linkList; // Surface-projected lumobjs (dynlights).
        } _dlights;*/

        /*struct {
            duint lastChangeOnFrame;
            dint editGrabbedID;
            dint numSourceDelta;

            struct sourcelist_s* sources;
            struct biassurface_s* surfaces; // Head of the biassurface list.
        } _bias;*/

        //bool load();
        void precache();

        //const char* ID();
        //const char* uniqueName();
        void bounds(dfloat* min, dfloat* max);
        dint ambientLightLevel();

        duint numSectors();
        duint numLineDefs();
        duint numSideDefs();
        duint numVertexes();
        //duint numPolyobjs();
        duint numSegs();
        duint numSubsectors();
        duint numNodes();
        duint numPlanes();

        void beginFrame(bool resetNextViewer);
        void endFrame();

        //void ticker(timespan_t time);

        void update();
        void updateWatchedPlanes();
        void updateMovingSurfaces();
        void updateSkyFixForSector(duint secIDX);

#if 0
        void linkMobj(struct mobj_s* mo, dbyte flags);
        dint unlinkMobj(struct mobj_s* mo);

        void addThinker(thinker_t* th, bool makePublic);
        void removeThinker(thinker_t* th);
        void thinkerSetStasis(thinker_t* th, bool on);

        /**
         * @defgroup iterateThinkerFlags Iterate Thinker Flags
         * Used with Map_IterateThinkers2 to specify which thinkers to iterate.
         */
        /*@{*/
        #define ITF_PUBLIC          0x1
        #define ITF_PRIVATE         0x2
        /*@}*/

        bool iterateThinkers2(think_t func, dbyte flags, dint (*callback) (void* p, void*), void* context);
#endif

        void addSubsectorContact(duint subsectorIdx, objcontacttype_t type, void* obj);
        bool iterateSubsectorContacts(duint subsectorIdx, objcontacttype_t type, bool (*func) (void*, void*), void* data);

        /**
         * Map Edit interface.
         */
        bool editEnd();

        objectrecordid_t createVertex(dfloat x, dfloat y);
        bool createVertices(dsize num, dfloat* values, objectrecordid_t* indices);
        objectrecordid_t createSideDef(objectrecordid_t sector, dshort flags,
            struct material_s* topMaterial,
            dfloat topOffsetX, dfloat topOffsetY, dfloat topRed,
            dfloat topGreen, dfloat topBlue,
            struct material_s* middleMaterial,
            dfloat middleOffsetX, dfloat middleOffsetY,
            dfloat middleRed, dfloat middleGreen,
            dfloat middleBlue, dfloat middleAlpha,
            struct material_s* bottomMaterial,
            dfloat bottomOffsetX, dfloat bottomOffsetY,
            dfloat bottomRed, dfloat bottomGreen,
            dfloat bottomBlue);
        objectrecordid_t createLineDef(objectrecordid_t v1, objectrecordid_t v2, duint frontSide, duint backSide, dint flags);
        objectrecordid_t createSector(dfloat lightlevel, dfloat red, dfloat green, dfloat blue);
        objectrecordid_t createPlane(dfloat height, struct material_s* material,
            dfloat matOffsetX, dfloat matOffsetY, dfloat r, dfloat g, dfloat b, dfloat a,
            dfloat normalX, dfloat normalY, dfloat normalZ);
        objectrecordid_t createPolyobj(objectrecordid_t* lines, duint linecount,
            dint tag, dint sequenceType, dfloat startX, dfloat startY);

        //bool gameObjectRecordProperty(const char* objName, duint idx, const char* propName, valuetype_t type, void* data);

        // Mobjs in bounding box iterators.
        bool mobjsBoxIterator(const dfloat box[4], bool (*func) (struct mobj_s*, void*), void* data);

        //bool mobjsBoxIteratorv(const arvec2_t box, bool (*func) (struct mobj_s*, void*), void* data);

        // LineDefs in bounding box iterators:
        //bool lineDefsBoxIteratorv(const arvec2_t box, bool (*func) (LineDef*, void*), void* data, bool retObjRecord);

        // Subsectors in bounding box iterators:
        bool subsectorsBoxIterator2(const dfloat box[4], Sector* sector,
             bool (*func) (Subsector*, void*),
             void* parm, bool retObjRecord);
        /*bool subsectorsBoxIteratorv(const arvec2_t box, Sector* sector,
             bool (*func) (Subsector*, void*),
             void* data, bool retObjRecord);*/

        //bool pathTraverse(dfloat x1, dfloat y1, dfloat x2, dfloat y2, dint flags, bool (*trav) (intercept_t*));
        bool checkLineSight(const dfloat from[3], const dfloat to[3], dfloat bottomSlope, dfloat topSlope, dint flags);

        Subsector* pointInSubsector2(dfloat x, dfloat y);
        Sector* sectorForOrigin(const void* ddMobjBase);

        //polyobj_t* polyobjForOrigin(const void* ddMobjBase);
        //polyobj_t* polyobj(duint num);

        // @todo the following should be Map private:
        //thinkers_t* thinkers();
        HalfEdgeDS& halfEdgeDS();
        MobjBlockmap* mobjBlockmap();
        LineDefBlockmap* lineDefBlockmap();
        SubsectorBlockmap* subsectorBlockmap();
        ParticleBlockmap* particleBlockmap();
        LumObjBlockmap* lumobjBlockmap();
        //lightgrid_t* lightGrid();

        // protected
        Seg* createSeg(LineDef* lineDef, dbyte side, HalfEdge* hEdge);
        Subsector* createSubsector(Face* face, Sector* sector);
        Node* createNode(dfloat x, dfloat y, dfloat dX, dfloat dY, dfloat rightAABB[4], dfloat leftAABB[4]);

        void initSkyFix();
        void markAllSectorsForLightGridUpdate();

        gameobjrecords_t* gameObjectRecords();
        void destroyGameObjectRecords();

        // Old public interface:
        //void initThinkers();
        //void runThinkers();
        //void thinkerAdd(thinker_t* th);
    };
}

#endif /* LIBDENG2_MAP_H */

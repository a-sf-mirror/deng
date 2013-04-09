/** @file edit_map.cpp Map Editor interface.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DENG_NO_API_MACROS_MAP_EDIT

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
#include "de_filesys.h"
#include "map/gamemap.h"
#ifdef __CLIENT__
#  include "render/rend_main.h"
#endif
#include <de/StringPool>

#include "audio/s_environ.h"

using namespace de;

#if 0
static int vertexCompare(void const *p1, void const *p2)
{
    Vertex const *a = (Vertex const *) *((void const **) p1);
    Vertex const *b = (Vertex const *) *((void const **) p2);

    if(a == b) return 0;

    if(int(a->origin()[VX]) != int(b->origin()[VX]))
        return int(a->origin()[VX]) - int(b->origin()[VX]);

    return int(a->origin()[VY]) - int(b->origin()[VY]);
}
#endif

/**
 * @defgroup pruneMapElementFlags Prune Map Element Flags
 * Flags for pruneMapElements()
 */
///@{
#define PRUNE_LINES         0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINES|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)
///@}

/**
 * Editable map data elements.
 *
 * @todo Move into GameMap.
 */
class EditableMap
{
public:
    GameMap::Vertexes vertexes;
    GameMap::Lines lines;
    GameMap::SideDefs sideDefs;
    GameMap::Sectors sectors;
    GameMap::Polyobjs polyobjs;

    /// Map entity data (things, line specials, etc...).
    EntityDatabase *entityDatabase;

    EditableMap::EditableMap()
        : entityDatabase(0)
    {}

    Vertex *createVertex(coord_t x, coord_t y)
    {
        Vertex *vtx = new Vertex(x, y);

        vertexes.append(vtx);
        vtx->_buildData.index = vertexes.count(); // 1-based index, 0 = NIL.

        return vtx;
    }

    LineDef *createLine()
    {
        LineDef *line = new LineDef;

        lines.append(line);
        line->setOrigIndex(lines.count()); // 1-based index, 0 = NIL.

        return line;
    }

    SideDef *createSideDef()
    {
        SideDef *sideDef = new SideDef;

        sideDefs.append(sideDef);
        sideDef->_buildData.index = sideDefs.count(); // 1-based index, 0 = NIL.

        return sideDef;
    }

    Sector *createSector(float lightLevel, Vector3f const &lightColor)
    {
        Sector *sector = new Sector(lightLevel, lightColor);

        sectors.append(sector);
        sector->_origIndex = sectors.count(); // 1-based index, 0 = NIL.

        return sector;
    }

    Polyobj *createPolyobj()
    {
        void *region = M_Calloc(POLYOBJ_SIZE);
        Polyobj *po = new (region) Polyobj;

        polyobjs.append(po);
        po->buildData.index = polyobjs.count(); // 1-based index, 0 = NIL.

        return po;
    }

    void clear()
    {
        qDeleteAll(vertexes);
        vertexes.clear();

        qDeleteAll(lines);
        lines.clear();

        qDeleteAll(sideDefs);
        sideDefs.clear();

        qDeleteAll(sectors);
        sectors.clear();

        foreach(Polyobj *po, polyobjs)
        {
            po->~Polyobj();
            M_Free(po);
        }
        polyobjs.clear();
    }

#if 0
    void markDuplicateVertexes()
    {
        Vertex **hits = (Vertex **) M_Malloc(vertexes.count() * sizeof(Vertex *));

        // Sort array of ptrs.
        for(uint i = 0; i < vertexes.count(); ++i)
        {
            hits[i] = vertexes[i];
        }
        qsort(hits, vertexes.count(), sizeof(Vertex *), vertexCompare);

        // Now mark them off.
        for(uint i = 0; i < vertexes.count() - 1; ++i)
        {
            // A duplicate?
            if(vertexCompare(hits + i, hits + i + 1) == 0)
            {
                // Yes.
                Vertex *a = hits[i];
                Vertex *b = hits[i + 1];

                b->_buildData.equiv = (a->_buildData.equiv ? a->_buildData.equiv : a);
            }
        }

        M_Free(hits);
    }

    void findEquivalentVertexes()
    {
        uint i, newNum;

        // Scan all linedefs.
        for(i = 0, newNum = 0; i < lines.count(); ++i)
        {
            LineDef *l = lines[i];

            // Handle duplicated vertices.
            while(l->v[0]->_buildData.equiv)
            {
                l->v[0]->_buildData.refCount--;
                l->v[0] = l->v[0]->_buildData.equiv;
                l->v[0]->_buildData.refCount++;
            }

            while(l->v[1]->_buildData.equiv)
            {
                l->v[1]->_buildData.refCount--;
                l->v[1] = l->v[1]->_buildData.equiv;
                l->v[1]->_buildData.refCount++;
            }

            l->_buildData.index = newNum + 1;
            lines[newNum++] = lines[i];
        }
    }

    void pruneLinedefs()
    {
        uint i, newNum, unused = 0;

        for(i = 0, newNum = 0; i < lines.count(); ++i)
        {
            LineDef *l = lines[i];

            if(!l->hasFrontSideDef() && !l->hasBackSideDef())
            {
                unused++;

                M_Free(l);
                continue;
            }

            l->_buildData.index = newNum + 1;
            lines[newNum++] = l;
        }

        if(newNum < lines.count())
        {
            if(unused > 0)
                Con_Message("  Pruned %d unused linedefs.", unused);

            lines.resize(newNum);
        }
    }

    void pruneVertices()
    {
        uint i, newNum, unused = 0;

        // Scan all vertices.
        for(i = 0, newNum = 0; i < vertexes.count(); ++i)
        {
            Vertex *v = vertexes[i];

            if(v->_buildData.refCount < 0)
                Con_Error("Vertex %d ref_count is %d", i, v->_buildData.refCount);

            if(v->_buildData.refCount == 0)
            {
                if(v->_buildData.equiv == NULL)
                    unused++;

                M_Free(v);
                continue;
            }

            v->_buildData.index = newNum + 1;
            vertexes[newNum++] = v;
        }

        if(newNum < vertexes.count())
        {
            int dupNum = vertexes.count() - newNum - unused;

            if(unused > 0)
                Con_Message("  Pruned %d unused vertices.", unused);

            if(dupNum > 0)
                Con_Message("  Pruned %d duplicate vertices.", dupNum);

            vertexes.resize(newNum);
        }
    }

    void pruneUnusedSidedefs()
    {
        uint i, newNum, unused = 0;

        for(i = 0, newNum = 0; i < sideDefs.count(); ++i)
        {
            SideDef *s = sideDefs[i];

            if(s->buildData.refCount == 0)
            {
                unused++;

                M_Free(s);
                continue;
            }

            s->_buildData.index = newNum + 1;
            sideDefs[newNum++] = s;
        }

        if(newNum < sideDefs.count())
        {
            int dupNum = sideDefs.count() - newNum - unused;

            if(unused > 0)
                Con_Message("  Pruned %d unused sidedefs.", unused);

            if(dupNum > 0)
                Con_Message("  Pruned %d duplicate sidedefs.", dupNum);

            sideDefs.resize(newNum);
        }
    }

    void pruneUnusedSectors()
    {
        for(uint i = 0; i < sideDefs.count(); ++i)
        {
            SideDef *s = sideDefs[i];

            if(s->sector)
                s->sector->buildData.refCount++;
        }

        // Scan all sectors.
        uint newNum = 0;
        for(uint i = 0; i < sectors.count(); ++i)
        {
            Sector* s = sectors[i];

            if(s->buildData.refCount == 0)
            {
                M_Free(s);
                continue;
            }

            s->buildData.index = newNum + 1;
            sectors[newNum++] = s;
        }

        if(newNum < sectors.count())
        {
            Con_Message("  Pruned %d unused sectors.", sectors.count() - newNum);
            sectors.resize(newNum);
        }
    }

    /**
     * @param flags  @ref pruneMapElementFlags
     */
    void pruneMapElements(int /*flags*/)
    {
        /**
         * @todo Pruning cannot be done as game map data object properties
         * are currently indexed by their original indices as determined by the
         * position in the map data. The same problem occurs within ACS scripts
         * and XG line/sector references.
         *
         * @warning Order here is critical!
         */

        findEquivalentVertexes();

        if(flags & PRUNE_LINES)
            pruneLines();

        if(flags & PRUNE_VERTEXES)
            pruneVertices();

        if(flags & PRUNE_SIDEDEFS)
            pruneUnusedSidedefs();

        if(flags & PRUNE_SECTORS)
            pruneUnusedSectors();
    }
#endif
};

static EditableMap editMap; // singleton

static bool editMapInited;
static bool lastBuiltMapResult;

static GameMap *lastBuiltMap;

int bspFactor = 7;

void MPE_Register()
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

/**
 * Material name references specified during map conversion are recorded in
 * this dictionary. A dictionary is used to avoid repeatedly resolving the same
 * URIs and to facilitate a log of missing materials encountered during the
 * process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool *materialDict;

/**
 * Either print or count-the-number-of unresolved references in the
 * material dictionary.
 *
 * @param internId    Unique id associated with the reference.
 * @param parameters  If a uint pointer operate in "count" mode (total written here).
 *                    Else operate in "print" mode.
 * @return Always @c 0 (for use as an iterator).
 */
static int printMissingMaterialWorker(StringPool::Id internId, void *parameters)
{
    uint *count = (uint *)parameters;

    // A valid id?
    if(materialDict->string(internId))
    {
        // Have we resolved this reference yet?
        if(!materialDict->userPointer(internId))
        {
            // An unresolved reference.
            if(count)
            {
                // Count mode.
                *count += 1;
            }
            else
            {
                // Print mode.
                int const refCount = materialDict->userValue(internId);
                String const &materialUri = materialDict->string(internId);
                QByteArray materialUriUtf8 = materialUri.toUtf8();
                Con_Message(" %4u x \"%s\"", refCount, materialUriUtf8.constData());
            }
        }
    }
    return 0; // Continue iteration.
}

/**
 * Destroy the missing material dictionary.
 */
static void clearMaterialDict()
{
    // Initialized?
    if(!materialDict) return;

    materialDict->clear();
    delete materialDict; materialDict = 0;
}

/**
 * Print any "missing" materials in the dictionary to the log.
 */
static void printMissingMaterialsInDict()
{
    // Initialized?
    if(!materialDict) return;

    // Count missing materials.
    uint numMissing = 0;
    materialDict->iterate(printMissingMaterialWorker, &numMissing);
    if(!numMissing) return;

    Con_Message("  [110] Warning: Found %u unknown %s:", numMissing, numMissing == 1? "material":"materials");
    // List the missing materials.
    materialDict->iterate(printMissingMaterialWorker, 0);
}

/**
 * Attempt to locate a material by its URI. A dictionary of previously
 * searched-for URIs is maintained to avoid repeated searching and to record
 * "missing" materials.
 *
 * @param materialUriStr  URI of the material to search for.
 *
 * @return  Pointer to the found material; otherwise @c 0.
 */
static Material *findMaterialInDict(ddstring_t const *materialUriStr)
{
    if(!materialUriStr || Str_IsEmpty(materialUriStr)) return 0;

    // Are we yet to instantiate the dictionary?
    if(!materialDict)
    {
        materialDict = new StringPool;
    }

    de::Uri materialUri(Str_Text(materialUriStr), RC_NULL);

    // Intern this reference.
    StringPool::Id internId = materialDict->intern(materialUri.compose());
    Material *material = 0;

    // Have we previously encountered this?.
    uint refCount = materialDict->userValue(internId);
    if(refCount)
    {
        // Yes, if resolved the user pointer holds the found material.
        material = (Material *) materialDict->userPointer(internId);
    }
    else
    {
        // No, attempt to resolve this URI and update the dictionary.
        // First try the preferred scheme, then any.
        try
        {
            material = &App_Materials().find(materialUri).material();
        }
        catch(Materials::NotFoundError const &)
        {
            // Try any scheme.
            try
            {
                materialUri.setScheme("");
                material = &App_Materials().find(materialUri).material();
            }
            catch(Materials::NotFoundError const &)
            {}
        }

        // Insert the possibly resolved material into the dictionary.
        materialDict->setUserPointer(internId, material);
    }

    // There is now one more reference.
    refCount++;
    materialDict->setUserValue(internId, refCount);

    return material;
}

/// Used when sorting vertex line owners.
static Vertex *rootVtx;

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int lineAngleSorter(void const *a, void const *b)
{
    binangle_t angles[2];

    LineOwner *own[2] = { (LineOwner *)a, (LineOwner *)b };
    for(uint i = 0; i < 2; ++i)
    {
        if(own[i]->_link[LineOwner::Previous]) // We have a cached result.
        {
            angles[i] = own[i]->angle();
        }
        else
        {
            LineDef *line = &own[i]->line();
            Vertex const &otherVtx = line->vertex(&line->v1() == rootVtx? 1:0);

            fixed_t dx = otherVtx.origin()[VX] - rootVtx->origin()[VX];
            fixed_t dy = otherVtx.origin()[VY] - rootVtx->origin()[VY];

            own[i]->_angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->_link[LineOwner::Previous] = (LineOwner *) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return  The newly merged list.
 */
static LineOwner *mergeLineOwners(LineOwner *left, LineOwner *right,
    int (*compare) (void const *a, void const *b))
{
    LineOwner tmp;
    LineOwner *np = &tmp;

    tmp._link[LineOwner::Next] = np;
    while(left && right)
    {
        if(compare(left, right) <= 0)
        {
            np->_link[LineOwner::Next] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->_link[LineOwner::Next] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
    {
        np->_link[LineOwner::Next] = left;
    }
    if(right)
    {
        np->_link[LineOwner::Next] = right;
    }

    // Is the list empty?
    if(!tmp.hasNext())
        return NULL;

    return &tmp.next();
}

static LineOwner *splitLineOwners(LineOwner *list)
{
    if(!list) return NULL;

    LineOwner *lista = list;
    LineOwner *listb = list;
    LineOwner *listc = list;

    do
    {
        listc = listb;
        listb = &listb->next();
        lista = &lista->next();
        if(lista)
        {
            lista = &lista->next();
        }
    } while(lista);

    listc->_link[LineOwner::Next] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static LineOwner *sortLineOwners(LineOwner *list,
    int (*compare) (void const *a, void const *b))
{
    if(list && list->_link[LineOwner::Next])
    {
        LineOwner *p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex *vtx, LineDef *lineptr, LineOwner **storage)
{
    if(!lineptr) return;

    // Has this line already been registered with this vertex?
    LineOwner const *own = vtx->firstLineOwner();
    while(own)
    {
        if(&own->line() == lineptr)
            return; // Yes, we can exit.

        own = &own->next();
    }

    // Add a new owner.
    vtx->_numLineOwners++;
    LineOwner *newOwner = (*storage)++;

    newOwner->_line = lineptr;
    newOwner->_link[LineOwner::Previous] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->_link[LineOwner::Next] = vtx->_lineOwners;
    vtx->_lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == &lineptr->v1())
        lineptr->_vo1 = newOwner;
    else
        lineptr->_vo2 = newOwner;
}

#ifdef DENG2_DEBUG
/**
 * Determines whether the specified vertex @a v has a correctly formed line
 * owner ring.
 */
static bool vertexHasValidLineOwnerRing(Vertex &v)
{
    LineOwner const *base = v.firstLineOwner();
    LineOwner const *cur = base;
    do
    {
        if(&cur->prev().next() != cur) return false;
        if(&cur->next().prev() != cur) return false;

    } while((cur = &cur->next()) != base);
    return true;
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexLineOwnerRings()
{
    LOG_AS("buildVertexLineOwnerRings");

    /*
     * Step 1: Find and link up all line owners.
     */
    // We know how many vertex line owners we need (numLineDefs * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editMap.lines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    foreach(LineDef *line, editMap.lines)
    for(uint p = 0; p < 2; ++p)
    {
        setVertexLineOwner(&line->vertex(p), line, &allocator);
    }

    /*
     * Step 2: Sort line owners of each vertex and finalize the rings.
     */
    foreach(Vertex *v, editMap.vertexes)
    {
        if(!v->_numLineOwners) continue;

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->_lineOwners->angle();
        LineOwner *last = v->_lineOwners;
        LineOwner *p = &last->next();
        while(p)
        {
            p->_link[LineOwner::Previous] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = &p->next();
        }
        last->_link[LineOwner::Next] = v->_lineOwners;
        v->_lineOwners->_link[LineOwner::Previous] = last;

        // Set the angle of the last owner.
        last->_angle = last->angle() - firstAngle;

/*#ifdef DENG2_DEBUG
        LOG_VERBOSE("Vertex #%i: line owners #%i")
            << editMap.vertexes.indexOf(v) << v->lineOwnerCount();

        LineOwner const *base = v->firstLineOwner();
        LineOwner const *cur = base;
        uint idx = 0;
        do
        {
            LOG_VERBOSE("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f")
                << idx << cur->prev().line().origIndex() << cur->line().origIndex()
                << cur->next().line().origIndex() << BANG2DEG(cur->angle());

            idx++;
        } while((cur = &cur->next()) != base);
#endif*/

        // Sanity check.
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

#if 0 /* Currently unused. */
/**
 * @return  The "lowest" vertex (normally the left-most, but if the line is vertical,
 *          then the bottom-most). @c => 0 for start, 1 for end.
 */
static inline int lineVertexLowest(LineDef const *l)
{
    if(int( l->v[0]->buildData.pos[VX] ) < int( l->v[1]->buildData.pos[VX] ))
        return 0;

    return (int( l->v[0]->buildData.pos[VX] ) == int( l->v[1]->buildData.pos[VX] ) &&
            int( l->v[0]->buildData.pos[VY] )  < int( l->v[1]->buildData.pos[VY] )? 0 : 1);
}

static int lineStartCompare(void const *p1, void const *p2)
{
    LineDef const *a = (LineDef const *) p1;
    LineDef const *b = (LineDef const *) p2;

    // Determine left-most vertex of each line.
    Vertex *c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    Vertex *d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if(int( c->buildData.pos[VX] ) != int( d->buildData.pos[VX] ))
        return int( c->buildData.pos[VX] ) - int( d->buildData.pos[VX] );

    return int( c->buildData.pos[VY] ) - int( d->buildData.pos[VY] );
}

static int lineEndCompare(void const *p1, void const *p2)
{
    LineDef const *a = (LineDef const *) p1;
    LineDef const *b = (LineDef const *) p2;

    // Determine right-most vertex of each line.
    Vertex *c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    Vertex *d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if(int( c->buildData.pos[VX] ) != int( d->buildData.pos[VX] ))
        return int( c->buildData.pos[VX] ) - int( d->buildData.pos[VX] );

    return int( c->buildData.pos[VY] ) - int( d->buildData.pos[VY] );
}

size_t numOverlaps;

int testOverlaps(LineDef *b, void *data)
{
    LineDef *a = (LineDef *) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {
                // Found an overlap!
                b->buildData.overlap = (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return false; // Continue iteration.
}

struct findoverlaps_params_t
{
    GameMap *map;
    uint coords[2]; // Blockmap cell coordinates.
};

static int findOverlapsForLine(LineDef *l, void *parameters)
{
    findoverlaps_params_t *p = (findoverlaps_params_t *) parameters;
    GameMap_IterateCellLineDefs(p->map, p->coords, testOverlaps, l);
    return false; // Continue iteration.
}

/**
 * @note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(GameMap *map)
{
    numOverlaps = 0;

    uint bmapDimensions[2];
    Blockmap_Size(map->lineDefBlockmap, bmapDimensions);

    findoverlaps_params_t parms;
    for(uint y = 0; y < bmapSize[VY]; ++y)
    for(uint x = 0; x < bmapSize[VX]; ++x)
    {
        parms.map = map;
        parms.coords[VX] = x;
        parms.coords[VY] = y;

        GameMap_IterateCellLineDefs(map, params.coords, findOverlapsForLine, &parms);
    }

    if(numOverlaps == 0) return;

    VERBOSE( Con_Message("Detected %lu overlapping linedefs.", (unsigned long) numOverlaps) )
}
#endif

GameMap *MPE_GetLastBuiltMap()
{
    return lastBuiltMap;
}

bool MPE_GetLastBuiltMapResult()
{
    return lastBuiltMapResult;
}

#undef MPE_Begin
boolean MPE_Begin(char const *mapUri)
{
    /// @todo Do not ignore; assign to the editable map.
    DENG_UNUSED(mapUri);

    // Already been here?
    if(editMapInited) return true;

    // Initialize the game-specific map entity property database.
    editMap.entityDatabase = EntityDatabase_New();
    editMap.clear();

    lastBuiltMap = 0;
    lastBuiltMapResult = false; // Failed (default).

    editMapInited = true;
    return true;
}

#undef MPE_End
boolean MPE_End()
{
    if(!editMapInited)
        return false;

    /*
     * Log warnings about any issues we encountered during conversion of
     * the basic map data elements.
     */
    printMissingMaterialsInDict();
    clearMaterialDict();

    GameMap *map = new GameMap;

    /*
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
#if 0
    markDuplicateVertexes(editMap);
    pruneMapElements(editMap, PRUNE_ALL);
#endif

    buildVertexLineOwnerRings();

    /*
     * Acquire ownership of the map elements from the editable map.
     */
    map->entityDatabase = editMap.entityDatabase; // Take ownership.
    editMap.entityDatabase = 0;

    DENG2_ASSERT(map->_vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_vertexes.reserve(editMap.vertexes.count());
#endif
    while(!editMap.vertexes.isEmpty())
    {
        map->_vertexes.append(editMap.vertexes.takeFirst());
    }

    // Collate sectors:
    DENG2_ASSERT(map->_sectors.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_sectors.reserve(editMap.sectors.count());
#endif
    while(!editMap.sectors.isEmpty())
    {
        map->_sectors.append(editMap.sectors.takeFirst());
    }

    // Collate sidedefs:
    DENG2_ASSERT(map->_sideDefs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_sideDefs.reserve(editMap.sideDefs.count());
#endif
    while(!editMap.sideDefs.isEmpty())
    {
        map->_sideDefs.append(editMap.sideDefs.takeFirst());
    }

    // Collate lines:
    DENG2_ASSERT(map->_lines.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_lines.reserve(editMap.lines.count());
#endif
    while(!editMap.lines.isEmpty())
    {
        map->_lines.append(editMap.lines.takeFirst());
        LineDef *line = map->_lines.back();

        /// @todo This init should already have been done elsewhere. -ds
        line->updateSlopeType();
        line->updateAABox();

        line->_length = V2d_Length(line->_direction);
        line->_angle  = bamsAtan2(int( line->_direction[VY] ),
                                  int( line->_direction[VX] ));
    }

    // Collate polyobjs:
    DENG2_ASSERT(map->_polyobjs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_polyobjs.reserve(editMap.polyobjs.count());
#endif
    while(!editMap.polyobjs.isEmpty())
    {
        map->_polyobjs.append(editMap.polyobjs.takeFirst());
        Polyobj *polyobj = map->_polyobjs.back();

        polyobj->idx = map->_polyobjs.count(); // 0-based index.

        // Create a hedge for each line of this polyobj.
        foreach(LineDef *line, polyobj->lines())
        {
            HEdge *hedge = new HEdge; // Polyobj has ownership.

            hedge->_v[0]    = &line->v1();
            hedge->_v[1]    = &line->v2();
            hedge->_line    = line;
            hedge->_length  = line->length();
            hedge->_sector  = line->frontSectorPtr();
            hedge->_twin    = 0;
            hedge->_bspLeaf = 0;

            line->front()._leftHEdge =
                line->front()._rightHEdge = hedge;
        }

        polyobj->buildUniqueVertexes();
        uint n = 0;
        foreach(Vertex *vertex, polyobj->uniqueVertexes())
        {
            // The originalPts are relative to the polyobj origin.
            vec2d_t const &vertexOrigin = vertex->origin();
            polyobj->originalPts[n].origin[VX] = vertexOrigin[VX] - polyobj->origin[VX];
            polyobj->originalPts[n].origin[VY] = vertexOrigin[VY] - polyobj->origin[VY];
            n++;
        }
    }

    map->updateBounds();
    AABoxd const &mapBounds = map->bounds();
    LOG_INFO("Map bounds: min%s max%s")
        << Vector2d(mapBounds.min).asText()
        << Vector2d(mapBounds.max).asText();

    /*
     * Build blockmaps.
     */
    vec2d_t min, max;
    map->bounds(min, max);

    map->initLineBlockmap(min, max);
    foreach(LineDef *line, map->lines())
    {
        map->linkLine(*line);
    }

    // The mobj and polyobj blockmaps are maintained dynamically.
    map->initMobjBlockmap(min, max);
    map->initPolyobjBlockmap(min, max);

    /*
     * Build a BSP.
     */
    bool builtOK = map->buildBsp();

    // Destroy the rest of editable map, we are finished with it.
    editMap.clear();

    if(!builtOK)
    {
        // Darn, clean up...
        delete map;

        lastBuiltMapResult = false; // Failed :$
        return lastBuiltMapResult;
    }

    editMapInited = false;

    map->finishMapElements();

    // We can now initialize the BSP leaf blockmap.
    map->initBspLeafBlockmap(min, max);
    foreach(BspLeaf *bspLeaf, map->bspLeafs())
    {
        map->linkBspLeaf(*bspLeaf);
    }

    S_DetermineBspLeafsAffectingSectorReverb(map);

    lastBuiltMap = map;
    lastBuiltMapResult = true; // Success.

    return lastBuiltMapResult;
}

#undef MPE_VertexCreate
uint MPE_VertexCreate(coord_t x, coord_t y)
{
    if(!editMapInited) return 0;

    Vertex *v = editMap.createVertex(x, y);

    return v->_buildData.index;
}

#undef MPE_VertexCreatev
boolean MPE_VertexCreatev(size_t num, coord_t *values, uint *indices)
{
    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(uint n = 0; n < num; ++n)
    {
        Vertex *v = editMap.createVertex(values[n * 2], values[n * 2 + 1]);

        if(indices)
            indices[n] = v->_buildData.index;
    }

    return true;
}

#undef MPE_SidedefCreate
uint MPE_SidedefCreate(short flags, ddstring_t const *topMaterialUri,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    ddstring_t const *middleMaterialUri, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, ddstring_t const *bottomMaterialUri,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue)
{
    if(!editMapInited) return 0;

    SideDef *s = editMap.createSideDef();
    s->_flags = flags;

    // Assign the resolved material if found.
    s->top().setMaterial(findMaterialInDict(topMaterialUri));
    s->top().setMaterialOrigin(topOffsetX, topOffsetY);
    s->top().setTintColor(topRed, topGreen, topBlue);

    s->middle().setMaterial(findMaterialInDict(middleMaterialUri));
    s->middle().setMaterialOrigin(middleOffsetX, middleOffsetY);
    s->middle().setTintColor(middleRed, middleGreen, middleBlue);
    s->middle().setOpacity(middleAlpha);

    s->bottom().setMaterial(findMaterialInDict(bottomMaterialUri));
    s->bottom().setMaterialOrigin(bottomOffsetX, bottomOffsetY);
    s->bottom().setTintColor(bottomRed, bottomGreen, bottomBlue);

    return s->_buildData.index;
}

#undef MPE_LinedefCreate
uint MPE_LinedefCreate(uint v1, uint v2, uint frontSector, uint backSector,
    uint frontSide, uint backSide, int flags)
{
    if(!editMapInited) return 0;

    if(frontSector > (uint)editMap.sectors.count())    return 0;
    if(backSector  > (uint)editMap.sectors.count())    return 0;
    if(frontSide   > (uint)editMap.sideDefs.count())   return 0;
    if(backSide    > (uint)editMap.sideDefs.count())   return 0;
    if(v1 == 0 || v1 > (uint)editMap.vertexes.count()) return 0;
    if(v2 == 0 || v2 > (uint)editMap.vertexes.count()) return 0;
    if(v1 == v2) return 0;

    // Ensure that the side indices are unique.
    if(frontSide && editMap.sideDefs[frontSide - 1]->_buildData.refCount)
        return 0;
    if(backSide && editMap.sideDefs[backSide - 1]->_buildData.refCount)
        return 0;

    // Next, check the length is not zero.
    Vertex *vtx1 = editMap.vertexes[v1 - 1];
    Vertex *vtx2 = editMap.vertexes[v2 - 1];
    coord_t length = V2d_Distance(vtx2->origin(), vtx1->origin());
    if(!(length > 0)) return 0;

    SideDef *front = frontSide? editMap.sideDefs[frontSide - 1] : 0;
    SideDef *back  = backSide?  editMap.sideDefs[backSide  - 1] : 0;

    LineDef *l = editMap.createLine();
    l->_v1 = vtx1;
    l->_v2 = vtx2;

    l->_v1->_buildData.refCount++;
    l->_v2->_buildData.refCount++;

    l->front()._sector = (frontSector == 0? NULL: editMap.sectors[frontSector-1]);
    l->back()._sector  = (backSector  == 0? NULL: editMap.sectors[backSector-1]);

    l->front()._sideDef = front;
    l->back()._sideDef = back;

    l->_length = length;

    l->updateSlopeType();
    l->updateAABox();

    l->_angle = bamsAtan2(int( l->_direction[VY] ),
                          int( l->_direction[VX] ));

    // Remember the number of unique references.
    if(l->hasFrontSideDef())
    {
        l->frontSideDef()._line = l;
        l->frontSideDef()._buildData.refCount++;
    }

    if(l->hasBackSideDef())
    {
        l->backSideDef()._line = l;
        l->backSideDef()._buildData.refCount++;
    }

    l->_inFlags = 0;

    // Determine the default linedef flags.
    l->_flags = flags;
    if(!front || !back)
        l->_flags |= DDLF_BLOCKING;

    return l->origIndex();
}

#undef MPE_PlaneCreate
uint MPE_PlaneCreate(uint sector, coord_t height, ddstring_t const *materialUri,
    float matOffsetX, float matOffsetY, float r, float g, float b, float a,
    float normalX, float normalY, float normalZ)
{
    if(!editMapInited) return 0;
    if(sector == 0 || sector > (uint)editMap.sectors.count()) return 0;

    Sector *s = editMap.sectors[sector - 1];

    vec3f_t normal; V3f_Set(normal, normalX, normalY, normalZ);

    Plane *pln = new Plane(*s, normal, height);

    pln->surface().setMaterial(findMaterialInDict(materialUri));
    pln->surface().setTintColor(r, g, b);
    pln->surface().setOpacity(a);
    pln->surface().setMaterialOrigin(matOffsetX, matOffsetY);

    s->_planes.append(pln);
    pln->_inSectorIndex = s->planeCount() - 1;

    return pln->inSectorIndex() + 1; // 1-based index.
}

#undef MPE_SectorCreate
uint MPE_SectorCreate(float lightlevel, float red, float green, float blue)
{
    if(!editMapInited) return 0;

    Sector *s = editMap.createSector(lightlevel, Vector3f(red, green, blue));
    return s->origIndex();
}

#undef MPE_PolyobjCreate
uint MPE_PolyobjCreate(uint *lines, uint lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY)
{
    if(!editMapInited || !lineCount || !lines) return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(uint i = 0; i < lineCount; ++i)
    {
        if(lines[i] == 0 || lines[i] > (uint)editMap.lines.count()) return 0;

        LineDef *line = editMap.lines[lines[i] - 1];
        if(line->isFromPolyobj()) return 0;
    }

    Polyobj *po = editMap.createPolyobj();
    po->tag = tag;
    po->seqType = sequenceType;
    po->origin[VX] = originX;
    po->origin[VY] = originY;

    for(uint i = 0; i < lineCount; ++i)
    {
        LineDef *line = editMap.lines[lines[i] - 1];

        // This line belongs to a polyobj.
        line->_inFlags |= LF_POLYOBJ;
        static_cast<Polyobj::Lines *>(po->_lines)->append(line);
    }

    return po->buildData.index;
}

#undef MPE_GameObjProperty
boolean MPE_GameObjProperty(char const *entityName, uint elementIndex,
                            char const *propertyName, valuetype_t type, void *valueAdr)
{
    if(!editMapInited) return false;

    if(!entityName || !propertyName || !valueAdr)
        return false; // Hmm...

    // Is this a known entity?
    MapEntityDef *entityDef = P_MapEntityDefByName(entityName);
    if(!entityDef)
    {
        Con_Message("Warning: MPE_GameObjProperty: Unknown entity name:\"%s\", ignoring.",
                    entityName);
        return false;
    }

    // Is this a known property?
    MapEntityPropertyDef *propertyDef;
    if(MapEntityDef_PropertyByName(entityDef, propertyName, &propertyDef) < 0)
    {
        Con_Message("Warning: MPE_GameObjProperty: Entity \"%s\" has no \"%s\" property, ignoring.",
                    entityName, propertyName);
        return false;
    }

    return P_SetMapEntityProperty(editMap.entityDatabase, propertyDef, elementIndex, type, valueAdr);
}

// p_data.cpp
#undef P_RegisterMapObj
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, char const *name);

#undef P_RegisterMapObjProperty
DENG_EXTERN_C boolean P_RegisterMapObjProperty(int entityId, int propertyId, char const *propertyName, valuetype_t type);

DENG_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT },

    P_RegisterMapObj,
    P_RegisterMapObjProperty,
    MPE_Begin,
    MPE_End,
    MPE_VertexCreate,
    MPE_VertexCreatev,
    MPE_SidedefCreate,
    MPE_LinedefCreate,
    MPE_SectorCreate,
    MPE_PlaneCreate,
    MPE_PolyobjCreate,
    MPE_GameObjProperty
};

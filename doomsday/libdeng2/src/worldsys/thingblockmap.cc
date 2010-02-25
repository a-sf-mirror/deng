/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#include <math.h>

#include "de/ThingBlockmap"

using namespace de;

namespace
{
/// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

typedef struct {
    bool          (*callback) (Thing*, void *);
    dint            localValidCount;
    void*           paramaters;
} iteratethings_params_t;

static bool iterateThings(ThingBlockmap::Things* things, void* paramaters)
{
    if(things)
    {
        const iteratethings_params_t* args = reinterpret_cast<iteratethings_params_t*>(paramaters);

        FOR_EACH(i, things, Things::iterator)
        {
            Thing* thing = *i;

            if(thing->validCount != args->localValidCount)
            {
                thing->validCount = args->localValidCount;

                if(!args->callback(thing, args->paramaters))
                    return false;
            }
        }
    }

    return true;
}

static bool clearThings(ThingBlockmap::Things* things, void* paramaters)
{
    if(things)
    {
        things->clear();
        delete things;
    }

    return true; // Continue iteration.
}

static void boxToBlocks(ThingBlockmap* blockmap, duint blockBox[4],
    const arvec2_t box)
{
    Vector2ui dimensions[2];
    blockmap->_gridmap.dimensions(dimensions);

    Vector2f min;
    V2_Set(min, max(blockmap->aabb[0][0], box[0][0]),
                max(blockmap->aabb[0][1], box[0][1]));

    Vector2f max;
    V2_Set(max, min(blockmap->aabb[1][0], box[1][0]),
                min(blockmap->aabb[1][1], box[1][1]));

    blockBox[BOXLEFT]   = clamp(0, (min[0] - blockmap->aabb[0][0]) / blockmap->_blockSize.x, dimensions[0]);
    blockBox[BOXBOTTOM] = clamp(0, (min[1] - blockmap->aabb[0][1]) / blockmap->_blockSize.y, dimensions[1]);

    blockBox[BOXRIGHT]  = clamp(0, (max[0] - blockmap->aabb[0][0]) / blockmap->_blockSize.x, dimensions[0]);
    blockBox[BOXTOP]    = clamp(0, (max[1] - blockmap->aabb[0][1]) / blockmap->_blockSize.y, dimensions[1]);
}
}

ThingBlockmap::ThingBlockmap(const Vector2f& min, const Vector2f& max,
    duint width, duint height)
 : _aaBounds(MapRectanglef(min, max), _gridmap(Gridmap(width, height))
{
    _blockSize = Vector2f(_aaBounds.width() / width, _aaBounds.height() / height);
}

ThingBlockmap::ThingBlockmap()
{
    _gridmap.iterate(clearThings, NULL);
}

void ThingBlockmap::linkInBlock(duint x, duint y, Thing* thing)
{
    Things* things = _gridmap.block(x, y);
    if(!things)
        things = _gridmap.setBlock(x, y, new Things());
    things->push_front(thing);
}

bool ThingBlockmap::unlinkInBlock(duint x, duint y, Thing* thing)
{
    Things* things = _gridmap.block(x, y);
    if(things)
    {
        Things::size_type sizeBefore = things->size();
        things->remove(thing);
        if(things->size() == 0)
        {
            delete things;
            _gridmap.setBlock(x, y, NULL);
        }
        return sizeBefore != things->size();
    }
    return false;
}

void ThingBlockmap::boxToBlocks(duint blockBox[4], const arvec2_t box)
{
    assert(blockBox);
    assert(box);
    boxToBlocks(this, blockBox, box);
}

bool ThingBlockmap::block(Vector2ui& dest, const Vector2f& pos)
{
    if(_aaBounds.contains(pos))
    {
        Vector2f blockPos = pos - _aaBounds.bottomLeft();
        dest = Vector2ui(blockPos.x / _blockSize.x, blockPos.y / _blockSize.y);
        return true;
    }
    return false;
}

void ThingBlockmap::link(Thing* thing)
{
    assert(thing);

    Vector2ui block;
    block(block, thing->origin);
    linkToBlock(block.x, block.y, thing);
}

bool ThingBlockmap::unlink(Thing* thing)
{
    assert(thing);

    const Vector2ui& dimensions = dimensions();

    bool result = false;
    for(duint y = 0; y < dimensions.y; ++y)
        for(duint x = 0; x < dimensions.x; ++x)
        {
            if(unlinkFromBlock(x, y, thing))
                if(!result)
                    result = true;
        }
    return result;
}

const MapRectanglef ThingBlockmap::aaBounds()
{
    return _aaBounds;
}

const Vector2f& ThingBlockmap::blockSize()
{
    return _blockSize;
}

const Vector2ui& ThingBlockmap::dimensions()
{
    return _gridmap.dimensions();
}

dsize ThingBlockmap::numInBlock(duint x, duint y)
{
    Things* things = _gridmap.block(x, y);
    if(things)
        return things->size();
    return 0;
}

bool ThingBlockmap::iterate(const Vector2ui& block,
    bool (*callback) (Thing*, void*), void* paramaters)
{
    assert(callback);

    iteratethings_params_t args;
    args.callback = callback;
    args.paramaters = paramaters;
    args.localValidCount = validCount;

    return iterateThings(_gridmap.block(block), reinterpret_cast<void*>(&args));
}

bool ThingBlockmap::iterate(const duint blockBox[4],
    bool (*callback) (Thing*, void*), void* paramaters)
{
    assert(blockBox);
    assert(callback);

    iteratethings_params_t args;
    args.callback = callback;
    args.paramaters = paramaters;
    args.localValidCount = validCount;

    return _gridmap.iterate(blockBox, iterateThings, reinterpret_cast<void*>(&args));
}

static bool addThingIntercepts(Thing* thing, void* paramaters)
{
    if(thing->object().user() && (thing->object().user().flags & DDPF_CAMERA))
        return true; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crossubsection for hit.
    Vector2f from = thing->origin;
    Vector2f to = thing->origin;
    if((traceLOS.dX ^ traceLOS.dY) > 0)
    {
        from.x += -thing->radius;
        from.y += thing->radius;

        to.x += thing->radius;
        to.y += -thing->radius;
    }
    else
    {
        from.x += -thing->radius;
        from.y += -thing->radius;

        to.x += thing->radius;
        to.y += thing->radius;
    }

    dint s[2];
    s[0] = P_PointOnDivlineSide(from.x, from.y, &traceLOS);
    s[1] = P_PointOnDivlineSide(to.x, to.y, &traceLOS);
    if(s[0] == s[1])
        return true; // Line isn't crossed.

    Line2i dl = Line2i(flt2fix(from.x), flt2fix(from.y), flt2fix(to.x - from.x), flt2fix(to.y - from.y);
    dfloat frac = P_InterceptVector(&traceLOS, dl);
    if(frac < 0)
        return true; // Behind source.

    P_AddIntercept(frac, false, thing);

    return true; // Keep going.
}

bool ThingBlockmap::pathTraverse(const duint originBlock[2],
    const duint destBlock[2], const dfloat origin[2], const dfloat dest[2],
    bool (*callback) (intercept_t*))
{
    Vector2ui block;
    Vector2f delta;
    Vector2i intercept, step;
    Vector2i stepDir;
    dfloat partial;
    duint count;

    if(destBlock[0] > originBlock[0])
    {
        stepDir[0] = 1;
        partial = fix2flt(FRACUNIT - ((flt2fix(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else if(destBlock[0] < originBlock[0])
    {
        stepDir[0] = -1;
        partial = fix2flt((flt2fix(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else
    {
        stepDir[0] = 0;
        partial = 1;
        delta[1] = 256;
    }
    intercept[1] = (flt2fix(origin[1]) >> MAPBTOFRAC) +
        flt2fix(partial * delta[1]);

    if(destBlock[1] > originBlock[1])
    {
        stepDir[1] = 1;
        partial = fix2flt(FRACUNIT - ((flt2fix(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else if(destBlock[1] < originBlock[1])
    {
        stepDir[1] = -1;
        partial = fix2flt((flt2fix(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else
    {
        stepDir[1] = 0;
        partial = 1;
        delta[0] = 256;
    }
    intercept[0] = (flt2fix(origin[0]) >> MAPBTOFRAC) + flt2fix(partial * delta[0]);

    /**
     * Step through map blocks.
     *
     * Count is present because DOOM fails to check the case where the ray
     * passes diagonally through a vertex into another block.
     *
     * @todo Fix the problem. This will require a game-side copy of the blockmap
     * where the original behaviour is emulated.
     */
    block[0] = originBlock[0];
    block[1] = originBlock[1];
    step[0] = flt2fix(delta[0]);
    step[1] = flt2fix(delta[1]);
    for(count = 0; count < 64; ++count)
    {
        if(!iterate(block, addMobjIntercepts, 0))
            return false; // Early out.

        if(block[0] == destBlock[0] && block[1] == destBlock[1])
            break;

        if((unsigned) (intercept[1] >> FRACBITS) == block[1])
        {
            intercept[1] += step[1];
            block[0] += stepDir[0];
        }
        else if((unsigned) (intercept[0] >> FRACBITS) == block[0])
        {
            intercept[0] += step[0];
            block[1] += stepDir[1];
        }
    }

    return true;
}

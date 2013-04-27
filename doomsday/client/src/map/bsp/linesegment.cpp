/** @file map/bsp/linesegment.cpp BSP Builder Line Segment.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
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

#include <de/mathutil.h>
#include <de/vector1.h> /// @todo remove me

#include <de/Observers>

#include "HEdge"

#include "map/bsp/linesegment.h"

namespace de {
namespace bsp {

DENG2_PIMPL(LineSegment),
DENG2_OBSERVES(Vertex, OriginChange)
{
    /// Vertexes of the segment (not owned).
    Vertex *from, *to;

    /// Direction vector from -> to.
    Vector2d direction;

    /// Linked @em twin segment (that on the other side of "this" line segment).
    LineSegment *twin;

    /// Map Line side that "this" segment initially comes or @c 0 signifying a
    /// partition line segment (not owned).
    Line::Side *mapSide;

    /// Map Line side that "this" segment initially comes from. For map lines,
    /// this is just the same as @var mapSide. For partition lines this is the
    /// the partition line's map Line side. (Not owned.)
    Line::Side *sourceMapSide;

    /// Half-edge produced from this segment (if any, not owned).
    HEdge *hedge;

    Instance(Public *i, Vertex &from_, Vertex &to_, Line::Side *mapSide,
             Line::Side *sourceMapSide)
        : Base(i),
          from(&from_),
          to(&to_),
          twin(0),
          mapSide(mapSide),
          sourceMapSide(sourceMapSide),
          hedge(0)
    {
        from->audienceForOriginChange += this;
        to->audienceForOriginChange   += this;
    }

    ~Instance()
    {
        from->audienceForOriginChange -= this;
        to->audienceForOriginChange   -= this;
    }

    inline Vertex **vertexAdr(int edge) {
        return edge? &to : &from;
    }

    void replaceVertex(int edge, Vertex &newVertex)
    {
        Vertex **adr = vertexAdr(edge);

        if(*adr && *adr == &newVertex) return;

        if(*adr) (*adr)->audienceForOriginChange -= this;
        *adr = &newVertex;
        if(*adr) (*adr)->audienceForOriginChange += this;

        updateCache();
    }

    /**
     * To be called to update precalculated vectors, distances, etc...
     * following a dependent vertex origin change notification.
     *
     * @todo Optimize: defer until next accessed. -ds
     */
    void updateCache()
    {
        direction = to->origin() - from->origin();

        self.pLength    = direction.length();
        DENG2_ASSERT(self.pLength > 0);
        self.pAngle     = M_DirectionToAngleXY(direction.x, direction.y);
        self.pSlopeType = M_SlopeTypeXY(direction.x, direction.y);

        self.pPerp =  from->origin().y * direction.x - from->origin().x * direction.y;
        self.pPara = -from->origin().x * direction.x - from->origin().y * direction.y;
    }

    void vertexOriginChanged(Vertex &vertex, Vector2d const &oldOrigin, int changedAxes)
    {
        DENG2_UNUSED3(vertex, oldOrigin, changedAxes);
        updateCache();
    }
};

LineSegment::LineSegment(Vertex &from, Vertex &to, Line::Side *mapSide,
                         Line::Side *sourceMapSide)
    : pLength(0),
      pAngle(0),
      pPara(0),
      pPerp(0),
      pSlopeType(ST_VERTICAL),
      nextOnSide(0),
      prevOnSide(0),
      bmapBlock(0),
      sector(0),
      d(new Instance(this, from, to, mapSide, sourceMapSide))
{
    d->updateCache();
}

LineSegment::LineSegment(LineSegment const &other)
    : pLength(other.pLength),
      pAngle(other.pAngle),
      pPara(other.pPara),
      pPerp(other.pPerp),
      pSlopeType(other.pSlopeType),
      nextOnSide(other.nextOnSide),
      prevOnSide(other.prevOnSide),
      bmapBlock(other.bmapBlock),
      sector(other.sector),
      d(new Instance(this, *other.d->from, *other.d->to,
                            other.d->mapSide, other.d->sourceMapSide))
{
    d->direction = other.d->direction;
    d->twin      = other.d->twin;
    d->hedge     = other.d->hedge;
}

LineSegment &LineSegment::operator = (LineSegment const &other)
{
    pLength    = other.pLength;
    pAngle     = other.pAngle;
    pPara      = other.pPara;
    pPerp      = other.pPerp;
    pSlopeType = other.pSlopeType;
    nextOnSide = other.nextOnSide;
    prevOnSide = other.prevOnSide;
    bmapBlock  = other.bmapBlock;
    sector     = other.sector;

    d->direction     = other.d->direction;
    d->mapSide       = other.d->mapSide;
    d->sourceMapSide = other.d->sourceMapSide;
    d->twin          = other.d->twin;
    d->hedge         = other.d->hedge;

    replaceFrom(*other.d->from);
    replaceTo(*other.d->to);

    d->updateCache();

    return *this;
}

Vertex &LineSegment::vertex(int to) const
{
    DENG_ASSERT(*d->vertexAdr(to) != 0);
    return **d->vertexAdr(to);
}

void LineSegment::replaceVertex(int to, Vertex &newVertex)
{
    d->replaceVertex(to, newVertex);
}

Vector2d const &LineSegment::direction() const
{
    return d->direction;
}

bool LineSegment::hasTwin() const
{
    return d->twin != 0;
}

LineSegment &LineSegment::twin() const
{
    if(d->twin)
    {
        return *d->twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("LineSegment::twin", "No twin line segment is associated");
}

void LineSegment::setTwin(LineSegment *newTwin)
{
    d->twin = newTwin;
}

bool LineSegment::hasHEdge() const
{
    return d->hedge != 0;
}

HEdge &LineSegment::hedge() const
{
    if(d->hedge)
    {
        return *d->hedge;
    }
    /// @throw MissingHEdgeError Attempted with no half-edge associated.
    throw MissingHEdgeError("LineSegment::hedge", "No half-edge is associated");
}

void LineSegment::setHEdge(HEdge *newHEdge)
{
    d->hedge = newHEdge;
}

bool LineSegment::hasMapSide() const
{
    return d->mapSide != 0;
}

Line::Side &LineSegment::mapSide() const
{
    if(d->mapSide)
    {
        return *d->mapSide;
    }
    /// @throw MissingMapSideError Attempted with no map line side attributed.
    throw MissingMapSideError("LineSegment::mapSide", "No map line side is attributed");
}

bool LineSegment::hasSourceMapSide() const
{
    return d->sourceMapSide != 0;
}

Line::Side &LineSegment::sourceMapSide() const
{
    if(d->sourceMapSide)
    {
        return *d->sourceMapSide;
    }
    /// @throw MissingMapSideError Attempted with no source map line side attributed.
    throw MissingMapSideError("LineSegment::sourceMapSide", "No source map line side is attributed");
}

void LineSegment::ceaseVertexObservation()
{
    d->from->audienceForOriginChange -= d.get();
    d->to->audienceForOriginChange   -= d.get();
}

} // namespace bsp
} // namespace de

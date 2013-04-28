/** @file map/bsp/hplane.cpp BSP Builder Half-plane.
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

#include <memory>

#include <QtAlgorithms>

#include <de/Error>
#include <de/Log>

#include "Line"
#include "Vertex"

#include "map/bsp/hedgeintercept.h"
#include "map/bsp/linesegment.h"

#include "map/bsp/hplane.h"

namespace de {
namespace bsp {

DENG2_PIMPL(HPlane)
{
    /// The partition line.
    Partition partition;

    /// Map line segment which is the basis for the half-plane.
    std::auto_ptr<LineSegment> lineSegment;

    /// Intercept points along the half-plane.
    Intercepts intercepts;

    /// Set to @c true when @var intercepts requires sorting.
    bool needSortIntercepts;

    Instance(Public *i, Vector2d const &partitionOrigin,
             Vector2d const &partitionDirection)
        : Base(i),
          partition(partitionOrigin, partitionDirection),
          needSortIntercepts(false)
    {}

    /**
     * Search the list of intercepts for to see if there is one for the
     * specified @a vertex.
     *
     * @param vertex  The vertex to look for.
     *
     * @return  @c true iff an intercept for @a vertex was found.
     */
    bool haveInterceptForVertex(Vertex const &vertex) const
    {
        foreach(LineSegmentIntercept const &icpt, intercepts)
        {
            if(icpt.vertex == &vertex)
                return true;
        }
        return false;
    }
};

HPlane::HPlane(Vector2d const &partitionOrigin, Vector2d const &partitionDirection)
    : d(new Instance(this, partitionOrigin, partitionDirection))
{}

void HPlane::clearIntercepts()
{
    d->intercepts.clear();
    // An empty intercept list is logically sorted.
    d->needSortIntercepts = false;
}

void HPlane::configure(LineSegment const &newBaseSeg)
{
    // Only map line segments are suitable.
    DENG_ASSERT(newBaseSeg.hasMapSide());

    LOG_AS("HPlane::configure");

    // Clear the list of intersection points.
    clearIntercepts();

    Line::Side &side = newBaseSeg.mapSide();
    d->partition.origin    = side.from().origin();
    d->partition.direction = side.to().origin() - side.from().origin();

    // Update/store a copy of the line segment.
    d->lineSegment.reset(new LineSegment(newBaseSeg));
    d->lineSegment->ceaseVertexObservation(); /// @todo refactor away -ds

    //LOG_DEBUG("line segment %p %s.")
    //    << de::dintptr(&newLineSeg) << d->partition.asText();
}

HPlane::Intercept *HPlane::interceptLineSegment(LineSegment const &lineSeg, int edge)
{
    // Already present for this vertex?
    Vertex &vertex = lineSeg.vertex(edge);
    if(d->haveInterceptForVertex(vertex)) return 0;

    LineSegmentIntercept inter;
    inter.vertex  = &vertex;
    inter.selfRef = (lineSeg.hasMapSide() && lineSeg.line().isSelfReferencing());

    d->intercepts.append(Intercept(d->lineSegment->distance(vertex.origin()), inter));
    Intercept *newIntercept = &d->intercepts.last();

    // The addition of a new intercept means we'll need to resort.
    d->needSortIntercepts = true;

    return newIntercept;
}

void HPlane::sortAndMergeIntercepts()
{
    // Any work to do?
    if(!d->needSortIntercepts) return;

    qSort(d->intercepts.begin(), d->intercepts.end());

    for(int i = 0; i < d->intercepts.count() - 1; ++i)
    {
        Intercept &cur  = d->intercepts[i];
        Intercept &next = d->intercepts[i+1];

        // Sanity check.
        ddouble distance = next.distance() - cur.distance();
        if(distance < -0.1)
        {
            throw Error("HPlane::sortAndMergeIntercepts",
                        String("Invalid intercept order - %1 > %2")
                            .arg(cur.distance(), 0, 'f', 3)
                            .arg(next.distance(), 0, 'f', 3));
        }

        // Are we merging this pair?
        if(distance <= HPLANE_INTERCEPT_MERGE_DISTANCE_EPSILON)
        {
            // Yes - merge the two intercepts into one.
            cur.merge(next);

            // Destroy the "next" intercept.
            d->intercepts.removeAt(i+1);

            // Process the new "cur" and "next" pairing.
            i -= 1;
        }
    }

    d->needSortIntercepts = false;
}

#ifdef DENG_DEBUG
void HPlane::printIntercepts() const
{
    uint index = 0;
    foreach(Intercept const &icpt, d->intercepts)
    {
        LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt.distance();
        icpt.debugPrint();
    }
}
#endif

Partition const &HPlane::partition() const
{
    return d->partition;
}

LineSegment const &HPlane::lineSegment() const
{
    DENG_ASSERT(d->lineSegment.get() != 0);
    return *d->lineSegment.get();
}

HPlane::Intercepts const &HPlane::intercepts() const
{
    return d->intercepts;
}

} // namespace bsp
} // namespace de

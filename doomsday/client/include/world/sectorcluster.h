/** @file sectorcluster.h  World map sector cluster.
 *
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_SECTORCLUSTER_H
#define DENG_WORLD_SECTORCLUSTER_H

#include "dd_share.h" // AudioEnvironmentFactors

#include "HEdge"

#include "MapElement"
#include "Line"
#include "Plane"
#include "Sector"

#ifdef __CLIENT__
#  include "render/lightgrid.h"
#endif

#include <de/Observers>
#include <de/Vector>
#include <de/aabox.h>
#include <QList>

class BspLeaf;
#ifdef __CLIENT__
class BiasDigest;
#endif

/**
 * Adjacent BSP leafs in the sector (i.e., those which share one or more
 * common edge) are grouped into a "cluster". Clusters are never empty and
 * will always contain at least one BSP leaf.
 *
 * @ingroup world
 */
class SectorCluster
#ifdef __CLIENT__
  : public de::LightGrid::IBlockLightSource
#endif
{
public:
    /// Notified when the cluster is about to be deleted.
    DENG2_DEFINE_AUDIENCE(Deletion, void sectorClusterBeingDeleted(SectorCluster const &cluster))

    typedef QList<BspLeaf *> BspLeafs;

public:
    /**
     * Construct a new sector cluster comprised of the specified set of BSP
     * leafs. It is assumed that all BSP leafs in the list are attributed to
     * the same sector and there is always at least one.
     *
     * @param bspLeafs  Set of BSP leafs comprising the resulting cluster.
     */
    SectorCluster(BspLeafs const &bspLeafs);
    virtual ~SectorCluster();

    /**
     * Determines whether the specified @a hedge is an "internal" edge:
     *
     * - both the half-edge and it's twin have a face.
     * - both faces are assigned to a BSP leaf.
     * - both of the assigned BSP leafs are in the same cluster.
     *
     * @param hedge  Half-edge to test.
     *
     * @return  @c true= @a hedge is a cluster internal edge.
     */
    static bool isInternalEdge(de::HEdge *hedge);

    /**
     * Returns the parent sector of the cluster.
     */
    Sector const &sector() const;

    /// @copydoc sector()
    Sector &sector();

    /**
     * Returns the identified @em physical plane of the parent sector. Note
     * that this is not the same as the "visual" plane which may well be
     * defined by another sector.
     *
     * @param planeIndex  Index of the plane to return.
     */
    Plane const &plane(int planeIndex) const;

    /// @copydoc plane()
    Plane &plane(int planeIndex);

    /**
     * Returns the sector plane which defines the @em physical floor of the
     * cluster.
     * @see hasSector(), plane()
     */
    inline Plane const &floor() const { return plane(Sector::Floor); }

    /// @copydoc floor()
    inline Plane &floor() { return plane(Sector::Floor); }

    /**
     * Returns the sector plane which defines the @em physical ceiling of
     * the cluster.
     * @see hasSector(), plane()
     */
    inline Plane const &ceiling() const { return plane(Sector::Ceiling); }

    /// @copydoc ceiling()
    inline Plane &ceiling() { return plane(Sector::Ceiling); }

    /**
     * Returns the identified @em visual sector plane for the cluster (which
     * may or may not be the same as the physical plane).
     *
     * @param planeIndex  Index of the plane to return.
     */
    Plane const &visPlane(int planeIndex) const;

    /// @copydoc visPlane()
    Plane &visPlane(int planeIndex);

    /**
     * Returns the sector plane which defines the @em visual floor of the
     * cluster.
     * @see hasSector(), floor()
     */
    inline Plane const &visFloor() const { return visPlane(Sector::Floor); }

    /// @copydoc visFloor()
    inline Plane &visFloor() { return visPlane(Sector::Floor); }

    /**
     * Returns the sector plane which defines the @em visual ceiling of the
     * cluster.
     * @see hasSector(), ceiling()
     */
    inline Plane const &visCeiling() const { return visPlane(Sector::Ceiling); }

    /// @copydoc visCeiling()
    inline Plane &visCeiling() { return visPlane(Sector::Ceiling); }

    /**
     * Returns the total number of @em visual planes in the cluster.
     */
    inline int visPlaneCount() const { return sector().planeCount(); }

    /**
     * To be called to force re-evaluation of mapped visual planes. This is
     * only necessary when a surface material change occurs on boundary line
     * of the cluster.
     */
    void markVisPlanesDirty();

    /**
     * Returns @c true iff at least one of the mapped visual planes of the
     * cluster presently has a sky-masked material bound.
     *
     * @see Surface::hasSkyMaskedMaterial()
     */
    bool hasSkyMaskedPlane() const;

    /**
     * Provides access to the list of all BSP leafs in the cluster, for
     * efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    /**
     * Returns the total number of BSP leafs in the cluster.
     */
    inline int bspLeafCount() const { return bspLeafs().count(); }

    /**
     * Returns the axis-aligned bounding box of the cluster.
     */
    AABoxd const &aaBox() const;

    /**
     * Returns the point defined by the center of the axis-aligned bounding
     * box in the map coordinate space.
     */
    inline de::Vector2d center() const {
        return (de::Vector2d(aaBox().min) + de::Vector2d(aaBox().max)) / 2;
    }

#ifdef __CLIENT__

    /**
     * Determines whether the cluster has a positive world volume, i.e., the
     * height of floor is lower than that of the ceiling plane.
     *
     * @param useSmoothedHeights  @c true= use the @em smoothed plane heights
     *                            instead of the @em sharp heights.
     */
    bool hasWorldVolume(bool useSmoothedHeights = true) const;

    /**
     * Returns a rough approximation of the total combined area of the geometry
     * for all BSP leafs which define the cluster (map units squared).
     */
    coord_t roughArea() const;

    /**
     * Request re-calculation of environmental audio (reverb) characteristics
     * for the cluster (update is deferred until next accessed).
     *
     * To be called whenever any of the properties governing reverb properties
     * have changed (i.e., wall/plane material changes).
     */
    void markReverbDirty(bool yes = true);

    /**
     * Returns the final environmental audio characteristics (reverb) of the
     * cluster. Note that if a reverb update is scheduled it will be done at
     * this time (@ref markReverbDirty()).
     */
    AudioEnvironmentFactors const &reverb() const;

    /**
     * Returns the unique identifier of the light source.
     */
    LightId lightSourceId() const;

    /**
     * Returns the final ambient light color for the source (which, may be affected
     * by the sky light color if one or more Plane Surfaces in the cluster are using
     * a sky-masked Material).
     */
    de::Vector3f lightSourceColorf() const;

    /**
     * Returns the final ambient light intensity for the source.
     * @see lightSourceColorf()
     */
    de::dfloat lightSourceIntensity(de::Vector3d const &viewPoint = de::Vector3d(0, 0, 0)) const;

    /**
     * Returns the final ambient light color and intensity for the source.
     * @see lightSourceColorf()
     */
    inline de::Vector4f lightSourceColorfIntensity() {
        return de::Vector4f(lightSourceColorf(), lightSourceIntensity());
    }

    /**
     * Returns the Z-axis bias scale factor for the light grid, block light source.
     */
    int blockLightSourceZBias();

    /**
     * Apply bias lighting changes to @em all geometry Shards within the cluster.
     *
     * @param changes  Digest of lighting changes to be applied.
     */
    void applyBiasDigest(BiasDigest &changes);

    /**
     * Perform bias lighting for the supplied Shard geometry.
     *
     * @param mapElement   MapElement for the geometry to be lit.
     * @param geomId       MapElement-unique geometry id.
     * @param posCoords    World coordinates for each vertex.
     * @param colorCoords  Final lighting values will be written here.
     */
    void applyBiasLightSources(de::MapElement &mapElement, int geomId,
                               de::Vector3f const *posCoords, de::Vector4f *colorCoords);

    /**
     * Schedule a lighting update to a geometry Shard following a move of some
     * other element of dependent geometry.
     *
     * @param mapElement  MapElement for the geometry to be updated.
     * @param geomId      MapElement-unique geometry id.
     */
    void updateBiasAfterGeometryMove(de::MapElement &mapElement, int geomId);

#endif // __CLIENT__

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

/**
 * Specialized sector cluster half-edge circulator. Used like an iterator, for
 * circumnavigating the boundary half-edges of a cluster.
 *
 * Cluster-internal edges (i.e., where both half-edge faces reference the same
 * cluster) are automatically skipped during traversal. Otherwise behavior is
 * the same as a "regular" half-edge face circulator.
 *
 * Also provides static search utilities for convenient, one-time use of this
 * specialized search logic (avoiding circulator instantiation).
 *
 * @ingroup world
 */
class SectorClusterCirculator
{
public:
    /// Attempt to dereference a NULL circulator. @ingroup errors
    DENG2_ERROR(NullError);

public:
    /**
     * Construct a new sector cluster circulator.
     *
     * @param hedge  Half-edge to circulate. It is assumed the half-edge lies on
     * the @em boundary of the cluster and is not an "internal" edge.
     */
    SectorClusterCirculator(de::HEdge *hedge = 0)
        : _hedge(hedge)
        , _current(hedge)
        , _cluster(hedge? getCluster(*hedge) : 0)
    {}

    /**
     * Intended as a convenient way to employ the specialized circulator logic
     * to locate the relative back of the next/previous neighboring half-edge.
     * Particularly useful when a geometry traversal requires a switch from the
     * cluster to face boundary, or when navigating the so-called "one-ring" of
     * a vertex.
     */
    static de::HEdge &findBackNeighbor(de::HEdge const &hedge, de::ClockDirection direction)
    {
        return getNeighbor(hedge, direction, getCluster(hedge)).twin();
    }

    /**
     * Returns the neighbor half-edge in the specified @a direction around the
     * boundary of the cluster.
     *
     * @param direction  Relative direction of the desired neighbor.
     */
    de::HEdge &neighbor(de::ClockDirection direction) {
        _current = &getNeighbor(*_current, direction, _cluster);
        return *_current;
    }

    /// Returns the next half-edge (clockwise) and advances the circulator.
    inline de::HEdge &next() { return neighbor(de::Clockwise); }

    /// Returns the previous half-edge (anticlockwise) and advances the circulator.
    inline de::HEdge &previous() { return neighbor(de::Anticlockwise); }

    /// Advance to the next half-edge (clockwise).
    inline SectorClusterCirculator &operator ++ () {
        next(); return *this;
    }
    /// Advance to the previous half-edge (anticlockwise).
    inline SectorClusterCirculator &operator -- () {
        previous(); return *this;
    }

    /// Returns @c true iff @a other references the same half-edge as "this"
    /// circulator; otherwise returns false.
    inline bool operator == (SectorClusterCirculator const &other) const {
        return _current == other._current;
    }
    inline bool operator != (SectorClusterCirculator const &other) const {
        return !(*this == other);
    }

    /// Returns @c true iff the range of the circulator [c, c) is not empty.
    inline operator bool () const { return _hedge != 0; }

    /// Makes the circulator operate on @a hedge.
    SectorClusterCirculator &operator = (de::HEdge &hedge) {
        _hedge = _current = &hedge;
        _cluster = getCluster(hedge);
        return *this;
    }

    /// Returns the current half-edge of a non-empty sequence.
    de::HEdge &operator * () const {
        if(!_current)
        {
            /// @throw NullError Attempted to dereference a "null" circulator.
            throw NullError("SectorClusterCirculator::operator *", "Circulator references an empty sequence");
        }
        return *_current;
    }

    /// Returns a pointer to the current half-edge (might be @c NULL, meaning the
    /// circulator references an empty sequence).
    de::HEdge *operator -> () { return _current; }

private:
    static SectorCluster *getCluster(de::HEdge const &hedge);

    static de::HEdge &getNeighbor(de::HEdge const &hedge, de::ClockDirection direction,
                                  SectorCluster const *cluster = 0);

    de::HEdge *_hedge;
    de::HEdge *_current;
    SectorCluster *_cluster;
};

#endif // DENG_WORLD_SECTORCLUSTER_H
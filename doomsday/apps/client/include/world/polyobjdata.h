/** @file polyobjdata.h  Private data for a polyobj
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_POLYOBJDATA_H
#define DENG_WORLD_POLYOBJDATA_H

#include <QList>
#include <QVector>
#include <de/Vector>
#include <doomsday/world/thinker.h>

#include "Polyobj"

#ifdef __CLIENT__
class ClPolyMover;
#endif

/**
 * Private data for a polyobj.
 *
 * Stored in the Polyobj's thinker.d (polyobjs are not normal thinkers).
 */
class PolyobjData : public Thinker::IData
{
public:
    /// Used to store the original/previous vertex coordinates.
    typedef QVector<de::Vector2d> VertexCoords;

public:
    PolyobjData();
    ~PolyobjData();

    void setThinker(thinker_s *thinker);
    void think();
    IData *duplicate() const;

#ifdef __CLIENT__
    void addMover(ClPolyMover &mover);
    void removeMover(ClPolyMover &mover);
    ClPolyMover *mover() const;
#endif

public:
    de::dint indexInMap = world::MapElement::NoIndex;
    de::duint origIndex = world::MapElement::NoIndex;

    de::Mesh *mesh = nullptr;
    QList<Line *> lines;
    QList<Vertex *> uniqueVertexes;
    VertexCoords originalPts;  ///< Used as the base for the rotations.
    VertexCoords prevPts;      ///< Use to restore the old point values.

private:
    polyobj_s *_polyobj = nullptr;
#ifdef __CLIENT__
    ClPolyMover *_mover = nullptr;
#endif
};

#endif  // DENG_WORLD_POLYOBJDATA_H

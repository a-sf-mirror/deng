/** @file clskyplane.h  Client-side world map sky plane.
 * @ingroup world
 *
 * @authors Copyright © 2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_WORLD_CLSKYPLANE_H
#define DENG_CLIENT_WORLD_CLSKYPLANE_H

#include <de/Observers>

namespace world {

/**
 *
 */
class ClSkyPlane
{
public:
    /// Notified whenever a @em height change occurs.
    DENG2_DEFINE_AUDIENCE2(HeightChange, void clSkyPlaneHeightChanged(ClSkyPlane &skyPlane))

    ClSkyPlane(bool isCeiling = false, de::ddouble defaultHeight = 0);

    /**
     * Returns @c true if this sky plane is configured as the "ceiling".
     *
     * @see isFloor()
     */
    bool isCeiling() const;

    /**
     * Returns @c true if this sky plane is configured as the "ceiling".
     *
     * @see isCeiling()
     */
    bool isFloor() const;

    /**
     * Returns the current height of the sky plane.
     */
    de::ddouble height() const;

    /**
     * Change the height of the sky plane to @a newHeight. The HeightChange audience will
     * be notified if a change occurs.
     */
    void setHeight(de::ddouble newHeight);

private:
    DENG2_PRIVATE(d)
};

} // namespace world

#endif // DENG_CLIENT_WORLD_CLSKYPLANE_H

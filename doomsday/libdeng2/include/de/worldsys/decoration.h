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
 
#ifndef LIBDENG2_DECORATION_H
#define LIBDENG2_DECORATION_H

#include "../Vector"
#include "../Subsector"

namespace de
{
    // Abstract base class for Surface decorations.
    class Decoration
    {
    public:
        Vector3f origin;

        Decoration(Vector3f _origin) : origin(_origin) {
           _subsector = &App::currentMap().pointInSubsector(origin);
        }
        virtual ~Decoration() {};

        Subsector& subsector() const { return *_subsector; }
    protected:
        Subsector* _subsector;
    };

    class LightDecoration : public Decoration
    {
    public:
        LightDecoration(Vector3f _origin, const struct ded_decorlight_s& lightDecorationDef)
            : Decoration(_origin), def(lightDecorationDef) {};
        ~LightDecoration();

    private:
        const struct ded_decorlight_s& def;
    };

    class ModelDecoration : public Decoration
    {
    public:
        ModelDecoration(Vector3f _origin, const struct ded_decormodel_s& modelDecorationDef)
            : Decoration(_origin), def(modelDecorationDef) {};
        ~ModelDecoration();

    private:
        const struct ded_decormodel_s& def;
        struct modeldef_s* mf;
        dfloat pitch, yaw;
    };
}

#endif /* LIBDENG2_DECORATION_H */

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

#include "de/MSurface"

using namespace de;

MSurface::MSurface(const Vector3f& normal, Material* material,
    const Vector2f& materialOffset, dfloat opacity,
    Blendmode blendmode, const Vector3f& tintColor)
  : normal(Vector3f(normal).normalize()),
    _material(material), _materialB(0), _materialBlendFactor(0),
    offset(materialOffset), visOffset(materialOffset), visOffsetDelta(0),
    opacity(opacity), blendmode(blendmode),
    tintColor = Vector3f(clamp(0, tintColor.x, 1), clamp(0, tintColor.y, 1), clamp(0, tintColor.z, 1)),
    _updateDecorations(false), _inFlags(0), owner(0), flags(0),
{
    oldOffset[0] = oldOffset[1] = offset;
}

MSurface::~MSurface()
{
    destroyDecorations();
}

void MSurface::resetScroll()
{
    visOffsetDelta = Vector2f(0, 0);
    oldOffset[0] = oldOffset[1] = offset;

    update();
}

void MSurface::interpolateScroll(dfloat frameTimePos)
{
    visOffsetDelta = oldOffset * (1 - frameTimePos);
    visOffsetDelta += offset * frameTimePos - offset;
    visOffset = offset + visOffsetDelta;

    update();
}

void MSurface::updateScroll()
{
    oldOffset[0] = oldOffset[1];
    oldOffset[1] = offset;

    Vector2f diff = oldOffset[0] - oldOffset[1];
    if(de::abs(diff.length()) > MAX_MATERIAL_SMOOTHSCROLL_DISTANCE)
    {   // Too fast, make an instantaneous jump.
        oldOffset[0] = oldOffset[1];
    }
}

void MSurface::setMaterial(Material* newMaterial, bool smooth)
{
    if(_material == newMaterial)
        return;

    // No longer a missing texture fix?
    if(newMaterial)
        _inFlags[MATERIALFIX] = false;

    const materialenvinfo_t* env;

    if(!ddMapSetup && smooth && rendMaterialFadeSeconds > 0 &&
       ((newMaterial && (env = S_MaterialEnvDef(Material_GetEnvClass(newMaterial))) && (env->flags & MEF_BLEND)) ||
        ((env = S_MaterialEnvDef(Material_GetEnvClass(material))) && (env->flags & MEF_BLEND))))
    {
        // @todo map should be determined from the surface.
        Map& map = App::app().currentMap();

        // Stop any active material fader on this surface.
        map.iterate(SID_MATERIALFADE_THINKER, RIT_StopMatFader, this);

        materialfade_t* fader = new MaterialFadeThinker;
        fader->suf = suf;
        map.add(fader);

        _materialB = _material;
    }

    _material = newMaterial;
    _updateDecorations = true;
}

void MSurface::setMaterialOffsetX(dfloat x)
{
    if(offset.x == x)
        return;

    offset.x = x;
    if(!ddMapSetup)
    {
        Map& map = App::app().currentMap();
        map->_movingSurfaces.insert(this);
    }
    else
        visOffset.x = offset.x;

    _updateDecorations = true;
}

void MSurface::setMaterialOffsetY(dfloat y)
{
    if(offset.y == y)
        return;

    offset.y = y;
    if(!ddMapSetup)
    {
        Map& map = App::app().currentMap();
        map->_movingSurfaces.insert(this);
    }
    else
        visOffset.y = offset.y;

    _updateDecorations = true;
}

void MSurface::setMaterialOffset(dfloat x, dfloat y)
{
    if(offset.x == x && offset.y == y)
        return;

    offset = Vector2f(x, y);
    if(!ddMapSetup)
    {
        Map& map = App::app().currentMap();
        map->_movingSurfaces.insert(this);
    }
    else
        visOffset = offset;

    _updateDecorations = true;
}

void MSurface::setTintColorRed(dfloat val)
{
    tintColor.x = clamp(0, val, 1);
}

void MSurface::setTintColorGreen(dfloat val)
{
    tintColor.y = clamp(0, val, 1);
}

void MSurface::setTintColorBlue(dfloat val)
{
    tintColor.z = clamp(0, val, 1);
}

void MSurface::setTintColor(const Vector3f& newColor)
{
    tintColor.x = clamp(0, newColor.x, 1);
    tintColor.y = clamp(0, newColor.y, 1);
    tintColor.z = clamp(0, newColor.z, 1);
}

void MSurface::setOpactiy(dfloat val)
{
    opacity = clamp(0, val, 1);
}

void MSurface::setBlendmode(Blendmode newBlendmode)
{
    blendmode = newBlendmode;
}

void MSurface::update()
{
    _updateDecorations = true;
}

Decoration& MSurface::add(Decoration* decoration)
{
    _decorations.push_back(decoration);
    return *decoration;
}

void MSurface::destroyDecorations()
{
    FOR_EACH(i, _decorations, Decorations::iterator)
        delete (*i);
    _decorations.clear();
}

#if 0
bool MSurface::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_BLENDMODE:
        {
        blendmode_t             blendMode;
        DMU_SetValue(DMT_SURFACE_BLENDMODE, &blendMode, args, 0);
        Surface_SetBlendMode(suf, blendMode);
        }
        break;

    case DMU_FLAGS:
        DMU_SetValue(DMT_SURFACE_FLAGS, &flags, args, 0);
        break;

    case DMU_COLOR:
        {
        float                   rgb[3];
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CR], args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CG], args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CB], args, 2);
        Surface_SetColorR(suf, rgb[CR]);
        Surface_SetColorG(suf, rgb[CG]);
        Surface_SetColorB(suf, rgb[CB]);
        }
        break;
    case DMU_COLOR_RED:
        {
        float           r;
        DMU_SetValue(DMT_SURFACE_RGBA, &r, args, 0);
        Surface_SetColorR(suf, r);
        }
        break;
    case DMU_COLOR_GREEN:
        {
        float           g;
        DMU_SetValue(DMT_SURFACE_RGBA, &g, args, 0);
        Surface_SetColorG(suf, g);
        }
        break;
    case DMU_COLOR_BLUE:
        {
        float           b;
        DMU_SetValue(DMT_SURFACE_RGBA, &b, args, 0);
        Surface_SetColorB(suf, b);
        }
        break;
    case DMU_ALPHA:
        {
        float           a;
        DMU_SetValue(DMT_SURFACE_RGBA, &a, args, 0);
        Surface_SetColorA(suf, a);
        }
        break;
    case DMU_MATERIAL:
        {
        void*           p;
        DMU_SetValue(DDVT_PTR, &p, args, 0);
        Surface_SetMaterial(suf, p? ((objectrecord_t*) p)->obj : NULL, true);
        }
        break;
    case DMU_OFFSET_X:
        {
        float           offX;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offX, args, 0);
        Surface_SetMaterialOffsetX(suf, offX);
        }
        break;
    case DMU_OFFSET_Y:
        {
        float           offY;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offY, args, 0);
        Surface_SetMaterialOffsetY(suf, offY);
        }
        break;
    case DMU_OFFSET_XY:
        {
        float           offset[2];
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VX], args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VY], args, 1);
        Surface_SetMaterialOffsetXY(suf, offset[VX], offset[VY]);
        }
        break;
    default:
        Con_Error("Surface_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

bool MSurface::getProperty(setargs_t *args) const
{
    switch(args->prop)
    {
    case DMU_MATERIAL:
        {
        Material*     mat = material;
        objectrecord_t* r;

        if(_inFlags & SUIF_MATERIAL_FIX)
            mat = NULL;
        r = P_ObjectRecord(DMU_MATERIAL, mat);
        DMU_GetValue(DMT_SURFACE_MATERIAL, &r, args, 0);
        break;
        }
    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VX], args, 0);
        break;
    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VY], args, 0);
        break;
    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &offset[VY], args, 1);
        break;
    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VX], args, 0);
        break;
    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VY], args, 0);
        break;
    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VZ], args, 0);
        break;
    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &normal[VZ], args, 2);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CR], args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CG], args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CB], args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CA], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CR], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CG], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CB], args, 0);
        break;
    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &rgba[CA], args, 0);
        break;
    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &blendMode, args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &flags, args, 0);
        break;
    default:
        Con_Error("Surface_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
#endif

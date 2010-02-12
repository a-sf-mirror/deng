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

MSurface::~MSurface()
{
    destroyDecorations();
}

void MSurface::resetScroll()
{
    // X Offset.
    visOffsetDelta[0] = 0;
    oldOffset[0][0] = oldOffset[0][1] = offset[0];
    // Y Offset.
    visOffsetDelta[1] = 0;
    oldOffset[1][0] = oldOffset[1][1] = offset[1];

    update();
}

void MSurface::interpolateScroll(dfloat frameTimePos)
{
    // X Offset.
    visOffsetDelta[0] =
        oldOffset[0][0] * (1 - frameTimePos) +
                offset[0] * frameTimePos - offset[0];

    // Y Offset.
    visOffsetDelta[1] =
        oldOffset[1][0] * (1 - frameTimePos) +
                offset[1] * frameTimePos - offset[1];

    // Visible material offset.
    visOffset[0] = offset[0] + visOffsetDelta[0];
    visOffset[1] = offset[1] + visOffsetDelta[1];

    update();
}

void MSurface::updateScroll()
{
    // X Offset
    oldOffset[0][0] = oldOffset[0][1];
    oldOffset[0][1] = offset[0];
    if(oldOffset[0][0] != oldOffset[0][1])
        if(fabs(oldOffset[0][0] - oldOffset[0][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            oldOffset[0][0] = oldOffset[0][1];
        }

    // Y Offset
    oldOffset[1][0] = oldOffset[1][1];
    oldOffset[1][1] = offset[1];
    if(oldOffset[1][0] != oldOffset[1][1])
        if(fabs(oldOffset[1][0] - oldOffset[1][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            oldOffset[1][0] = oldOffset[1][1];
        }
}

bool MSurface::setMaterial(Material* mat, bool fade)
{
    matfader_t* fader;
    const materialenvinfo_t* env;

    if(!mat)
        return false;

    if(suf->material == mat)
        return true;

    // No longer a missing texture fix?
    if(mat && (suf->oldFlags & SUIF_MATERIAL_FIX))
        suf->inFlags &= ~SUIF_MATERIAL_FIX;

    if(!ddMapSetup && fade && rendMaterialFadeSeconds > 0 &&
       ((mat && (env = S_MaterialEnvDef(Material_GetEnvClass(mat))) && (env->flags & MEF_BLEND)) ||
        ((env = S_MaterialEnvDef(Material_GetEnvClass(suf->material))) && (env->flags & MEF_BLEND))))
    {
        // Stop active material fade on this surface.
        Map_IterateThinkers2(P_CurrentMap(), R_MatFaderThinker, ITF_PRIVATE, // Always non-public
                            RIT_StopMatFader, suf);

        fader = Z_Malloc(sizeof(matfader_t), PU_MAP, 0);
        memset(fader, 0, sizeof(*fader));
        fader->thinker.function = R_MatFaderThinker;
        fader->suf = suf;
        fader->tics = 0;

        // @todo map should be determined from the surface.
        Map_AddThinker(P_CurrentMap(), &fader->thinker, false);

        suf->materialB = suf->material;
    }

    suf->material = mat;

    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    suf->oldFlags = suf->inFlags;

    return true;
}

/**
 * Update the surface, material X offset.
 *
 * @param suf           The surface to be updated.
 * @param x             Material offset, X delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetX(MSurface* suf, float x)
{
    if(!suf)
        return false;

    if(suf->offset[0] == x)
        return true;

    suf->offset[0] = x;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
    {
        Map* map = P_CurrentMap();

        if(!map->_movingSurfaceList)
            map->_movingSurfaceList = P_CreateSurfaceList();

        SurfaceList_Add(map->_movingSurfaceList, suf);
    }
    else
    {
        suf->visOffset[0] = suf->offset[0];
    }

    return true;
}

/**
 * Update the surface, material Y offset.
 *
 * @param suf           The surface to be updated.
 * @param y             Material offset, Y delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetY(MSurface* suf, float y)
{
    if(!suf)
        return false;

    if(suf->offset[1] == y)
        return true;

    suf->offset[1] = y;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
    {
        Map* map = P_CurrentMap();

        if(!map->_movingSurfaceList)
            map->_movingSurfaceList = P_CreateSurfaceList();

        SurfaceList_Add(map->_movingSurfaceList, suf);
    }
    else
    {
        suf->visOffset[1] = suf->offset[1];
    }

    return true;
}

/**
 * Update the surface, material X+Y offset.
 *
 * @param suf           The surface to be updated.
 * @param x             Material offset, X delta.
 * @param y             Material offset, Y delta.
 *
 * @return              @c true, if the change was made successfully.
 */
boolean Surface_SetMaterialOffsetXY(MSurface* suf, float x, float y)
{
    if(!suf)
        return false;

    if(suf->offset[0] == x && suf->offset[1] == y)
        return true;

    suf->offset[0] = x;
    suf->offset[1] = y;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    if(!ddMapSetup)
    {
        Map* map = P_CurrentMap();

        if(!map->_movingSurfaceList)
            map->_movingSurfaceList = P_CreateSurfaceList();

        SurfaceList_Add(map->_movingSurfaceList, suf);
    }
    else
    {
        suf->visOffset[0] = suf->offset[0];
        suf->visOffset[1] = suf->offset[1];
    }

    return true;
}

/**
 * Update the surface, red color component.
 */
boolean Surface_SetColorR(MSurface* suf, float r)
{
    if(!suf)
        return false;

    r = MINMAX_OF(0, r, 1);

    if(suf->rgba[CR] == r)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CR] = r;

    return true;
}

/**
 * Update the surface, green color component.
 */
boolean Surface_SetColorG(MSurface* suf, float g)
{
    if(!suf)
        return false;

    g = MINMAX_OF(0, g, 1);

    if(suf->rgba[CG] == g)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CG] = g;

    return true;
}

/**
 * Update the surface, blue color component.
 */
boolean Surface_SetColorB(MSurface* suf, float b)
{
    if(!suf)
        return false;

    b = MINMAX_OF(0, b, 1);

    if(suf->rgba[CB] == b)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CB] = b;

    return true;
}

/**
 * Update the surface, alpha.
 */
boolean Surface_SetColorA(MSurface* suf, float a)
{
    if(!suf)
        return false;

    a = MINMAX_OF(0, a, 1);

    if(suf->rgba[CA] == a)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CA] = a;

    return true;
}

bool MSurface::setColorRGBA(dfloat r, dfloat g, dfloat b, dfloat a)
{
    r = de::clamp(0, r, 1);
    g = de::clamp(0, g, 1);
    b = de::clamp(0, b, 1);
    a = de::clamp(0, a, 1);

    if(rgba[CR] == r && rgba[CG] == g && rgba[CB] == b && rgba[CA] == a)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    rgba[CR] = r;
    rgba[CG] = g;
    rgba[CB] = b;
    rgba[CA] = a;

    return true;
}

#if 0
bool MSurface::setBlendMode(blendmode_t newBlendMode)
{
    if(blendMode != newBlendMode)
    blendMode = newBlendMode;
    return true;
}
#endif

void MSurface::update()
{
    inFlags |= SUIF_UPDATE_DECORATIONS;
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
        DMU_SetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
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
        Material*     mat = suf->material;
        objectrecord_t* r;

        if(suf->inFlags & SUIF_MATERIAL_FIX)
            mat = NULL;
        r = P_ObjectRecord(DMU_MATERIAL, mat);
        DMU_GetValue(DMT_SURFACE_MATERIAL, &r, args, 0);
        break;
        }
    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        break;
    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 0);
        break;
    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 1);
        break;
    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        break;
    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 0);
        break;
    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 0);
        break;
    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 2);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 0);
        break;
    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 0);
        break;
    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &suf->blendMode, args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
        break;
    default:
        Con_Error("Surface_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
#endif

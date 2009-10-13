/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * rend_dyn.c: Projected lumobjs (dynlight) lists.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define LUM_FACTOR(dist, radius) (1.5f - 1.5f*(dist)/(radius))

// TYPES -------------------------------------------------------------------

typedef struct dynlistnode_s {
    struct dynlistnode_s* next;
    void*           data;
} dynlistnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

dynlight_t* DL_NewDynlight(DGLuint texture, const float color[3],
                           const float s[2], const float t[2]);
void DL_DeleteDynlight(dynlight_t* dyn);

void Dynlight_SetTex(dynlight_t* dyn, DGLuint tex);
void Dynlight_SetTexCoords(dynlight_t* dyn, const float s[2], const float t[2]);
void Dynlight_SetColor(dynlight_t* dyn, const float color[3]);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int useDynlights = true, dlBlend = 0;
float dlFactor = .7f;
float dlFogBright = .15f;

int useWallGlow = true;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint numDynlists = 0, maxDynlists = 0;
static dynlist_t* dynlists = NULL;

// Unused dynlights.
static dynlist_t unusedDynlights = { NULL, NULL };

// Unused listnodes.
static dynlistnode_t* unusedNodes = NULL;

// CODE --------------------------------------------------------------------

void DL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-glow", &glowingTextures, 0, 0, 1);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);

    C_VAR_INT2("rend-light", &useDynlights, 0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT("rend-light-blend", &dlBlend, 0, 0, 2);
    C_VAR_FLOAT("rend-light-bright", &dlFactor, 0, 0, 1);
    C_VAR_FLOAT("rend-light-fog-bright", &dlFogBright, 0, 0, 1);
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);

    C_VAR_INT("rend-mobj-light-auto", &useMobjAutoLights, 0, 0, 1);

    LO_Register();
    Rend_DecorRegister();
}

static dynlistnode_t* newListNode(void)
{
    dynlistnode_t*         node;

    if(unusedNodes)
    {
        node = unusedNodes;
        unusedNodes = unusedNodes->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {
        node = Z_Calloc(sizeof(*node), PU_STATIC, NULL);
    }

    return node;
}

static void deleteListNode(dynlistnode_t* node)
{
    node->next = unusedNodes;
    unusedNodes = node;
}

static void listPushFront(dynlist_t* list, void* data)
{
    dynlistnode_t*          node = newListNode();

    node->data = data;

    node->next = list->head;
    list->head = node;

    if(!list->tail)
        list->tail = node;
}

static void* listPopFront(dynlist_t* list)
{
    if(list->head)
    {
        dynlistnode_t*      node = list->head;

        list->head = list->head->next;
        if(!list->head)
            list->tail = NULL;

        deleteListNode(node);

        return node->data;
    }

    Con_Error("listPopFront: Failed, list is empty.");
    return NULL;
}

static void listInsertAt(dynlist_t* list, dynlistnode_t** at, void* data)
{
    dynlistnode_t*          node = newListNode();

    node->data = data;

    node->next = (*at);
    (*at) = node;

    if(!list->tail || !(*at)->next)
        list->tail = node;
}

/**
 * Join the two lists by adding @a other to the back of @a dst.
 */
static void listSpliceBack(dynlist_t* dst, dynlist_t* other)
{
    if(dst->tail)
    {
        dst->tail->next = other->head;
        dst->tail = other->tail;
    }
    else
    {
        dst->head = other->head;
        dst->tail = other->tail;
    }

    other->head = other->tail = NULL;
}

/**
 * Clear all data ptrs in the list. The data pointed to is untouched.
 */
static void listClear(dynlist_t* list)
{
    if(list->head)
    {
        // Join the existing unusedNodes to the tail of this list,
        // then set the unusedNodes list head to the head of this list.
        list->tail->next = unusedNodes;
        unusedNodes = list->head;
    }

    list->head = list->tail = NULL;
}

static boolean listIsEmpty(dynlist_t* list)
{
    return (list->head? false : true);
}

static int listIterate(dynlist_t* list,
                       boolean (*callback) (void* obj, void* callerData),
                       void* callerData)
{
    dynlistnode_t*      node;
    boolean             retVal;

    node = list->head;
    retVal = true;
    while(node)
    {
        if(!callback(node->data, callerData))
        {
            retVal = false;

            node = NULL; // break the loop.
        }
        else
            node = node->next;
    }

    return retVal;
}

static dynlight_t* allocDynlight(void)
{
    dynlight_t*         dyn;

    if(!listIsEmpty(&unusedDynlights))
    {   // We have an unused dynlight we can re-use.       
        dyn = listPopFront(&unusedDynlights);
    }
    else
    {
        dyn = Z_Malloc(sizeof(*dyn), PU_STATIC, 0);
    }

    return dyn;
}

static void linkDynlightToMap(gamemap_t* map, dynlight_t* dyn)
{
    listPushFront(&map->dlights.linkList, dyn);
}

static dynlist_t* allocDynlist(void)
{
    dynlist_t*          list;

    // Ran out of light link lists?
    if(++numDynlists >= maxDynlists)
    {
        uint                newNum = maxDynlists * 2;

        if(!newNum)
            newNum = 2;

        dynlists =
            Z_Realloc(dynlists, newNum * sizeof(dynlist_t), PU_STATIC);
        maxDynlists = newNum;
    }

    list = &dynlists[numDynlists - 1];
    memset(list, 0, sizeof(list));

    return list;
}

/**
 * Compare two dynlights.
 *
 * @result              @c <0 = @param b is brighter than @param a.
 *                      @c >0 = @param a is brighter than @param b.
 *                      @c  0 = @param a and @param b are equivalent.
 */
static int compareDynlights(const dynlight_t* a, const dynlight_t* b)
{
    float               la = (a->color[CR] + a->color[CG] + a->color[CB]) / 3;
    float               lb = (b->color[CR] + b->color[CG] + b->color[CB]) / 3;

    if(la < lb)
        return -1;
    if(la > lb)
        return 1;
    return 0;
}

/**
 * Find the potential insertion point in list for dynlight.
 *
 * @param list          The list to be searched.
 * @param dyn           The dynlight for potential insertion.
 * @param sortBright    @c true = List is to be ordered from "bright" to
 *                      "dim" and older dynlights take precedence over new.
 *
 * @return              Ptr to the chosen insertion point. Never @c NULL.
 */
static dynlistnode_t** findPlaceInListForDynlight(dynlist_t* list, dynlight_t* dyn,
                                              boolean sortBright)
{
    dynlistnode_t**         at = &list->head;

    if(sortBright && list->head)
    {
        for(;;)
        {
            if(!(compareDynlights((*at)->data, dyn) < 0))
            {
                if(*(at = &(*at)->next) != NULL)
                    continue;
            }

            break; // Insert it here.
        }
    }

    return at;
}

static void linkDynlightToList(dynlist_t* list, dynlight_t* dyn,
                               boolean sortBright)
{
    dynlistnode_t**         at;

    if((at = findPlaceInListForDynlight(list, dyn, sortBright)))
    {
        listInsertAt(list, at, dyn);
    }
}

/**
 * Project the given plane light onto the specified surface.
 *
 * @param lum           Ptr to the lumobj lighting the surface.
 * @param bottom        Z height (bottom) of the section being lit.
 * @param top           Z height (top) of the section being lit.
 * @param s             Texture s coords written back here.
 * @param t             Texture t coords written back here.
 *
 * @return              @c true, if the generated coords are within bounds.
 */
static boolean genGlowTexCoords(const lumobj_t* lum, float bottom, float top,
                                pvec2_t s, pvec2_t t)
{
    float               glowHeight;

    if(bottom >= top)
        return false; // No height.

    glowHeight =
        (MAX_GLOWHEIGHT * LUM_PLANE(lum)->intensity) * glowHeightFactor;

    // Don't make too small or too large glows.
    if(glowHeight <= 2)
        return false;

    if(glowHeight > glowHeightMax)
        glowHeight = glowHeightMax;

    // Calculate texture coords for the light.
    if(LUM_PLANE(lum)->normal[VZ] < 0)
    {   // Light is cast downwards.
        t[1] = t[0] = (lum->pos[VZ] - top) / glowHeight;
        t[1]+= (top - bottom) / glowHeight;
    }
    else
    {   // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;
    }

    if(!(t[0] <= 1 || t[1] >= 0))
        return false; // Is above/below on the Y axis.

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    return true;
}

/**
 * Generate texcoords on surface centered on point.
 *
 * @param point         Point on surface around which texture is centered.
 * @param scale         Scale multiplier for texture.
 * @param v1            Top left vertex of the surface being projected on.
 * @param v2            Bottom right vertex of the surface being projected on.
 * @param normal        Normal of the surface being projected on.
 * @param s             Texture s coords written back here.
 * @param t             Texture t coords written back here.
 *
 * @return              @c true, if the generated coords are within bounds.
 */
static boolean genTexCoords(const_pvec3_t point, float scale,
                            const_pvec3_t v1, const_pvec3_t v2,
                            const_pvec3_t normal, pvec2_t s, pvec2_t t)
{
    vec3_t              vToPoint, right, up;

    V3_BuildUpRight(up, right, normal);
    V3_Subtract(vToPoint, v1, point);
    s[0] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[0] = V3_DotProduct(vToPoint, up) * scale + .5f;

    V3_Subtract(vToPoint, v2, point);
    s[1] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[1] = V3_DotProduct(vToPoint, up) * scale + .5f;

    // Would the light be visible?
    if(!(s[0] <= 1 || s[1] >= 0))
        return false; // Is right/left on the X axis.

    if(!(t[0] <= 1 || t[1] >= 0))
        return false; // Is above/below on the Y axis.

    return true;
}

static void applyDynlightColorModifiers(float color[3], float luma)
{
    luma *= dlFactor;

    // In fog, additive blending is used. The normal fog color
    // is way too bright.
    if(usingFog)
        luma *= dlFogBright; // Would be too much.

    luma = MINMAX_OF(0, luma, 1);

    // Multiply luma with color.
    color[CR] *= luma;
    color[CG] *= luma;
    color[CB] *= luma;
}

static dynlight_t* omniProject(const lumobj_t* lum, byte texSlot,
                               const_pvec3_t v1, const_pvec3_t v2,
                               const_pvec3_t normal)
{
    DGLuint             tex;
    vec3_t              lumCenter, vToLum;
    uint                lumIdx = LO_ToIndex(lum);

    if(LO_IsHidden(lumIdx, viewPlayer - ddPlayers))
        return 0;

    switch(texSlot)
    {
    default:    tex = LUM_OMNI(lum)->tex;       break;
    case 1:     tex = LUM_OMNI(lum)->floorTex;  break;
    case 2:     tex = LUM_OMNI(lum)->ceilTex;   break;
    }

    if(!tex)
        return 0;

    V3_Set(lumCenter, lum->pos[VX], lum->pos[VY],
           lum->pos[VZ] + LUM_OMNI(lum)->zOff);

    // On the right side?
    V3_Subtract(vToLum, v1, lumCenter);
    if(V3_DotProduct(vToLum, normal) < 0.f)
    {
        float               dist;
        vec3_t              point;

        // Calculate 3D distance between surface and lumobj.
        V3_ClosestPointOnPlane(point, normal, v1, lumCenter);
        dist = V3_Distance(point, lumCenter);
        if(dist > 0 && dist <= LUM_OMNI(lum)->radius)
        {
            float               luma =
                LUM_FACTOR(dist, LUM_OMNI(lum)->radius);

            // If a max distance limit is set, lumobjs fade out.
            if(lum->maxDistance > 0)
            {
                float               distFromViewer =
                    LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);

                if(distFromViewer > lum->maxDistance)
                    luma = 0;

                if(distFromViewer > .67f * lum->maxDistance)
                {
                    luma *= (lum->maxDistance - distFromViewer) /
                         (.33f * lum->maxDistance);
                }
            }

            if(luma >= .05f)
            {
                vec2_t              s, t;
                float               scale = 1.0f /
                    ((2.f * LUM_OMNI(lum)->radius) - dist);

                if(genTexCoords(point, scale, v1, v2, normal, s, t))
                {
                    float               color[3];

                    color[CR] = LUM_OMNI(lum)->color[CR];
                    color[CG] = LUM_OMNI(lum)->color[CG];
                    color[CB] = LUM_OMNI(lum)->color[CB];

                    applyDynlightColorModifiers(color, luma);

                    return DL_NewDynlight(tex, color, s, t);
                }
            }
        }
    }

    return NULL;
}

static dynlight_t* planeProject(const lumobj_t* lum, const_pvec3_t v1,
                                const_pvec3_t v2)
{
    vec2_t              s, t;

    if(LUM_PLANE(lum)->tex && genGlowTexCoords(lum, v2[VZ], v1[VZ], s, t))
    {
        float               color[3];

        color[CR] = LUM_PLANE(lum)->color[CR];
        color[CG] = LUM_PLANE(lum)->color[CG];
        color[CB] = LUM_PLANE(lum)->color[CB];

        applyDynlightColorModifiers(color, 1);

        return DL_NewDynlight(LUM_PLANE(lum)->tex, color, s, t);
    }

    return NULL;
}

typedef struct surfacelumobjiterparams_s {
    gamemap_t*      map;
    vec3_t          v1, v2;
    vec3_t          normal;
    uint            listIdx;
    byte            flags; // DLF_* flags.
} surfacelumobjiterparams_t;

static boolean DLIT_SurfaceLumobjContacts(void* ptr, void* context)
{
    surfacelumobjiterparams_t* p = (surfacelumobjiterparams_t*) context;
    lumobj_t*           lum = (lumobj_t*) ptr;
    dynlight_t*         dyn = NULL;

    if(lum->type == LT_PLANE && (p->flags & DLF_NO_PLANAR))
        return true; // Continue iteration.

    switch(lum->type)
    {
    case LT_OMNI:
        {
        byte                texSlot = ((p->flags & DLF_TEX_CEILING)? 2 :
            (p->flags & DLF_TEX_FLOOR)? 1 : 0);
        
        dyn = omniProject(lum, texSlot, p->v1, p->v2, p->normal);
        break;
        }
    case LT_PLANE:
        dyn = planeProject(lum, p->v1, p->v2 /*, p->normal*/);
        break;

    default:
        break;
    }

    if(dyn)
    {   // Projection reached the surface.
        dynlist_t*          list;

        // Got a list for this surface yet?
        if(p->listIdx)
        {
            list = &dynlists[p->listIdx - 1];
        }
        else
        {
            list = allocDynlist();
            p->listIdx = numDynlists; // 1-based index.
        }

        linkDynlightToMap(p->map, dyn);
        linkDynlightToList(list, dyn, (p->flags & DLF_SORT_LUMADSC)? true : false);
    }

    return true; // Continue iteration.
}

/**
 * Destroys all dynlights for the specified map.
 */
void DL_DestroyDynlights(gamemap_t* map)
{
    if(!map)
        return;

    // Move dynlights to the global list of unused dynlights for recycling.
    listSpliceBack(&unusedDynlights, &map->dlights.linkList);
}

/**
 * @note Only clears the lists, the dynlights they link to are not touched.
 */
void DL_ClearDynlists(void)
{
    uint                i;

    // Move all listnodes to the global list of unused listnodes for recycling.
    for(i = 0; i < numDynlists; ++i)
    {
        listClear(&dynlists[i]);
    }

    numDynlists = 0;
}

/**
 * Project all lumobjs affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of dynlights.
 *
 * \assume The coordinates of the given quad must be contained wholly within
 * the subsector specified. This is due to an optimization within the lumobj
 * management which seperates them according to their position in the BSP.
 *
 * @param subSector          SubSector within which the quad wholly resides.
 * @param topLeft       Coordinates of the top left corner of the quad.
 * @param bottomRight   Coordinates of the bottom right corner of the quad.
 * @param normal        Normalized normal of the quad.
 * @param flags         DLF_* flags.
 *
 * @return              Dynlight list name if the quad is lit by one or more
 *                      light sources, else @c 0.
 */
uint DL_ProjectOnSurface(gamemap_t* map, subsector_t* subsector,
                         const vectorcomp_t topLeft[3],
                         const vectorcomp_t bottomRight[3],
                         const vectorcomp_t normal[3], byte flags)
{
    surfacelumobjiterparams_t params;

    if(!map || !subsector)
        return 0;

    if(!useDynlights && !useWallGlow)
        return 0; // Disabled.

    V3_Copy(params.v1, topLeft);
    V3_Copy(params.v2, bottomRight);
    V3_Set(params.normal, normal[VX], normal[VY], normal[VZ]);

    params.map = map;
    params.flags = flags;
    params.listIdx = 0;

    // Process each lumobj contacting the subsector.
    R_IterateSubsectorContacts(subsector, OT_LUMOBJ, DLIT_SurfaceLumobjContacts,
                               &params);

    // Did we generate a light list?
    return params.listIdx;
}

/**
 * Calls func for all projected dynlights in the specified dynlist.
 *
 * @param dynlistID     Identifier of the dynlist to process.
 * @param callback      Callback to make for each object.
 * @param data          Ptr to pass to the callback.
 *
 * @return              @c true, iff every callback returns @c true.
 */
boolean DL_DynlistIterator(uint dynlistID,
                           boolean (*callback) (void* dynlight, void* context),
                           void* context)
{
    dynlist_t*          list;

    if(dynlistID == 0 || dynlistID > numDynlists)
        return true;

    list = &dynlists[dynlistID - 1 /*1-based index*/];

    return listIterate(list, callback, context);
}

dynlight_t* DL_NewDynlight(DGLuint texture, const float color[3],
                           const float s[2], const float t[2])
{
    dynlight_t*         dyn = allocDynlight();
        
    Dynlight_SetTex(dyn, texture);
    Dynlight_SetTexCoords(dyn, s, t);
    Dynlight_SetColor(dyn, color);

    return dyn;
}

void DL_DeleteDynlight(dynlight_t* dyn)
{
    if(!dyn)
        return;

    Z_Free(dyn);
}

void Dynlight_SetTex(dynlight_t* dyn, DGLuint tex)
{
    if(!dyn)
        return;

    dyn->texture = tex;
}

void Dynlight_SetTexCoords(dynlight_t* dyn, const float s[2], const float t[2])
{
    if(!dyn)
        return;

    if(s)
    {
        dyn->s[0] = s[0];
        dyn->s[1] = s[1];
    }

    if(t)
    {
        dyn->t[0] = t[0];
        dyn->t[1] = t[1];
    }
}

/**
 * Blend the given light value with the color (applying any global modifiers
 * in the process).
 *
 * @param dyn           Dynlight to be updated.
 * @param color         Light color.
 */
void Dynlight_SetColor(dynlight_t* dyn, const float color[3])
{
    if(!dyn)
        return;

    dyn->color[CR] = color[CR];
    dyn->color[CG] = color[CG];
    dyn->color[CB] = color[CB];
}

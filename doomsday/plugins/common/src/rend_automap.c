/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * rend_automap.c : Automap drawing.
 *
 * Code herein is considered a friend of automap_t. Consequently this means
 * that it need not negotiate the automap manager any may access automaps
 * directly.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "am_map.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "r_common.h"
#if __JDOOM64__
#  include "p_inventory.h"
#endif

#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

#define AM_LINE_WIDTH       (1.25f)

// TYPES -------------------------------------------------------------------

typedef struct {
    int             scissorState[5];

// DGL Display lists.
    DGLuint         lists[NUM_MAP_OBJECTLISTS]; // Each list contains one or more of given type of automap obj.
    boolean         constructMap; // @c true = force a rebuild of all lists.
} rautomap_data_t;

typedef struct {
    player_t*       plr;
    const automap_t* map;
    const automapcfg_t* cfg;
    // The type of object we want to draw. If @c -1, draw only line specials.
    int             objType;
    boolean         addToLists;
} rendwallseg_params_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean freezeMapRLs = false;

// if -1 no background image will be drawn.
#if __JDOOM__ || __JDOOM64__
static int autopageLumpNum = -1;
#elif __JHERETIC__
static int autopageLumpNum = 1;
#else
static int autopageLumpNum = 1;
#endif

dpatch_t markerPatches[10]; // Numbers used for marking by the automap (lump indices).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static rautomap_data_t rautomaps[MAXPLAYERS];

#if __JDOOM__ || __JDOOM64__
static int their_colors[] = {
    GREENS,
    GRAYS,
    BROWNS,
    REDS
};
#elif __JHERETIC__
static int their_colors[] = {
    KEY3_COLOR,
    KEY2_COLOR,
    BLOODRED,
    KEY1_COLOR
};
#else
static int their_colors[] = {
    AM_PLR1_COLOR,
    AM_PLR2_COLOR,
    AM_PLR3_COLOR,
    AM_PLR4_COLOR,
    AM_PLR5_COLOR,
    AM_PLR6_COLOR,
    AM_PLR7_COLOR,
    AM_PLR8_COLOR
};
#endif

static int numTexUnits;
static boolean envModAdd; // TexEnv: modulate and add is available.
static DGLuint amMaskTexture = 0; // Used to mask the map primitives.

// CODE --------------------------------------------------------------------

static void deleteMapLists(rautomap_data_t* rmap)
{
    uint                i;

    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        if(rmap->lists[i])
            DGL_DeleteLists(rmap->lists[i], 1);
        rmap->lists[i] = 0;
    }
}

void Rend_AutomapInit(void)
{
    // Does the graphics library support multitexturing?
    numTexUnits = DD_GetInteger(DD_MAX_TEXTURE_UNITS);
    envModAdd = (DGL_GetInteger(DGL_MODULATE_ADD_COMBINE)? true : false);

    memset(rautomaps, 0, sizeof(rautomaps));
}

/**
 * Load any resources needed for drawing the automap.
 * Called during startup (post init) and after a renderer restart.
 */
void Rend_AutomapLoadData(void)
{
#if !__JDOOM64__
    int                 i;
    char                namebuf[9];
#endif

    if(IS_DEDICATED)
        return; // Nothing to do.

#if !__JDOOM64__
    // Load the marker patches.
    for(i = 0; i < 10; ++i)
    {
        MARKERPATCHES; // Check the macros eg: "sprintf(namebuf, "AMMNUM%d", i)" for jDoom
        R_CachePatch(&markerPatches[i], namebuf);
    }
#endif

    if(autopageLumpNum != -1)
        autopageLumpNum = W_CheckNumForName("AUTOPAGE");

    if(numTexUnits > 1)
    {   // Great, we can replicate the map fade out effect using multitexture,
        // load the mask texture.
        if(!amMaskTexture && !Get(DD_NOVIDEO))
        {
            amMaskTexture =
                GL_NewTextureWithParams3(DGL_LUMINANCE, 256, 256,
                                         W_CacheLumpName("mapmask", PU_CACHE),
                                         0x8, DGL_NEAREST, DGL_LINEAR,
                                         0 /*no anisotropy*/,
                                         DGL_REPEAT, DGL_REPEAT);
        }
    }
}

/**
 * Unload any resources needed for drawing the automap.
 * Called during shutdown and before a renderer restart.
 */
void Rend_AutomapUnloadData(void)
{
    int                 i;

    if(Get(DD_NOVIDEO) || IS_DEDICATED)
        return; // Nothing to do.

    // Destroy all display lists.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        rautomap_data_t*    rmap = &rautomaps[i];

        deleteMapLists(rmap);
        rmap->constructMap = true;
    }

    for(i = 0; i < NUM_VECTOR_GRAPHS; ++i)
    {
        vectorgrap_t*       vgraph = AM_GetVectorGraph(i);

        if(vgraph->dlist)
            DGL_DeleteLists(vgraph->dlist, 1);
        vgraph->dlist = 0;
    }

    if(amMaskTexture)
        DGL_DeleteTextures(1, (DGLuint*) &amMaskTexture);
    amMaskTexture = 0;
}

/**
 * Called immediately after map load.
 */
void Rend_AutomapInitForMap(void)
{
    uint                i;

    if(Get(DD_NOVIDEO) || IS_DEDICATED)
        return; // Nothing to do.

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        rautomap_data_t*    rmap = &rautomaps[i];

        deleteMapLists(rmap);
        rmap->constructMap = true;
    }
}

/**
 * Draws the given line including any optional extras.
 */
static void rendLine2(const automap_t* map, const automapcfg_t* mcfg,
                      float x1, float y1, float x2,
                      float y2, const float color[4], glowtype_t glowType,
                      float glowAlpha, float glowWidth,
                      boolean glowOnly, boolean scaleGlowWithView,
                      boolean caps, blendmode_t blend,
                      boolean drawNormal, boolean addToLists)
{
    float               a[2], b[2];
    float               length, dx, dy;
    float               normal[2], unit[2];

    // Scale into map, screen space units.
    a[VX] = x1;
    a[VY] = y1;
    b[VX] = x2;
    b[VY] = y2;

    dx = b[VX] - a[VX];
    dy = b[VY] - a[VY];
    length = sqrt(dx * dx + dy * dy);
    if(length <= 0)
        return;

    unit[VX] = dx / length;
    unit[VY] = dy / length;
    normal[VX] = unit[VY];
    normal[VY] = -unit[VX];

    // Is this a glowing line?
    if(glowType != NO_GLOW)
    {
        int                 tex;
        float               thickness;

        // Scale line thickness relative to zoom level?
        if(scaleGlowWithView)
            thickness = mcfg->lineGlowScale * 2.5f + 3;
        else
            thickness = glowWidth;

        tex = Get(DD_DYNLIGHT_TEXTURE);

        if(caps)
        {   // Draw a "cap" at the start of the line.
            float               v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] - unit[VX] * thickness + normal[VX] * thickness;
            v1[VY] = a[VY] - unit[VY] * thickness + normal[VY] * thickness;
            v2[VX] = a[VX] + normal[VX] * thickness;
            v2[VY] = a[VY] + normal[VY] * thickness;
            v3[VX] = a[VX] - normal[VX] * thickness;
            v3[VY] = a[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - unit[VX] * thickness - normal[VX] * thickness;
            v4[VY] = a[VY] - unit[VY] * thickness - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2],
                            glowAlpha * Automap_GetOpacity(map));
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, 0, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, 0, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
        }

        // The middle part of the line.
        switch(glowType)
        {
        case TWOSIDED_GLOW:
            {
            float               v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] + normal[VX] * thickness;
            v1[VY] = a[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + normal[VX] * thickness;
            v2[VY] = b[VY] + normal[VY] * thickness;
            v3[VX] = b[VX] - normal[VX] * thickness;
            v3[VY] = b[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - normal[VX] * thickness;
            v4[VY] = a[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2],
                            glowAlpha * Automap_GetOpacity(map));
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        case BACK_GLOW:
            {
            float               v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX] + normal[VX] * thickness;
            v1[VY] = a[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + normal[VX] * thickness;
            v2[VY] = b[VY] + normal[VY] * thickness;
            v3[VX] = b[VX];
            v3[VY] = b[VY];
            v4[VX] = a[VX];
            v4[VY] = a[VY];

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2],
                            glowAlpha * Automap_GetOpacity(map));
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        case FRONT_GLOW:
            {
            float               v1[2], v2[2], v3[2], v4[2];

            v1[VX] = a[VX];
            v1[VY] = a[VY];
            v2[VX] = b[VX];
            v2[VY] = b[VY];
            v3[VX] = b[VX] - normal[VX] * thickness;
            v3[VY] = b[VY] - normal[VY] * thickness;
            v4[VX] = a[VX] - normal[VX] * thickness;
            v4[VY] = a[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2],
                            glowAlpha * Automap_GetOpacity(map));
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
            }
            break;

        default:
            break; // Impossible.
        }

        if(caps)
        {
            float               v1[2], v2[2], v3[2], v4[2];

            v1[VX] = b[VX] + normal[VX] * thickness;
            v1[VY] = b[VY] + normal[VY] * thickness;
            v2[VX] = b[VX] + unit[VX] * thickness + normal[VX] * thickness;
            v2[VY] = b[VY] + unit[VY] * thickness + normal[VY] * thickness;
            v3[VX] = b[VX] + unit[VX] * thickness - normal[VX] * thickness;
            v3[VY] = b[VY] + unit[VY] * thickness - normal[VY] * thickness;
            v4[VX] = b[VX] - normal[VX] * thickness;
            v4[VY] = b[VY] - normal[VY] * thickness;

            if(!addToLists)
            {
                DGL_Bind(tex);

                DGL_Color4f(color[0], color[1], color[2],
                            glowAlpha * Automap_GetOpacity(map));
                DGL_BlendMode(blend);
            }

            DGL_Begin(DGL_QUADS);
                // V1
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                // V2
                DGL_TexCoord2f(0, 1, 0);
                DGL_TexCoord2f(1, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);

                // V3
                DGL_TexCoord2f(0, 1, 1);
                DGL_TexCoord2f(1, v3[VX], v3[VY]);
                DGL_Vertex2f(v3[VX], v3[VY]);

                // V4
                DGL_TexCoord2f(0, .5, 1);
                DGL_TexCoord2f(1, v4[VX], v4[VY]);
                DGL_Vertex2f(v4[VX], v4[VY]);
            DGL_End();

            if(!addToLists)
                DGL_BlendMode(BM_NORMAL);
        }
    }

    if(!glowOnly)
    {
        if(!addToLists)
        {
            DGL_Color4f(color[0], color[1], color[2],
                        color[3] * Automap_GetOpacity(map));
            DGL_BlendMode(blend);
        }

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, a[VX], a[VY]);
            DGL_Vertex2f(a[VX], a[VY]);
            DGL_TexCoord2f(0, b[VX], b[VY]);
            DGL_Vertex2f(b[VX], b[VY]);
        DGL_End();

        if(!addToLists)
            DGL_BlendMode(BM_NORMAL);
    }

    if(drawNormal)
    {
#define NORMTAIL_LENGTH         8

        float               center[2];

        center[VX] = a[VX] + (length / 2) * unit[VX];
        center[VY] = a[VY] + (length / 2) * unit[VY];

        a[VX] = center[VX];
        a[VY] = center[VY];
        b[VX] = center[VX] + normal[VX] * NORMTAIL_LENGTH;
        b[VY] = center[VY] + normal[VY] * NORMTAIL_LENGTH;

        if(!addToLists)
        {
            DGL_Color4f(color[0], color[1], color[2],
                        color[3] * Automap_GetOpacity(map));
            DGL_BlendMode(blend);
        }

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, a[VX], a[VY]);
            DGL_Vertex2f(a[VX], a[VY]);
            DGL_TexCoord2f(0, b[VX], b[VY]);
            DGL_Vertex2f(b[VX], b[VY]);
        DGL_End();

        if(!addToLists)
            DGL_BlendMode(BM_NORMAL);

#undef NORMTAIL_LENGTH
    }
}

int Rend_AutomapSeg(void* obj, void* data)
{
    hedge_t*            seg = (hedge_t*) obj;
    rendwallseg_params_t* p = (rendwallseg_params_t*) data;
    float               v1[2], v2[2];
    linedef_t*          line;
    xline_t*            xLine;
    sector_t*           frontSector, *backSector;
    const mapobjectinfo_t* info;
    player_t*           plr = p->plr;
    automapid_t         id;

    line = DMU_GetPtrp(seg, DMU_LINEDEF);
    if(!line)
        return 1;

    xLine = P_ToXLine(line);
    if(xLine->validCount == VALIDCOUNT)
        return 1; // Already drawn once.

    if((xLine->flags & ML_DONTDRAW) &&
       !(p->map->flags & AMF_REND_ALLLINES))
        return 1;

    frontSector = DMU_GetPtrp(line, DMU_FRONT_SECTOR);
    if(frontSector != DMU_GetPtrp(line, DMU_SIDEDEF0_OF_LINE | DMU_SECTOR))
        return 1; // We only want to draw twosided lines once.

    id = AM_MapForPlayer(plr - players);
    info = NULL;
    if((p->map->flags & AMF_REND_ALLLINES) ||
       xLine->mapped[plr - players])
    {
        backSector = DMU_GetPtrp(line, DMU_BACK_SECTOR);

        // Perhaps this is a specially colored line?
        info = AM_GetInfoForSpecialLine(id, xLine->special, frontSector,
                                        backSector);
        if(p->objType != -1 && !info)
        {   // Perhaps a default colored line?
            if(!(frontSector && backSector) || (xLine->flags & ML_SECRET))
            {
                // solid wall (well probably anyway...)
                info = AM_GetMapObjectInfo(id, AMO_SINGLESIDEDLINE);
            }
            else
            {
                if(DMU_GetFloatp(backSector, DMU_FLOOR_HEIGHT) !=
                   DMU_GetFloatp(frontSector, DMU_FLOOR_HEIGHT))
                {
                    // Floor level change.
                    info = AM_GetMapObjectInfo(id, AMO_FLOORCHANGELINE);
                }
                else if(DMU_GetFloatp(backSector, DMU_CEILING_HEIGHT) !=
                        DMU_GetFloatp(frontSector, DMU_CEILING_HEIGHT))
                {
                    // Ceiling level change.
                    info = AM_GetMapObjectInfo(id, AMO_CEILINGCHANGELINE);
                }
                else if(p->map->flags & AMF_REND_ALLLINES)
                {
                    info = AM_GetMapObjectInfo(id, AMO_UNSEENLINE);
                }
            }
        }
    }
    else if(p->objType != -1 && p->cfg->revealed)
    {
        if(!(xLine->flags & ML_DONTDRAW))
        {
            // An as yet, unseen line.
            info = AM_GetMapObjectInfo(id, AMO_UNSEENLINE);
        }
    }

    if(info && (p->objType == -1 ||
                info == &p->cfg->mapObjectInfo[p->objType]))
    {
        DMU_GetFloatpv(DMU_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        DMU_GetFloatpv(DMU_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        rendLine2(p->map, p->cfg, v1[VX], v1[VY], v2[VX], v2[VY],
                  info->rgba,
                  (xLine->special && !p->cfg->glowingLineSpecials ?
                        NO_GLOW : info->glow),
                  info->glowAlpha,
                  info->glowWidth, !p->addToLists, info->scaleWithView,
                  (info->glow && !(xLine->special && !p->cfg->glowingLineSpecials)),
                  (xLine->special && !p->cfg->glowingLineSpecials ?
                        BM_NORMAL : info->blendMode),
                  (p->map->flags & AMF_REND_LINE_NORMALS),
                  p->addToLists);

        xLine->validCount = VALIDCOUNT; // Mark as drawn this frame.
    }

    return 1; // Continue iteration.
}

static boolean drawSegsOfSubsector(face_t* ssec, void* context)
{
    return DMU_Iteratep(ssec, DMU_HEDGE, Rend_AutomapSeg, context);
}

/**
 * Determines visible lines, draws them.
 *
 * @params objType      Type of map object being drawn.
 */
static void renderWalls(const automap_t* map, const automapcfg_t* cfg,
                        int player, int objType, boolean addToLists)
{
    uint                i;
    rendwallseg_params_t params;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    params.plr = &players[player];
    params.map = map;
    params.cfg = cfg;
    params.objType = objType;
    params.addToLists = addToLists;

    // Can we use the automap's in-view bounding box to cull out of view
    // objects?
    if(!addToLists)
    {
        float               aabb[4];

        Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                              &aabb[BOXBOTTOM], &aabb[BOXTOP]);
        DMU_SubSectorsBoxIterator(aabb, NULL, drawSegsOfSubsector, &params);
    }
    else
    {   // No. As the map lists are considered static we want them to
        // contain all walls, not just those visible *now*.
        for(i = 0; i < numsubsectors; ++i)
        {
            DMU_Iteratep(DMU_ToPtr(DMU_FACE, i), DMU_HEDGE, Rend_AutomapSeg, &params);
        }
    }
}

static void renderLinedef(linedef_t* line, float r, float g, float b,
                          float a, blendmode_t blendMode,
                          boolean drawNormal)
{
    float               length = DMU_GetFloatp(line, DMU_LENGTH);

    if(length > 0)
    {
        float               v1[2], v2[2];

        DMU_GetFloatpv(DMU_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
        DMU_GetFloatpv(DMU_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

        DGL_BlendMode(blendMode);
        DGL_Color4f(r, g, b, a);

        DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, v1[VX], v1[VY]);
            DGL_Vertex2f(v1[VX], v1[VY]);

            DGL_TexCoord2f(0, v2[VX], v2[VY]);
            DGL_Vertex2f(v2[VX], v2[VY]);
        DGL_End();

        if(drawNormal)
        {
#define NORMTAIL_LENGTH         8

            float               normal[2], unit[2], d1[2];

            DMU_GetFloatpv(line, DMU_DXY, d1);

            unit[VX] = d1[0] / length;
            unit[VY] = d1[1] / length;
            normal[VX] = unit[VY];
            normal[VY] = -unit[VX];

            // The center of the linedef.
            v1[VX] += (length / 2) * unit[VX];
            v1[VY] += (length / 2) * unit[VY];

            // Outside point.
            v2[VX] = v1[VX] + normal[VX] * NORMTAIL_LENGTH;
            v2[VY] = v1[VY] + normal[VY] * NORMTAIL_LENGTH;

            DGL_Begin(DGL_LINES);
                DGL_TexCoord2f(0, v1[VX], v1[VY]);
                DGL_Vertex2f(v1[VX], v1[VY]);

                DGL_TexCoord2f(0, v2[VX], v2[VY]);
                DGL_Vertex2f(v2[VX], v2[VY]);
            DGL_End();
#undef NORMTAIL_LENGTH
        }

        DGL_BlendMode(BM_NORMAL);
    }
}

int renderPolyObjLinedef(void* obj, void* context)
{
    rendwallseg_params_t* p = (rendwallseg_params_t*) context;
    linedef_t*          line = (linedef_t*) obj;
    xline_t*            xLine = P_ToXLine(line);
    const mapobjectinfo_t* info;
    automapobjectname_t amo;

    if(xLine->validCount == VALIDCOUNT)
        return 1; // Already processed this frame.

    if((xLine->flags & ML_DONTDRAW) && !(p->map->flags & AMF_REND_ALLLINES))
        return 1;

    amo = AMO_NONE;
    if((p->map->flags & AMF_REND_ALLLINES) ||
       xLine->mapped[p->plr - players])
    {
        amo = AMO_SINGLESIDEDLINE;
    }
    else if(p->map->flags && !(xLine->flags & ML_DONTDRAW))
    {   // An as yet, unseen line.
        amo = AMO_UNSEENLINE;
    }

    if((info = AM_GetMapObjectInfo(AM_MapForPlayer(p->plr - players), amo)))
    {
        renderLinedef(line, info->rgba[0], info->rgba[1], info->rgba[2],
                      info->rgba[3] * cfg.automapLineAlpha * Automap_GetOpacity(p->map),
                      info->blendMode,
                      (p->map->flags & AMF_REND_LINE_NORMALS)? true : false);
    }

    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return 1; // Continue iteration.
}

boolean drawLinedefsOfPolyobject(polyobj_t* po, void* context)
{
    linedef_t**         linePtr;
    int                 result = 1;

    linePtr = po->lineDefs;
    while(*linePtr && (result = renderPolyObjLinedef(*linePtr, context)) != 0)
        *linePtr++;

    return result;
}

static void renderPolyObjs(const automap_t* map, const automapcfg_t* cfg,
                           int player)
{
    float               aabb[4];
    rendwallseg_params_t params;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    params.plr = &players[player];
    params.map = map;
    params.cfg = cfg;
    params.objType = MOL_LINEDEF;

    // Next, draw any polyobjects in view.
    Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                          &aabb[BOXBOTTOM], &aabb[BOXTOP]);
    P_PolyobjsBoxIterator(aabb, drawLinedefsOfPolyobject, &params);
}

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean renderXGLinedef(linedef_t* line, void* context)
{
    rendwallseg_params_t* p = (rendwallseg_params_t*) context;
    xline_t*            xLine;

    xLine = P_ToXLine(line);
    if(!xLine || xLine->validCount == VALIDCOUNT ||
        ((xLine->flags & ML_DONTDRAW) && !(p->map->flags & AMF_REND_ALLLINES)))
        return 1;

    // Show only active XG lines.
    if(!(xLine->xg && xLine->xg->active && (mapTime & 4)))
        return 1;

    renderLinedef(line, .8f, 0, .8f, 1, BM_ADD,
                  (p->map->flags & AMF_REND_LINE_NORMALS)? true : false);

    xLine->validCount = VALIDCOUNT; // Mark as processed this frame.

    return 1; // Continue iteration.
}
#endif

static void renderXGLinedefs(const automap_t* map, const automapcfg_t* cfg,
                             int player)
{
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    float               aabb[4];
    rendwallseg_params_t params;

    if(!(map->flags & AMF_REND_XGLINES))
        return;

    // VALIDCOUNT is used to track which lines have been drawn this frame.
    VALIDCOUNT++;

    // Set the vars used during iteration.
    params.plr = &players[player];
    params.map = map;
    params.cfg = cfg;
    params.addToLists = false;
    params.objType = -1;

    Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                          &aabb[BOXBOTTOM], &aabb[BOXTOP]);
    DMU_LinesBoxIterator(aabb, renderXGLinedef, &params);
#endif
}

static DGLuint constructLineCharacter(DGLuint name, const vectorgrap_t* vg)
{
    if(DGL_NewList(name, DGL_COMPILE))
    {
        uint                i;

        DGL_Begin(DGL_LINES);
        for(i = 0; i < vg->count; ++i)
        {
            vgline_t*           vgl = &vg->lines[i];

            DGL_TexCoord2f(0, vgl->a.pos[VX], vgl->a.pos[VY]);
            DGL_Vertex2f(vgl->a.pos[VX], vgl->a.pos[VY]);
            DGL_TexCoord2f(0, vgl->b.pos[VX], vgl->b.pos[VY]);
            DGL_Vertex2f(vgl->b.pos[VX], vgl->b.pos[VY]);
        }
        DGL_End();

        return DGL_EndList();
    }

    return 0;
}

/**
 * Draws a line character (eg the player arrow)
 */
static void renderLineCharacter(vectorgrap_t* vg, float x, float y,
                                float angle, float scale,
                                const float rgb[3], float alpha,
                                blendmode_t blendmode)
{
    if(!vg->dlist)
        vg->dlist = constructLineCharacter(0, vg);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();

    DGL_Translatef(x, y, 1);
    DGL_Rotatef(angle, 0, 0, 1);
    DGL_Scalef(scale, scale, 1);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 1);

    DGL_Color4f(rgb[0], rgb[1], rgb[2], alpha);
    DGL_BlendMode(blendmode);

    DGL_CallList(vg->dlist);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

/**
 * Draws all players on the map using a line character
 */
static void renderPlayers(const automap_t* map, const automapcfg_t* mcfg,
                          int player)
{
    int                 i;
    float               size = PLAYERRADIUS;
    vectorgrap_t*       vg;

    vg = AM_GetVectorGraph(AM_GetVectorGraphic(mcfg, AMO_THINGPLAYER));
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           p = &players[i];
        float               rgb[3], alpha;
        mobj_t*             mo;

        if(!p->plr->inGame)
            continue;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if(deathmatch && p != &players[player])
            continue;
#endif

        R_GetColorPaletteRGBf(0, rgb, (!IS_NETGAME? WHITE :
            their_colors[cfg.playerColor[i]]), false);
        alpha = cfg.automapLineAlpha;
#if !__JHEXEN__
        if(p->powers[PT_INVISIBILITY])
            alpha *= .125f;
#endif
        alpha = MINMAX_OF(0.f, alpha * Automap_GetOpacity(map), 1.f);

        mo = p->plr->mo;

        /* $unifiedangles */
        renderLineCharacter(vg, mo->pos[VX], mo->pos[VY],
                            mo->angle / (float) ANGLE_MAX * 360, size,
                            rgb, alpha, BM_NORMAL);
    }
}

static int getKeyColorForMobjType(int type)
{
    struct keycolor_s {
        int             moType;
        int             color;
    } keyColors[] =
    {
#if __JDOOM__ || __JDOOM64__
        {MT_MISC4, KEY1_COLOR},
        {MT_MISC5, KEY2_COLOR},
        {MT_MISC6, KEY3_COLOR},
        {MT_MISC7, KEY4_COLOR},
        {MT_MISC8, KEY5_COLOR},
        {MT_MISC9, KEY6_COLOR},
#elif __JHERETIC__
        {MT_CKEY, KEY1_COLOR},
        {MT_BKYY, KEY2_COLOR},
        {MT_AKYY, KEY3_COLOR},
#endif
        {-1, -1} // Terminate.
    };
    uint                i;

    for(i = 0; keyColors[i].moType != -1; ++i)
        if(keyColors[i].moType == type)
            return keyColors[i].color;

    return -1; // Not a key.
}

typedef struct {
    int             flags; // AMF_* flags.
    vectorgrap_t*   vgraph;
    float           rgb[3], alpha;
} renderthing_params_t;

/**
 * Draws all things on the map
 */
static boolean renderThing(mobj_t* mo, void* context)
{
    renderthing_params_t* p = (renderthing_params_t*) context;

    if(p->flags & AMF_REND_KEYS)
    {
        int                 keyColor;

        // Is this a key?
        if((keyColor = getKeyColorForMobjType(mo->type)) != -1)
        {   // This mobj is indeed a key.
            float               rgb[4];

            R_GetColorPaletteRGBf(0, rgb, keyColor, false);

            /* $unifiedangles */
            renderLineCharacter(AM_GetVectorGraph(VG_KEYSQUARE),
                                mo->pos[VX], mo->pos[VY], 0,
                                PLAYERRADIUS, rgb, p->alpha, BM_NORMAL);
            return true; // Continue iteration.
        }
    }

    if(p->flags & AMF_REND_THINGS)
    {   // Something else.
        /* $unifiedangles */
        renderLineCharacter(p->vgraph, mo->pos[VX], mo->pos[VY],
                            mo->angle / (float) ANGLE_MAX * 360,
                            PLAYERRADIUS, p->rgb, p->alpha, BM_NORMAL);
    }

    return true; // Continue iteration.
}

/**
 * Draws all the points marked by the player.
 */
static void drawMarks(const automap_t* map)
{
#if !__JDOOM64__
    float               x, y, w, h;
    dpatch_t*           patch;
    float               scrwidth = Get(DD_WINDOW_WIDTH);
    float               scrheight = Get(DD_WINDOW_HEIGHT);
    float               aabb[4];
    unsigned int        i, numMarks;

    Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                          &aabb[BOXBOTTOM], &aabb[BOXTOP]);
    numMarks = Automap_GetNumMarks(map);
    for(i = 0; i < numMarks; ++i)
    {
        if(Automap_GetMark(map, i, &x, &y, NULL) &&
           !(x < aabb[BOXLEFT]   || x > aabb[BOXRIGHT] ||
             y < aabb[BOXBOTTOM] || y > aabb[BOXTOP]))
        {
            patch = &markerPatches[i];

            w = Automap_FrameToMap(map, FIXXTOSCREENX(patch->width));
            h = Automap_FrameToMap(map, FIXYTOSCREENY(patch->height));

            DGL_SetPatch(patch->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Color4f(1, 1, 1, Automap_GetOpacity(map));

            DGL_MatrixMode(DGL_PROJECTION);
            DGL_PushMatrix();
            DGL_Translatef(x, y, 0);
            DGL_Rotatef(Automap_GetViewAngle(map), 0, 0, 1);

            DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, 0);
                DGL_Vertex2f(-(w / 2), h / 2);

                DGL_TexCoord2f(0, 1, 0);
                DGL_Vertex2f(w / 2, h / 2);

                DGL_TexCoord2f(0, 1, 1);
                DGL_Vertex2f(w / 2, -(h / 2));

                DGL_TexCoord2f(0, 0, 1);
                DGL_Vertex2f(-(w / 2), -(h / 2));
            DGL_End();

            DGL_MatrixMode(DGL_PROJECTION);
            DGL_PopMatrix();
        }
    }
#endif
}

/**
 * Sets up the state for automap drawing.
 */
static void setupGLStateForMap(const automap_t* map,
                               const automapcfg_t* mcfg, int player)
{
    float               wx, wy, ww, wh, angle, plx, ply;
    float               scrwidth = Get(DD_WINDOW_WIDTH);
    float               scrheight = Get(DD_WINDOW_HEIGHT);
    rautomap_data_t*    rmap = &rautomaps[AM_MapForPlayer(player)-1];

    Automap_GetWindow(map, &wx, &wy, &ww, &wh);
    Automap_GetViewParallaxPosition(map, &plx, &ply);
    angle = Automap_GetViewAngle(map);

    // Check for scissor box (to clip the map lines and stuff).
    // Store the old scissor state.
    DGL_GetIntegerv(DGL_SCISSOR_TEST, rmap->scissorState);
    DGL_GetIntegerv(DGL_SCISSOR_BOX, rmap->scissorState + 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, scrwidth, scrheight, -1, 1);

    // Do we want a background texture?
    if(autopageLumpNum != -1)
    {
        // Apply the background texture onto a parallaxing layer which
        // follows the map view target (not player).
        DGL_Enable(DGL_TEXTURING);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_LoadIdentity();

         // We only want the left portion.
        DGL_SetRawImage(autopageLumpNum, false, DGL_REPEAT, DGL_REPEAT);

        DGL_Color4f(mcfg->backgroundRGBA[0],
                    mcfg->backgroundRGBA[1],
                    mcfg->backgroundRGBA[2],
                    Automap_GetOpacity(map) * mcfg->backgroundRGBA[3]);

        // Scale from texture to window space
        DGL_Translatef(wx, wy, 0);

        // Apply the parallax scrolling, map rotation and counteract the
        // aspect of the quad (sized to map window dimensions).
        DGL_Translatef(Automap_MapToFrame(map, plx) + .5f,
                       Automap_MapToFrame(map, ply) + .5f, 0);
        DGL_Rotatef(angle, 0, 0, 1);
        DGL_Scalef(1, wh / ww, 1);
        DGL_Translatef(-(.5f), -(.5f), 0);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(wx, wy);
            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(wx + ww, wy);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(wx + ww, wy + wh);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(wx, wy + wh);
        DGL_End();

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();

        DGL_MatrixMode(DGL_PROJECTION);
    }
    else
    {
        // Nope just a solid color.
        DGL_SetNoMaterial();
        DGL_DrawRect(wx, wy, ww, wh,
                     mcfg->backgroundRGBA[0],
                     mcfg->backgroundRGBA[1],
                     mcfg->backgroundRGBA[2],
                     Automap_GetOpacity(map) * mcfg->backgroundRGBA[3]);
    }

#if __JDOOM64__
    // jd64 > Demon keys
    // If drawn in HUD we don't need them visible in the map too.
    if(!cfg.hudShown[HUD_INVENTORY])
    {
        int                 i, num = 0;
        player_t*           plr = &players[player];
        inventoryitemtype_t items[3] = {
            IIT_DEMONKEY1,
            IIT_DEMONKEY2,
            IIT_DEMONKEY3
        };

        for(i = 0; i < 3; ++i)
            num += P_InventoryCount(player, items[i])? 1 : 0;

        if(num > 0)
        {
            float               x, y, w, h, spacing, scale, iconAlpha;
            spriteinfo_t        sprInfo;
            int                 invItemSprites[NUM_INVENTORYITEM_TYPES] = {
                SPR_ART1, SPR_ART2, SPR_ART3
            };

            iconAlpha = MINMAX_OF(.0f, Automap_GetOpacity(map), .5f);

            spacing = wh / num;
            y = 0;

            for(i = 0; i < 3; ++i)
            {
                if(P_InventoryCount(player, items[i]))
                {
                    R_GetSpriteInfo(invItemSprites[i], 0, &sprInfo);
                    DGL_SetPSprite(sprInfo.material);

                    scale = wh / (sprInfo.height * num);
                    x = ww - sprInfo.width * scale;
                    w = sprInfo.width;
                    h = sprInfo.height;

                    {
                    float               s, t;

                    // Let's calculate texture coordinates.
                    // To remove a possible edge artifact, move the corner a bit up/left.
                    s = (w - 0.4f) / M_CeilPow2(w);
                    t = (h - 0.4f) / M_CeilPow2(h);

                    DGL_Color4f(1, 1, 1, iconAlpha);
                    DGL_Begin(DGL_QUADS);
                        DGL_TexCoord2f(0, 0, 0);
                        DGL_Vertex2f(x, y);

                        DGL_TexCoord2f(0, s, 0);
                        DGL_Vertex2f(x + w * scale, y);

                        DGL_TexCoord2f(0, s, t);
                        DGL_Vertex2f(x + w * scale, y + h * scale);

                        DGL_TexCoord2f(0, 0, t);
                        DGL_Vertex2f(x, y + h * scale);
                    DGL_End();
                    }

                    y += spacing;
                }
            }
        }
    }
    // < d64tc
#endif

    // Setup the scissor clipper.
    DGL_Scissor(wx, wy, ww, wh);
    DGL_Enable(DGL_SCISSOR_TEST);
}

/**
 * Restores the previous gl draw state
 */
static void restoreGLStateFromMap(rautomap_data_t* rmap)
{
    // Return to the normal GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(!rmap->scissorState[0])
        DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Scissor(rmap->scissorState[1], rmap->scissorState[2],
                rmap->scissorState[3], rmap->scissorState[4]);
}

static void drawMapName(float x, float y, float alpha, dpatch_t* patch,
                        const char* lname)
{
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

    Draw_BeginZoom(.4f, x, y);
    y -= 24; // border
    WI_DrawPatch(x, y, 1, 1, 1, alpha, patch, lname, false, ALIGN_CENTER);
    Draw_EndZoom();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

/**
 * Draws the map name into the automap window
 */
static void renderMapName(const automap_t* map)
{
    float               x, y, otherY;
    char*               lname;
    dpatch_t*           patch = NULL;
#if __JDOOM__ || __JDOOM64__
    int                 mapNum;
#endif

    lname = P_GetMapNiceName();

    if(lname)
    {
        float               wx, wy, ww, wh;
        float               scrwidth = Get(DD_WINDOW_WIDTH);
        float               scrheight = Get(DD_WINDOW_HEIGHT);

        // Compose the mapnumber used to check the map name patches array.
#if __JDOOM64__
        mapNum = gameMap -1;
        patch = &mapNamePatches[mapNum];
#elif __JDOOM__
        if(gameMode == commercial)
            mapNum = gameMap -1;
        else
            mapNum = ((gameEpisode -1) * 9) + gameMap -1;

        patch = &mapNamePatches[mapNum];
#endif

        Automap_GetWindow(map, &wx, &wy, &ww, &wh);

        x = SCREENXTOFIXX(wx + (ww * .5f));
        y = SCREENYTOFIXY(wy + wh);
        if(cfg.setBlocks < 13)
        {
#if !__JDOOM64__
            if(cfg.setBlocks <= 11 || cfg.automapHudDisplay == 2)
            {   // We may need to adjust for the height of the statusbar
                otherY = ST_Y;
                otherY += ST_HEIGHT * (1 - (cfg.statusbarScale / 20.0f));

                if(y > otherY)
                    y = otherY;
            }
            else if(cfg.setBlocks == 12)
#endif
            {   // We may need to adjust for the height of the HUD icons.
                otherY = y;
                otherY += -(y * (cfg.hudScale / 10.0f));

                if(y > otherY)
                    y = otherY;
            }
        }

        drawMapName(x, y, Automap_GetOpacity(map), patch, lname);
    }
}

static void renderVertexes(float alpha)
{
    uint                i;
    float               v[2], oldPointSize;

    DGL_Color4f(.2f, .5f, 1, alpha);

    DGL_Enable(DGL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 4);

    DGL_Begin(DGL_POINTS);
    for(i = 0; i < numvertexes; ++i)
    {
        DMU_GetFloatv(DMU_VERTEX, i, DMU_XY, v);
        DGL_TexCoord2f(0, v[VX], v[VY]);
        DGL_Vertex2f(v[VX], v[VY]);
    }
    DGL_End();

    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    DGL_Disable(DGL_POINT_SMOOTH);
}

/**
 * Compile OpenGL commands for drawing the map objects into display lists.
 */
static void compileObjectLists(rautomap_data_t* rmap, const automap_t* map,
                               const automapcfg_t* cfg, int player)
{
    uint                i;

    deleteMapLists(rmap);

    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        // Build commands and compile to a display list.
        if(DGL_NewList(0, DGL_COMPILE))
        {
            renderWalls(map, cfg, player, i, true);
            rmap->lists[i] = DGL_EndList();
        }
    }

    rmap->constructMap = false;
}

void Rend_AutomapRebuild(int player)
{
    automapid_t         map;

    if((map = AM_MapForPlayer(player)))
    {
        rautomap_data_t*    rmap = &rautomaps[map-1];

        rmap->constructMap = true;
    }
}

/**
 * Render the automap view window for the specified player.
 */
void Rend_Automap(int player, const automap_t* map)
{
    static int          updateWait = 0;

    uint                i;
    automapid_t         id = AM_MapForPlayer(player);
    player_t*           plr;
    float               wx, wy, ww, wh, vx, vy, angle, oldLineWidth, mtof;
    const automapcfg_t* mcfg;
    rautomap_data_t*    rmap;

    plr = &players[player];

    if(!(/*(plr->plr->flags & DDPF_LOCAL) && */ plr->plr->inGame))
        return;

    if(!(Automap_GetOpacity(map) > 0))
        return;

    mcfg = AM_GetMapConfig(id);
    rmap = &rautomaps[id-1];

    Automap_GetWindow(map, &wx, &wy, &ww, &wh);
    Automap_GetLocation(map, &vx, &vy);
    mtof = Automap_MapToFrameMultiplier(map);
    angle = Automap_GetViewAngle(map);

    // Freeze the lists if the map is fading out from being open or if set
    // to frozen for debug.
    if((++updateWait % 10) && rmap->constructMap && !freezeMapRLs)
    {   // Its time to rebuild the automap object display lists.
        compileObjectLists(rmap, map, mcfg, player);
    }

    // Setup for frame.
    setupGLStateForMap(map, mcfg, player);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_Translatef(wx + ww / 2, wy + wh / 2, 0);
    DGL_Rotatef(angle, 0, 0, 1);
    DGL_Scalef(1, -1, 1);
    DGL_Scalef(mtof, mtof, 1);
    DGL_Translatef(-vx, -vy, 0);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, AM_LINE_WIDTH);

    if(amMaskTexture)
    {
        DGL_Enable(DGL_TEXTURING);
        DGL_Bind(amMaskTexture);

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_LoadIdentity();

        DGL_PushMatrix();
        DGL_Scalef(1.f / ww, 1.f / wh, 1);
        DGL_Translatef(ww / 2, wh / 2, 0);
        DGL_Rotatef(-angle, 0, 0, 1);
        DGL_Scalef(mtof, mtof, 1);
        DGL_Translatef(-vx, -vy, 0);
    }

    // Draw static map geometry.
    for(i = 0; i < NUM_MAP_OBJECTLISTS; ++i)
    {
        if(rmap->lists[i])
        {
            const mapobjectinfo_t* info = &mcfg->mapObjectInfo[i];

            // Setup the global list state.
            DGL_Color4f(info->rgba[0], info->rgba[1], info->rgba[2],
                        info->rgba[3] * cfg.automapLineAlpha * Automap_GetOpacity(map));
            DGL_BlendMode(info->blendMode);

            // Draw.
            DGL_CallList(rmap->lists[i]);
        }
    }

    // Draw dynamic map geometry.
    renderXGLinedefs(map, mcfg, player);
    renderPolyObjs(map, mcfg, player);

    // Restore the previous state.
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    if(Automap_GetFlags(map) & AMF_REND_VERTEXES)
        renderVertexes(Automap_GetOpacity(map));

    // Draw the map objects:
    renderPlayers(map, mcfg, player);
    if(Automap_GetFlags(map) & (AMF_REND_THINGS|AMF_REND_KEYS))
    {
        float               aabb[4];
        renderthing_params_t params;

        params.flags = Automap_GetFlags(map);
        params.vgraph = AM_GetVectorGraph(AM_GetVectorGraphic(mcfg, AMO_THING));
        AM_GetMapColor(params.rgb, cfg.automapMobj, THINGCOLORS,
                       !W_IsFromIWAD(W_GetNumForName("PLAYPAL")));
        params.alpha = MINMAX_OF(0.f,
            cfg.automapLineAlpha * Automap_GetOpacity(map), 1.f);

        Automap_GetInViewAABB(map, &aabb[BOXLEFT], &aabb[BOXRIGHT],
                              &aabb[BOXBOTTOM], &aabb[BOXTOP]);
        VALIDCOUNT++;
        P_MobjsBoxIterator(aabb, renderThing, &params);
    }

    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

    if(amMaskTexture)
    {
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
        DGL_Bind(0);
    }

    // Draw glows.
    renderWalls(map, mcfg, player, -1, false);

    // Draw any marked points.
    drawMarks(map);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    renderMapName(map);

    restoreGLStateFromMap(rmap);
}

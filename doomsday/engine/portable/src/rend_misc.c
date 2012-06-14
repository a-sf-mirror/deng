/**
 * @file rend_misc.c
 * Miscellaneous renderer routines. @ingroup render
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include "materialvariant.h"

static DGLuint dlBBox = 0; // Display list: active-textured bbox model.

static void drawVector(const_pvec3f_t origin, const_pvec3f_t normal, float scalar, const float color[3])
{
    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * normal[VX], scalar * normal[VZ], scalar * normal[VY]);
        glColor3fv(color);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawSurfaceTangentSpaceVectors(Surface* suf, const_pvec3f_t origin)
{
#define VISUAL_LENGTH       (20)

    static const float red[3]   = { 1, 0, 0 };
    static const float green[3] = { 0, 1, 0 };
    static const float blue[3]  = { 0, 0, 1 };

    assert(origin && suf);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin[VX], origin[VZ], origin[VY]);

    if(devSurfaceVectors & SVF_TANGENT)   drawVector(origin, suf->tangent,   VISUAL_LENGTH, red);
    if(devSurfaceVectors & SVF_BITANGENT) drawVector(origin, suf->bitangent, VISUAL_LENGTH, green);
    if(devSurfaceVectors & SVF_NORMAL)    drawVector(origin, suf->normal,    VISUAL_LENGTH, blue);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

#undef VISUAL_LENGTH
}

void Rend_RenderSurfaceVectors(void)
{
    uint i;

    if(devSurfaceVectors == 0 || !theMap) return;

    glDisable(GL_CULL_FACE);

    for(i = 0; i < NUM_HEDGES; ++i)
    {
        HEdge* hedge = GameMap_HEdge(theMap, i);
        float x, y, bottom, top;
        Sector* backSec;
        LineDef* line;
        Surface* suf;
        vec3f_t origin;

        if(!hedge->lineDef || !hedge->sector ||
           (hedge->lineDef->inFlags & LF_POLYOBJ))
            continue;

        line = hedge->lineDef;
        x = hedge->HE_v1origin[VX] + (hedge->HE_v2origin[VX] - hedge->HE_v1origin[VX]) / 2;
        y = hedge->HE_v1origin[VY] + (hedge->HE_v2origin[VY] - hedge->HE_v1origin[VY]) / 2;

        backSec = HEDGE_BACK_SECTOR(hedge);
        if(!backSec)
        {
            bottom = hedge->sector->SP_floorvisheight;
            top = hedge->sector->SP_ceilvisheight;
            suf = &HEDGE_SIDEDEF(hedge)->SW_middlesurface;

            V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            SideDef* side = HEDGE_SIDEDEF(hedge);
            if(side->SW_middlesurface.material)
            {
                top = hedge->sector->SP_ceilvisheight;
                bottom = hedge->sector->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->SP_ceilvisheight <
               hedge->sector->SP_ceilvisheight &&
               !(Surface_IsSkyMasked(&hedge->sector->SP_ceilsurface) &&
                 Surface_IsSkyMasked(&backSec->SP_ceilsurface)))
            {
                bottom = backSec->SP_ceilvisheight;
                top = hedge->sector->SP_ceilvisheight;
                suf = &side->SW_topsurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->SP_floorvisheight >
               hedge->sector->SP_floorvisheight &&
               !(Surface_IsSkyMasked(&hedge->sector->SP_floorsurface) &&
                 Surface_IsSkyMasked(&backSec->SP_floorsurface)))
            {
                bottom = hedge->sector->SP_floorvisheight;
                top = backSec->SP_floorvisheight;
                suf = &side->SW_bottomsurface;

                V3f_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }
        }
    }

    for(i = 0; i < NUM_BSPLEAFS; ++i)
    {
        BspLeaf* bspLeaf = bspLeafs[i];
        uint j;

        if(!bspLeaf->sector) continue;

        for(j = 0; j < bspLeaf->sector->planeCount; ++j)
        {
            Plane* pln = bspLeaf->sector->SP_plane(j);
            vec3f_t origin;

            V3f_Set(origin, bspLeaf->midPoint[VX], bspLeaf->midPoint[VY], pln->visHeight);
            if(pln->type != PLN_MID && Surface_IsSkyMasked(&pln->surface))
                origin[VZ] = GameMap_SkyFix(theMap, pln->type == PLN_CEILING);

            drawSurfaceTangentSpaceVectors(&pln->surface, origin);
        }
    }

    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        const Polyobj* po = polyObjs[i];
        const Sector* sec = po->bspLeaf->sector;
        float zPos = sec->SP_floorheight + (sec->SP_ceilheight - sec->SP_floorheight)/2;
        vec3f_t origin;
        uint j;

        for(j = 0; j < po->lineCount; ++j)
        {
            LineDef* line = po->lines[j];

            V3f_Set(origin, (line->L_v2origin[VX] + line->L_v1origin[VX])/2,
                            (line->L_v2origin[VY] + line->L_v1origin[VY])/2, zPos);
            drawSurfaceTangentSpaceVectors(&line->L_frontsidedef->SW_middlesurface, origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void drawSoundOrigin(coord_t const origin[3], const char* label, coord_t const eye[3])
{
#define MAX_SOUNDORIGIN_DIST    384 ///< Maximum distance from origin to eye in map coordinates.

    const Point2Raw labelOrigin = { 2, 2 };
    coord_t dist;
    float alpha;

    if(!origin || !label || !eye) return;

    dist = V3d_Distance(origin, eye);
    alpha = 1.f - MIN_OF(dist, MAX_SOUNDORIGIN_DIST) / MAX_SOUNDORIGIN_DIST;

    if(alpha > 0)
    {
        float scale = dist / (Window_Width(theWindow) / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(origin[VX], origin[VZ], origin[VY]);
        glRotatef(-vang + 180, 0, 1, 0);
        glRotatef(vpitch, 1, 0, 0);
        glScalef(-scale, -scale, 1);

        UI_TextOutEx(label, &labelOrigin, UI_Color(UIC_TITLE), alpha);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

#undef MAX_SOUNDORIGIN_DIST
}

static int drawSideDefSoundOrigins(SideDef* side, void* parameters)
{
    uint idx = GameMap_SideDefIndex(theMap, side); /// @todo Do not assume current map.
    char buf[80];

    dd_snprintf(buf, 80, "Side #%i (middle)", idx);
    drawSoundOrigin(side->SW_middlesurface.base.origin, buf, parameters);

    dd_snprintf(buf, 80, "Side #%i (bottom)", idx);
    drawSoundOrigin(side->SW_bottomsurface.base.origin, buf, parameters);

    dd_snprintf(buf, 80, "Side #%i (top)", idx);
    drawSoundOrigin(side->SW_topsurface.base.origin, buf, parameters);
    return false; // Continue iteration.
}

static int drawSectorSoundOrigins(Sector* sec, void* parameters)
{
    uint idx = GameMap_SectorIndex(theMap, sec); /// @todo Do not assume current map.
    char buf[80];

    if(devSoundOrigins & SOF_PLANE)
    {
        uint i;
        for(i = 0; i < sec->planeCount; ++i)
        {
            Plane* pln = sec->SP_plane(i);
            dd_snprintf(buf, 80, "Sector #%i (pln:%i)", idx, i);
            drawSoundOrigin(pln->PS_base.origin, buf, parameters);
        }
    }

    if(devSoundOrigins & SOF_SECTOR)
    {
        dd_snprintf(buf, 80, "Sector #%i", idx);
        drawSoundOrigin(sec->base.origin, buf, parameters);
    }

    return false; // Continue iteration.
}

void Rend_RenderSoundOrigins(void)
{
    coord_t eye[3];

    if(!devSoundOrigins || !theMap) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    if(devSoundOrigins & SOF_SIDEDEF)
    {
        GameMap_SideDefIterator(theMap, drawSideDefSoundOrigins, eye);
    }

    if(devSoundOrigins & (SOF_SECTOR|SOF_PLANE))
    {
        GameMap_SectorIterator(theMap, drawSectorSoundOrigins, eye);
    }

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}

static void getVertexPlaneMinMax(const Vertex* vtx, coord_t* min, coord_t* max)
{
    lineowner_t* vo, *base;

    if(!vtx || (!min && !max))
        return; // Wha?

    vo = base = vtx->lineOwners;
    do
    {
        LineDef* li = vo->lineDef;

        if(li->L_frontsidedef)
        {
            if(min && li->L_frontsector->SP_floorvisheight < *min)
                *min = li->L_frontsector->SP_floorvisheight;

            if(max && li->L_frontsector->SP_ceilvisheight > *max)
                *max = li->L_frontsector->SP_ceilvisheight;
        }

        if(li->L_backsidedef)
        {
            if(min && li->L_backsector->SP_floorvisheight < *min)
                *min = li->L_backsector->SP_floorvisheight;

            if(max && li->L_backsector->SP_ceilvisheight > *max)
                *max = li->L_backsector->SP_ceilvisheight;
        }

        vo = vo->LO_next;
    } while(vo != base);
}

static void drawVertexPoint(const Vertex* vtx, coord_t z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->origin[VX], z, vtx->origin[VY]);
    glEnd();
}

static void drawVertexBar(const Vertex* vtx, coord_t bottom, coord_t top, float alpha)
{
#define EXTEND_DIST     64

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->origin[VX], bottom - EXTEND_DIST, vtx->origin[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->origin[VX], bottom, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], bottom, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], top, vtx->origin[VY]);
        glVertex3f(vtx->origin[VX], top, vtx->origin[VY]);
        glColor4fv(black);
        glVertex3f(vtx->origin[VX], top + EXTEND_DIST, vtx->origin[VY]);
    glEnd();

#undef EXTEND_DIST
}

static void drawVertexIndex(const Vertex* vtx, coord_t z, float scale, float alpha)
{
    const Point2Raw origin = { 2, 2 };
    char buf[80];

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    sprintf(buf, "%lu", (unsigned long) (vtx - vertexes));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->origin[VX], z, vtx->origin[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    glEnable(GL_TEXTURE_2D);

    UI_TextOutEx(buf, &origin, UI_Color(UIC_TITLE), alpha);

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

#define MAX_VERTEX_POINT_DIST 1280

static int drawVertex1(LineDef* li, void* context)
{
    Vertex* vtx = li->L_v1;
    Polyobj* po = context;
    coord_t dist2D = M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            coord_t bottom = po->bspLeaf->sector->SP_floorvisheight;
            coord_t top = po->bspLeaf->sector->SP_ceilvisheight;

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);
        }
    }

    if(devVertexIndices)
    {
        coord_t eye[3], pos[3], dist3D;

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        pos[VX] = vtx->origin[VX];
        pos[VY] = vtx->origin[VY];
        pos[VZ] = po->bspLeaf->sector->SP_floorvisheight;

        dist3D = V3d_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (Window_Width(theWindow) / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return false; // Continue iteration.
}

int drawPolyObjVertexes(Polyobj* po, void* context)
{
    return Polyobj_LineIterator(po, drawVertex1, po);
}

void Rend_RenderVertexes(void)
{
    float oldPointSize, oldLineWidth = 1;
    uint i;
    AABoxd box;

    if(!devVertexBars && !devVertexIndices) return;

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        for(i = 0; i < NUM_VERTEXES; ++i)
        {
            Vertex* vtx = &vertexes[i];
            float alpha;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            alpha = 1 - M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]) / MAX_VERTEX_POINT_DIST;
            alpha = MIN_OF(alpha, .15f);

            if(alpha > 0)
            {
                coord_t bottom, top;

                bottom = DDMAXFLOAT;
                top = DDMINFLOAT;
                getVertexPlaneMinMax(vtx, &bottom, &top);

                drawVertexBar(vtx, bottom, top, alpha);
            }
        }
    }

    // Always draw the vertex point nodes.
    glEnable(GL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    for(i = 0; i < NUM_VERTEXES; ++i)
    {
        Vertex* vtx = &vertexes[i];
        coord_t dist;

        if(!vtx->lineOwners)
            continue; // Not a linedef vertex.
        if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
            continue; // A polyobj linedef vertex.

        dist = M_ApproxDistance(vOrigin[VX] - vtx->origin[VX], vOrigin[VZ] - vtx->origin[VY]);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            coord_t bottom;

            bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &bottom, NULL);

            drawVertexPoint(vtx, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }


    if(devVertexIndices)
    {
        coord_t eye[3];

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        for(i = 0; i < NUM_VERTEXES; ++i)
        {
            Vertex* vtx = &vertexes[i];
            coord_t pos[3], dist;

            if(!vtx->lineOwners) continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ) continue; // A polyobj linedef vertex.

            pos[VX] = vtx->origin[VX];
            pos[VY] = vtx->origin[VY];
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &pos[VZ], NULL);

            dist = V3d_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float alpha, scale;

                alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                scale = dist / (Window_Width(theWindow) / 2);

                drawVertexIndex(vtx, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    box.minX = vOrigin[VX] - MAX_VERTEX_POINT_DIST;
    box.minY = vOrigin[VY] - MAX_VERTEX_POINT_DIST;
    box.maxX = vOrigin[VX] + MAX_VERTEX_POINT_DIST;
    box.maxY = vOrigin[VY] + MAX_VERTEX_POINT_DIST;
    P_PolyobjsBoxIterator(&box, drawPolyObjVertexes, NULL);

    // Restore previous state.
    if(devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
        glDisable(GL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    glDisable(GL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightModRange(void)
{
    GameMap* map = theMap;
    int j, mapAmbient;
    float f;

    if(novideo) return;

    memset(lightModRange, 0, sizeof(float) * 255);

    if(!map)
    {
        rAmbient = 0;
        return;
    }

    mapAmbient = GameMap_AmbientLightLevel(map);
    if(mapAmbient > ambientLight)
        rAmbient = mapAmbient;
    else
        rAmbient = ambientLight;

    for(j = 0; j < 255; ++j)
    {
        // Adjust the white point/dark point?
        f = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                f = (float) (255 - j) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                f = (float) -j * -lightRangeCompression;
            }
        }

        // Lower than the ambient limit?
        if(rAmbient != 0 && j+f <= rAmbient)
            f = rAmbient - j;

        // Clamp the result as a modifier to the light value (j).
        if((j+f) >= 255)
            f = 255 - j;
        else if((j+f) <= 0)
            f = -j;

        // Insert it into the matrix
        lightModRange[j] = f / 255.0f;
    }
}

float Rend_LightAdaptationDelta(float val)
{
    int clampedVal;

    clampedVal = ROUND(255.0f * val);
    if(clampedVal > 254)
        clampedVal = 254;
    else if(clampedVal < 0)
        clampedVal = 0;

    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(float* val)
{
    if(!val) return;
    *val += Rend_LightAdaptationDelta(*val);
}

/**
 * Draws the lightModRange (for debug)
 */
void R_DrawLightRange(void)
{
#define BLOCK_WIDTH             (1.0f)
#define BLOCK_HEIGHT            (BLOCK_WIDTH * 255.0f)
#define BORDER                  (20)

    ui_color_t color;
    float c, off;
    int i;

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    // Draw an outside border.
    glColor4f(1, 1, 0, 1);
    glBegin(GL_LINES);
        glVertex2f(-1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, -1);
    glEnd();

    glBegin(GL_QUADS);
    for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        off = lightModRange[i];

        glColor4f(c + off, c + off, c + off, 1);
        glVertex2f(i * BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, BLOCK_HEIGHT);
        glVertex2f(i * BLOCK_WIDTH, BLOCK_HEIGHT);
    }
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef BORDER
#undef BLOCK_HEIGHT
#undef BLOCK_WIDTH
}

static DGLuint constructBBox(DGLuint name, float br)
{
    if(GL_NewList(name, GL_COMPILE))
    {
        glBegin(GL_QUADS);
        {
            // Top
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
            // Bottom
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
            // Front
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
            // Back
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
            // Left
            glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
            // Right
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
        }
        glEnd();
        return GL_EndList();
    }
    return 0;
}

void Rend_ClearBoundingBoxGLData(void)
{
    if(dlBBox)
    {
        GL_DeleteLists(dlBBox, 1);
        dlBBox = 0;
    }
}

void Rend_RenderBoundingBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, boolean alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslated(pos[VX], pos[VZ] + h, pos[VY]);
    else
        glTranslated(pos[VX], pos[VZ], pos[VY]);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScaled(w - br - br, h - br - br, l - br - br);
    glColor4f(color[CR], color[CG], color[CB], alpha);

    GL_CallList(dlBBox);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Rend_RenderArrow(coord_t const pos[3], float a, float s, float const color[3], float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslated(pos[VX], pos[VZ], pos[VY]);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScalef(s, 0, s);

    glBegin(GL_TRIANGLES);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f( 1.0f, 1.0f,-1.0f);  // L

        glColor4f(color[0], color[1], color[2], alpha);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f,-1.0f);  // Point

        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static int drawMobjBBox(thinker_t* th, void* context)
{
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    mobj_t* mo = (mobj_t*) th;
    coord_t eye[3], size;
    float alpha;

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false; // Continue iteration.
    // Is it vissible?
    if(!(mo->bspLeaf && mo->bspLeaf->sector->frameFlags & SIF_VISIBLE))
        return false; // Continue iteration.

    V3d_Set(eye, vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    alpha = 1 - ((V3d_Distance(mo->origin, eye) / (Window_Width(theWindow)/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    size = mo->radius;
    Rend_RenderBoundingBox(mo->origin, size, size, mo->height/2, 0,
                  (mo->ddFlags & DDMF_MISSILE)? yellow :
                  (mo->ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f, true);

    Rend_RenderArrow(mo->origin, ((mo->angle + ANG45 + ANG90) / (float) ANGLE_MAX *-360), size*1.25,
                   (mo->ddFlags & DDMF_MISSILE)? yellow :
                   (mo->ddFlags & DDMF_SOLID)? green : red, alpha);
    return false; // Continue iteration.
}

void Rend_RenderBoundingBoxes(void)
{
    //static const float red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    const materialvariantspecification_t* spec;
    const materialsnapshot_t* ms;
    material_t* mat;
    coord_t eye[3];
    uint i;

    if(!devMobjBBox && !devPolyobjBBox)
        return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if(netGame) return;
#endif

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vOrigin[VX];
    eye[VY] = vOrigin[VZ];
    eye[VZ] = vOrigin[VY];

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":bbox"));
    spec = Materials_VariantSpecificationForContext(MC_SPRITE, 0, 0, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, -1, true, true, true, false);
    ms = Materials_Prepare(mat, spec, true);

    GL_BindTexture(MST(ms, MTU_PRIMARY));
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
        GameMap_IterateThinkers(theMap, gx.MobjThinker, 0x1, drawMobjBBox, NULL);

    if(devPolyobjBBox)
    for(i = 0; i < NUM_POLYOBJS; ++i)
    {
        const Polyobj* po = polyObjs[i];
        const Sector* sec = po->bspLeaf->sector;
        coord_t width  = (po->aaBox.maxX - po->aaBox.minX)/2;
        coord_t length = (po->aaBox.maxY - po->aaBox.minY)/2;
        coord_t height = (sec->SP_ceilheight - sec->SP_floorheight)/2;
        coord_t pos[3];
        float alpha;

        pos[VX] = po->aaBox.minX + width;
        pos[VY] = po->aaBox.minY + length;
        pos[VZ] = sec->SP_floorheight;

        alpha = 1 - ((V3d_Distance(pos, eye) / (Window_Width(theWindow)/2)) / 4);
        if(alpha < .25f)
            alpha = .25f; // Don't make them totally invisible.

        Rend_RenderBoundingBox(pos, width, length, height, 0, yellow, alpha, .08f, true);

        {uint j;
        for(j = 0; j < po->lineCount; ++j)
        {
            LineDef* line = po->lines[j];
            coord_t width  = (line->aaBox.maxX - line->aaBox.minX)/2;
            coord_t length = (line->aaBox.maxY - line->aaBox.minY)/2;
            coord_t pos[3];

            /** Draw a bounding box for the lineDef.
            pos[VX] = line->aaBox.minX + width;
            pos[VY] = line->aaBox.minY + length;
            pos[VZ] = sec->SP_floorheight;
            Rend_RenderBoundingBox(pos, width, length, height, 0, red, alpha, .08f, true);
            */

            pos[VX] = (line->L_v2origin[VX] + line->L_v1origin[VX])/2;
            pos[VY] = (line->L_v2origin[VY] + line->L_v1origin[VY])/2;
            pos[VZ] = sec->SP_floorheight;
            width = 0;
            length = line->length/2;

            Rend_RenderBoundingBox(pos, width, length, height, BANG2DEG(BANG_90 - line->angle), green, alpha, 0, true);
        }}
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

void Rend_LightVertex(ColorRawf* color, const rvertex_t* vtx, float lightLevel,
    const float* ambientColor)
{
    float dist = Rend_PointDist2D(vtx->pos);
    float lightVal = R_DistAttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += R_ExtraLightDelta();

    Rend_ApplyLightAdaptation(&lightVal);

    // Mix with the surface color.
    color->rgba[CR] = lightVal * ambientColor[CR];
    color->rgba[CG] = lightVal * ambientColor[CG];
    color->rgba[CB] = lightVal * ambientColor[CB];
}

void Rend_LightVertices(uint num, ColorRawf* colors, const rvertex_t* verts,
    float lightLevel, const float* ambientColor)
{
    uint i;
    for(i = 0; i < num; ++i)
    {
        Rend_LightVertex(colors+i, verts+i, lightLevel, ambientColor);
    }
}

void Rend_SetUniformColor(ColorRawf* colors, uint num, float grey)
{
    uint i;
    for(i = 0; i < num; ++i)
    {
        ColorRawf_SetUniformColor(&colors[i], grey);
    }
}

void Rend_SetAlpha(ColorRawf* colors, uint num, float alpha)
{
    uint i;
    for(i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float* color, float distance)
{
    ddplayer_t* ddpl = &viewPlayer->shared;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    // Check for torch.
    if(distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        int ll = 16 - ddpl->fixedColorMap;
        float d = (1024 - distance) / 1024.0f * ll / 15.0f;

        if(torchAdditive)
        {
            color[CR] += d * torchColor[CR];
            color[CG] += d * torchColor[CG];
            color[CB] += d * torchColor[CB];
        }
        else
        {
            color[CR] += d * ((color[CR] * torchColor[CR]) - color[CR]);
            color[CG] += d * ((color[CG] * torchColor[CG]) - color[CG]);
            color[CB] += d * ((color[CB] * torchColor[CB]) - color[CB]);
        }
    }
}

void Rend_VertexColorsApplyTorchLight(ColorRawf* colors, const rvertex_t* vertices,
    size_t numVertices)
{
    ddplayer_t* ddpl = &viewPlayer->shared;
    size_t i;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t* vtx = &vertices[i];
        ColorRawf* c = &colors[i];
        Rend_ApplyTorchLight(c->rgba, Rend_PointDist2D(vtx->pos));
    }
}

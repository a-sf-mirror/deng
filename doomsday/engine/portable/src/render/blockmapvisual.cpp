/**
 * @file blockmapvisual.cpp
 * Graphical Blockmap Visual. @ingroup render
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_ui.h"

#include <de/concurrency.h>
#include "gridmap.h"
#include "blockmap.h"
#include "blockmapvisual.h"

byte bmapShowDebug = 0; // 1 = mobjs, 2 = linedefs, 3 = BSP leafs, 4 = polyobjs.
float bmapDebugSize = 1.5f;

static int rendMobj(mobj_t* mo, void* /*parameters*/)
{
    if(mo->validCount != validCount)
    {
        vec2f_t start, end;
        V2f_Set(start, mo->origin[VX] - mo->radius, mo->origin[VY] - mo->radius);
        V2f_Set(end,   mo->origin[VX] + mo->radius, mo->origin[VY] + mo->radius);
        glVertex2f(start[VX], start[VY]);
        glVertex2f(  end[VX], start[VY]);
        glVertex2f(  end[VX],   end[VY]);
        glVertex2f(start[VX],   end[VY]);

        mo->validCount = validCount;
    }
    return false; // Continue iteration.
}

static int rendLineDef(LineDef* line, void* /*parameters*/)
{
    if(line->validCount != validCount)
    {
        glVertex2f(line->L_v1origin[VX], line->L_v1origin[VY]);
        glVertex2f(line->L_v2origin[VX], line->L_v2origin[VY]);

        line->validCount = validCount;
    }
    return false; // Continue iteration.
}

static int rendBspLeaf(BspLeaf* bspLeaf, void* /*parameters*/)
{
    if(bspLeaf->validCount != validCount)
    {
        const float scale = MAX_OF(bmapDebugSize, 1);
        const float width = (Window_Width(theWindow) / 16) / scale;
        float length, dx, dy, normal[2], unit[2];
        HEdge* hedge;
        vec2f_t start, end;

        if(bspLeaf->hedge)
        {
            hedge = bspLeaf->hedge;
            do
            {
                V2f_Set(start, hedge->HE_v1origin[VX], hedge->HE_v1origin[VY]);
                V2f_Set(end,   hedge->HE_v2origin[VX], hedge->HE_v2origin[VY]);

                glBegin(GL_LINES);
                    glVertex2fv(start);
                    glVertex2fv(end);
                glEnd();

                dx = end[VX] - start[VX];
                dy = end[VY] - start[VY];
                length = sqrt(dx * dx + dy * dy);
                if(length > 0)
                {
                    unit[VX] = dx / length;
                    unit[VY] = dy / length;
                    normal[VX] = -unit[VY];
                    normal[VY] = unit[VX];

                    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC), GL_LINEAR);
                    glEnable(GL_TEXTURE_2D);

                    GL_BlendOp(GL_FUNC_ADD);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                    glBegin(GL_QUADS);
                        glTexCoord2f(0.75f, 0.5f);
                        glVertex2fv(start);
                        glTexCoord2f(0.75f, 0.5f);
                        glVertex2fv(end);
                        glTexCoord2f(0.75f, 1);
                        glVertex2f(end[VX] - normal[VX] * width,
                                   end[VY] - normal[VY] * width);
                        glTexCoord2f(0.75f, 1);
                        glVertex2f(start[VX] - normal[VX] * width,
                                   start[VY] - normal[VY] * width);
                    glEnd();

                    glDisable(GL_TEXTURE_2D);
                    GL_BlendMode(BM_NORMAL);
                }

                // Draw the bounding box.
                V2f_Set(start, bspLeaf->aaBox.minX, bspLeaf->aaBox.minY);
                V2f_Set(end,   bspLeaf->aaBox.maxX, bspLeaf->aaBox.maxY);

                glBegin(GL_LINES);
                    glVertex2f(start[VX], start[VY]);
                    glVertex2f(  end[VX], start[VY]);
                    glVertex2f(  end[VX], start[VY]);
                    glVertex2f(  end[VX],   end[VY]);
                    glVertex2f(  end[VX],   end[VY]);
                    glVertex2f(start[VX],   end[VY]);
                    glVertex2f(start[VX],   end[VY]);
                    glVertex2f(start[VX], start[VY]);
                glEnd();
            } while((hedge = hedge->next) != bspLeaf->hedge);
        }

        bspLeaf->validCount = validCount;
    }
    return false; // Continue iteration.
}

int rendCellLineDefs(de::Blockmap* blockmap, uint const coords[2], void* parameters)
{
    glBegin(GL_LINES);
        blockmap->iterateCellObjects(coords, (int (*)(void*,void*)) rendLineDef, parameters);
    glEnd();
    return false; // Continue iteration.
}

int rendCellPolyobjLineDefs(void* object, void* parameters)
{
    return Polyobj_LineIterator((Polyobj*)object, rendLineDef, parameters);
}

int rendCellPolyobjs(de::Blockmap* blockmap, uint const coords[2], void* parameters)
{
    glBegin(GL_LINES);
        blockmap->iterateCellObjects(coords, (int (*)(void*,void*)) rendCellPolyobjLineDefs, parameters);
    glEnd();
    return false; // Continue iteration.
}

int rendCellMobjs(de::Blockmap* blockmap, uint const coords[2], void* parameters)
{
    glBegin(GL_QUADS);
        blockmap->iterateCellObjects(coords, (int (*)(void*,void*)) rendMobj, parameters);
    glEnd();
    return false; // Continue iteration.
}

int rendCellBspLeafs(de::Blockmap* blockmap, uint const coords[2], void* parameters)
{
    blockmap->iterateCellObjects(coords, (int (*)(void*,void*)) rendBspLeaf, parameters);
    return false; // Continue iteration.
}

void rendBlockmapBackground(de::Blockmap* blockmap)
{
    DENG2_ASSERT(blockmap);

    const BlockmapCell& bmapSize = blockmap->widthHeight();

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(blockmap->cellWidth(), blockmap->cellHeight(), 1);

    /**
     * Draw the translucent quad which represents the "used" cells.
     */
    vec2f_t start, end;
    V2f_Set(start, 0, 0);
    V2f_Set(end, bmapSize[VX], bmapSize[VY]);
    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(start[VX], start[VY]);
        glVertex2f(  end[VX], start[VY]);
        glVertex2f(  end[VX],   end[VY]);
        glVertex2f(start[VX],   end[VY]);
    glEnd();

    /**
     * Draw the "null cells" over the top.
     */
    glColor4f(0, 0, 0, .95f);
    BlockmapCoord x, y;
    for(y = 0; y < bmapSize[VY]; ++y)
    for(x = 0; x < bmapSize[VX]; ++x)
    {
        if(blockmap->cellObjectCount(x, y)) continue;

        glBegin(GL_QUADS);
            glVertex2f(x,   y);
            glVertex2f(x+1, y);
            glVertex2f(x+1, y+1);
            glVertex2f(x,   y+1);
        glEnd();
    }

    // Restore previous GL state.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static void drawCellInfo(const Point2Raw* _origin, const char* info)
{
    DENG2_ASSERT(_origin);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size = Size2Raw(FR_TextWidth(info)  + 16, FR_SingleLineHeight(info) + 16);

    Point2Raw origin = *_origin;
    origin.x -= size.width / 2;

    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(info, &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawBlockmapInfo(const Point2Raw* _origin, de::Blockmap* blockmap)
{
    DENG2_ASSERT(blockmap);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    int th = FR_SingleLineHeight("Info");
    Size2Raw size = Size2Raw(16 + FR_TextWidth("(+000.0,+000.0)(+000.0,+000.0)"), th * 4 + 16);

    Point2Raw origin = *_origin;
    origin.x -= size.width;
    origin.y -= size.height;

    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += 8 + th/2;

    UI_TextOutEx2("Blockmap", &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    BlockmapCell const& bmapSize = blockmap->widthHeight();
    char buf[80];
    dd_snprintf(buf, 80, "Dimensions:[%u,%u] #%li", bmapSize[VX], bmapSize[VY],
               (long) bmapSize[VY] * bmapSize[VX]);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "Cellsize:[%.3f,%.3f]", blockmap->cellWidth(), blockmap->cellHeight());
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "(%+06.0f,%+06.0f)(%+06.0f,%+06.0f)",
                blockmap->bounds().minX, blockmap->bounds().minY,
                blockmap->bounds().maxX, blockmap->bounds().maxY);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawCellInfoBox(de::Blockmap* blockmap, const Point2Raw* origin,
    const char* objectTypeName, const_BlockmapCell cell)
{
    uint count = blockmap->cellObjectCount(cell);
    char info[160];

    dd_snprintf(info, 160, "Cell:[%u,%u] %s:#%u", cell[VX], cell[VY], objectTypeName, count);
    drawCellInfo(origin, info);
}

/**
 * @param blockmap  Blockmap to be rendered.
 * @param followMobj  Mobj to center/focus the visual on. Can be @c NULL.
 * @param cellDrawer  Blockmap cell content drawing callback. Can be @a NULL.
 */
static void rendBlockmap(de::Blockmap* blockmap, mobj_t* followMobj,
    int (*cellDrawer) (de::Blockmap* blockmap, const_BlockmapCell cell, void* parameters))
{
    BlockmapCellBlock vCellBlock;
    BlockmapCell vCell, cell;
    vec2d_t start, end;

    BlockmapCell const& dimensions = blockmap->widthHeight();
    vec2d_t cellSize;
    V2d_Copy(cellSize, blockmap->cellSize());

    if(followMobj)
    {
        // Determine the followed Mobj's blockmap coords.
        if(blockmap->cell(vCell, followMobj->origin))
            followMobj = NULL; // Outside the blockmap.

        if(followMobj)
        {
            // Determine the extended blockmap coords for the followed
            // Mobj's "touch" range.
            const float radius = followMobj->radius + DDMOBJ_RADIUS_MAX * 2;
            AABoxd aaBox;
            V2d_Set(start, followMobj->origin[VX] - radius, followMobj->origin[VY] - radius);
            V2d_Set(end,   followMobj->origin[VX] + radius, followMobj->origin[VY] + radius);
            V2d_InitBox(aaBox.arvec2, start);
            V2d_AddToBox(aaBox.arvec2, end);
            blockmap->cellBlock(vCellBlock, aaBox);
        }
    }

    if(followMobj)
    {
        // Orient on the center of the followed Mobj.
        V2d_Set(start, vCell[VX] * cellSize[VX],
                       vCell[VY] * cellSize[VY]);
        glTranslated(-start[VX], -start[VY], 0);
    }
    else
    {
        // Orient on center of the Blockmap.
        glTranslated(-(cellSize[VX] * dimensions[VX])/2,
                     -(cellSize[VY] * dimensions[VY])/2, 0);
    }

    // First we'll draw a background showing the "null" cells.
    rendBlockmapBackground(blockmap);
    if(followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        glBegin(GL_QUADS);

        for(cell[VY] = vCellBlock.minY; cell[VY] <= vCellBlock.maxY; ++cell[VY])
        for(cell[VX] = vCellBlock.minX; cell[VX] <= vCellBlock.maxX; ++cell[VX])
        {
            if(cell[VX] == vCell[VX] && cell[VY] == vCell[VY])
            {
                // The cell the followed Mobj is actually in.
                glColor4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                glColor4f(.33f, .33f, .66f, .33f);
            }

            V2d_Set(start, cell[VX] * cellSize[VX], cell[VY] * cellSize[VY]);
            V2d_Set(end, cellSize[VX], cellSize[VY]);
            V2d_Sum(end, end, start);

            glVertex2d(start[VX], start[VY]);
            glVertex2d(  end[VX], start[VY]);
            glVertex2d(  end[VX],   end[VY]);
            glVertex2d(start[VX],   end[VY]);
        }

        glEnd();
    }

    /**
     * Draw the Gridmap visual.
     * @note Gridmap uses a cell unit size of [width:1,height:1], so we need to
     *       scale it up so it aligns correctly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScaled(cellSize[VX], cellSize[VY], 1);

    Gridmap_DebugDrawer(blockmap->gridmap());

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    /**
     * Draw the blockmap-linked data.
     * Translate the modelview matrix so that objects can be drawn using
     * the map space coordinates directly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslated(-blockmap->origin()[VX], -blockmap->origin()[VY], 0);

    if(cellDrawer)
    {
        if(followMobj)
        {
            /**
             * Draw cell contents color coded according to their range from the
             * followed Mobj.
             */

            // First, the cells outside the "touch" range (crimson).
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            for(cell[VY] = 0; cell[VY] < dimensions[VY]; ++cell[VY])
            for(cell[VX] = 0; cell[VX] < dimensions[VX]; ++cell[VX])
            {
                if(cell[VX] >= vCellBlock.minX && cell[VX] <= vCellBlock.maxX &&
                   cell[VY] >= vCellBlock.minY && cell[VY] <= vCellBlock.maxY) continue;
                if(!blockmap->cellObjectCount(cell)) continue;

                cellDrawer(blockmap, cell, NULL/*no params*/);
            }

            // Next, the cells within the "touch" range (orange).
            validCount++;
            glColor3f(1, .5f, 0);
            for(cell[VY] = vCellBlock.minY; cell[VY] <= vCellBlock.maxY; ++cell[VY])
            for(cell[VX] = vCellBlock.minX; cell[VX] <= vCellBlock.maxX; ++cell[VX])
            {
                if(cell[VX] == vCell[VX] && cell[VY] == vCell[VY]) continue;
                if(!blockmap->cellObjectCount(cell)) continue;

                cellDrawer(blockmap, cell, NULL/*no params*/);
            }

            // Lastly, the cell the followed Mobj is in (yellow).
            validCount++;
            glColor3f(1, 1, 0);
            if(blockmap->cellObjectCount(vCell))
            {
                cellDrawer(blockmap, vCell, NULL/*no params*/);
            }
        }
        else
        {
            // Draw all cells without color coding.
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            for(cell[VY] = 0; cell[VY] < dimensions[VY]; ++cell[VY])
            for(cell[VX] = 0; cell[VX] < dimensions[VX]; ++cell[VX])
            {
                if(!blockmap->cellObjectCount(cell)) continue;

                cellDrawer(blockmap, cell, NULL/*no params*/);
            }
        }
    }

    /**
     * Draw the followed mobj, if any.
     */
    if(followMobj)
    {
        validCount++;
        glColor3f(0, 1, 0);
        glBegin(GL_QUADS);
            rendMobj(followMobj, NULL/*no params*/);
        glEnd();
    }

    // Undo the map coordinate space translation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Rend_BlockmapDebug(void)
{
    int (*cellDrawer) (de::Blockmap* blockmap, uint const coords[2], void* parameters);
    const char* objectTypeName;
    mobj_t* followMobj = NULL;
    de::Blockmap* blockmap;
    Point2Raw origin;
    GameMap* map;
    float scale;

    // Disabled?
    if(!bmapShowDebug || bmapShowDebug > 4) return;

    map = theMap;
    if(!map) return;

    switch(bmapShowDebug)
    {
    default: // MobjLinks.
        if(!map->mobjBlockmap) return;

        blockmap = reinterpret_cast<de::Blockmap*>(map->mobjBlockmap);
        cellDrawer = rendCellMobjs;
        objectTypeName = "Mobj";
        break;

    case 2: // LineDefs.
        if(!map->lineDefBlockmap) return;

        blockmap = reinterpret_cast<de::Blockmap*>(map->lineDefBlockmap);
        cellDrawer = rendCellLineDefs;
        objectTypeName = "LineDef";
        break;

    case 3: // BspLeafs.
        if(!map->bspLeafBlockmap) return;

        blockmap = reinterpret_cast<de::Blockmap*>(map->bspLeafBlockmap);
        cellDrawer = rendCellBspLeafs;
        objectTypeName = "BspLeaf";
        break;

    case 4: // PolyobjLinks.
        if(!map->polyobjBlockmap) return;

        blockmap = reinterpret_cast<de::Blockmap*>(map->polyobjBlockmap);
        cellDrawer = rendCellPolyobjs;
        objectTypeName = "Polyobj";
        break;
    }

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    /**
     * Draw the blockmap.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);
    // Orient on the center of the window.
    glTranslatef((Window_Width(theWindow) / 2), (Window_Height(theWindow) / 2), 0);

    // Uniform scaling factor for this visual.
    scale = bmapDebugSize / MAX_OF(Window_Height(theWindow) / 100, 1);
    glScalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->shared.mo)
        followMobj = viewPlayer->shared.mo;

    // Draw!
    rendBlockmap(blockmap, followMobj, cellDrawer);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    /**
     * Draw HUD info.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    if(followMobj)
    {
        // About the cell the followed Mobj is in.
        BlockmapCell mcell;
        if(!blockmap->cell(mcell, followMobj->origin))
        {
            origin.x = Window_Width(theWindow) / 2;
            origin.y = 30;
            drawCellInfoBox(blockmap, &origin, objectTypeName, mcell);
        }
    }

    // About the Blockmap itself.
    origin.x = Window_Width(theWindow)  - 10;
    origin.y = Window_Height(theWindow) - 10;
    drawBlockmapInfo(&origin, blockmap);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

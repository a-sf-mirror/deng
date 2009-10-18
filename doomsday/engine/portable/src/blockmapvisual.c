/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2009 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_ui.h"
#include "de_play.h"

#include "blockmapvisual.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean drawMobj(mobj_t* mo, void* data)
{
    vec2_t start, end;

    V2_Set(start, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2_Set(end,   mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);

    glVertex2f(start[0], start[1]);
    glVertex2f(end  [0], start[1]);
    glVertex2f(end  [0], end  [1]);
    glVertex2f(start[0], end  [1]);

    return true; // Continue iteration.
}

static boolean drawLineDef(linedef_t* line, void* data)
{
    glVertex2f(line->L_v1->pos[0], line->L_v1->pos[1]);
    glVertex2f(line->L_v2->pos[0], line->L_v2->pos[1]);

    return true; // Continue iteration.
}

static boolean drawSubsector(subsector_t* subsector, void* context)
{
    hedge_t* hEdge;
    boolean textured = *((boolean*) context);

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            if(!textured)
            {
                glBegin(GL_LINES);
                    glVertex2f(hEdge->HE_v1->pos[0], hEdge->HE_v1->pos[1]);
                    glVertex2f(hEdge->HE_v2->pos[0], hEdge->HE_v2->pos[1]);
                glEnd();
            }
            else
            {
                vec2_t start, end, delta, normal, unit;
                float scale = MAX_OF(devBlockmapSize, 1);
                float width = (theWindow->width / 16) / scale;

                V2_Set(start, hEdge->HE_v1->pos[0], hEdge->HE_v1->pos[1]);
                V2_Set(end,   hEdge->HE_v2->pos[0], hEdge->HE_v2->pos[1]);

                V2_Subtract(delta, end, start);
                V2_Copy(unit, delta);
                V2_Normalize(unit);
                V2_Set(normal, -unit[1], unit[0]);

                glBegin(GL_QUADS);
                    glTexCoord2f(0.75f, 0.5f);
                    glVertex2fv(start);

                    glTexCoord2f(0.75f, 0.5f);
                    glVertex2fv(end);

                    glTexCoord2f(0.75f, 1);
                    glVertex2f(end[0] - normal[0] * width,
                               end[1] - normal[1] * width);

                    glTexCoord2f(0.75f, 1);
                    glVertex2f(start[0] - normal[0] * width,
                               start[1] - normal[1] * width);
                glEnd();
            }
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }

    return true; // Continue iteration.
}

static boolean drawSubsectorAABB(subsector_t* subsector, void* data)
{
    vec2_t start, end;

    // Draw the bounding box.
    V2_Set(start, subsector->bBox[0].pos[0], subsector->bBox[0].pos[1]);
    V2_Set(end,   subsector->bBox[1].pos[0], subsector->bBox[1].pos[1]);

    glBegin(GL_LINES);
        glVertex2f(start[0], start[1]);
        glVertex2f(end  [0], start[1]);
        glVertex2f(end  [0], start[1]);
        glVertex2f(end  [0], end  [1]);
        glVertex2f(end  [0], end  [1]);
        glVertex2f(start[0], end  [1]);
        glVertex2f(start[0], end  [1]);
        glVertex2f(start[0], start[1]);
    glEnd();

    return true; // Continue iteration.
}

static void drawLineDefsInBlock(blockmap_t* blockmap, uint x, uint y,
                                float r, float g, float b, float a, void* context)
{
    uint block[2];

    block[0] = x;
    block[1] = y;

    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);

    glBegin(GL_LINES);
    LineDefBlockmap_Iterate((linedefblockmap_t*) blockmap, block, drawLineDef, context, false);
    glEnd();

    glBegin(GL_LINES);
    P_IterateLineDefsOfPolyobjs(blockmap, block, drawLineDef, context, false);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

static void drawMobjsInBlock(blockmap_t* blockmap, uint x, uint y, 
                             float r, float g, float b, float a, void* context)
{
    uint block[2];

    block[0] = x;
    block[1] = y;

    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);

    glBegin(GL_QUADS);
    MobjBlockmap_Iterate(blockmap, block, drawMobj, context);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

static void drawSubsectorsInBlock(blockmap_t* blockmap, uint x, uint y,
                                  float r, float g, float b, float a, void* context)
{
    uint block[2];
    boolean textured;

    block[0] = x;
    block[1] = y;

    glDisable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, .1f);

    validCount;
    SubsectorBlockmap_Iterate((subsectorblockmap_t*) blockmap, block, NULL, NULL, validCount, drawSubsectorAABB, context);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, GL_PrepareLSTexture(LST_DYNAMIC));
    glColor4f(r, g, b, a);
    GL_BlendMode(BM_ADD);

    textured = true;
    validCount++;
    SubsectorBlockmap_Iterate((subsectorblockmap_t*) blockmap, block, NULL, NULL, validCount, drawSubsector, &textured);

    GL_BlendMode(BM_NORMAL);
    glDisable(GL_TEXTURE_2D);

    textured = false;
    validCount++;
    SubsectorBlockmap_Iterate((subsectorblockmap_t*) blockmap, block, NULL, NULL, validCount, drawSubsector, &textured);

    glEnable(GL_TEXTURE_2D);
}

static void drawInfoBox(int x, int y, uint blockX, uint blockY, int lineCount,
                        int moCount, int poCount)
{
    int w, h;
    char buf[160];

    sprintf(buf, "Block: [%u, %u] Lines: #%i Mobjs: #%i Polyobjs: #%i",
            blockX, blockY, lineCount, moCount, poCount);
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    x -= w / 2;
    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx(buf, x + 8, y + h / 2, false, true, UI_Color(UIC_TITLE), 1);
}

static void drawInfoBox2(float minX, float minY, float maxX, float maxY,
                         float blockWidth, float blockHeight,
                         uint width, uint height)
{
    int w = 16 + FR_TextWidth("(+000.0,+000.0)(+000.0,+000.0)");
    int th = FR_TextHeight("a"), h = th * 4 + 16;
    int x, y;
    char buf[80];

    x = theWindow->width - 10 - w;
    y = theWindow->height - 10 - h;

    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    x += 8;
    y += 8 + th/2;

    UI_TextOutEx("Blockmap", x, y, false, true, UI_Color(UIC_TITLE), 1);
    y += th;

    sprintf(buf, "Dimensions:[%u,%u] #%li", width, height,
            width * (long) height);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    sprintf(buf, "Blksize:[%.2f,%.2f]", blockWidth, blockHeight);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f)(%+06.0f,%+06.0f)",
            minX, minY, maxX, maxY);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;
}

static void drawBlockInfoBox(blockmap_t* blockmap, uint x, uint y)
{
    drawInfoBox(theWindow->width / 2, 30, x, y,
                LineDefBlockmap_NumInBlock((linedefblockmap_t*) blockmap, x, y),
                MobjBlockmap_NumInBlock((mobjblockmap_t*) blockmap, x, y),
                PolyobjBlockmap_NumInBlock(blockmap, x, y));
}

static void drawBackground(blockmap_t* blockmap, uint viewerBlock[2], uint viewerBlockBox[4],
                           boolean centerOnViewer, byte mode)
{
    vec2_t start, end, min, max, blockSize;
    uint x, y, dimensions[2];
    
    MobjBlockmap_Bounds(blockmap, min, max);
    MobjBlockmap_Dimensions(blockmap, dimensions);
    MobjBlockmap_BlockSize(blockmap, blockSize);

    glDisable(GL_TEXTURE_2D);

    V2_Set(start, 0, 0);
    V2_Set(end, blockSize[0] * dimensions[0], blockSize[1] * dimensions[1]);

    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(start[0], start[1]);
        glVertex2f(end  [0], start[1]);
        glVertex2f(end  [0], end  [1]);
        glVertex2f(start[0], end  [1]);
    glEnd();

    /**
     * Draw the blocks.
     */
    for(y = 0; y < dimensions[1]; ++y)
    for(x = 0; x < dimensions[0]; ++x)
    {
        boolean draw = false;

        if(centerOnViewer)
        {
            if(x == viewerBlock[0] && y == viewerBlock[1])
            {   // The block the viewPlayer is in.
                glColor4f(.66f, .66f, 1, .66f);
                draw = true;
            }
            else if(x >= viewerBlockBox[BOXLEFT]   && x <= viewerBlockBox[BOXRIGHT] &&
                    y >= viewerBlockBox[BOXBOTTOM] && y <= viewerBlockBox[BOXTOP])
            {   // In the viewPlayer's extended collision range.
                glColor4f(.33f, .33f, .66f, .33f);
                draw = true;
            }
        }

        if(!draw)
        {
            int num;

            // Anything linked in this block?
            switch(mode)
            {
            case BLOCKMAPVISUAL_MOBJS:
            default:
                num = MobjBlockmap_NumInBlock((mobjblockmap_t*) blockmap, x, y);
                break;

            case BLOCKMAPVISUAL_LINEDEFS:
                num = LineDefBlockmap_NumInBlock((linedefblockmap_t*) blockmap, x, y);
                break;

            case BLOCKMAPVISUAL_SUBSECTORS:
                num = SubsectorBlockmap_NumInBlock((subsectorblockmap_t*) blockmap, x, y);
                break;
            }

            if(num < 0)
            {   // NULL block.
                glColor4f(0, 0, 0, .95f);
                draw = true;
            }
        }

        if(draw)
        {
            V2_Set(start, x * blockSize[0], y * blockSize[1]);
            V2_Set(end, blockSize[0], blockSize[1]);
            V2_Sum(end, end, start);

            glBegin(GL_QUADS);
                glVertex2f(start[0], start[1]);
                glVertex2f(end  [0], start[1]);
                glVertex2f(end  [0], end  [1]);
                glVertex2f(start[0], end  [1]);
            glEnd();
        }
    }

    /**
     * Draw the grid lines
     */

    glColor4f(.5f, .5f, .5f, .125f);

    // Vertical lines:
    glBegin(GL_LINES);
    for(x = 1; x < dimensions[0]; ++x)
    {
        glVertex2f(x * blockSize[0],  0);
        glVertex2f(x * blockSize[0], blockSize[1] * dimensions[1]);
    }
    glEnd();

    // Horizontal lines
    glBegin(GL_LINES);
    for(y = 1; y < dimensions[1]; ++y)
    {
        glVertex2f(0, y * blockSize[1]);
        glVertex2f(blockSize[0] * dimensions[0], y * blockSize[1]);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

/**
 * Draw the blockmap in 2D HUD mode.
 */
static void drawBlockmap(blockmap_t* blockmap, byte mode, mobj_t* followMobj,
                         void (*func) (blockmap_t*, uint x, uint y, float, float, float, float, void*))
{
    uint x, y, viewerBlock[2], viewerBlockBox[4];
    float radius;
    vec2_t start, end, box[2], min, max, blockSize;
    uint dimensions[2];
    
    MobjBlockmap_Bounds(blockmap, min, max);
    MobjBlockmap_Dimensions(blockmap, dimensions);
    MobjBlockmap_BlockSize(blockmap, blockSize);

    if(followMobj)
    {   // Determine the mobj's block.
        if(!MobjBlockmap_Block2fv(blockmap, viewerBlock, followMobj->pos))
            followMobj = NULL; // The target is outside the blockmap.
    }

    if(followMobj)
    {
        // Determine the mobj's collision blockbox.
        radius = followMobj->radius + DDMOBJ_RADIUS_MAX * 2;
        V2_Set(start, followMobj->pos[VX] - radius, followMobj->pos[VY] - radius);
        V2_Set(end,   followMobj->pos[VX] + radius, followMobj->pos[VY] + radius);
        V2_InitBox(box, start);
        V2_AddToBox(box, end);
        MobjBlockmap_BoxToBlocks(blockmap, viewerBlockBox, box);
    }

    drawBackground(blockmap, viewerBlock, viewerBlockBox, followMobj != NULL, mode);

    /**
     * Draw the blockmap-linked data.
     */

    validCount++;

    glTranslatef(-min[0], -min[1], 0);
    if(followMobj)
    {
        // First, the blocks outside the viewPlayer's range.
        for(y = 0; y < dimensions[1]; ++y)
            for(x = 0; x < dimensions[0]; ++x)
            {
                if(x >= viewerBlockBox[BOXLEFT]   && x <= viewerBlockBox[BOXRIGHT] &&
                   y >= viewerBlockBox[BOXBOTTOM] && y <= viewerBlockBox[BOXTOP])
                    continue;

                func(blockmap, x, y, .33f, 0, 0, .75f, NULL);
            }

        validCount++;

        // Next, the blocks within the viewPlayer's extended collision range.
        for(y = viewerBlockBox[BOXBOTTOM]; y <= viewerBlockBox[BOXTOP]; ++y)
            for(x = viewerBlockBox[BOXLEFT]; x <= viewerBlockBox[BOXRIGHT]; ++x)
            {
                if(x == viewerBlock[0] && y == viewerBlock[1])
                    continue;

                func(blockmap, x, y, 1, .5f, 0, 1, NULL);
            }

        validCount++;

        // Lastly, the block the viewer is in.
        func(blockmap, viewerBlock[0], viewerBlock[1], 1, 1, 0, 1, NULL);

        /**
         * Draw the followMobj.
         */

        radius = followMobj->radius;
        V2_Set(start, followMobj->pos[VX] - radius, followMobj->pos[VY] - radius);
        V2_Set(end,   followMobj->pos[VX] + radius, followMobj->pos[VY] + radius);

        glColor4f(0, 1, 0, 1);
        glDisable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);
            glVertex2f(start[0], start[1]);
            glVertex2f(end  [0], start[1]);
            glVertex2f(end  [0], end  [1]);
            glVertex2f(start[0], end  [1]);
        glEnd();

        glEnable(GL_TEXTURE_2D);
    }
    else
    {   // Draw everything.
        for(y = 0; y < dimensions[1]; ++y)
            for(x = 0; x < dimensions[0]; ++x)
            {
                func(blockmap, x, y, .33f, 0, 0, .75f, NULL);
            }
    }
    glTranslatef(min[0], min[1], 0);

    /**
     * Draw the blockmap bounds.
     */

    V2_Set(start, -1, -1);
    V2_Set(end, 1 + blockSize[0] * dimensions[0], 1 + blockSize[1] * dimensions[1]);

    glColor4f(1, .5f, .5f, 1);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_LINES);
        glVertex2f(start[0], start[1]);
        glVertex2f(end  [0], start[1]);

        glVertex2f(end  [0], start[1]);
        glVertex2f(end  [0], end  [1]);

        glVertex2f(end  [0], end  [1]);
        glVertex2f(start[0], end  [1]);

        glVertex2f(start[0], end  [1]);
        glVertex2f(start[0], start[1]);
    glEnd();
}

void Rend_BlockmapVisual(gamemap_t* map, byte mode)
{
    vec2_t min, max, blockSize;
    uint dimensions[2], viewerBlock[2];
    float scale;
    mobj_t* followMobj;
    blockmap_t* blockmap;
    void (*func) (blockmap_t*, uint x, uint y, float, float, float, float, void*);

    if(!map)
        return;

    switch(mode)
    {
    case BLOCKMAPVISUAL_MOBJS:
    default:
        blockmap = (blockmap_t*) Map_MobjBlockmap(map);
        func = drawMobjsInBlock;
        break;

    case BLOCKMAPVISUAL_LINEDEFS:
        blockmap = (blockmap_t*) Map_LineDefBlockmap(map);
        func = drawLineDefsInBlock;
        break;

    case BLOCKMAPVISUAL_SUBSECTORS:
        blockmap = (blockmap_t*) Map_SubsectorBlockmap(map);
        func = drawSubsectorsInBlock;
        break;
    }

    MobjBlockmap_Bounds(blockmap, min, max);
    MobjBlockmap_Dimensions(blockmap, dimensions);
    MobjBlockmap_BlockSize(blockmap, blockSize);

    // If possible, we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->shared.mo)
    {
        followMobj = viewPlayer->shared.mo;
        // Determine the block the viewer is in.
        MobjBlockmap_Block2fv(blockmap, viewerBlock, followMobj->pos);
    }
    else
    {
        followMobj = NULL;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glTranslatef((theWindow->width / 2), (theWindow->height / 2), 0);
    scale = devBlockmapSize / MAX_OF(theWindow->height / 100, 1);
    glScalef(scale, -scale, 1);

    if(followMobj) // Offset relatively to center on the location of the mobj.
        glTranslatef(-(viewerBlock[0] * blockSize[0] + blockSize[0] / 2),
                     -(viewerBlock[1] * blockSize[1] + blockSize[1] / 2), 0);
    else // Offset to center the blockmap on the screen.
        glTranslatef(-(blockSize[0] * dimensions[0] / 2),
                     -(blockSize[1] * dimensions[1] / 2), 0);

    drawBlockmap(blockmap, mode, followMobj, func);

    glPopMatrix();
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    if(followMobj && mode != BLOCKMAPVISUAL_SUBSECTORS)
    {
        // Draw info about the block mobj is in.
        drawBlockInfoBox(blockmap, viewerBlock[0], viewerBlock[1]);
    }

    // Draw info about the blockmap.
    drawInfoBox2(min[0], min[1], max[0], max[1], blockSize[0], blockSize[1],
                 dimensions[0], dimensions[1]);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

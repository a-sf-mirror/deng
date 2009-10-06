/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * edit_bias.c: Bias light source editor.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_console.h"
#include "de_play.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(SBE_Begin);
D_CMD(SBE_End);

D_CMD(BLEditor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/**
 * Editing variables:
 *
 * edit-bias-blink: keep blinking the nearest light (unless a light is grabbed)
 * edit-bias-grab-distance: how far the light is when grabbed
 * edit-bias-{red,green,blue,intensity}: RGBI of the light
 */
static int editBlink = false;
static float editDistance = 300;
static float editColor[3];
static float editIntensity;

/**
 * Editor status.
 */
static int editActive = false; // Edit mode active?
static int editHidden = false;
static int editShowAll = false;
static int editShowIndices = true;
static int editHueCircle = false;
static float hueDistance = 100;
static vec3_t hueOrigin, hueSide, hueUp;

// CODE --------------------------------------------------------------------

/**
 * Register console variables for Shadow Bias.
 */
void SBE_Register(void)
{
    // Editing variables.
    C_VAR_FLOAT("edit-bias-grab-distance", &editDistance, 0, 10, 1000);

    C_VAR_FLOAT("edit-bias-red", &editColor[0], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-green", &editColor[1], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-blue", &editColor[2], CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("edit-bias-intensity", &editIntensity, CVF_NO_ARCHIVE, 1, 50000);

    C_VAR_INT("edit-bias-blink", &editBlink, 0, 0, 1);
    C_VAR_INT("edit-bias-hide", &editHidden, 0, 0, 1);
    C_VAR_INT("edit-bias-show-sources", &editShowAll, 0, 0, 1);
    C_VAR_INT("edit-bias-show-indices", &editShowIndices, 0, 0, 1);

    // Commands for light editing.
    C_CMD_FLAGS("bledit", "", SBE_Begin, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blquit", "", SBE_End, CMDF_NO_DEDICATED);

    C_CMD_FLAGS("blclear", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blsave", NULL, BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blnew", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldel", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllock", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blunlock", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blgrab", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldup", "", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blc", "fff", BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bli", NULL, BLEditor, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blhue", NULL, BLEditor, CMDF_NO_DEDICATED);
}

static void getHand(float pos[3])
{
    pos[0] = vx + viewFrontVec[VX] * editDistance;
    pos[1] = vz + viewFrontVec[VZ] * editDistance;
    pos[2] = vy + viewFrontVec[VY] * editDistance;
}

static source_t* grabSource(gamemap_t* map, int index)
{
    source_t*           s;
    int                 i;

    map->bias.editGrabbedID = index;
    s = SB_GetSource(map, index);

    // Update the property cvars.
    editIntensity = s->primaryIntensity;
    for(i = 0; i < 3; ++i)
        editColor[i] = s->color[i];

    return s;
}

static source_t* getGrabbed(gamemap_t* map)
{
    if(map->bias.editGrabbedID >= 0 && map->bias.editGrabbedID < map->bias.numSources)
    {
        return SB_GetSource(map, map->bias.editGrabbedID);
    }
    return NULL;
}

static source_t* getNearest(gamemap_t* map)
{
    float               hand[3];
    source_t*           nearest = NULL, *s;
    float               minDist = 0, len;
    int                 i;

    getHand(hand);

    s = SB_GetSource(map, 0);
    for(i = 0; i < map->bias.numSources; ++i, s++)
    {
        len = M_Distance(hand, s->pos);
        if(i == 0 || len < minDist)
        {
            minDist = len;
            nearest = s;
        }
    }

    return nearest;
}

static void getHueColor(float* color, float* angle, float* sat)
{
    int                 i;
    float               dot;
    float               saturation, hue, scale;
    float               minAngle = 0.1f, range = 0.19f;
    vec3_t              h, proj;

    dot = M_DotProduct(viewFrontVec, hueOrigin);
    saturation = (acos(dot) - minAngle) / range;

    if(saturation < 0)
        saturation = 0;
    if(saturation > 1)
        saturation = 1;
    if(sat)
        *sat = saturation;

    if(saturation == 0 || dot > .999f)
    {
        if(angle)
            *angle = 0;
        if(sat)
            *sat = 0;

        R_HSVToRGB(color, 0, 0, 1);
        return;
    }

    // Calculate hue angle by projecting the current viewfront to the
    // hue circle plane.  Project onto the normal and subtract.
    scale = M_DotProduct(viewFrontVec, hueOrigin) /
        M_DotProduct(hueOrigin, hueOrigin);
    M_Scale(h, hueOrigin, scale);

    for(i = 0; i < 3; ++i)
        proj[i] = viewFrontVec[i] - h[i];

    // Now we have the projected view vector on the circle's plane.
    // Normalize the projected vector.
    M_Normalize(proj);

    hue = acos(M_DotProduct(proj, hueUp));

    if(M_DotProduct(proj, hueSide) > 0)
        hue = 2*PI - hue;

    hue /= (float) (2*PI);
    hue += 0.25;

    if(angle)
        *angle = hue;

    //Con_Printf("sat=%f, hue=%f\n", saturation, hue);

    R_HSVToRGB(color, hue, saturation, 1);
}

static void drawBox(int x, int y, int w, int h, ui_color_t* c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_Color(UIC_BG_MEDIUM),
                  c ? c : UI_Color(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_Color(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void drawInfoBox(int x, int y, const char* title, float rgb[3],
                        float textAlpha, int srcIndex, float srcPos[3],
                        float srcColor[3], float srcPrimaryIntensity,
                        float srcSectorLevel[2], boolean srcLocked)
{
    float               eye[3];
    int                 w = 16 + FR_TextWidth("R:0.000 G:0.000 B:0.000");
    int                 th = FR_TextHeight("a"), h = th * 6 + 16;
    char                buf[80];
    ui_color_t          color;

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;

    color.red = rgb[0];
    color.green = rgb[1];
    color.blue = rgb[2];

    drawBox(x, y, w, h, &color);
    x += 8;
    y += 8 + th/2;

    // - index #
    // - locked status
    // - coordinates
    // - distance from eye
    // - intensity
    // - color

    UI_TextOutEx(title, x, y, false, true, UI_Color(UIC_TITLE), textAlpha);
    y += th;

    sprintf(buf, "# %03i %s", srcIndex, srcLocked? "(lock)" : "");
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), textAlpha);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f,%+06.0f)", srcPos[0], srcPos[1], srcPos[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), textAlpha);
    y += th;

    sprintf(buf, "Distance:%-.0f", M_Distance(eye, srcPos));
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), textAlpha);
    y += th;

    sprintf(buf, "Intens:%-5.0f L:%3i/%3i", srcPrimaryIntensity,
            (int) (255.0f * srcSectorLevel[0]),
            (int) (255.0f * srcSectorLevel[1]));

    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), textAlpha);
    y += th;

    sprintf(buf, "R:%.3f G:%.3f B:%.3f",
            srcColor[0], srcColor[1], srcColor[2]);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), textAlpha);
    y += th;
}

static void drawLightLevelGauge(int x, int y, int height, source_t* src)
{
    static sector_t*    lastSector = NULL;
    static float        minLevel = 0, maxLevel = 0;

    sector_t*           sector;
    int                 off = FR_TextWidth("000");
    int                 secY, maxY = 0, minY = 0, p;
    char                buf[80];

    sector = ((subsector_t*)
        R_PointInSubSector(src->pos[VX], src->pos[VY])->data)->sector;

    if(lastSector != sector)
    {
        minLevel = maxLevel = sector->lightLevel;
        lastSector = sector;
    }

    if(sector->lightLevel < minLevel)
        minLevel = sector->lightLevel;
    if(sector->lightLevel > maxLevel)
        maxLevel = sector->lightLevel;

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_LINES);
    glColor4f(1, 1, 1, .5f);
    glVertex2f(x + off, y);
    glVertex2f(x + off, y + height);
    // Normal lightlevel.
    secY = y + height * (1.0f - sector->lightLevel);
    glVertex2f(x + off - 4, secY);
    glVertex2f(x + off, secY);
    if(maxLevel != minLevel)
    {
        // Max lightlevel.
        maxY = y + height * (1.0f - maxLevel);
        glVertex2f(x + off + 4, maxY);
        glVertex2f(x + off, maxY);
        // Min lightlevel.
        minY = y + height * (1.0f - minLevel);
        glVertex2f(x + off + 4, minY);
        glVertex2f(x + off, minY);
    }
    // Current min/max bias sector level.
    if(src->sectorLevel[0] > 0 || src->sectorLevel[1] > 0)
    {
        glColor3f(1, 0, 0);
        p = y + height * (1.0f - src->sectorLevel[0]);
        glVertex2f(x + off + 2, p);
        glVertex2f(x + off - 2, p);

        glColor3f(0, 1, 0);
        p = y + height * (1.0f - src->sectorLevel[1]);
        glVertex2f(x + off + 2, p);
        glVertex2f(x + off - 2, p);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The number values.
    sprintf(buf, "%03i", (short) (255.0f * sector->lightLevel));
    UI_TextOutEx(buf, x, secY, true, true, UI_Color(UIC_TITLE), .7f);
    if(maxLevel != minLevel)
    {
        sprintf(buf, "%03i", (short) (255.0f * maxLevel));
        UI_TextOutEx(buf, x + 2*off, maxY, true, true, UI_Color(UIC_TEXT), .7f);
        sprintf(buf, "%03i", (short) (255.0f * minLevel));
        UI_TextOutEx(buf, x + 2*off, minY, true, true, UI_Color(UIC_TEXT), .7f);
    }
}

static void drawStar(float pos[3], float size, float color[4])
{
    static const float  black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(pos[VX] - size, pos[VZ], pos[VY]);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX] + size, pos[VZ], pos[VY]);

        glVertex3f(pos[VX], pos[VZ] - size, pos[VY]);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX], pos[VZ] + size, pos[VY]);

        glVertex3f(pos[VX], pos[VZ], pos[VY] - size);
        glColor4fv(color);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glVertex3f(pos[VX], pos[VZ], pos[VY]);
        glColor4fv(black);
        glVertex3f(pos[VX], pos[VZ], pos[VY] + size);
    glEnd();
}

static void drawIndex(float pos[3], int index)
{
    char                buf[80];
    float               eye[3], scale;

    if(!editShowIndices)
        return;

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;
    scale = M_Distance(pos, eye) / (theWindow->width / 2);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(pos[VX], pos[VZ], pos[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    // Show the index number of the source.
    sprintf(buf, "%i", index);
    UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE),
                 1 - M_Distance(pos, eye)/2000);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

static void drawSource(float pos[3], float color[3], float intensity,
                       int index)
{
    float               col[4], d;
    float               eye[3];

    V3_Set(eye, vx, vz, vy);
    
    col[0] = color[0];
    col[1] = color[1];
    col[2] = color[2];

    d = (M_Distance(eye, pos) - 100) / 1000;
    if(d < 1) d = 1;
    col[3] = 1.0f / d;

    drawStar(pos, 25 + intensity / 20, col);
    drawIndex(pos, index);
}

static void hueOffset(double angle, float *offset)
{
    offset[0] = cos(angle) * hueSide[VX] + sin(angle) * hueUp[VX];
    offset[1] = sin(angle) * hueUp[VY];
    offset[2] = cos(angle) * hueSide[VZ] + sin(angle) * hueUp[VZ];
}

static void drawHue(void)
{
    vec3_t              eye;
    vec3_t              center, off, off2;
    float               steps = 32, inner = 10, outer = 30, s;
    double              angle;
    float               color[4], sel[4], hue, saturation;
    int                 i;

    eye[0] = vx;
    eye[1] = vy;
    eye[2] = vz;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(vx, vy, vz);
    glScalef(1, 1.0f/1.2f, 1);
    glTranslatef(-vx, -vy, -vz);

    // The origin of the circle.
    for(i = 0; i < 3; ++i)
        center[i] = eye[i] + hueOrigin[i] * hueDistance;

    // Draw the circle.
    glBegin(GL_QUAD_STRIP);
    for(i = 0; i <= steps; ++i)
    {
        angle = 2*PI * i/steps;

        // Calculate the hue color for this angle.
        R_HSVToRGB(color, i/steps, 1, 1);
        color[3] = .5f;

        hueOffset(angle, off);

        glColor4fv(color);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);

        // Saturation decreases in the center.
        color[0] = 1;
        color[1] = 1;
        color[2] = 1;
        color[3] = .15f;
        glColor4fv(color);
        glVertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                   center[2] + inner * off[2]);
    }
    glEnd();

    glBegin(GL_LINES);

    // Draw the current hue.
    getHueColor(sel, &hue, &saturation);
    hueOffset(2*PI * hue, off);
    sel[3] = 1;
    if(saturation > 0)
    {
        glColor4fv(sel);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);
        glVertex3f(center[0] + inner * off[0], center[1] + inner * off[1],
                   center[2] + inner * off[2]);
    }

    // Draw the edges.
    for(i = 0; i < steps; ++i)
    {
        hueOffset(2*PI * i/steps, off);
        hueOffset(2*PI * (i + 1)/steps, off2);

        // Calculate the hue color for this angle.
        R_HSVToRGB(color, i/steps, 1, 1);
        color[3] = 1;

        glColor4fv(color);
        glVertex3f(center[0] + outer * off[0], center[1] + outer * off[1],
                   center[2] + outer * off[2]);
        glVertex3f(center[0] + outer * off2[0], center[1] + outer * off2[1],
                   center[2] + outer * off2[2]);

        if(saturation > 0)
        {
            // Saturation decreases in the center.
            sel[3] = 1 - fabs(M_CycleIntoRange(hue - i/steps + .5f, 1) - .5f)
                * 2.5f;
        }
        else
        {
            sel[3] = 1;
        }
        glColor4fv(sel);
        s = inner + (outer - inner) * saturation;
        glVertex3f(center[0] + s * off[0], center[1] + s * off[1],
                   center[2] + s * off[2]);
        glVertex3f(center[0] + s * off2[0], center[1] + s * off2[1],
                   center[2] + s * off2[2]);
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
}

static boolean newSource(gamemap_t* map)
{
    source_t*           s;

    if(map->bias.numSources == MAX_BIAS_LIGHTS)
        return false;

    s = grabSource(map, map->bias.numSources);
    s->flags &= ~BLF_LOCKED;
    s->flags |= BLF_COLOR_OVERRIDE;
    map->bias.numSources++;

    editIntensity = 200;
    editColor[0] = editColor[1] = editColor[2] = 1;

    return true;
}

static void deleteSource(gamemap_t* map, int which)
{
    if(map->bias.editGrabbedID == which)
        map->bias.editGrabbedID = -1;
    else if(map->bias.editGrabbedID > which)
        map->bias.editGrabbedID--;

    SB_DeleteSource(map, which);
}

static void lockSource(gamemap_t* map, int which)
{
    source_t*           s;

    if((s = SB_GetSource(map, which)) != NULL)
        s->flags |= BLF_LOCKED;
}

static void unlockSource(gamemap_t* map, int which)
{
    source_t*           s;

    if((s = SB_GetSource(map, which)) != NULL)
        s->flags &= ~BLF_LOCKED;
}

static void grab(gamemap_t* map, int which)
{
    if(map->bias.editGrabbedID != which)
        grabSource(map, which);
    else
        map->bias.editGrabbedID = -1;
}

static void dupeSource(gamemap_t* map, int which)
{
    source_t*           s, *orig = SB_GetSource(map, which);
    int                 i;

    if(newSource(map))
    {
        s = getGrabbed(map);
        s->flags &= ~BLF_LOCKED;
        s->sectorLevel[0] = orig->sectorLevel[0];
        s->sectorLevel[1] = orig->sectorLevel[1];

        editIntensity = orig->primaryIntensity;
        for(i = 0; i < 3; ++i)
            editColor[i] = orig->color[i];
    }
}

static boolean save(gamemap_t* map, const char* name)
{
    int                 i;
    FILE*               file;
    filename_t          fileName;
    const char*         uid = P_GetUniqueMapID(map);

    if(!name)
    {
        sprintf(fileName, "%s.ded", P_GetMapID(map));
    }
    else
    {
        strcpy(fileName, name);
        if(!strchr(fileName, '.'))
        {
            // Append the file name extension.
            strcat(fileName, ".ded");
        }
    }

    Con_Printf("Saving to %s...\n", fileName);

    if((file = fopen(fileName, "wt")) == NULL)
        return false;

    fprintf(file, "# %i Bias Lights for %s\n\n", map->bias.numSources, uid);

    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "SkipIf Not %s\n", (char *) gx.GetVariable(DD_GAME_MODE));
    
    for(i = 0; i < map->bias.numSources; ++i)
    {
        source_t*           s = SB_GetSource(map, i);

        fprintf(file, "\nLight {\n");
        fprintf(file, "  Map = \"%s\"\n", uid);
        fprintf(file, "  Origin { %g %g %g }\n",
                s->pos[0], s->pos[1], s->pos[2]);
        fprintf(file, "  Color { %g %g %g }\n",
                s->color[0], s->color[1], s->color[2]);
        fprintf(file, "  Intensity = %g\n", s->primaryIntensity);
        fprintf(file, "  Sector levels { %g %g }\n", s->sectorLevel[0],
                s->sectorLevel[1]);
        fprintf(file, "}\n");
    }

    fclose(file);
    return true;
}

void SBE_InitForMap(gamemap_t* map)
{
    if(!map)
        return;

    map->bias.editGrabbedID = -1;
}

void SBE_EndFrame(gamemap_t* map)
{
    source_t*           src;

    if(!map)
        return;

    // Update the grabbed light.
    if(editActive && (src = getGrabbed(map)) != NULL)
    {
        source_t            old;

        memcpy(&old, src, sizeof(old));

        if(editHueCircle)
        {
            // Get the new color from the circle.
            getHueColor(editColor, NULL, NULL);
        }

        SB_SourceSetColor(src, editColor);
        src->primaryIntensity = src->intensity = editIntensity;
        if(!(src->flags & BLF_LOCKED))
        {
            // Update source properties.
            getHand(src->pos);
        }

        if(memcmp(&old, src, sizeof(old)))
        {
            // The light must be re-evaluated.
            src->flags |= BLF_CHANGED;
        }
    }
}

void SBE_SetHueCircle(boolean activate)
{
    int                 i;

    if((signed) activate == editHueCircle)
        return; // No change in state.

    if(activate && getGrabbed(P_GetCurrentMap()) == NULL)
        return;

    editHueCircle = activate;

    if(activate)
    {
        // Determine the orientation of the hue circle.
        for(i = 0; i < 3; ++i)
        {
            hueOrigin[i] = viewFrontVec[i];
            hueSide[i] = viewSideVec[i];
            hueUp[i] = viewUpVec[i];
        }
    }
}

/**
 * Returns true if the console player is currently using the HueCircle.
 */
boolean SBE_UsingHueCircle(void)
{
    return (editActive && editHueCircle);
}

void SBE_DrawHUD(void)
{
#define INFOBOX_X(x)    (theWindow->width  - 10 - boxWidth - (x))
#define INFOBOX_Y(y)    (theWindow->height - 10 - boxHeight - (y))

#define DRAW_INFOBOX(x, y, title, rgb, textAlpha, src) drawInfoBox((x), (y), (title), (rgb), (textAlpha), SB_ToIndex(map, (src)), (src)->pos, (src)->color, (src)->primaryIntensity, (src)->sectorLevel, ((src)->flags & BLF_LOCKED)? true : false);

    int                 w, h, y, boxWidth, boxHeight;
    char                buf[80];
    float               textAlpha = .8f;
    gamemap_t*          map;
    source_t*           grabbed, *nearest;

    if(!editActive || editHidden)
        return;

    map = P_GetCurrentMap();
    nearest = getNearest(map);
    grabbed = getGrabbed(map);

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    // Overall stats: numSources / MAX (left)
    sprintf(buf, "%i / %i (%i free)", map->bias.numSources, MAX_BIAS_LIGHTS,
            MAX_BIAS_LIGHTS - map->bias.numSources);
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    y = theWindow->height - 10 - h;
    drawBox(10, y, w, h, 0);
    UI_TextOutEx(buf, 18, y + h / 2, false, true, UI_Color(UIC_TITLE),
                 textAlpha);

    // The map ID.
    UI_TextOutEx(P_GetUniqueMapID(map), 18, y - h/2, false, true,
                 UI_Color(UIC_TITLE), textAlpha);

    // Stats for nearest & grabbed:
    boxWidth = 16 + FR_TextWidth("R:0.000 G:0.000 B:0.000");
    boxHeight = FR_TextHeight("a") * 6 + 16;
    if(nearest)
    {
        DRAW_INFOBOX(INFOBOX_X(0), INFOBOX_Y(0),
                     grabbed? "Nearest" : "Highlighted", nearest->color,
                     textAlpha, nearest)
    }

    if(grabbed)
    {
        DRAW_INFOBOX(INFOBOX_X(boxWidth + FR_TextWidth("0")), INFOBOX_Y(0),
                     "Grabbed", grabbed->color, textAlpha, grabbed)
    }

    if(grabbed || nearest)
    {
        source_t*           src = grabbed? grabbed : nearest;

        drawLightLevelGauge(20, theWindow->height/2 - 255/2, 255, src);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef INFOBOX_X
#undef INFOBOX_Y
#undef DRAW_INFOBOX
}

static void drawCursor(gamemap_t* map)
{
#define SET_COL(x, r, g, b, a) {x[0]=(r); x[1]=(g); x[2]=(b); x[3]=(a);}

    double              t = Sys_GetRealTime()/100.0f;
    source_t*           s;
    float               hand[3];
    float               size = 10000, distance;
    float               col[4], eye[3];

    eye[0] = vx;
    eye[1] = vz;
    eye[2] = vy;

    if(editHueCircle && getGrabbed(map))
        drawHue();

    // The grabbed cursor blinks yellow.
    if(!editBlink || (currentTimeSB & 0x80))
        SET_COL(col, 1.0f, 1.0f, .8f, .5f)
    else
        SET_COL(col, .7f, .7f, .5f, .4f)

    s = getGrabbed(map);
    if(!s)
    {
        // The nearest cursor phases blue.
        SET_COL(col, sin(t)*.2f, .2f + sin(t)*.15f, .9f + sin(t)*.3f,
                   .8f - sin(t)*.2f)

        s = getNearest(map);
    }

    glDisable(GL_TEXTURE_2D);

    getHand(hand);
    if((distance = M_Distance(s->pos, hand)) > 2 * editDistance)
    {
        // Show where it is.
        glDisable(GL_DEPTH_TEST);
    }

    drawStar(s->pos, size, col);
    drawIndex(s->pos, SB_ToIndex(map, s));

    // Show if the source is locked.
    if(s->flags & BLF_LOCKED)
    {
        float lock = 2 + M_Distance(eye, s->pos)/100;

        glColor4f(1, 1, 1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(s->pos[VX], s->pos[VZ], s->pos[VY]);

        glRotatef(t/2, 0, 0, 1);
        glRotatef(t, 1, 0, 0);
        glRotatef(t * 15, 0, 1, 0);

        glBegin(GL_LINES);
            glVertex3f(-lock, 0, -lock);
            glVertex3f(+lock, 0, -lock);

            glVertex3f(+lock, 0, -lock);
            glVertex3f(+lock, 0, +lock);

            glVertex3f(+lock, 0, +lock);
            glVertex3f(-lock, 0, +lock);

            glVertex3f(-lock, 0, +lock);
            glVertex3f(-lock, 0, -lock);
        glEnd();

        glPopMatrix();
    }

    if(getNearest(map) != getGrabbed(map) && getGrabbed(map))
    {
        source_t*           src = getNearest(map);

        glDisable(GL_DEPTH_TEST);
        drawSource(src->pos, src->color, src->intensity, SB_ToIndex(map, src));
    }

    // Show all sources?
    if(editShowAll)
    {
        int                 i;

        glDisable(GL_DEPTH_TEST);
        
        for(i = 0; i < map->bias.numSources; ++i)
        {
            source_t*           src = SB_GetSource(map, i);

            if(s == src)
                continue;

            drawSource(src->pos, src->color, src->intensity, SB_ToIndex(map, src));
        }
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

#undef SET_COL
}

void SBE_DrawCursor(gamemap_t* map)
{
    if(!map || !map->bias.numSources)
        return;

    if(!editActive || editHidden)
        return;

    drawCursor(map);
}

D_CMD(SBE_Begin)
{
    gamemap_t*          map;

    if(editActive)
        return false;

    map = P_GetCurrentMap();

    // Advise the game not to draw any HUD displays
    gameDrawHUD = false;
    editActive = true;

    map->bias.editGrabbedID = -1;

    Con_Printf("Bias light editor: ON\n");
    return true;
}

D_CMD(SBE_End)
{
    if(!editActive)
    {
        Con_Printf("The bias light editor is not active.\n");
        return false;
    }

    // Advise the game it can safely draw any HUD displays again
    gameDrawHUD = true;
    editActive = false;

    Con_Printf("Bias light editor: OFF\n");
    return true;
}

D_CMD(BLEditor)
{
    char*               cmd = argv[0] + 2;
    int                 which;
    gamemap_t*          map;

    if(!editActive)
    {
        Con_Printf("The bias light editor is not active.\n");
        return false;
    }

    if(!stricmp(cmd, "save"))
    {
        return save(P_GetCurrentMap(), argc >= 2 ? argv[1] : NULL);
    }

    if(!stricmp(cmd, "clear"))
    {
        gamemap_t*          map = P_GetCurrentMap();

        SB_ClearSources(map);

        map->bias.editGrabbedID = -1;
        newSource(map);

        return true;
    }

    if(!stricmp(cmd, "hue"))
    {
        int                 activate =
            (argc >= 2 ? stricmp(argv[1], "off") : !editHueCircle);

        SBE_SetHueCircle(activate);
        return true;
    }

    if(!stricmp(cmd, "new"))
    {
        return newSource(P_GetCurrentMap());
    }

    map = P_GetCurrentMap();

    // Has the light index been given as an argument?
    if(map->bias.editGrabbedID >= 0)
        which = map->bias.editGrabbedID;
    else
        which = SB_ToIndex(map, getNearest(map));

    if(!stricmp(cmd, "c") && map->bias.numSources > 0)
    {
        source_t*           src = SB_GetSource(map, which);
        float               r = 1, g = 1, b = 1;

        if(argc >= 4)
        {
            r = strtod(argv[1], NULL);
            g = strtod(argv[2], NULL);
            b = strtod(argv[3], NULL);
        }

        editColor[0] = r;
        editColor[1] = g;
        editColor[2] = b;

        SB_SourceSetColor(src, editColor);
        src->flags |= BLF_CHANGED;
        return true;
    }

    if(!stricmp(cmd, "i") && map->bias.numSources > 0)
    {
        source_t*           src = SB_GetSource(map, which);

        if(argc >= 3)
        {
            src->sectorLevel[0] = strtod(argv[1], NULL) / 255.0f;
            if(src->sectorLevel[0] < 0)
                src->sectorLevel[0] = 0;
            else if(src->sectorLevel[0] > 1)
                src->sectorLevel[0] = 1;

            src->sectorLevel[1] = strtod(argv[2], NULL) / 255.0f;
            if(src->sectorLevel[1] < 0)
                src->sectorLevel[1] = 0;
            else if(src->sectorLevel[1] > 1)
                src->sectorLevel[1] = 1;
        }
        else if(argc >= 2)
        {
            editIntensity = strtod(argv[1], NULL);
        }

        src->primaryIntensity = src->intensity = editIntensity;
        src->flags |= BLF_CHANGED;
        return true;
    }

    if(argc > 1)
    {
        which = atoi(argv[1]);
    }

    if(which < 0 || which >= map->bias.numSources)
    {
        Con_Printf("Invalid light index %i.\n", which);
        return false;
    }

    if(!stricmp(cmd, "del"))
    {
        deleteSource(map, which);
        return true;
    }

    if(!stricmp(cmd, "dup"))
    {
        dupeSource(map, which);
        return true;
    }

    if(!stricmp(cmd, "lock"))
    {
        lockSource(map, which);
        return true;
    }

    if(!stricmp(cmd, "unlock"))
    {
        unlockSource(map, which);
        return true;
    }

    if(!stricmp(cmd, "grab"))
    {
        grab(map, which);
        return true;
    }

    return false;
}

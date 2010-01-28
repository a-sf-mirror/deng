/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * animated.c: Reader for BOOM "ANIMATED" lumps.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "wadconverter.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

/**
 * This struct is directly read from the lump, therefor we must ensure that
 * it is aligned.
 *
 * Do NOT change these members in any way!
 */
#pragma pack(1)
typedef struct animdef_s {
    signed char istexture; // @c false = it is a flat (instead of bool)
    char        endname[9];
    char        startname[9];
    int         speed;
} animdef_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * From PrBoom:
 * Load the table of animation definitions, checking for existence of
 * the start and end of each frame. If the start doesn't exist the sequence
 * is skipped, if the last doesn't exist, BOOM exits.
 *
 * Wall/Flat animation sequences, defined by name of first and last frame,
 * The full animation sequence is given using all lumps between the start
 * and end entry, in the order found in the WAD file.
 *
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called ANIMATED rather than a static table in this module to
 * allow wad designers to insert or modify animation sequences.
 *
 * Lump format is an array of byte packed animdef_t structures, terminated
 * by a structure with istexture == -1. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 */
static void loadAnimDefs(animdef_t* animDefs)
{
    int                 i;

    // Read structures until -1 is found
    for(i = 0; animDefs[i].istexture != -1 ; ++i)
    {
        int                 groupNum, ticsPerFrame, numFrames;
        material_namespace_t     mnamespace =
            (animDefs[i].istexture? MN_TEXTURES : MN_FLATS);

        switch(mnamespace)
        {
        case MN_FLATS:
            {
            lumpnum_t           startFrame, endFrame, n;

            if((startFrame = R_TextureIdForName(MN_FLATS, animDefs[i].startname)) == -1 ||
               (endFrame = R_TextureIdForName(MN_FLATS, animDefs[i].endname)) == -1)
                continue;

            numFrames = endFrame - startFrame + 1;
            ticsPerFrame = LONG(animDefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                          animDefs[i].startname, animDefs[i].endname);

            if(startFrame && endFrame)
            {   // We have a valid animation.
                // Create a new animation group for it.
                groupNum = P_NewMaterialGroup(AGF_SMOOTH);

                /**
                 * Doomsday's group animation needs to know the texture/flat
                 * numbers of ALL frames in the animation group so we'll
                 * have to step through the directory adding frames as we
                 * go. (DOOM only required the start/end texture/flat
                 * numbers and would animate all textures/flats inbetween).
                 */

                if(ArgExists("-verbose"))
                    Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                animDefs[i].startname, animDefs[i].endname,
                                ticsPerFrame);

                // Add all frames from start to end to the group.
                if(endFrame > startFrame)
                {
                    for(n = startFrame; n <= endFrame; n++)
                    {
                        material_t* mat = R_MaterialForTextureId(MN_FLATS, n);

                        if(mat)
                            P_AddMaterialToGroup(groupNum, P_ToIndex(mat), ticsPerFrame, 0);
                    }
                }
                else
                {
                    for(n = endFrame; n >= startFrame; n--)
                    {
                        material_t*         mat = R_MaterialForTextureId(MN_FLATS, n);

                        if(mat)
                            P_AddMaterialToGroup(groupNum, P_ToIndex(mat), ticsPerFrame, 0);
                    }
                }
            }
            break;
            }
        case MN_TEXTURES:
            {   // Same as above but for texture groups.
            int             startFrame, endFrame, n;

            if((startFrame = R_TextureIdForName(MN_TEXTURES, animDefs[i].startname)) == -1 ||
               (endFrame = R_TextureIdForName(MN_TEXTURES, animDefs[i].endname)) == -1)
                continue;

            numFrames = endFrame - startFrame + 1;
            ticsPerFrame = LONG(animDefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                          animDefs[i].startname, animDefs[i].endname);

            if(startFrame && endFrame)
            {
                groupNum = P_NewMaterialGroup(AGF_SMOOTH);

                if(ArgExists("-verbose"))
                    Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                animDefs[i].startname, animDefs[i].endname,
                                ticsPerFrame);

                if(endFrame > startFrame)
                {
                    for(n = startFrame; n <= endFrame; n++)
                        P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), ticsPerFrame, 0);
                }
                else
                {
                    for(n = endFrame; n >= startFrame; n--)
                        P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), ticsPerFrame, 0);
                }
            }
            break;
            }
        default:
            Con_Error("loadAnimDefs: Internal Error, invalid namespace %i.",
                      (int) mnamespace);
        }
    }
}

void wadconverter::LoadANIMATED(void)
{
    int lump;

    // Is there an ANIMATED lump?
    if((lump = W_CheckNumForName("ANIMATED")) > 0)
    {
        animdef_t* animDefs;

        /**
         * We'll support this BOOM extension by reading the data and then
         * registering the new animations into Doomsday using the animation
         * groups feature.
         *
         * Support for this extension should be considered depreciated.
         * All new features should be added, accessed via DED.
         */
        Con_Message("WadConverter::LoadAnimated: Reading animations...\n");

        animDefs = (animdef_t *)W_CacheLumpNum(lump, PU_STATIC);
        loadAnimDefs(animDefs);
        Z_Free(animDefs);
    }
}

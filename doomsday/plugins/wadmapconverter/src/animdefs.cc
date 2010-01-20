/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * animdefs.c: Parser for ANIMDEFS lumps.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdlib.h>

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void parseAnimGroup(material_namespace_t mnamespace)
{
    boolean ignore;
    boolean done;
    int groupNumber = 0, texNumBase = 0, flatNumBase = 0;

    if(!(mnamespace == MN_FLATS || mnamespace == MN_TEXTURES))
        Con_Error("parseAnimGroup: Internal Error, invalid namespace %i.",
                  (int) mnamespace);

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    ignore = true;
    if(mnamespace == MN_TEXTURES)
    {
        if((texNumBase = R_TextureIdForName(MN_TEXTURES, sc_String)) != -1)
            ignore = false;
    }
    else
    {
        if((flatNumBase = R_TextureIdForName(MN_FLATS, sc_String)) != -1)
            ignore = false;
    }

    if(!ignore)
        groupNumber = P_NewMaterialGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

    done = false;
    do
    {
        if(SC_GetString())
        {
            if(SC_Compare("pic"))
            {
                int picNum, min, max = 0;

                SC_MustGetNumber();
                picNum = sc_Number;

                SC_MustGetString();
                if(SC_Compare("tics"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                }
                else if(SC_Compare("rand"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                    SC_MustGetNumber();
                    max = sc_Number;
                }
                else
                {
                    SC_ScriptError(NULL);
                }

                if(!ignore)
                {
                    material_t*         mat = R_MaterialForTextureId(mnamespace,
                        (mnamespace == MN_TEXTURES ? texNumBase : flatNumBase) + picNum - 1);

                    if(mat)
                        P_AddMaterialToGroup(groupNumber, P_ToIndex(mat),
                                             min, (max > 0? max - min : 0));
                }
            }
            else
            {
                SC_UnGet();
                done = true;
            }
        }
        else
        {
            done = true;
        }
    } while(!done);
}

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void LoadANIMDEFS(void)
{
    lumpnum_t lump = W_CheckNumForName("ANIMDEFS");

    if(lump != -1)
    {
        SC_OpenLump(lump);

        while(SC_GetString())
        {
            if(SC_Compare("flat"))
            {
                parseAnimGroup(MN_FLATS);
            }
            else if(SC_Compare("texture"))
            {
                parseAnimGroup(MN_TEXTURES);
            }
            else
            {
                Con_Message("WadMapConverter::HexenScriptParse: %s Unknown token '%s', "
                            "at line #%i, skipping.\n", sc_ScriptName, sc_String,
                            sc_LineNumber);
                SC_SkipToStartOfNextLine();
            }
        }

        SC_Close();
    }
}

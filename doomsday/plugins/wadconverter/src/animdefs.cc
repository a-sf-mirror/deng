/*
 * The Doomsday Engine Project -- wadconverter
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

#include <cstring>
#include <cstdlib>
#include <sstream>

#include <de/Log>
#include <de/Material>

#include "wadconverter/HexenDefParser"

using namespace wadconverter;

static void parseAnimGroup(HexenDefParser& parser, material_namespace_t mnamespace)
{
    bool ignore, done;
    int groupNumber = 0, texNumBase = 0, flatNumBase = 0;

    if(!(mnamespace == MN_FLATS || mnamespace == MN_TEXTURES))
        Con_Error("parseAnimGroup: Internal Error, invalid namespace %i.",
                  (int) mnamespace);

    if(!parser.getString()) // Name.
    {
        parser.scriptError("Missing string.");
    }

    ignore = true;
    if(mnamespace == MN_TEXTURES)
    {
        if((texNumBase = R_TextureIdForName(MN_TEXTURES, parser.string)) != -1)
            ignore = false;
    }
    else
    {
        if((flatNumBase = R_TextureIdForName(MN_FLATS, parser.string)) != -1)
            ignore = false;
    }

    if(!ignore)
        groupNumber = P_NewMaterialGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

    done = false;
    do
    {
        if(parser.getString())
        {
            if(parser.compare("pic"))
            {
                int picNum, min, max = 0;

                parser.mustGetNumber();
                picNum = parser.number;

                parser.mustGetString();
                if(parser.compare("tics"))
                {
                    parser.mustGetNumber();
                    min = parser.number;
                }
                else if(parser.compare("rand"))
                {
                    parser.mustGetNumber();
                    min = parser.number;
                    parser.mustGetNumber();
                    max = parser.number;
                }
                else
                {
                    parser.scriptError(NULL);
                }

                if(!ignore)
                {
                    de::Material* mat = R_MaterialForTextureId(mnamespace,
                        (mnamespace == MN_TEXTURES ? texNumBase : flatNumBase) + picNum - 1);

                    if(mat)
                        P_AddMaterialToGroup(groupNumber, P_ToIndex(mat),
                                             min, (max > 0? max - min : 0));
                }
            }
            else
            {
                parser.unGet();
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
void LoadANIMDEFS(wadconverter::lumpnum_t lump)
{
    char* script = reinterpret_cast<char*>(W_CacheLumpNum(lump, PU_STATIC));
    HexenDefParser* parser = new HexenDefParser(W_LumpName(lump), script, W_LumpLength(lump));

    while(parser->getString())
    {
        try
        {
            if(parser->compare("flat"))
            {
                parseAnimGroup(MN_FLATS);
                continue;
            }
            if(parser->compare("texture"))
            {
                parseAnimGroup(MN_TEXTURES);
                continue;
            }

            std::ostringstream os;
            os << "Unknown token " << parser->string << " in " << parser->scriptName << " at line #" << parser->lineNumber;
            /// @throw UnknownTokenError An unknown token was encountered.
            throw UnknownTokenError("WadConverter::LoadANIMDEFS", os.str());
        }
        catch(de::Error& err)
        {
            // If recoverable, log the error and attempt to continue parsing.
            de::LOG_WARNING("") << err.asText();
            parser->skipToStartOfNextLine();
        }
    }

    delete parser;
    Z_Free(script);
}

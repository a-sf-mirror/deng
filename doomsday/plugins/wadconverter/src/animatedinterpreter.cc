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

#include <sstream>

#include <de/Material>

#include "wadconverter/AnimatedInterpreter"

using namespace wadconverter;

de::String AnimatedInterpreter::interpretTextureAnimationDef(const AnimationDef& def)
{
    int startFrame = R_TextureIdForName(MN_TEXTURES, def.startName);
    if(startFrame == -1)
    {
        std::ostringstream os;
        os << "Unknown Texture: \"" << def.startName << "\".";
        /// @throw UnknownMaterialError Reference to unknown texture.
        throw UnknownMaterialError("interpretTextureAnimationDef", os.str());
    }
    
    int endFrame = R_TextureIdForName(MN_TEXTURES, def.endName);
    if(endFrame == -1)
    {
        std::ostringstream os;
        os << "Unknown Texture: \"" << def.endName << "\".";
        /// @throw UnknownMaterialError Reference to unknown texture.
        throw UnknownMaterialError("interpretTextureAnimationDef", os.str());
    }

    int numFrames = endFrame - startFrame + 1;
    if(numFrames < 2)
    {
        std::ostringstream os;
        os << "Bad cycle: \"" << def.startName << "\"...\"" << def.endName;
        /// @throw InterpretError Bad/negative cycle in definition.
        throw InterpretError("interpretTextureAnimationDef", os.str());
    }

    // Add all frames from start to end to the group.
    de::String frames;
    if(endFrame > startFrame)
    {
        for(int n = startFrame; n <= endFrame; n++)
        {
            de::Material* material = R_MaterialForTextureId(MN_TEXTURES, n);
            frames += de::String(4, ' ') + "Texture { ID = \"" + reinterpret_cast<char*>(P_GetPtrp(material, DMU_NAME)) + "\"; Tics = " + def.speed + "; }\n";
        }
    }
    else
    {
        for(int n = endFrame; n >= startFrame; n--)
        {
            de::Material* material = R_MaterialForTextureId(MN_TEXTURES, n);
            frames += de::String(4, ' ') + "Texture { ID = \"" + reinterpret_cast<char*>(P_GetPtrp(material, DMU_NAME)) + "\"; Tics = " + def.speed + "; }\n";
        }
    }

    return de::String("Group {\n") + frames + de::String("}\n");
}

de::String AnimatedInterpreter::interpretFlatAnimationDef(const AnimationDef& def)
{
    lumpnum_t startFrame = R_TextureIdForName(MN_FLATS, def.startName);
    if(startFrame == -1)
    {
        std::ostringstream os;
        os << "Unknown Flat: \"" << def.startName << "\".";
        /// @throw UnknownMaterialError Reference to unknown flat.
        throw UnknownMaterialError("interpretFlatAnimationDef", os.str());
    }

    lumpnum_t endFrame = R_TextureIdForName(MN_FLATS, def.endName);
    if(endFrame == -1)
    {
        std::ostringstream os;
        os << "Unknown Flat: \"" << def.endName << "\".";
        /// @throw UnknownMaterialError Reference to unknown flat.
        throw UnknownMaterialError("interpretFlatAnimationDef", os.str());
    }

    int numFrames = endFrame - startFrame + 1;
    if(numFrames < 2)
    {
        std::ostringstream os;
        os << "Bad cycle: \"" << def.startName << "\"...\"" << def.endName;
        /// @throw InterpretError Bad/negative cycle in definition.
        throw InterpretError("interpretFlatAnimationDef", os.str());
    }

    /**
     * Doomsday's group animation needs to know the texture/flat
     * numbers of ALL frames in the animation group so we'll
     * have to step through the directory adding frames as we
     * go. (DOOM only required the start/end texture/flat
     * numbers and would animate all textures/flats inbetween).
     */

    // Add all frames from start to end to the group.
    de::String frames;
    if(endFrame > startFrame)
    {
        for(lumpnum_t n = startFrame; n <= endFrame; n++)
        {
            de::Material* material = R_MaterialForTextureId(MN_FLATS, n);
            frames += de::String(4, ' ') + "Flat { ID = \"" + reinterpret_cast<char*>(P_GetPtrp(material, DMU_NAME)) + "\"; Tics = " + def.speed + "; }\n";
        }
    }
    else
    {
        for(lumpnum_t n = endFrame; n >= startFrame; n--)
        {
            de::Material* material = R_MaterialForTextureId(MN_FLATS, n);
            frames += de::String(4, ' ') + "Flat { ID = \"" + reinterpret_cast<char*>(P_GetPtrp(material, DMU_NAME)) + "\"; Tics = " + def.speed + "; }\n";
        }
    }

    return de::String("Group {\n") + frames + de::String("}\n");
}

de::String AnimatedInterpreter::interpretAnimationDef(const AnimationDef& def)
{
    if(def.flags[AnimationDef::ISTEXTURE])
    {
        return interpretTextureAnimationDef(def);
    }
    return interpretFlatAnimationDef(def);
}

AnimatedInterpreter::AnimationDefs* AnimatedInterpreter::readAnimationDefs(const de::File& file)
{
    if(!(file.size() > 23+4) || (file.size()-4) % 23 != 0)
        /// @throw FormatError Data does not conform to the expected format.
        throw FormatError("Unexpected source data format.");

    uint32_t numDefs = (file.size()-4) / 23;
    AnimationDefs* defs = new AnimationDefs();
    defs->reserve(numDefs);
    de::Reader reader = de::Reader(file, de::littleEndianByteOrder);
    for(uint32_t i = 0; i < numDefs; ++i)
    {
        AnimationDef def;
        def << reader;
        defs->push_back(def);
    }
    return defs;
}

void AnimatedInterpreter::interpret(const de::File& file, de::String& output)
{
    const AnimationDefs* defs = readAnimationDefs(file);
    for(AnimationDefs::const_iterator i = defs->begin();
        i != defs->end(); ++i)
    {
        try
        {
            output / interpretAnimationDef(*i);
        }
        catch(InterpretError& err)
        {
            /// Non-critical so simply log and continue.
            std::ostringstream os;
            os << "Error interpreting definition #" << i - defs->begin() << ": " << err.asText();
            LOG_WARNING(os.str());
        }
    }
    delete defs;
}

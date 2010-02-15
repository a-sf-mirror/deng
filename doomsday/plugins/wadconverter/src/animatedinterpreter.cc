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

void AnimatedInterpreter::interpretTextureAnimationDef(const AnimationDef& def)
{
    int startFrame = R_TextureIdForName(MN_TEXTURES, def.startName);
    int endFrame = R_TextureIdForName(MN_TEXTURES, def.endName);

    if(startFrame == -1 || endFrame == -1)
        return;

    int numFrames = endFrame - startFrame + 1;
    if(numFrames < 2)
    {
        std::ostringstream os;
        os << "Bad cycle: \"" << def.startName << "\"...\"" << def.endName;
        /// @throw InterpretError Bad/negative cycle in definition.
        throw InterpretError("interpretTextureAnimationDef", os.str());
    }

    // Create a new animation group for it.
    int groupNum = P_NewMaterialGroup(AGF_SMOOTH);

    if(endFrame > startFrame)
    {
        for(int n = startFrame; n <= endFrame; n++)
            P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), def.speed, 0);
    }
    else
    {
        for(int n = endFrame; n >= startFrame; n--)
            P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), def.speed, 0);
    }

    std::ostringstream os;
    os << "New Material animation (Texture): \"" << def.startName << "\"...\"" << def.endName << " (" << def.speed << ")";
    LOG_VERBOSE(os.str());
}

void AnimatedInterpreter::interpretFlatAnimationDef(const AnimationDef& def)
{
    lumpnum_t startFrame = R_TextureIdForName(MN_FLATS, def.startName);
    lumpnum_t endFrame = R_TextureIdForName(MN_FLATS, def.endName);

    if(startFrame == -1 || endFrame == -1)
        return;

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

    // Create a new animation group for it.
    int groupNum = P_NewMaterialGroup(AGF_SMOOTH);

    // Add all frames from start to end to the group.
    if(endFrame > startFrame)
    {
        for(lumpnum_t n = startFrame; n <= endFrame; n++)
        {
            de::Material* mat = R_MaterialForTextureId(MN_FLATS, n);

            if(mat)
                P_AddMaterialToGroup(groupNum, P_ToIndex(mat), def.speed, 0);
        }
    }
    else
    {
        for(lumpnum_t n = endFrame; n >= startFrame; n--)
        {
            de::Material* mat = R_MaterialForTextureId(MN_FLATS, n);

            if(mat)
                P_AddMaterialToGroup(groupNum, P_ToIndex(mat), def.speed, 0);
        }
    }

    std::ostringstream os;
    os << "New Material animation (Flat): \"" << def.startName << "\"...\"" << def.endName << " (" << def.speed << ")";
    LOG_VERBOSE(os.str());
}

void AnimatedInterpreter::interpretAnimationDef(const AnimationDef& def)
{
    if(def.isTexture)
    {
        interpretTextureAnimationDef(def);
        return;
    }
    interpretFlatAnimationDef(def);
}

AnimatedInterpreter::AnimationDefs* AnimatedInterpreter::readAnimationDefs(const de::File& file)
{
    if(!(file.size() > 23+4) || (file.size()-4) % 23 != 0)
        /// @throw FormatError Data does not conform to the expected format.
        throw AnimatedInterpreter::FormatError("Unexpected source data format.");

    uint32_t numDefs = (file.size()-4) / 23;
    AnimationDefs* defs = new AnimationDefs(numDefs);
    de::Reader reader = de::Reader(file, de::littleEndianByteOrder);
    for(uint32_t i = 0; i < numDefs; ++i)
    {
        AnimationDef def;
        def << reader;
        defs->push_back(def);
    }
    return defs;
}

void AnimatedInterpreter::interpret(const de::File& file)
{
    const AnimationDefs* defs = readAnimationDefs(file);
    for(AnimationDefs::const_iterator i = defs->begin();
        i != defs->end(); ++i)
    {
        try
        {
            interpretAnimationDef(*i);
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

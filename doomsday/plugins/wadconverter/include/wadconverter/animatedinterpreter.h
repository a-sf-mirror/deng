/*
 * The Doomsday Engine Project -- wadconverter
 *
 * Copyright © 2006-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBWADCONVERTER_ANIMATEDINTERPRETER_H
#define LIBWADCONVERTER_ANIMATEDINTERPRETER_H

#include <de/Error>
#include <de/Fs>
#include <de/FixedByteArray>
#include <de/Reader>

#include <vector>

namespace wadconverter
{
    typedef unsigned int lumpnum_t;

    /**
     * File format interpreter for the ANIMATED texture/flat definition format,
     * originally designed and implemented in BOOM. Given ANIMATED format
     * source data this class will read and interpret them into Doomsday
     * Material animations using the AnimGroups feature.
     * 
     * \note Support for the ANIMATED format is to be considered depreciated.
     *
     * Format definition:
     * Wall/Flat animations defined as all frames (inclusive) between the name
     * of the first and last flat/texture as a sequence. Frame sequences are
     * interpreted using the original indices as follows:
     *
     * Flat animations:
     *   Indicies are absolute lump numbers beginning at lump zero in the
     *   current lump directory.
     * Texture animations:
     *   Indicies are absolute texture definition indices beginning at
     *   definition zero in the last encountered TEXTURE1 lump.
     */
    class AnimatedInterpreter
    {
    public:
        /// Source data format error (it may not be in ANIMATED format). @ingroup errors
        DEFINE_ERROR(FormatError);

        static void interpret(const de::File& file);

    private:
        /// Failed interpretation of the source data. @ingroup errors
        DEFINE_ERROR(InterpretError);

        /// Intermediate representation of ANIMATED definitions.
        struct AnimationDef {
            bool isTexture; // @c false = it is a flat.
            de::String endName;
            de::String startName;
            int32_t speed;

            void operator << (de::Reader& from) {
                int8_t _isTexture;
                from >> _isTexture;
                isTexture = _isTexture != 0;

                de::FixedByteArray byteSeq(endName, 0, 8);
                from >> byteSeq; from.seek(1);

                de::FixedByteArray byteSeq2(startName, 0, 8);
                from >> byteSeq2; from.seek(1);

                from >> speed;        
            }
        };

        typedef std::vector<AnimationDef> AnimationDefs;

        static AnimationDefs* readAnimationDefs(const de::File& file);
        static void interpretAnimationDef(const AnimationDef& def);
        static void interpretFlatAnimationDef(const AnimationDef& def);
        static void interpretTextureAnimationDef(const AnimationDef& def);
    };
}

#endif /* LIBWADCONVERTER_ANIMATEDINTERPRETER_H */

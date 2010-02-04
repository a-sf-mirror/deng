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

#ifndef LIBDENG2_GRIDMAP_H
#define LIBDENG2_GRIDMAP_H

#include "de/Vector"
#include "de/Error"

#include <vector>

namespace de
{
    /**
     * Generalized blockmap.
     */
    template <class T>
    class Gridmap
    {
    public:
        /// Caller is attempting to reference outside the valid range of the map. @ingroup errors
        DEFINE_ERROR(OutOfRangeError);

    public:
        /**
         * Construct a new Gridmap.
         *
         * @param width         X dimension of the grid.
         * @param height        Y dimension of the grid.
         */
        Gridmap(duint width, duint height);

        ~Gridmap();

        /**
         * Empty the map (only zeros the block links, it is the caller's
         * responsibility to keep track of any linked to storage).
         */
        void empty();

        /**
         * Retrieve the width of the map.
         */
        duint width() const { return _width; }

        /**
         * Retrieve the height of the map.
         */
        duint height() const { return _height; }

        /**
         * Retrieve the width and height of the map as a 2D vector.
         */
        void dimensions(Vector2<duint>& dimensions) const {
                dimensions.x = _width;
                dimensions.y = _height;
        }

        T block(duint x, duint y) const;

        T setBlock(duint x, duint y, T data);

        /**
         * Iterate all the blocks in the map, making a callback for each.
         *
         * @param callback      The callback to be made for each block.
         * @param context       Miscellaneous data to be passed in the callback, can be @c NULL.
         *
         * @return              @c true, iff all callbacks return @c true;
         */
        bool iterate(bool (*callback) (T& data, void* context), void* param);

        /**
         * Iterate a subset of the blocks in the map, making a callback for each.
         *
         * @param callback      The callback to be made for each block.
         * @param context       Miscellaneous data to be passed in the callback, can be @c NULL.
         *
         * @return              @c true, iff all callbacks return @c true;
         */
        bool iterate(duint xl, duint xh, duint yl, duint yh, bool (*callback) (T& data, void* context), void* param);

        bool iterate(const Vector2<duint>& bottomLeft, const Vector2<duint>& topRight, bool (*callback) (T& data, void* context), void* param);

    private:
        duint _width, _height;
        std::vector<T> _blockData;
    };

    template <class T>
    Gridmap<T>::Gridmap(duint width, duint height)
      :  _width(width), _height(height)
    {
        _blockData.reserve(_width * _height);
    }

    template <class T>
    Gridmap<T>::~Gridmap()
    {
        empty();
    }

    template <class T>
    void Gridmap<T>::empty()
    {
        _blockData.clear();
    }

    template <class T>
    T Gridmap<T>::block(duint x, duint y) const
    {
        if(!(x < _width && y < _height))
            /// @throw OutOfRangeError  Could not set block outside valid range.
            throw OutOfRangeError("Gridmap::block", "Block would be outside map.");

        return _blockData[y * _width + x];
    }

    template <class T>
    T Gridmap<T>::setBlock(duint x, duint y, T data)
    {
        if(!(x < _width && y < _height))
            /// @throw OutOfRangeError  Could not set block outside valid range.
            throw OutOfRangeError("Gridmap::setBlock", "Block would be outside map.");

        _blockData[y * _width + x] = data;
        return data;
    }

    template <class T>
    bool Gridmap<T>::iterate(bool (*callback) (T& data, void* ctx), void* context)
    {
        assert(callback);

        for(duint x = 0; x <= _width; ++x)
        {
            for(duint y = 0; y <= _height; ++y)
            {
                void* data = block(x, y);

                if(data)
                {
                    if(!callback(data, context))
                        return false;
                }
            }
        }
        return true;
    }

    template <class T>
    bool Gridmap<T>::iterate(duint xl, duint xh, duint yl, duint yh,
                             bool (*callback) (T& data, void* ctx), void* context)
    {
        assert(callback);

        if(xh >= _width)
            xh = _width - 1;
        if(yh >= _height)
            yh = _height - 1;

        for(duint x = xl; x <= xh; ++x)
        {
            for(duint y = yl; y <= yh; ++y)
            {
                void* data = block(x, y);

                if(data)
                {
                    if(!callback(data, context))
                        return false;
                }
            }
        }
        return true;
    }

    template <class T>
    bool Gridmap<T>::iterate(const Vector2<duint>& bottomLeft, const Vector2<duint>& topRight,
                             bool (*callback) (T& data, void* ctx), void* param)
    {
        return iterate(bottomLeft.x, topRight.x, bottomLeft.y, topRight.y, callback, param);
    }
}

#endif /* LIBDENG2_GRIDMAP_H */

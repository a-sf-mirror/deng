/*
 * The Doomsday Engine Project -- libdeng2
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

#ifndef LIBDENG2_RENDER_H
#define LIBDENG2_RENDER_H

namespace de
{
    typedef enum {
        BM_ZEROALPHA = -1,
        BM_NORMAL,
        BM_ADD,
        BM_DARK,
        BM_SUBTRACT,
        BM_REVERSE_SUBTRACT,
        BM_MUL,
        BM_INVERSE,
        BM_INVERSE_MUL,
        BM_ALPHA_SUBTRACT
    } Blendmode;
}

#endif /* LIBDENG2_RENDER_H */

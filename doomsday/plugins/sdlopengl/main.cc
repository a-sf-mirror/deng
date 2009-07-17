/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/deng.h>
#include "sdlopenglvideo.h"

extern "C"
{
    PUBLIC_API const char* deng_LibraryType(void)
    {
        return "deng-plugin/video";
    }

    PUBLIC_API void deng_InitializePlugin(void)
    {}

    PUBLIC_API void deng_ShutdownPlugin(void)
    {}

    PUBLIC_API de::Video* deng_NewVideo(void)
    {
        return new SDLOpenGLVideo();
    }
}

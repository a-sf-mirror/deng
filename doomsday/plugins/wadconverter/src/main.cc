/*
 * The Doomsday Engine Project -- wadconverter
 *
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

#include "wadconverter.h"

#include "wadconverter/MapReader"

#include <de/Log>
#include <de/Error>

/**
 * This function is called when Doomsday is asked to load a map that is not
 * presently available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
bool ConvertMap(const char* identifier)
{
    using namespace wadconverter;

    MapReader* mapReader = NULL;
    try
    {
        mapReader = MapReader::construct(identifier);
        mapReader->load(); // Load it in.
        mapReader->transfer(); // Transfer the map to the engine.
        delete mapReader;
    }
    catch(de::Error &err)
    {
        if(mapReader) delete mapReader;
        /// \fixme Obviously we should be re-throwing back to the caller
        /// but for now we'll just log it and signal 'failure'.
        de::LOG_MESSAGE("WadConverter::Convert: %s.") << err.asText();
        return false;
    }

    return true; // We're done; signal success!
}

void LoadResources(void)
{
    using namespace wadconverter;

    de::LOG_MESSAGE("WadConverter::LoadResources: Processing...");

    {lumpnum_t lumpnum = W_CheckNumForName("ANIMATED");
    if(lumpnum != -1)
        LoadANIMATED(lumpnum);
    }

    {lumpnum_t lumpnum = W_CheckNumForName("ANIMDEFS");
    if(lumpnum != -1)
        LoadANIMDEFS(lumpnum);
    }
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do by attaching to hooks.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_INIT, LoadResources);
    Plug_AddHook(HOOK_UPDATE, LoadResources);
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMap);
}

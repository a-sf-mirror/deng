/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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
 * wadconverter.h: Doomsday plugin for converting WAD format game data
 * (DOOM, Hexen and DOOM64).
 */

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "doomsday.h"
#include "wadconverter.h"

extern "C"
{
    /**
     * This function is called when Doomsday is asked to load a map that is not
     * presently available in its native map format.
     *
     * Our job is to read in the map data structures then use the Doomsday map
     * editing interface to recreate the map in native format.
     */
    int ConvertMap(int hookType, int param, void* data)
    {
        using namespace wadconverter;

        Map* map = NULL;
        try
        {
            map = Map::construct((char*) data);
            map->load(); // Load it in.
            map->transfer(); // Transfer the map to the engine.
            delete map;
        }
        catch(std::runtime_error &err)
        {
            if(map) delete map;
            /// \fixme Obviously we should be re-throwing back to the caller
            /// but for now we'll just log it and signal 'failure'.
            Con_Message("WadConverter::Convert: %s.\n", err.what());
            return false;
        }

        return true; // We're done; signal success!
    }

    int LoadResources(int hookType, int param, void* data)
    {
        using namespace wadconverter;

        Con_Message("WadConverter::LoadResources: Processing...\n");
        LoadANIMATED();
        LoadANIMDEFS();

        return true;
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
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH: DP_Initialize(); break;
    default: break;
    }
    return TRUE;
}
#endif

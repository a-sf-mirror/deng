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

#include <de/App>
#include <de/Log>
#include <de/Error>
#include <de/FS>

#include "wadconverter.h"

#include "wadconverter/MapReader"
#include "wadconverter/AnimatedInterpreter"

using namespace wadconverter;

/**
 * This function is called when Doomsday is asked to load a map that is not
 * presently available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
bool ConvertMap(const char* identifier)
{
    using namespace de;

    MapReader* mapReader = NULL;
    try
    {
        mapReader = MapReader::construct(identifier);
        mapReader->load(); // Load it in.
        mapReader->transfer(); // Transfer the map to the engine.
        delete mapReader;
    }
    catch(Error &err)
    {
        if(mapReader) delete mapReader;
        /// \fixme Obviously we should be re-throwing back to the caller
        /// but for now we'll just log it and signal 'failure'.
        LOG_WARNING("WadConverter::Convert: %s.") << err.asText();
        return false;
    }

    return true; // We're done; signal success!
}

void LoadResources(void)
{
    using namespace de;

    LOG_TRACE("WadConverter::LoadResources: Processing...");

    try
    {
        const File& file = App::app().fileSystem().findSingle("animated.lmp");
        try
        {
            de::String definitions;
            AnimatedInterpreter::interpret(file, &definitions);

            /// \todo Do something with the newly interpreted definitions.
        }
        catch(AnimatedInterpreter::FormatError& err)
        {
            /// Announce but otherwise quitely ignore errors with ANIMATED data.
            LOG_WARNING("Error reading " + file.name() + (file.source()? " (" + file.source()->name() + "):" : ":") + err.asText());
        }
    }
    catch(FS::NotFoundError)
    {
        /// Ignore, this is not required data.
    }

    try
    {
        const File& file = App::app().fileSystem().findSingle("animdefs.lmp");
        LoadANIMDEFS(file);
    }
    catch(Error & err)
    {
        LOG_MESSAGE(err.asText());
    }
}

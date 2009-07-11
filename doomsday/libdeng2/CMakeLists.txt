# The Doomsday Engine Project -- libdeng2
#
# Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

INCLUDE_DIRECTORIES ( include ${CMAKE_SOURCE_DIR}/libdeng/api 
    ${SDL_INCLUDE_DIR} ${SDLNET_INCLUDE_DIR} )

FILE ( GLOB SOURCES portable/*.cc portable/*.h include/de/*.h )

# Platform-specific configuration.
IF ( UNIX AND NOT APPLE )
    # In non-Apple Unix, we use the SDLNet socket set to wait to socket activity.
    ADD_DEFINITIONS ( -DLIBDENG2_USE_SOCKET_SET )
ENDIF ()

IF ( APPLE )
    FILE ( GLOB MAC_SOURCES mac/*.m mac/*.h )
    SET ( SOURCES ${SOURCES} ${MAC_SOURCES} )
ENDIF ( APPLE )

ADD_LIBRARY ( deng2 SHARED ${SOURCES} )

TARGET_LINK_LIBRARIES ( deng2
    ${SDL_LIBRARY} ${SDLNET_LIBRARY} )

# Installation.
INSTALL ( TARGETS deng2 
    RUNTIME DESTINATION ${DENG_BINARY_DIR} 
    LIBRARY DESTINATION ${DENG_BINARY_DIR} )
    
# Documentation.
IF ( DOXYGEN_FOUND )
    ADD_CUSTOM_TARGET ( deng2docs
        ${DOXYGEN_EXECUTABLE} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
ENDIF ( DOXYGEN_FOUND )

IF ( APPLE )
    # Make sure the library is found inside the bundle.
    SET_TARGET_PROPERTIES ( deng2 PROPERTIES INSTALL_NAME_DIR "@executable_path/../MacOS")
ENDIF ( APPLE )

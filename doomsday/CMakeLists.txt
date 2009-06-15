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

CMAKE_MINIMUM_REQUIRED ( VERSION 2.6 )

PROJECT ( deng )

# Check for an in-tree build.
IF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )
    MESSAGE ( FATAL_ERROR 
        "In-tree builds are not supported. For instance, try building in ${CMAKE_SOURCE_DIR}/mybuild/." )
ENDIF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )

# Win32 configuration.
IF ( WIN32 )
    INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/opengl )
    SET ( CMAKE_INCLUDE_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/include 
        ${CMAKE_SOURCE_DIR}/external/zlib/include
        ${CMAKE_SOURCE_DIR}/external/libpng/include )
    SET ( CMAKE_LIBRARY_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/lib 
        ${CMAKE_SOURCE_DIR}/external/zlib/lib
        ${CMAKE_SOURCE_DIR}/external/libpng/lib )
    SET ( PNG_NAMES libpng13 )
ENDIF ( WIN32 )

# Dependencies.
INCLUDE ( ${CMAKE_ROOT}/Modules/FindDoxygen.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindOpenGL.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindZLIB.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindPNG.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindCURL.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_mixer.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_image.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_net.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_ttf.cmake )

# Set up project-wide variables and definitions.
IF ( WIN32 )
    ADD_DEFINITIONS ( -DWIN32 -D_CRT_SECURE_NO_WARNINGS )
ENDIF ( WIN32 )
IF ( APPLE )
    INCLUDE ( ${CMAKE_ROOT}/Modules/FindQuickTime.cmake )

    ADD_DEFINITIONS ( -DMACOSX )
    IF ( ${CMAKE_OSX_SYSROOT} STREQUAL /Developer/SDKs/MacOSX10.4u.sdk )
	    SET ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.4" )
	    SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.4" )
	ENDIF ( ${CMAKE_OSX_SYSROOT} STREQUAL /Developer/SDKs/MacOSX10.4u.sdk )
ENDIF ( APPLE )
IF ( UNIX )
    INCLUDE ( ${CMAKE_ROOT}/Modules/FindCurses.cmake )

    ADD_DEFINITIONS ( -DUNIX -DNO_FIXED_ASM )
ENDIF ( UNIX )

SUBDIRS ( libdeng libdeng2 server client plugins )

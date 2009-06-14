# CMake project file for the Doomsday Engine.
#
# Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>

CMAKE_MINIMUM_REQUIRED ( VERSION 2.6 )

PROJECT ( deng )

# Check for an in-tree build.
IF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )
    MESSAGE ( FATAL_ERROR 
        "In-tree builds are not supported. For instance, try building in ${CMAKE_SOURCE_DIR}/mybuild/." )
ENDIF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )

# Dependencies.
INCLUDE ( ${CMAKE_ROOT}/Modules/FindDoxygen.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindOpenGL.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindZLIB.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_image.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_net.cmake )
INCLUDE ( ${CMAKE_ROOT}/Modules/FindSDL_ttf.cmake )

# Set up project-wide variables.

SUBDIRS ( libdeng libdeng2 server client plugins )

# Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>

INCLUDE_DIRECTORIES ( api portable/include ${SDL_INCLUDE_DIR} ${SDLMIXER_INCLUDE_DIR}
    ${SDLNET_INCLUDE_DIR} ${PNG_PNG_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${CURL_INCLUDE_DIR}
    ${CURSES_INCLUDE_PATH} ${ZLIB_INCLUDE_DIR} )

FILE ( GLOB SOURCES portable/src/*.c )

# Platform-specific sources.
IF ( WIN32 )
    #INCLUDE_DIRECTORIES ( win32/include )
    FILE ( GLOB WIN_SOURCES win32/src/*.c )
    SET ( SOURCES ${SOURCES} ${WIN_SOURCES} )
ENDIF ( WIN32 )

IF ( APPLE )
    INCLUDE_DIRECTORIES ( ${QUICKTIME_INCLUDE_DIR} )
    
    FILE ( GLOB MAC_SOURCES mac/src/*.m mac/src/*.c )
    SET ( SOURCES ${SOURCES} ${MAC_SOURCES} )
ENDIF ( APPLE )

IF ( UNIX )
    ADD_DEFINITIONS ( -DNO_FIXED_ASM )

    INCLUDE_DIRECTORIES ( unix/include ../external/lzss/portable/include )
    FILE ( GLOB UNIX_SOURCES 
        unix/src/*.c 
        ../external/lzss/unix/src/*.c )
    SET ( SOURCES ${SOURCES} ${UNIX_SOURCES} )
ENDIF ( UNIX )

ADD_LIBRARY ( deng SHARED ${SOURCES} )

TARGET_LINK_LIBRARIES ( deng ${SDL_LIBRARY} ${SDLMIXER_LIBRARY} ${SDLNET_LIBRARY} ${PNG_LIBRARY} 
    ${OPENGL_gl_LIBRARY} ${CURL_LIBRARY} ${CURSES_NCURSES_LIBRARY} ${ZLIB_LIBRARY} )

IF ( APPLE )
    TARGET_LINK_LIBRARIES ( deng ${QUICKTIME_LIBRARY} )
ENDIF ( APPLE )

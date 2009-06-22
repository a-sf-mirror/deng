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

/**
 * dd_api.h: Data Structures for the Engine/Game Interface
 */

#ifndef __DOOMSDAY_GAME_API_H__
#define __DOOMSDAY_GAME_API_H__

#include "dd_export.h"
#include "dd_share.h"

/**
 * The routines/data exported out of the Doomsday engine:
 *
 * This structure contains pointers to routines that can have alternative
 * handlers in the engine. Also, some select global variables are exported
 * using this structure (most importantly the map data).
 */
typedef struct game_import_s {
    size_t          apiSize; // sizeof(game_import_t)
    int             version; // Doomsday Engine version.

    //
    // DATA
    //
    // Data arrays.
    mobjinfo_t**    mobjInfo;
    state_t**       states;
    sprname_t**     sprNames;
    ddtext_t**      text;

    // General information.
    int*            validCount;
} game_import_t;

/**
 * The routines/data exported from the game DLL.
 */
typedef struct {
    size_t          apiSize; // sizeof(game_export_t)

    // Base-level.
    void          (*PreInit) (void);
    void          (*PostInit) (void);
    void          (*Shutdown) (void);
    void          (*UpdateState) (int step);
    int           (*GetInteger) (int id);
    void         *(*GetVariable) (int id);

    // Networking.
    int           (*NetServerStart) (int before);
    int           (*NetServerStop) (int before);
    int           (*NetConnect) (int before);
    int           (*NetDisconnect) (int before);
    long int      (*NetPlayerEvent) (int playernum, int type, void* data);
    int           (*NetWorldEvent) (int type, int parm, void* data);
    void          (*HandlePacket) (int fromplayer, int type, void* data,
                                   size_t length);
    void         *(*NetWriteCommands) (int numCommands, void* data);
    void         *(*NetReadCommands) (size_t pktLength, void* data);

    // Tickers.
    void          (*Ticker) (timespan_t ticLength);

    // Responders.
    boolean       (*PrivilegedResponder) (event_t* ev);
    boolean       (*G_Responder) (event_t* ev);
    boolean       (*FallbackResponder) (event_t* ev);

    // Refresh.
    void          (*BeginFrame) (void);
    void          (*EndFrame) (void);
    void          (*G_Drawer) (int layer);
    void          (*G_Drawer2) (void);
    void          (*ConsoleBackground) (int* width, int* height);

    // Miscellaneous.
    void          (*MobjThinker) ();
    float         (*MobjFriction) (void* mobj); // Returns a friction factor.

    // Main structure sizes.
    size_t          ticcmdSize; // sizeof(ticcmd_t)
    size_t          mobjSize; // sizeof(mobj_t)
    size_t          polyobjSize; // sizeof(polyobj_t)

    // Map data setup
    // This routine is called before any data is read
    // (with the number of items to be read) to allow the
    // game do any initialization it needs (eg create an
    // array of its own private data structures).
    void          (*SetupForMapData) (int type, uint num);

    // This routine is called when trying to assign a value read
    // from the map data (to a property known to us) that we don't
    // know what to do with.

    // (eg the side->toptexture field contains a text string that
    // we don't understand but the game might).

    // The action code returned by the game depends on the context.
    int           (*HandleMapDataPropertyValue) (uint id, int dtype, int prop,
                                                 valuetype_t type, void* data);
    // Post map setup
    // The engine calls this to inform the game of any changes it is
    // making to map data object to which the game might want to
    // take further action.
    int           (*HandleMapObjectStatusReport) (int code, uint id, int dtype,
                                                  void* data);
} game_export_t;

typedef game_export_t* (*GETGAMEAPI) (game_import_t *);

#endif

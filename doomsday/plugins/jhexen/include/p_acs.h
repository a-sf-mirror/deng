/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_acs.h: ACS scripting system.
 */

#ifndef __P_ACS_H__
#define __P_ACS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef int actionscriptid_t;


#define MAX_MAP_VARS            32
#define MAX_WORLD_VARS          64

#define PRINT_BUFFER_SIZE       256

typedef struct {
    const byte*     base;
    int             numScripts;
    struct script_info_s* scriptInfo;
    int             numStrings;
    char const**    strings;
} script_bytecode_t;

typedef struct {
    uint             map; // Target map.
    actionscriptid_t scriptId; // Script number on target map.
    byte            args[4]; // Padded to 4 for alignment.
} script_store_t;

typedef struct {
    script_bytecode_t bytecode;
    int             scriptStoreSize;
    script_store_t* scriptStore;

    int             worldVars[MAX_WORLD_VARS];
    byte            specArgs[8];
    int             mapVars[MAX_MAP_VARS];

    char            printBuffer[PRINT_BUFFER_SIZE];
} actionscriptinterpreter_t;

extern actionscriptinterpreter_t* ActionScriptInterpreter; // Singleton instance.

actionscriptinterpreter_t* P_CreateActionScriptInterpreter(void);
void            P_DestroyActionScriptInterpreter(actionscriptinterpreter_t* asi);

void            ActionScriptInterpreter_Load(actionscriptinterpreter_t* asi, uint map, lumpnum_t lumpNum);

void            ActionScriptInterpreter_WriteWorldState(actionscriptinterpreter_t* asi);
void            ActionScriptInterpreter_ReadWorldState(actionscriptinterpreter_t* asi);
void            ActionScriptInterpreter_WriteMapState(actionscriptinterpreter_t* asi);
void            ActionScriptInterpreter_ReadMapState(actionscriptinterpreter_t* asi);

void            ActionScriptInterpreter_StartAll(actionscriptinterpreter_t* asi, uint map);
boolean         ActionScriptInterpreter_Start(actionscriptinterpreter_t* asi, actionscriptid_t scriptId, uint map, byte* args, mobj_t* activator, linedef_t* line, int side);
boolean         ActionScriptInterpreter_Stop(actionscriptinterpreter_t* asi, actionscriptid_t scriptId, uint map);
boolean         ActionScriptInterpreter_Suspend(actionscriptinterpreter_t* asi, actionscriptid_t scriptId, uint map);

void            ActionScriptInterpreter_TagFinished(actionscriptinterpreter_t* asi, int tag);
void            ActionScriptInterpreter_PolyobjFinished(actionscriptinterpreter_t* asi, int po);

/**
 * Action Script Thinker.
 */
#define AST_MAX_VARS            10
#define AST_STACK_DEPTH         32

typedef struct actionscript_thinker_s {
    thinker_t       thinker;
    int             delayCount;
    actionscriptid_t scriptId;
    const int*      bytecodePos;
    int             infoIndex;
    int             stack[AST_STACK_DEPTH];
    int             stackDepth;
    int             vars[AST_MAX_VARS];
    mobj_t*         activator;
    linedef_t*      lineDef;
    int             lineSide;
} actionscript_thinker_t;

void            ActionScriptThinker_Think(thinker_t* thinker);
void            ActionScriptThinker_Write(const thinker_t* thinker);
int             ActionScriptThinker_Read(thinker_t* thinker);

#endif

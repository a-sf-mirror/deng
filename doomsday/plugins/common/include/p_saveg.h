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
 * p_saveg.h: Common save game handling.
 */

#ifndef __P_SAVEG_H__
#define __P_SAVEG_H__

void            SV_Init(void);
void            SV_GetSaveGameFileName(char* str, int slot, size_t len);
int             SV_GetSaveDescription(char* str, const char* filename,
                                      size_t len);
#if __JHEXEN__
boolean         SV_SaveGame(int slot, const char* description);
boolean         SV_LoadGame(int slot);
void            SV_MapTeleport(int map, int position);

void            SV_HxInitBaseSlot(void);
void            SV_HxUpdateRebornSlot(void);
void            SV_HxClearRebornSlot(void);
boolean         SV_HxRebornSlotAvailable(void);
int             SV_HxGetRebornSlot(void);
#else
boolean         SV_SaveGame(const char* filename, const char* description);
boolean         SV_LoadGame(const char* filename);
#endif

// Write a client savegame file.
void            SV_SaveClient(unsigned int gameid);
void            SV_GetClientSaveGameFileName(char* str, unsigned int gameID,
                                             size_t len);
void            SV_LoadClient(unsigned int gameid);

#if __JHEXEN__
int             SV_ThingArchiveNum(mobj_t* mo);
#else
unsigned short  SV_ThingArchiveNum(mobj_t* mo);
#endif
mobj_t*         SV_GetArchiveThing(int thingid, void* address);

void            SV_Write(const void* data, int len);
void            SV_WriteByte(byte val);
#if __JHEXEN__
void            SV_WriteShort(unsigned short val);
#else
void            SV_WriteShort(short val);
#endif
#if __JHEXEN__
void            SV_WriteLong(unsigned int val);
#else
void            SV_WriteLong(long val);
#endif
void            SV_WriteFloat(float val);
void            SV_Read(void* data, int len);
byte            SV_ReadByte(void);
short           SV_ReadShort(void);
long            SV_ReadLong(void);
float           SV_ReadFloat(void);

// Misc save/load routines.
void            SV_UpdateReadMobjFlags(mobj_t* mo, int ver);
#endif

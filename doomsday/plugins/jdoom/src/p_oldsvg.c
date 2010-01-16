/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 *
 * @fixme Not 64bit clean: In function 'SV_ReadMobj': cast to pointer from integer of different size
 * @fixme Not 64bit clean: In function 'P_v19_UnArchivePlayers': cast from pointer to integer of different size
 * @fixme Not 64bit clean: In function 'P_v19_UnArchiveThinkers': cast from pointer to integer of different size
 */

/**
 * p_oldsvg.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "gamemap.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "dmu_lib.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define PADSAVEP()          savePtr += (4 - ((savePtr - saveBuffer) & 3)) & 3

// All the versions of DOOM have different savegame IDs, but 500 will be the
// savegame base from now on.
#define SAVE_VERSION_BASE   500
#define SAVE_VERSION        (SAVE_VERSION_BASE + gameMode)
#define V19_SAVESTRINGSIZE  (24)
#define VERSIONSIZE         (16)

#define FF_FULLBRIGHT       (0x8000) // Used to be a flag in thing->frame.
#define FF_FRAMEMASK        (0x7fff)

#define SIZEOF_V19_THINKER_T 12
#define V19_THINKER_T_FUNC_OFFSET 8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void SV_UpdateReadMobjFlags(mobj_t *mo, int ver);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte* savePtr;
static byte* saveBuffer;

// CODE --------------------------------------------------------------------

static void SV_Read(void* data, int len)
{
    if(data)
        memcpy(data, savePtr, len);
    savePtr += len;
}

static short SV_ReadShort(void)
{
    savePtr += 2;
    return *(short *) (savePtr - 2);
}

static int SV_ReadLong(void)
{
    savePtr += 4;
    return *(int *) (savePtr - 4);
}

static void SV_ReadPlayer(player_t* pl)
{
    int             i;

    SV_ReadLong();
    pl->playerState = SV_ReadLong();
    SV_Read(NULL, 8);
    pl->viewZ = FIX2FLT(SV_ReadLong());
    pl->viewHeight = FIX2FLT(SV_ReadLong());
    pl->viewHeightDelta = FIX2FLT(SV_ReadLong());
    pl->bob = FLT2FIX(SV_ReadLong());
    pl->flyHeight = 0;
    pl->health = SV_ReadLong();
    pl->armorPoints = SV_ReadLong();
    pl->armorType = SV_ReadLong();

    memset(pl->powers, 0, sizeof(pl->powers));
    pl->powers[PT_INVULNERABILITY] = (SV_ReadLong()? true : false);
    pl->powers[PT_STRENGTH] = (SV_ReadLong()? true : false);
    pl->powers[PT_INVISIBILITY] = (SV_ReadLong()? true : false);
    pl->powers[PT_IRONFEET] = (SV_ReadLong()? true : false);
    pl->powers[PT_ALLMAP] = (SV_ReadLong()? true : false);
    if(pl->powers[PT_ALLMAP])
        AM_RevealMap(AM_MapForPlayer(pl - players), true);
    pl->powers[PT_INFRARED] = (SV_ReadLong()? true : false);

    memset(pl->keys, 0, sizeof(pl->keys));
    pl->keys[KT_BLUECARD] = (SV_ReadLong()? true : false);
    pl->keys[KT_YELLOWCARD] = (SV_ReadLong()? true : false);
    pl->keys[KT_REDCARD] = (SV_ReadLong()? true : false);
    pl->keys[KT_BLUESKULL] = (SV_ReadLong()? true : false);
    pl->keys[KT_YELLOWSKULL] = (SV_ReadLong()? true : false);
    pl->keys[KT_REDSKULL] = (SV_ReadLong()? true : false);

    pl->backpack = SV_ReadLong();

    memset(pl->frags, 0, sizeof(pl->frags));
    pl->frags[0] = SV_ReadLong();
    pl->frags[1] = SV_ReadLong();
    pl->frags[2] = SV_ReadLong();
    pl->frags[3] = SV_ReadLong();

    pl->readyWeapon = SV_ReadLong();
    pl->pendingWeapon = SV_ReadLong();

    memset(pl->weapons, 0, sizeof(pl->weapons));
    pl->weapons[WT_FIRST].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_SECOND].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_THIRD].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_FOURTH].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_FIFTH].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_SIXTH].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_SEVENTH].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_EIGHTH].owned = (SV_ReadLong()? true : false);
    pl->weapons[WT_NINETH].owned = (SV_ReadLong()? true : false);

    memset(pl->ammo, 0, sizeof(pl->ammo));
    pl->ammo[AT_CLIP].owned = SV_ReadLong();
    pl->ammo[AT_SHELL].owned = SV_ReadLong();
    pl->ammo[AT_CELL].owned = SV_ReadLong();
    pl->ammo[AT_MISSILE].owned = SV_ReadLong();
    pl->ammo[AT_CLIP].max = SV_ReadLong();
    pl->ammo[AT_SHELL].max = SV_ReadLong();
    pl->ammo[AT_CELL].max = SV_ReadLong();
    pl->ammo[AT_MISSILE].max = SV_ReadLong();

    pl->attackDown = SV_ReadLong();
    pl->useDown = SV_ReadLong();

    pl->cheats = SV_ReadLong();
    pl->refire = SV_ReadLong();

    pl->killCount = SV_ReadLong();
    pl->itemCount = SV_ReadLong();
    pl->secretCount = SV_ReadLong();

    SV_ReadLong();

    pl->damageCount = SV_ReadLong();
    pl->bonusCount = SV_ReadLong();

    SV_ReadLong();

    pl->plr->extraLight = SV_ReadLong();
    pl->plr->fixedColorMap = SV_ReadLong();
    pl->colorMap = SV_ReadLong();
    for(i = 0; i < 2; ++i)
    {
        pspdef_t       *psp = &pl->pSprites[i];

        psp->state = (state_t*) SV_ReadLong();
        psp->pos[VX] = SV_ReadLong();
        psp->pos[VY] = SV_ReadLong();
        psp->tics = SV_ReadLong();
    }
    pl->didSecret = SV_ReadLong();
}

static void SV_ReadMobj(void)
{
    float pos[3], mom[3], radius, height, floorz, ceilingz;
    angle_t angle;
    spritenum_t sprite;
    int frame, valid, type, ddflags = 0;
    mobj_t* mo;
    mobjinfo_t* info;

    // List: thinker links.
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // Info for drawing: position.
    pos[VX] = FIX2FLT(SV_ReadLong());
    pos[VY] = FIX2FLT(SV_ReadLong());
    pos[VZ] = FIX2FLT(SV_ReadLong());

    // More list: links in sector (if needed)
    SV_ReadLong();
    SV_ReadLong();

    //More drawing info: to determine current sprite.
    angle = SV_ReadLong();  // orientation
    sprite = SV_ReadLong(); // used to find patch_t and flip value
    frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
        frame &= FF_FRAMEMASK; // not used anymore.

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // The closest interval over all contacted Sectors.
    floorz = FIX2FLT(SV_ReadLong());
    ceilingz = FIX2FLT(SV_ReadLong());

    // For movement checking.
    radius = FIX2FLT(SV_ReadLong());
    height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mom[MX] = FIX2FLT(SV_ReadLong());
    mom[MY] = FIX2FLT(SV_ReadLong());
    mom[MZ] = FIX2FLT(SV_ReadLong());

    valid = SV_ReadLong();
    type = SV_ReadLong();
    info = &MOBJINFO[type];

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    /**
     * We now have all the information we need to create the mobj.
     */
    mo = P_MobjCreate(P_MobjThinker, pos[VX], pos[VY], pos[VZ], angle,
                      radius, height, ddflags);

    mo->sprite = sprite;
    mo->frame = frame;
    mo->floorZ = floorz;
    mo->ceilingZ = ceilingz;
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];
    mo->mom[MZ] = mom[MZ];
    mo->valid = valid;
    mo->type = type;
    mo->moveDir = DI_NODIR;

    /**
     * Continue reading the mobj data.
     */
    SV_ReadLong(); // &mobjinfo[mo->type]

    mo->tics = SV_ReadLong(); // state tic counter
    mo->state = (state_t *) SV_ReadLong();
    mo->damage = DDMAXINT; // Use damage set in mo->info->damage
    mo->flags = SV_ReadLong();
    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir = SV_ReadLong(); // 0-7
    mo->moveCount = SV_ReadLong(); // when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_ReadLong();

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = SV_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastLook = SV_ReadLong();

    // For nightmare respawn.
    mo->spawnSpot.pos[VX] = (float) SV_ReadShort();
    mo->spawnSpot.pos[VY] = (float) SV_ReadShort();
    mo->spawnSpot.pos[VZ] = 0; // Initialize with "something".
    mo->spawnSpot.angle = (angle_t) (ANG45 * ((int)SV_ReadShort() / 45));
    /* mo->spawnSpot.type = (int) */ SV_ReadShort();

    mo->spawnSpot.flags = (int) SV_ReadShort();
    mo->spawnSpot.flags &= ~MASK_UNKNOWN_MSF_FLAGS;
    // Spawn on the floor by default unless the mobjtype flags override.
    mo->spawnSpot.flags |= MSF_Z_FLOOR;

    // Thing being chased/attacked for tracers.
    SV_ReadLong();

    SV_UpdateReadMobjFlags(mo, 0);

    mo->state = &STATES[(int) mo->state];
    mo->target = NULL;
    if(mo->player)
    {
        int pnum = (int) mo->player - 1;

        mo->player = &players[pnum];
        mo->dPlayer = mo->player->plr;
        mo->dPlayer->mo = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }
    P_MobjSetPosition(mo);
    mo->info = info;
    mo->floorZ = DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->ceilingZ = DMU_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);
}

void P_v19_UnArchivePlayers(void)
{
    int i, j;

    for(i = 0; i < 4; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        PADSAVEP();

        SV_ReadPlayer(players + i);

        // will be set when unarc thinker
        players[i].plr->mo = NULL;
        players[i].attacker = NULL;

        for(j = 0; j < NUMPSPRITES; ++j)
        {
            if(players[i].pSprites[j].state)
            {
                players[i].pSprites[j].state =
                    &STATES[(int) players[i].pSprites[j].state];
            }
        }
    }
}

void P_v19_UnArchiveWorld(void)
{
    uint                i, j;
    float               matOffset[2];
    short              *get;
    sector_t           *sec;
    xsector_t          *xsec;
    linedef_t          *line;
    xlinedef_t            *xline;

    get = (short *) savePtr;

    // Do sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = DMU_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        DMU_SetFloatp(sec, DMU_FLOOR_HEIGHT, (float) (*get++));
        DMU_SetFloatp(sec, DMU_CEILING_HEIGHT, (float) (*get++));
        DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, R_MaterialForTextureId(MN_FLATS, *get++));
        DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, R_MaterialForTextureId(MN_FLATS, *get++));

        DMU_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) (*get++) / 255.0f);
        xsec->special = *get++; // needed?
        /*xsec->tag =*/ *get++; // needed?
        xsec->specialData = 0;
        xsec->soundTarget = 0;
    }

    // Do lines.
    for(i = 0; i < numlines; ++i)
    {
        line = DMU_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        xline->flags = *get++;
        xline->special = *get++;
        /*xline->tag =*/ *get++;

        for(j = 0; j < 2; ++j)
        {
            sidedef_t* sdef = DMU_GetPtrp(line, (j? DMU_SIDEDEF1:DMU_SIDEDEF0));

            if(!sdef)
                continue;

            matOffset[VX] = (float) (*get++);
            matOffset[VY] = (float) (*get++);
            DMU_SetFloatpv(sdef, DMU_TOP_MATERIAL_OFFSET_XY, matOffset);
            DMU_SetFloatpv(sdef, DMU_MIDDLE_MATERIAL_OFFSET_XY, matOffset);
            DMU_SetFloatpv(sdef, DMU_BOTTOM_MATERIAL_OFFSET_XY, matOffset);

            DMU_SetPtrp(sdef, DMU_TOP_MATERIAL, R_MaterialForTextureId(MN_TEXTURES, *get++));
            DMU_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, R_MaterialForTextureId(MN_TEXTURES, *get++));
            DMU_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, R_MaterialForTextureId(MN_TEXTURES, *get++));
        }
    }

    savePtr = (byte *) get;
}

static int removeThinker(void* p, void* context)
{
    thinker_t*          th = (thinker_t*) p;

    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return true; // Continue iteration.
}

void P_v19_UnArchiveThinkers(void)
{
enum thinkerclass_e {
    TC_END,
    TC_MOBJ
};

    byte                tClass;

    // Remove all the current thinkers.
    DD_IterateThinkers(NULL, removeThinker, NULL);
    DD_InitThinkers();

    // Read in saved thinkers.
    for(;;)
    {
        tClass = *savePtr++;
        switch(tClass)
        {
        case TC_END:
            return; // End of list.

        case TC_MOBJ:
            PADSAVEP();
            SV_ReadMobj();
            break;

        default:
            Con_Error("Unknown tclass %i in savegame", tClass);
        }
    }
}

static int SV_ReadCeiling(ceiling_t *ceiling)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    ceilingtype_e type; // was 32bit int
    sector_t *sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    boolean crush;
    int     direction;
    int     tag;
    int     olddirection;
} v19_ceiling_t;
*/
    byte                temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct).
    SV_Read(&temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    ceiling->type = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    ceiling->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!ceiling->sector)
        Con_Error("tc_ceiling: bad sector number\n");

    ceiling->bottomHeight = FIX2FLT(SV_ReadLong());
    ceiling->topHeight = FIX2FLT(SV_ReadLong());
    ceiling->speed = FIX2FLT(SV_ReadLong());
    ceiling->crush = SV_ReadLong();
    ceiling->state = (SV_ReadLong() == -1? CS_DOWN : CS_UP);
    ceiling->tag = SV_ReadLong();
    ceiling->oldState = (SV_ReadLong() == -1? CS_DOWN : CS_UP);

    ceiling->thinker.function = T_MoveCeiling;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&ceiling->thinker, true);

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static int SV_ReadDoor(door_t *door)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    doortype_e type; // was 32bit int
    sector_t *sector;
    fixed_t topheight;
    fixed_t speed;
    int     direction;
    int     topwait;
    int     topcountdown;
} v19_vldoor_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    door->type = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    door->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!door->sector)
        Con_Error("tc_door: bad sector number\n");

    door->topHeight = FIX2FLT(SV_ReadLong());
    door->speed = FIX2FLT(SV_ReadLong());
    door->state = SV_ReadLong();
    door->topWait = SV_ReadLong();
    door->topCountDown = SV_ReadLong();

    door->thinker.function = T_Door;
    P_ToXSector(door->sector)->specialData = door;
    return true; // Add this thinker.
}

static int SV_ReadFloor(floor_t *floor)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    floortype_e type; // was 32bit int
    boolean crush;
    sector_t *sector;
    int     direction;
    int     newspecial;
    short   texture;
    fixed_t floordestheight;
    fixed_t speed;
} v19_floormove_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    floor->type = SV_ReadLong();
    floor->crush = SV_ReadLong();

    // A 32bit pointer to sector, serialized.
    floor->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!floor->sector)
        Con_Error("tc_floor: bad sector number\n");

    floor->state = (int) SV_ReadLong();
    floor->newSpecial = SV_ReadLong();
    floor->material = R_MaterialForTextureId(MN_FLATS, SV_ReadShort());
    floor->floorDestHeight = FIX2FLT(SV_ReadLong());
    floor->speed = FIX2FLT(SV_ReadLong());

    floor->thinker.function = T_MoveFloor;
    P_ToXSector(floor->sector)->specialData = floor;
    return true; // Add this thinker.
}

static int SV_ReadPlat(plat_t *plat)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int     wait;
    int     count;
    platstate_e  status; // was 32bit int
    platstate_e  oldstatus; // was 32bit int
    boolean crush;
    int     tag;
    plattype_e type; // was 32bit int
} v19_plat_t;
*/
    byte                temp[SIZEOF_V19_THINKER_T];

    // Padding at the start (an old thinker_t struct)
    SV_Read(temp, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    plat->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!plat->sector)
        Con_Error("tc_plat: bad sector number\n");

    plat->speed = FIX2FLT(SV_ReadLong());
    plat->low = FIX2FLT(SV_ReadLong());
    plat->high = FIX2FLT(SV_ReadLong());
    plat->wait = SV_ReadLong();
    plat->count = SV_ReadLong();
    plat->state = SV_ReadLong();
    plat->oldState = SV_ReadLong();
    plat->crush = SV_ReadLong();
    plat->tag = SV_ReadLong();
    plat->type = SV_ReadLong();

    plat->thinker.function = T_PlatRaise;
    if(!(temp + V19_THINKER_T_FUNC_OFFSET))
        DD_ThinkerSetStasis(&plat->thinker, true);

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

static int SV_ReadFlash(lightflash_t *flash)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     count;
    int     maxlight;
    int     minlight;
    int     maxtime;
    int     mintime;
} v19_lightflash_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    flash->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!flash->sector)
        Con_Error("tc_flash: bad sector number\n");

    flash->count = SV_ReadLong();
    flash->maxLight = (float) SV_ReadLong() / 255.0f;
    flash->minLight = (float) SV_ReadLong() / 255.0f;
    flash->maxTime = SV_ReadLong();
    flash->minTime = SV_ReadLong();

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static int SV_ReadStrobe(strobe_t *strobe)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     count;
    int     minlight;
    int     maxlight;
    int     darktime;
    int     brighttime;
} v19_strobe_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    strobe->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!strobe->sector)
        Con_Error("tc_strobe: bad sector number\n");

    strobe->count = SV_ReadLong();
    strobe->minLight = (float) SV_ReadLong() / 255.0f;
    strobe->maxLight = (float) SV_ReadLong() / 255.0f;
    strobe->darkTime = SV_ReadLong();
    strobe->brightTime = SV_ReadLong();

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static int SV_ReadGlow(glow_t *glow)
{
/* Original DOOM format:
typedef struct {
    thinker_t thinker; // was 12 bytes
    sector_t *sector;
    int     minlight;
    int     maxlight;
    int     direction;
} v19_glow_t;
*/
    // Padding at the start (an old thinker_t struct)
    SV_Read(NULL, SIZEOF_V19_THINKER_T);

    // Start of used data members.
    // A 32bit pointer to sector, serialized.
    glow->sector = DMU_ToPtr(DMU_SECTOR, SV_ReadLong());
    if(!glow->sector)
        Con_Error("tc_glow: bad sector number\n");

    glow->minLight = (float) SV_ReadLong() / 255.0f;
    glow->maxLight = (float) SV_ReadLong() / 255.0f;
    glow->direction = SV_ReadLong();

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

/*
 * Things to handle:
 *
 * T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
 * T_Door, (door_t: sector_t * swizzle),
 * T_MoveFloor, (floor_t: sector_t * swizzle),
 * T_LightFlash, (lightflash_t: sector_t * swizzle),
 * T_StrobeFlash, (strobe_t: sector_t *),
 * T_Glow, (glow_t: sector_t *),
 * T_PlatRaise, (plat_t: sector_t *), - active list
 */
void P_v19_UnArchiveSpecials(void)
{
    enum {
        tc_ceiling,
        tc_door,
        tc_floor,
        tc_plat,
        tc_flash,
        tc_strobe,
        tc_glow,
        tc_endspecials
    };

    byte                tClass;
    ceiling_t          *ceiling;
    door_t           *door;
    floor_t        *floor;
    plat_t             *plat;
    lightflash_t       *flash;
    strobe_t           *strobe;
    glow_t             *glow;

    // Read in saved thinkers.
    for(;;)
    {
        tClass = *savePtr++;
        switch(tClass)
        {
        case tc_endspecials:
            return; // End of list.

        case tc_ceiling:
            PADSAVEP();
            ceiling = Z_Calloc(sizeof(*ceiling), PU_MAP, NULL);

            SV_ReadCeiling(ceiling);

            DD_ThinkerAdd(&ceiling->thinker);
            break;

        case tc_door:
            PADSAVEP();
            door = Z_Calloc(sizeof(*door), PU_MAP, NULL);

            SV_ReadDoor(door);

            DD_ThinkerAdd(&door->thinker);
            break;

        case tc_floor:
            PADSAVEP();
            floor = Z_Calloc(sizeof(*floor), PU_MAP, NULL);

            SV_ReadFloor(floor);

            DD_ThinkerAdd(&floor->thinker);
            break;

        case tc_plat:
            PADSAVEP();
            plat = Z_Calloc(sizeof(*plat), PU_MAP, NULL);

            SV_ReadPlat(plat);

            DD_ThinkerAdd(&plat->thinker);
            break;

        case tc_flash:
            PADSAVEP();
            flash = Z_Calloc(sizeof(*flash), PU_MAP, NULL);

            SV_ReadFlash(flash);

            DD_ThinkerAdd(&flash->thinker);
            break;

        case tc_strobe:
            PADSAVEP();
            strobe = Z_Calloc(sizeof(*strobe), PU_MAP, NULL);

            SV_ReadStrobe(strobe);

            DD_ThinkerAdd(&strobe->thinker);
            break;

        case tc_glow:
            PADSAVEP();
            glow = Z_Calloc(sizeof(*glow), PU_MAP, NULL);

            SV_ReadGlow(glow);

            DD_ThinkerAdd(&glow->thinker);
            break;

        default:
            Con_Error("P_UnarchiveSpecials:Unknown tclass %i in savegame",
                      tClass);
        }
    }
}

boolean SV_v19_LoadGame(const char* savename)
{
    gamemap_t* map = P_CurrentGameMap();
    int i, a, b, c;
    size_t length;
    char vcheck[VERSIONSIZE];

    if(!(length = M_ReadFile(savename, &saveBuffer)))
        return false;

    // Skip the description field.
    savePtr = saveBuffer + V19_SAVESTRINGSIZE;

    // Check version.
    memset(vcheck, 0, sizeof(vcheck));
    sprintf(vcheck, "version %i", SAVE_VERSION);
    if(strcmp((const char*) savePtr, vcheck))
    {
        int                 saveVer;

        sscanf((const char*) savePtr, "version %i", &saveVer);
        if(saveVer >= SAVE_VERSION_BASE)
        {
            // Must be from the wrong game.
            Con_Message("Bad savegame version.\n");
            return false;
        }

        // Just give a warning.
        Con_Message("Savegame ID '%s': incompatible?\n", savePtr);
    }
    savePtr += VERSIONSIZE;

    gameSkill = *savePtr++;
    gameEpisode = *savePtr++;
    gameMap = *savePtr++;
    for(i = 0; i < 4; ++i)
        players[i].plr->inGame = *savePtr++;

    // Load a base map.
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Get the map time.
    a = *savePtr++;
    b = *savePtr++;
    c = *savePtr++;
    map->time = (a << 16) + (b << 8) + c;

    // Dearchive all the modifications.
    P_v19_UnArchivePlayers();
    P_v19_UnArchiveWorld();
    P_v19_UnArchiveThinkers();
    P_v19_UnArchiveSpecials();

    if(*savePtr != 0x1d)
        Con_Error
            ("SV_v19_LoadGame: Bad savegame (consistency test failed!)\n");

    // Success!
    Z_Free(saveBuffer);
    saveBuffer = NULL;

    // Spawn particle generators.
    R_SetupMap(DDSMM_AFTER_LOADING, 0);

    return true;
}

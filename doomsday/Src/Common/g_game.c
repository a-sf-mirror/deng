
//**************************************************************************
//**
//** G_GAME.C
//**
//** Top-level game routines.
//** Compiles for jDoom, jHeretic and jHexen.
//**
//** Could use a LOT of tidying up!
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------
#include <ctype.h>

#if __JDOOM__
#  include <string.h>
#  include <stdlib.h>
#  include <math.h>
#  include "doomdef.h"
#  include "doomstat.h"
#  include "D_Action.h"
#  include "d_config.h"
#  include "m_argv.h"
#  include "m_misc.h"
#  include "m_menu.h"
#  include "m_random.h"
#  include "p_local.h"
#  include "p_tick.h"
#  include "wi_stuff.h"
#  include "st_stuff.h"
#  include "s_sound.h"
#  include "dstrings.h"
#  include "g_game.h"
#  include "Common/p_saveg.h"
#elif __JHERETIC__
#  include <stdio.h>
#  include <string.h>
#  include <math.h>
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/h_stat.h"
#  include "jHeretic/H_Action.h"
#  include "jHeretic/h_config.h"
#  include "jHeretic/m_menu.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/Soundst.h"
#  include "jHeretic/Dstrings.h"
#  include "jHeretic/G_game.h"
#  include "jHeretic/st_stuff.h"
#  include "Common/p_saveg.h"
#elif __JHEXEN__
#  include <string.h>
#  include <math.h>
#  include <assert.h>
#  include "jHexen/h2def.h"
#  include "jHexen/p_local.h"
#  include "jHexen/soundst.h"
#  include "x_config.h"
#  include "jHexen/h2_actn.h"
#  include "jHexen/st_stuff.h"
#  include "jHexen/mn_def.h"
#elif __JSTRIFE__
#  include <string.h>
#  include <math.h>
#  include <assert.h>
#  include "jStrife/h2def.h"
#  include "jStrife/p_local.h"
#  include "jStrife/soundst.h"
#  include "jStrife/d_config.h"
#  include "jStrife/h2_actn.h"
#  include "jStrife/st_stuff.h"
#endif

#include "Common/p_mapsetup.h"
#include "Common/am_map.h"
#include "Common/hu_stuff.h"
#include "Common/hu_msg.h"
#include "Common/g_common.h"
#include "Common/g_update.h"
#include "Common/d_net.h"
#include "Common/x_hair.h"

#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define LOCKF_FULL      0x10000
#define LOCKF_MASK      0xff

#define BODYQUESIZE         32

#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
struct
{
    mobjtype_t type;
    int     speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__
    {MT_BRUISERSHOT, {15, 20}},
    {MT_HEADSHOT, {10, 20}},
    {MT_TROOPSHOT, {10, 20}},
#elif __JHERETIC__
    {MT_IMPBALL, {10, 20}},
    {MT_MUMMYFX1, {9, 18}},
    {MT_KNIGHTAXE, {9, 18}},
    {MT_REDAXE, {9, 18}},
    {MT_BEASTBALL, {12, 20}},
    {MT_WIZFX1, {18, 24}},
    {MT_SNAKEPRO_A, {14, 20}},
    {MT_SNAKEPRO_B, {14, 20}},
    {MT_HEADFX1, {13, 20}},
    {MT_HEADFX3, {10, 18}},
    {MT_MNTRFX1, {20, 26}},
    {MT_MNTRFX2, {14, 20}},
    {MT_SRCRFX1, {20, 28}},
    {MT_SOR2FX1, {20, 28}},
#endif
    {-1, {-1, -1}}                  // Terminator
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_DefaultBindings(void);
void    G_BindClassRegistration(void);
void    G_ResetMousePos(void);
boolean G_AdjustControlState(event_t* ev);
void    G_LookAround(void);

boolean cht_Responder(event_t *ev);

boolean M_EditResponder(event_t *ev);

void    P_InitPlayerValues(player_t *p);
void    P_RunPlayers(void);
boolean P_IsPaused(void);
void    P_DoTick(void);

#if __JHEXEN__
void    P_InitSky(int map);
#endif

void    HU_UpdatePsprites(void);

void    G_ConsoleRegistration(void);
void    DetectIWADs(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    G_PlayerReborn(int player);
void    G_InitNew(skill_t skill, int episode, int map);
void    G_DoInitNew(void);
void    G_DoReborn(int playernum);
void    G_DoLoadLevel(void);
void    G_DoNewGame(void);
void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoCompleted(void);
void    G_DoVictory(void);
void    G_DoWorldDone(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);

#if __JHEXEN__ || __JSTRIFE__
void    G_DoTeleportNewMap(void);
void    G_DoSingleReborn(void);
void    H2_PageTicker(void);
void    H2_AdvanceDemo(void);
#endif

void    G_StopDemo(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JDOOM__
extern char *pagename;
extern GameMission_t gamemission;
#endif

#if __JHERETIC__
extern boolean inventory;
extern int curpos;
extern boolean usearti;
extern int inv_ptr;
extern boolean artiskip;
extern int curpos;
extern int inv_ptr;
extern int playerkeys;
#endif

extern int povangle;
extern float targetLookOffset;
extern float lookOffset;

extern char *borderLumps[];

#if __JHEXEN__ || __JSTRIFE__
extern boolean inventory;
extern int curpos;
extern boolean usearti;
extern int inv_ptr;
extern boolean artiskip;
extern int     AutoArmorSave[NUMCLASSES];
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifdef TIC_DEBUG
FILE   *rndDebugfile;
#endif

// The global cfg.
#if __JDOOM__
jdoom_config_t cfg;
#elif __JHERETIC__
jheretic_config_t cfg;
#elif __JHEXEN__
jhexen_config_t cfg;
#endif

gameaction_t gameaction;
gamestate_t gamestate = GS_DEMOSCREEN;
skill_t gameskill;
int     gameepisode;
int     gamemap;
int     nextmap;                // if non zero this will be the next map

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
boolean respawnmonsters;
#endif

#ifndef __JDOOM__
int     prevmap;
#endif

#if __JDOOM__ || __JHERETIC__
skill_t d_skill;
int     d_episode;
int     d_map;
#endif

boolean paused;
boolean sendpause;              // send a pause event next tic
boolean usergame;               // ok to save / end game

boolean timingdemo;             // if true, exit with report on completion
boolean nodrawers;              // for comparative timing purposes
boolean noblit;                 // for comparative timing purposes
int     starttime;              // for comparative timing purposes

boolean viewactive;

boolean deathmatch;             // only if started as net death
player_t players[MAXPLAYERS];

int     levelstarttic;          // gametic at level start
int     totalkills, totalitems, totalsecret;    // for intermission

char    defdemoname[32];
boolean singledemo;             // quit after playing a demo from cmdline

boolean precache = true;        // if true, load all graphics at start

#if __JDOOM__
wbstartstruct_t wminfo;         // parms for world map / intermission
#endif

int     savegameslot;
char    savedescription[32];

#if __JDOOM__
mobj_t *bodyque[BODYQUESIZE];
int     bodyqueslot;
#endif

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
int     inventoryTics;
#endif

#if __JHEXEN__ || __JSTRIFE__
byte    demoDisabled = 0;       // is demo playing disabled?

// Position indicator for cooperative net-play reborn
int     RebornPosition;
int     LeaveMap;
int     LeavePosition;
#endif

boolean secretexit;
char    savename[256];

#if __JHEXEN__ || __JSTRIFE__
int maphub = 0;
#endif

// vars used with game status cvars
int gsvInLevel = 0;
int gsvCurrentMusic = 0;
int gsvMapMusic = -1;

int gsvArmor = 0;
int gsvHealth = 0;

#if !__JHEXEN__
int gsvKills = 0;
int gsvItems = 0;
int gsvSecrets = 0;
#endif

int gsvCurrentWeapon;
int gsvWeapons[NUMWEAPONS];
int gsvKeys[NUMKEYS];

#if !__JHEXEN__
int gsvAmmo[NUMAMMO];
#else
int gsvAmmo[NUMMANA];
#endif

char *gsvMapName = "N/A";

#if __JHERETIC__ || __JHEXEN__
int gsvArtifacts[NUMARTIFACTS];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

cvar_t gamestatusCVars[] =
{
   {"game-state", READONLYCVAR, CVT_INT, &gamestate, 0, 0,
       "Current game state."},
   {"game-state-level", READONLYCVAR, CVT_INT, &gsvInLevel, 0, 0,
       "1=Currently playing a level."},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0,
       "1=Game paused."},
   {"game-skill", READONLYCVAR, CVT_INT, &gameskill, 0, 0,
       "Current skill level."},

   {"map-id", READONLYCVAR, CVT_INT, &gamemap, 0, 0,
       "Current map id."},
   {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapName, 0, 0,
       "Current map name."},
   {"map-episode", READONLYCVAR, CVT_INT, &gameepisode, 0, 0,
       "Current episode."},
#if __JDOOM__
   {"map-mission", READONLYCVAR, CVT_INT, &gamemission, 0, 0,
       "Current mission."},
#endif
#if __JHEXEN__ || __JSTRIFE__
   {"map-hub", READONLYCVAR, CVT_INT, &maphub, 0, 0,
       "Current hub."},
#endif
   {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0,
       "Currently playing music (id)."},
   {"map-music", READONLYCVAR, CVT_INT, &gsvMapMusic, 0, 0,
       "Music (id) for current map."},
#if !__JHEXEN__
   {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0,
       "Current number of kills."},
   {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0,
       "Current number of items."},
   {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0,
       "Current number of discovered secrets."},
#endif

   {"player-health", READONLYCVAR, CVT_INT, &gsvHealth, 0, 0,
       "Current health ammount."},
   {"player-armor", READONLYCVAR, CVT_INT, &gsvArmor, 0, 0,
       "Current armor ammount."},
   {"player-weapons-current", READONLYCVAR, CVT_INT, &gsvCurrentWeapon, 0, 0,
       "Current weapon (id)"},

#if __JDOOM__
   // Ammo
   {"player-ammo-bullets", READONLYCVAR, CVT_INT, &gsvAmmo[am_clip], 0, 0,
       "Current number of bullets."},
   {"player-ammo-shells", READONLYCVAR, CVT_INT, &gsvAmmo[am_shell], 0, 0,
       "Current number of shells."},
   {"player-ammo-cells", READONLYCVAR, CVT_INT, &gsvAmmo[am_cell], 0, 0,
       "Current number of cells."},
   {"player-ammo-missiles", READONLYCVAR, CVT_INT, &gsvAmmo[am_misl], 0, 0,
       "Current number of missiles."},
   // Weapons
   {"player-weapons-fist", READONLYCVAR, CVT_INT, &gsvWeapons[wp_fist], 0, 0,
       "1 = Player has fist."},
   {"player-weapons-pistol", READONLYCVAR, CVT_INT, &gsvWeapons[wp_pistol], 0, 0,
       "1 = Player has pistol."},
   {"player-weapons-shotgun", READONLYCVAR, CVT_INT, &gsvWeapons[wp_shotgun], 0, 0,
       "1 = Player has shotgun."},
   {"player-weapons-chaingun", READONLYCVAR, CVT_INT, &gsvWeapons[wp_chaingun], 0, 0,
       "1 = Player has chaingun."},
   {"player-weapons-mlauncher", READONLYCVAR, CVT_INT, &gsvWeapons[wp_missile], 0, 0,
       "1 = Player has missile launcher."},
   {"player-weapons-plasmarifle", READONLYCVAR, CVT_INT, &gsvWeapons[wp_plasma], 0, 0,
       "1 = Player has plasma rifle."},
   {"player-weapons-bfg", READONLYCVAR, CVT_INT, &gsvWeapons[wp_bfg], 0, 0,
       "1 = Player has BFG."},
   {"player-weapons-chainsaw", READONLYCVAR, CVT_INT, &gsvWeapons[wp_chainsaw], 0, 0,
       "1 = Player has chainsaw."},
   {"player-weapons-sshotgun", READONLYCVAR, CVT_INT, &gsvWeapons[wp_supershotgun], 0, 0,
       "1 = Player has super shotgun."},
   {"player-keycards-blue", READONLYCVAR, CVT_INT, &gsvKeys[it_bluecard], 0, 0,
       "1 = Player has blue keycard."},
   {"player-keycards-yellow", READONLYCVAR, CVT_INT, &gsvKeys[it_yellowcard], 0, 0,
       "1 = Player has yellow keycard."},
   // Keys
   {"player-keycards-red", READONLYCVAR, CVT_INT, &gsvKeys[it_redcard], 0, 0,
       "1 = Player has red keycard."},
   {"player-skullkeys-blue", READONLYCVAR, CVT_INT, &gsvKeys[it_blueskull], 0, 0,
       "1 = Player has blue skullkey."},
   {"player-skullkeys-yellow", READONLYCVAR, CVT_INT, &gsvKeys[it_yellowskull], 0, 0,
       "1 = Player has yellow skullkey."},
   {"player-skullkeys-red", READONLYCVAR, CVT_INT, &gsvKeys[it_redskull], 0, 0,
       "1 = Player has red skullkey."},
#elif __JHERETIC__
   // Ammo
   {"player-ammo-goldwand", READONLYCVAR, CVT_INT, &gsvAmmo[am_goldwand], 0, 0,
       "Current amount of ammo for the goldwand."},
   {"player-ammo-crossbow", READONLYCVAR, CVT_INT, &gsvAmmo[am_crossbow], 0, 0,
       "Current amount of ammo for the crossbow."},
   {"player-ammo-dragonclaw", READONLYCVAR, CVT_INT, &gsvAmmo[am_blaster], 0, 0,
       "Current amount of ammo for the Dragon Claw."},
   {"player-ammo-hellstaff", READONLYCVAR, CVT_INT, &gsvAmmo[am_skullrod], 0, 0,
       "Current amount of ammo for the Hell Staff."},
   {"player-ammo-phoenixrod", READONLYCVAR, CVT_INT, &gsvAmmo[am_phoenixrod], 0, 0,
       "Current amount of ammo for the Phoenix Rod."},
   {"player-ammo-mace", READONLYCVAR, CVT_INT, &gsvAmmo[am_mace], 0, 0,
       "Current amount of ammo for the mace."},
    // Weapons
   {"player-weapons-staff", READONLYCVAR, CVT_INT, &gsvWeapons[wp_staff], 0, 0,
       "1 = Player has staff."},
   {"player-weapons-goldwand", READONLYCVAR, CVT_INT, &gsvWeapons[wp_goldwand], 0, 0,
       "1 = Player has goldwand."},
   {"player-weapons-crossbow", READONLYCVAR, CVT_INT, &gsvWeapons[wp_crossbow], 0, 0,
       "1 = Player has crossbow."},
   {"player-weapons-dragonclaw", READONLYCVAR, CVT_INT, &gsvWeapons[wp_blaster], 0, 0,
       "1 = Player has the Dragon Claw."},
   {"player-weapons-hellstaff", READONLYCVAR, CVT_INT, &gsvWeapons[wp_skullrod], 0, 0,
       "1 = Player has the Hell Staff."},
   {"player-weapons-phoenixrod", READONLYCVAR, CVT_INT, &gsvWeapons[wp_phoenixrod], 0, 0,
       "1 = Player has the Phoenix Rod."},
   {"player-weapons-mace", READONLYCVAR, CVT_INT, &gsvWeapons[wp_mace], 0, 0,
       "1 = Player has mace."},
   {"player-weapons-gauntlets", READONLYCVAR, CVT_INT, &gsvWeapons[wp_gauntlets], 0, 0,
       "1 = Player has gauntlets."},
   // Keys
   {"player-keys-yellow", READONLYCVAR, CVT_INT, &gsvKeys[key_yellow], 0, 0,
       "1 = Player has yellow key."},
   {"player-keys-green", READONLYCVAR, CVT_INT, &gsvKeys[key_green], 0, 0,
       "1 = Player has green key."},
   {"player-keys-blue", READONLYCVAR, CVT_INT, &gsvKeys[key_blue], 0, 0,
       "1 = Player has blue key."},
   // Artifacts
   {"player-artifacts-ring", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invulnerability], 0, 0,
       "Current number of Rings of Invincibility."},
   {"player-artifacts-shadowsphere", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invisibility], 0, 0,
       "Current number of Shadowsphere artifacts."},
   {"player-artifacts-crystalvial", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_health], 0, 0,
       "Current number of Crystal Vials."},
   {"player-artifacts-mysticurn", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_superhealth], 0, 0,
       "Current number of Mystic Urn artifacts."},
   {"player-artifacts-tomeofpower", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_tomeofpower], 0, 0,
       "Current number of Tome of Power artifacts."},
   {"player-artifacts-torch", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_torch], 0, 0,
       "Current number of torches."},
   {"player-artifacts-firebomb", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_firebomb], 0, 0,
       "Current number of Time Bombs Of The Ancients."},
   {"player-artifacts-egg", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_egg], 0, 0,
       "Current number of Morph Ovum artifacts."},
   {"player-artifacts-wings", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_fly], 0, 0,
       "Current number of Wings of Wrath artifacts."},
   {"player-artifacts-chaosdevice", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleport], 0, 0,
       "Current number of Chaos Devices."},
#elif __JHEXEN__
   // Mana
   {"player-mana-blue", READONLYCVAR, CVT_INT, &gsvAmmo[MANA_1], 0, 0,
       "Current amount of blue mana."},
   {"player-mana-green", READONLYCVAR, CVT_INT, &gsvAmmo[MANA_2], 0, 0,
       "Current ammount of green mana."},
   // Keys
   {"player-keys-steel", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_1], 0, 0,
       "1 = Player has steel key."},
   {"player-keys-cave", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_2], 0, 0,
       "1 = Player has cave key."},
   {"player-keys-axe", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_3], 0, 0,
       "1 = Player has axe key."},
   {"player-keys-fire", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_4], 0, 0,
       "1 = Player has fire key."},
   {"player-keys-emerald", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_5], 0, 0,
       "1 = Player has emerald key."},
   {"player-keys-dungeon", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_6], 0, 0,
       "1 = Player has dungeon key."},
   {"player-keys-silver", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_7], 0, 0,
       "1 = Player has silver key."},
   {"player-keys-rusted", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_8], 0, 0,
       "1 = Player has rusted key."},
   {"player-keys-horn", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_9], 0, 0,
       "1 = Player has horn key."},
   {"player-keys-swamp", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_A], 0, 0,
       "1 = Player has swamp key."},
   {"player-keys-castle", READONLYCVAR, CVT_INT, &gsvKeys[KKEY_B], 0, 0,
       "1 = Player has castle key."},
   // Weapons
   {"player-weapons-first", READONLYCVAR, CVT_INT, &gsvWeapons[WP_FIRST], 0, 0,
       "1 = Player has first weapon."},
   {"player-weapons-second", READONLYCVAR, CVT_INT, &gsvWeapons[WP_SECOND], 0, 0,
       "1 = Player has second weapon."},
   {"player-weapons-third", READONLYCVAR, CVT_INT, &gsvWeapons[WP_THIRD], 0, 0,
       "1 = Player has third weapon."},
   {"player-weapons-fourth", READONLYCVAR, CVT_INT, &gsvWeapons[WP_FOURTH], 0, 0,
       "1 = Player has fourth weapon."},
   // Weapon Pieces
   {"player-weapons-piece1", READONLYCVAR, CVT_INT, &gsvWPieces[0], 0, 0,
       "1 = Player has piece 1."},
   {"player-weapons-piece2", READONLYCVAR, CVT_INT, &gsvWPieces[1], 0, 0,
       "1 = Player has piece 2."},
   {"player-weapons-piece3", READONLYCVAR, CVT_INT, &gsvWPieces[2], 0, 0,
       "1 = Player has piece 3."},
   {"player-weapons-allpieces", READONLYCVAR, CVT_INT, &gsvWPieces[3], 0, 0,
       "1 = Player has all pieces."},
   // Artifacts
   {"player-artifacts-defender", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invulnerability], 0, 0,
       "Current number of Icons Of The Defender."},
   {"player-artifacts-quartzflask", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_health], 0, 0,
       "Current number of Quartz Flasks."},
   {"player-artifacts-mysticurn", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_superhealth], 0, 0,
       "Current number of Mystic Urn artifacts."},
   {"player-artifacts-mysticambit", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_healingradius], 0, 0,
       "Current number of Mystic Ambit Incantations."},
   {"player-artifacts-darkservant", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_summon], 0, 0,
       "Current number of Dark Servant artifacts."},
   {"player-artifacts-torch", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_torch], 0, 0,
       "Current number of torches."},
   {"player-artifacts-porkalator", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_egg], 0, 0,
       "Current number of Porkalaor artifacts."},
   {"player-artifacts-wings", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_fly], 0, 0,
       "Current number of Wings of Wrath artifacts."},
   {"player-artifacts-repulsion", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_blastradius], 0, 0,
       "Current number of Discs Of Repulsion."},
   {"player-artifacts-flechette", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_poisonbag], 0, 0,
       "Current number of Flechettes."},
   {"player-artifacts-banishment", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleportother], 0, 0,
       "Current number of Banishment Devices."},
   {"player-artifacts-speed", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_speed], 0, 0,
       "Current number of Boots of Speed."},
   {"player-artifacts-might", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_boostmana], 0, 0,
       "Current number of Kraters Of Might."},
   {"player-artifacts-bracers", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_boostarmor], 0, 0,
       "Current number of Dragonskin Bracers."},
   {"player-artifacts-chaosdevice", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleport], 0, 0,
       "Current number of Chaos Devices."},
   {"player-artifacts-skull", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzskull], 0, 0,
       "1 = Player has Yorick's Skull."},
   {"player-artifacts-heart", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgembig], 0, 0,
       "1 = Player has Heart Of D'Sparil."},
   {"player-artifacts-ruby", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemred], 0, 0,
       "1 = Player has Ruby Planet."},
   {"player-artifacts-emerald1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemgreen1], 0, 0,
       "1 = Player has Emerald Planet 1."},
   {"player-artifacts-emerald2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemgreen2], 0, 0,
       "1 = Player has Emerald Planet 2."},
   {"player-artifacts-sapphire1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemblue1], 0, 0,
       "1 = Player has Sapphire Planet 1."},
   {"player-artifacts-sapphire2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemblue2], 0, 0,
       "1 = Player has Sapphire Planet 2."},
   {"player-artifacts-daemoncodex", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzbook1], 0, 0,
       "1 = Player has Daemon Codex."},
   {"player-artifacts-liberoscura", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzbook2], 0, 0,
       "1 = Player has Liber Oscura."},
   {"player-artifacts-flamemask", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzskull2], 0, 0,
       "1 = Player has Flame Mask."},
   {"player-artifacts-glaiveseal", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzfweapon], 0, 0,
       "1 = Player has Glaive Seal."},
   {"player-artifacts-holyrelic", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzcweapon], 0, 0,
       "1 = Player has Holy Relic."},
   {"player-artifacts-sigilmagus", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzmweapon], 0, 0,
       "1 = Player has Sigil of the Magus."},
   {"player-artifacts-gear1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear1], 0, 0,
       "1 = Player has Clock Gear 1."},
   {"player-artifacts-gear2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear2], 0, 0,
       "1 = Player has Clock Gear 2."},
   {"player-artifacts-gear3", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear3], 0, 0,
       "1 = Player has Clock Gear 3."},
   {"player-artifacts-gear4", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear4], 0, 0,
       "1 = Player has Clock Gear 4."},
#endif
   {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHEXEN__ || __JSTRIFE__
static skill_t TempSkill;
static int TempEpisode;
static int TempMap;
static int GameLoadSlot;
#endif

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int     i;

    for(i = 0; gamestatusCVars[i].name; i++)
        Con_AddVariable(gamestatusCVars + i);
}

/*
 *  Common Pre Engine Initialization routine.
 *    Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_PreInit(void)
{
    int i;

#ifdef TIC_DEBUG
    rndDebugfile = fopen("rndtrace.txt", "wt");
#endif

    // Make sure game.dll isn't newer than Doomsday...
    if(gi.version < DOOMSDAY_VERSION)
        Con_Error(GAMENAMETEXT " requires at least Doomsday " DOOMSDAY_VERSION_TEXT
                  "!\n");

    // Setup the DGL interface.
    G_InitDGL();

    // Setup the players.
    for(i = 0; i < MAXPLAYERS; i++)
    {
        players[i].plr = DD_GetPlayer(i);
        players[i].plr->extradata = (void *) &players[i];
    }

    DD_SetConfigFile( CONFIGFILE );
    DD_SetDefsFile( DEFSFILE );
    R_SetDataPath( DATAPATH );

    R_SetBorderGfx(borderLumps);
    Con_DefineActions(actions);

    DD_SetVariable(DD_SKYFLAT_NAME, SKYFLATNAME);

    G_BindClassRegistration();

    // Add the cvars and ccmds to the console databases
    G_ConsoleRegistration();    // main command list
    D_NetConsoleRegistration(); // for network
    G_Register();               // read-only game status cvars (for playsim)
    AM_Register();              // for the automap
    MN_Register();              // for the menu
    HUMsg_Register();           // for the message buffer/chat widget
    ST_Register();              // for the hud/statusbar
    X_Register();               // for the crosshair

    DD_AddStartupWAD( STARTUPWAD );
    DetectIWADs();
}

/*
 *  Common Post Engine Initialization routine.
 *    Game-specific post init actions should be placed in eg D_PostInit() (for jDoom)
 */
void G_PostInit(void)
{
    // Init the save system and create the game save directory
    SV_Init();

#ifndef __JHEXEN__
    XG_ReadTypes();
    XG_Register();              // register XG classnames
#endif

    G_DefaultBindings();
    R_SetViewSize(cfg.screenblocks, 0);
    G_SetGlowing();

    Con_Message("P_Init: Init Playloop state.\n");
    P_Init();

    Con_Message("HU_Init: Setting up heads up display.\n");
    HU_Init();

    Con_Message("ST_Init: Init status bar.\n");
    ST_Init();

    Con_Message("MN_Init: Init miscellaneous info.\n");
    MN_Init();
}

/*
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void)
{

    char   *name = "title";
    void   *script;

    G_StopDemo();
    usergame = false;

    // The title script must always be defined.
    if(!Def_Get(DD_DEF_FINALE, name, &script))
    {
        Con_Error("G_StartTitle: Script \"%s\" not defined.\n", name);
    }

    FI_Start(script, FIMODE_LOCAL);

}

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
int findWeapon(player_t *plr, boolean forward)
{
    int     i, c;

    for(i = plr->readyweapon + (forward ? 1 : -1), c = 0;
#  if __JHERETIC__
        c < NUMWEAPONS - 1; c++, forward ? i++ : i--)
    {
        if(i >= NUMWEAPONS - 1)
            i = 0;
        if(i < 0)
            i = NUMWEAPONS - 2;
#  elif __JHEXEN__ || __JSTRIFE__
        c < NUMWEAPONS; c++, forward ? i++ : i--)
    {
        if(i > NUMWEAPONS - 1)
            i = 0;
        if(i < 0)
            i = NUMWEAPONS - 1;
#  endif
        if(plr->weaponowned[i])
            return i;
    }
    return plr->readyweapon;
}

boolean inventoryMove(player_t *plr, int dir)
{
    inventoryTics = 5 * 35;
    if(!inventory)
    {
        inventory = true;
        return (false);
    }

    if(dir == 0)
    {
        inv_ptr--;
        if(inv_ptr < 0)
        {
            inv_ptr = 0;
        }
        else
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
    }
    else
    {
        inv_ptr++;
        if(inv_ptr >= plr->inventorySlotNum)
        {
            inv_ptr--;
            if(inv_ptr < 0)
                inv_ptr = 0;
        }
        else
        {
            curpos++;
            if(curpos > 6)
            {
                curpos = 6;
            }
        }
    }
    return (true);
}
#endif

void G_DoLoadLevel(void)
{
    action_t *act;
    int     i;
    char   *lname, *ptr;

#if __JHEXEN__ || __JSTRIFE__
    static int firstFragReset = 1;
#endif

    levelstarttic = gametic;    // for time calculation
    gamestate = GS_LEVEL;

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) ||
            firstFragReset == 1)
        {
            memset(players[i].frags, 0, sizeof(players[i].frags));
            firstFragReset = 0;
        }
#else
        memset(players[i].frags, 0, sizeof(players[i].frags));
#endif
    }

#if __JHEXEN__ || __JSTRIFE__
    SN_StopAllSequences();
#endif

    // Set all player mobjs to NULL.
    for(i = 0; i < MAXPLAYERS; i++)
        players[i].plr->mo = NULL;

    P_SetupLevel(gameepisode, gamemap, 0, gameskill);
    Set(DD_DISPLAYPLAYER, consoleplayer);   // view the guy you are playing
    starttime = Sys_GetTime();
    gameaction = ga_nothing;
    Z_CheckHeap();

    // clear cmd building stuff
    G_ResetMousePos();
    sendpause = paused = false;

    // Deactivate all action keys.
    for(act = actions; act->name[0]; act++)
        act->on = false;

    // set the game status cvar for map name
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    if(lname)
    {
        ptr = strchr(lname, ':');   // Skip the E#M# or Level #.
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
    }
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name
    if(!lname)
        lname = P_GetMapName(gamemap);
#endif
    // If still no name, call it unnamed.
    if(!lname)
        lname = "unnamed";

    // Set the map name
    gsvMapName = lname;

    // Start a briefing, if there is one.
    FI_Briefing(gameepisode, gamemap);
}

/*
 * Get info needed to make ticcmd_ts for the players.
 * Return false if the event should be checked for bindings.
 */
boolean G_Responder(event_t *ev)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    // DJS - Why is this here??
    player_t *plr = &players[consoleplayer];

    if(!actions[A_USEARTIFACT].on)
    {                           // flag to denote that it's okay to use an artifact
        if(!inventory)
        {
            plr->readyArtifact = plr->inventory[inv_ptr].type;
        }
        usearti = true;
    }

#endif
    // any key pops up menu if in demos
    if(gameaction == ga_nothing && !singledemo && !menuactive &&
       (Get(DD_PLAYBACK) || FI_IsMenuTrigger(ev)))
    {
        if(ev->type == ev_keydown || ev->type == ev_mousebdown ||
           ev->type == ev_joybdown)
        {
            M_StartControlPanel();
            return true;
        }
        return false;
    }

    // Try Infine
    if(FI_Responder(ev))
        return true;

    // Try the chatmode responder
    if(HU_Responder(ev))
        return true;

    // Check for cheats
    if(cht_Responder(ev))
        return true;

    // Try the edit responder
    if(M_EditResponder(ev))
        return true;

    // We may wish to eat the event depending on type...
    if(G_AdjustControlState(ev))
        return true;

    // The event wasn't used.
    return false;
}

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
void G_InventoryTicker(void)
{
    if(!players[consoleplayer].plr->ingame)
        return;

    // turn inventory off after a certain amount of time
    if(inventory && !(--inventoryTics))
    {
        players[consoleplayer].readyArtifact =
            players[consoleplayer].inventory[inv_ptr].type;
        inventory = false;
        /*#if __JHEXEN__
           cmd->arti = 0;
           #endif */
    }
}
#endif

void G_SpecialButton(player_t *pl)
{
    if(pl->plr->ingame)
    {
        if(pl->cmd.pause)
        {
            paused ^= 1;
            if(paused)
            {
                // This will stop all sounds from all origins.
                S_StopSound(0, 0);
            }

            // Servers are responsible for informing clients about
            // pauses in the game.
            NetSv_Paused(paused);

            pl->cmd.pause = 0;
        }
    }
}

/*
 * The core of the game timing loop.
 * Game state, game actions etc occur here.
 */
void G_Ticker(void)
{
    int     i;
    player_t *plyr = &players[consoleplayer];
    static gamestate_t oldgamestate = -1;

    if(IS_CLIENT && !Get(DD_GAME_READY))
        return;

#if _DEBUG
    Z_CheckHeap();
#endif
    // do player reborns if needed
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame && players[i].playerstate == PST_REBORN)
            G_DoReborn(i);

        // Player has left?
        if(players[i].playerstate == PST_GONE)
        {
            players[i].playerstate = PST_REBORN;
            if(!IS_CLIENT)
            {
                P_SpawnTeleFog(players[i].plr->mo->x, players[i].plr->mo->y);
            }
            // Let's get rid of the mobj.
#if _DEBUG
            Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
            P_RemoveMobj(players[i].plr->mo);
            players[i].plr->mo = NULL;
        }
    }

    // do things to change the game state
    while(gameaction != ga_nothing)
    {
        switch (gameaction)
        {
#if __JHEXEN__ || __JSTRIFE__
        case ga_initnew:
            G_DoInitNew();
            break;
        case ga_singlereborn:
            G_DoSingleReborn();
            break;
        case ga_leavemap:
            Draw_TeleportIcon();
            G_DoTeleportNewMap();
            break;
#endif
        case ga_loadlevel:
            G_DoLoadLevel();
            break;
        case ga_newgame:
            G_DoNewGame();
            break;
        case ga_loadgame:
            G_DoLoadGame();
            break;
        case ga_savegame:
            G_DoSaveGame();
            break;
        case ga_playdemo:
            G_DoPlayDemo();
            break;
        case ga_completed:
            G_DoCompleted();
            break;
        case ga_victory:
            gameaction = ga_nothing;
            break;
        case ga_worlddone:
            G_DoWorldDone();
            break;
        case ga_screenshot:
            G_DoScreenShot();
            gameaction = ga_nothing;
            break;
        case ga_nothing:
            break;
        }
    }

    // Update the viewer's look angle
    G_LookAround();

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    G_InventoryTicker();
#endif

    // Enable/disable sending of frames (delta sets) to clients.
    Set(DD_ALLOW_FRAMES, gamestate == GS_LEVEL);
    if(!IS_CLIENT)
    {
        // Tell Doomsday when the game is paused (clients can't pause
        // the game.)
        Set(DD_CLIENT_PAUSED, P_IsPaused());
    }

    // Must be called on every tick.
    P_RunPlayers();

    // Do main actions.
    switch (gamestate)
    {
    case GS_LEVEL:
        // update in-level game status cvar
        if(oldgamestate != GS_LEVEL)
            gsvInLevel = 1;

        P_DoTick();
        HU_UpdatePsprites();

        // Active briefings once again (they were disabled when loading
        // a saved game).
        brief_disabled = false;

        if(IS_DEDICATED)
            break;

        ST_Ticker();
        AM_Ticker();
        HU_Ticker();

        break;

    case GS_INTERMISSION:
#if __JDOOM__
        WI_Ticker();
#else
        IN_Ticker();
#endif

    default:
        if(oldgamestate != gamestate)
        {
            // update game status cvars
            gsvInLevel = 0;
            gsvMapName = "N/A";
            gsvMapMusic = -1;
        }
        break;
    }

    oldgamestate = gamestate;

    // DJS 07/05/05
    // Update the game status cvars for player data
    if(plyr)
    {
        gsvHealth = plyr->health;
#if !__JHEXEN__
        // Level stats
        gsvKills = plyr->killcount;
        gsvItems = plyr->itemcount;
        gsvSecrets = plyr->secretcount;
#endif
        // armor
#if __JHEXEN__
        gsvArmor = FixedDiv(
        AutoArmorSave[plyr->class] + plyr->armorpoints[ARMOR_ARMOR] +
        plyr->armorpoints[ARMOR_SHIELD] +
        plyr->armorpoints[ARMOR_HELMET] +
        plyr->armorpoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
        gsvArmor = plyr->armorpoints;
#endif
        // owned keys
#if __JHEXEN__
        for( i= 0; i < NUMKEYS; i++)
            gsvKeys[i] = (plyr->keys & (1 << i))? 1 : 0;
#else
        for( i= 0; i < NUMKEYS; i++)
            gsvKeys[i] = plyr->keys[i];
#endif
        // current weapon
        gsvCurrentWeapon = plyr->readyweapon;

        // owned weapons
        for( i= 0; i < NUMWEAPONS; i++)
            gsvWeapons[i] = plyr->weaponowned[i];

#if __JHEXEN__
        // mana amounts
        for( i= 0; i < NUMMANA; i++)
            gsvAmmo[i] = plyr->mana[i];

        // weapon pieces
        gsvWPieces[0] = (plyr->pieces & WPIECE1)? 1 : 0;
        gsvWPieces[1] = (plyr->pieces & WPIECE2)? 1 : 0;
        gsvWPieces[2] = (plyr->pieces & WPIECE3)? 1 : 0;
        gsvWPieces[3] = (plyr->pieces == 7)? 1 : 0;
#else
        // current ammo amounts
        for( i= 0; i < NUMAMMO; i++)
            gsvAmmo[i] = plyr->ammo[i];
#endif

#if __JHERETIC__ || __JHEXEN__
        // artifacts
        for( i= 0; i < NUMINVENTORYSLOTS; i++)
            gsvArtifacts[plyr->inventory[i].type] = plyr->inventory[i].count;
#endif
    }

    // InFine ticks whenever it's active.
    FI_Ticker();

    // Servers will have to update player information and do such stuff.
    if(!IS_CLIENT)
        NetSv_Ticker();
}

/*
 * Called at the start.
 * Called by the game initialization functions.
 */
void G_InitPlayer(int player)
{
    player_t *p;

    // set up the saved info
    p = &players[player];

    // clear everything else to defaults
    G_PlayerReborn(player);
}


#if __JHEXEN__ || __JSTRIFE__

/*
 * Called when the player leaves a map.
 */
void G_PlayerExitMap(int playerNumber)
{
    int     i;
    player_t *player;
    int     flightPower;

    player = &players[playerNumber];

    // Strip all current powers (retain flight)
    flightPower = player->powers[pw_flight];
    memset(player->powers, 0, sizeof(player->powers));
    player->powers[pw_flight] = flightPower;
    player->update |= PSF_POWERS;

    if(deathmatch)
    {
        player->powers[pw_flight] = 0;
    }
    else
    {
        if(P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap))
        {                       // Entering new cluster
            // Strip all keys
            player->keys = 0;

            // Strip flight artifact
            for(i = 0; i < 25; i++)
            {
                player->powers[pw_flight] = 0;
                P_PlayerUseArtifact(player, arti_fly);
            }
            player->powers[pw_flight] = 0;
        }
    }

    player->update |= PSF_MORPH_TIME;
    if(player->morphTics)
    {
        player->readyweapon = player->plr->mo->special1;    // Restore weapon
        player->morphTics = 0;
    }

    player->messageTics = 0;
    player->plr->lookdir = 0;
    player->plr->mo->flags &= ~MF_SHADOW;   // Remove invisibility
    player->plr->extralight = 0;    // Remove weapon flashes
    player->plr->fixedcolormap = 0; // Remove torch

    // Clear filter.
    player->plr->filter = 0;
    player->plr->flags |= DDPF_FILTER;
    player->damagecount = 0;    // No palette changes
    player->bonuscount = 0;
    player->poisoncount = 0;
}

#else

/*
 * Called when a player completes a level.
 */
void G_PlayerFinishLevel(int player)
{
    player_t *p = &players[player];

#  if __JHERETIC__
    int     i;

    for(i = 0; i < p->inventorySlotNum; i++)
    {
        p->inventory[i].count = 1;
    }
    p->artifactCount = p->inventorySlotNum;
    if(!deathmatch)
        for(i = 0; i < 16; i++)
        {
            P_PlayerUseArtifact(p, arti_fly);
        }
#  endif

    p->update |= PSF_POWERS | PSF_KEYS;
    memset(p->powers, 0, sizeof(p->powers));
    memset(p->keys, 0, sizeof(p->keys));

#  if __JHERETIC__
    playerkeys = 0;
    p->update |= PSF_CHICKEN_TIME;
    if(p->chickenTics)
    {
        p->readyweapon = p->plr->mo->special1;  // Restore weapon
        p->chickenTics = 0;
    }

    p->messageTics = 0;
    p->rain1 = NULL;
    p->rain2 = NULL;
#  endif

    p->plr->lookdir = 0;
    p->plr->mo->flags &= ~MF_SHADOW;    // cancel invisibility
    p->plr->extralight = 0;     // cancel gun flashes
    p->plr->fixedcolormap = 0;  // cancel ir gogles

    // Clear filter.
    p->plr->filter = 0;
    p->plr->flags |= DDPF_FILTER;
    p->damagecount = 0;         // no palette changes
    p->bonuscount = 0;
}
#endif

/*
 * Safely clears the player data structures.
 */
void ClearPlayer(player_t *p)
{
    ddplayer_t *ddplayer = p->plr;
    int     playeringame = ddplayer->ingame;
    int     flags = ddplayer->flags;
    int     start = p->startspot;

    memset(p, 0, sizeof(*p));
    // Restore the pointer to ddplayer.
    p->plr = ddplayer;
    // Also clear ddplayer.
    memset(ddplayer, 0, sizeof(*ddplayer));
    // Restore the pointer to this player.
    ddplayer->extradata = p;
    // Restore the playeringame data.
    ddplayer->ingame = playeringame;
    ddplayer->flags = flags;
    // Don't clear the start spot.
    p->startspot = start;
}

/*
 * Called after a player dies
 * almost everything is cleared and initialized
 */
void G_PlayerReborn(int player)
{
    player_t *p;
    int     frags[MAXPLAYERS];
    int     killcount;
    int     itemcount;
    int     secretcount;

#if __JDOOM__ || __JHERETIC__
    int     i;
#endif
#if __JHERETIC__
    boolean secret = false;
    int     spot;
#elif __JHEXEN__ || __JSTRIFE__
    uint    worldTimer;
#endif

    memcpy(frags, players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;
#if __JHEXEN__ || __JSTRIFE__
    worldTimer = players[player].worldTimer;
#endif

    p = &players[player];
#if __JHERETIC__
    if(p->didsecret)
        secret = true;
    spot = p->startspot;
#endif

    // Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
    p->startspot = spot;
#endif

    memcpy(players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;
#if __JHEXEN__ || __JSTRIFE__
    players[player].worldTimer = worldTimer;
    players[player].colormap = cfg.PlayerColor[player];
#endif
#if __JHEXEN__
    players[player].class = cfg.PlayerClass[player];
#endif
    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;

#if __JDOOM__
    p->readyweapon = p->pendingweapon = wp_pistol;
    p->weaponowned[wp_fist] = true;
    p->weaponowned[wp_pistol] = true;
    p->ammo[am_clip] = 50;

    // See if the Values specify anything.
    P_InitPlayerValues(p);

#elif __JHERETIC__
    p->readyweapon = p->pendingweapon = wp_goldwand;
    p->weaponowned[wp_staff] = true;
    p->weaponowned[wp_goldwand] = true;
    p->ammo[am_goldwand] = 50;

    if(gamemap == 9 || secret)
    {
        p->didsecret = true;
    }

#else
    p->readyweapon = p->pendingweapon = WP_FIRST;
    p->weaponowned[WP_FIRST] = true;
    localQuakeHappening[player] = false;

#endif

#if __JDOOM__ || __JHERETIC__
    // Reset maxammo.
    for(i = 0; i < NUMAMMO; i++)
        p->maxammo[i] = maxammo[i];
#endif

#if __JDOOM__
    // We'll need to update almost everything.
    p->update |= PSF_REBORN;

#elif __JHERETIC__
    if(p == &players[consoleplayer])
    {
        inv_ptr = 0;            // reset the inventory pointer
        curpos = 0;
    }
    // We'll need to update almost everything.
    p->update |=
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;

#elif __JHEXEN__ || __JSTRIFE__
    if(p == &players[consoleplayer])
    {
        SB_state = -1;          // refresh the status bar
        inv_ptr = 0;            // reset the inventory pointer
        curpos = 0;
    }
    // We'll need to update almost everything.
    p->update |= PSF_REBORN;
#endif

    p->plr->flags &= ~DDPF_DEAD;
}

/*
 * Spawns a player at one of the random death match spots
 * called at level load and each death
 */
void G_DeathMatchSpawnPlayer(int playernum)
{
    int     i, j;
    int     selections;
    thing_t faraway;
    boolean using_dummy = false;
    ddplayer_t *pl = players[playernum].plr;

    // Spawn player initially at a distant location.
    if(!pl->mo)
    {
        faraway.x = faraway.y = DDMAXSHORT;
        faraway.angle = 0;
        P_SpawnPlayer(&faraway, playernum);
        using_dummy = true;
    }

    // Now let's find an available deathmatch start.
    selections = deathmatch_p - deathmatchstarts;
    if(selections < 2)
        Con_Error("Only %i deathmatch spots, 2 required", selections);

    for(j = 0; j < 20; j++)
    {
        i = P_Random() % selections;
        if(P_CheckSpot(playernum, &deathmatchstarts[i], true))
        {
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
            deathmatchstarts[i].type = playernum + 1;
#endif
            break;
        }
    }
    if(using_dummy)
    {
        // Destroy the dummy.
        P_RemoveMobj(pl->mo);
        pl->mo = NULL;
    }
    P_SpawnPlayer(&deathmatchstarts[i], playernum);

#if __JDOOM__ || __JHERETIC__
    // Gib anything at the spot.
    P_Telefrag(players[playernum].plr->mo);
#endif
}

/*
 *  Spawns the given player at a dummy place.
 */
void G_DummySpawnPlayer(int playernum)
{
    thing_t faraway;

    faraway.x = faraway.y = DDMAXSHORT;
    faraway.angle = (short) 0;
    P_SpawnPlayer(&faraway, playernum);
}

#ifdef __JDOOM__
void G_QueueBody(mobj_t *body)
{
    // flush an old corpse if needed
    if(bodyqueslot >= BODYQUESIZE)
        P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]);
    bodyque[bodyqueslot % BODYQUESIZE] = body;
    bodyqueslot++;
}
#endif

void G_DoReborn(int playernum)
{
#if __JHEXEN__ || __JSTRIFE__
    int     i;
    boolean oldWeaponowned[NUMWEAPONS];
    int     oldKeys;
    int     oldPieces;
    boolean foundSpot;
    int     bestWeapon;
#else
    thing_t *assigned;
#endif

    // Clear the currently playing script, if any.
    FI_Reset();

    if(!IS_NETGAME)
    {
        // We've just died, don't do a briefing now.
        brief_disabled = true;

#if __JHEXEN__ || __JSTRIFE__
        if(SV_HxRebornSlotAvailable())
        {                       // Use the reborn code if the slot is available
            gameaction = ga_singlereborn;
        }
        else
        {                       // Start a new game if there's no reborn info
            gameaction = ga_newgame;
        }
#else
        // reload the level from scratch
        gameaction = ga_loadlevel;
#endif
    }
    else                        // Netgame
    {
        if(players[playernum].plr->mo)
        {
            // first dissasociate the corpse
            players[playernum].plr->mo->player = NULL;
            players[playernum].plr->mo->dplayer = NULL;
        }

        if(IS_CLIENT)
        {
            G_DummySpawnPlayer(playernum);
            return;
        }

        Con_Printf("G_DoReborn for %i.\n", playernum);

        // spawn at random spot if in death match
        if(deathmatch)
        {
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

#if __JHEXEN__ || __JSTRIFE__
        // Cooperative net-play, retain keys and weapons
        oldKeys = players[playernum].keys;
        oldPieces = players[playernum].pieces;
        for(i = 0; i < NUMWEAPONS; i++)
            oldWeaponowned[i] = players[playernum].weaponowned[i];
#endif

#if __JDOOM__
        assigned = &playerstarts[players[playernum].startspot];
        if(P_CheckSpot(playernum, assigned, true))
        {
            P_SpawnPlayer(assigned, playernum);
            return;
        }
#elif __JHERETIC__
        // Try to spawn at the assigned spot.
        assigned = &playerstarts[players[playernum].startspot];
        if(P_CheckSpot(playernum, assigned, true))
        {
            Con_Printf("- spawning at assigned spot %i.\n",
                       players[playernum].startspot);
            P_SpawnPlayer(assigned, playernum);
            return;
        }
#elif __JHEXEN__ || __JSTRIFE__
        foundSpot = false;
        if(P_CheckSpot
           (playernum, P_GetPlayerStart(RebornPosition, playernum), true))
        {
            // Appropriate player start spot is open
            P_SpawnPlayer(P_GetPlayerStart(RebornPosition, playernum),
                          playernum);
            foundSpot = true;
        }
        else
        {
            // Try to spawn at one of the other player start spots
            for(i = 0; i < MAXPLAYERS; i++)
            {
                if(P_CheckSpot
                   (playernum, P_GetPlayerStart(RebornPosition, i), true))
                {
                    // Found an open start spot
                    P_SpawnPlayer(P_GetPlayerStart(RebornPosition, i),
                                  playernum);
                    foundSpot = true;
                    break;
                }
            }
        }
        if(foundSpot == false)
        {
            // Player's going to be inside something
            P_SpawnPlayer(P_GetPlayerStart(RebornPosition, playernum),
                          playernum);
        }

        // Restore keys and weapons
        players[playernum].keys = oldKeys;
        players[playernum].pieces = oldPieces;
        for(bestWeapon = 0, i = 0; i < NUMWEAPONS; i++)
        {
            if(oldWeaponowned[i])
            {
                bestWeapon = i;
                players[playernum].weaponowned[i] = true;
            }
        }

        players[playernum].mana[MANA_1] = 25;
        players[playernum].mana[MANA_2] = 25;
        if(bestWeapon)
        {                       // Bring up the best weapon
            players[playernum].pendingweapon = bestWeapon;
        }
#endif

#if __JDOOM__ || __JHERETIC__
        Con_Printf("- force spawning at %i.\n", players[playernum].startspot);

        // Fuzzy returns false if it needs telefragging.
        if(!P_FuzzySpawn(assigned, playernum, true))
        {
            // Spawn at the assigned spot, telefrag whoever's there.
            P_Telefrag(players[playernum].plr->mo);
        }
#endif
    }
}

void G_ScreenShot(void)
{
    gameaction = ga_screenshot;
}

void G_DoScreenShot(void)
{
    int     i;
    filename_t name;
    char   *numPos;

    // Use game mode as the file name base.
    sprintf(name, "%s-", G_Get(DD_GAME_MODE));
    numPos = name + strlen(name);

    // Find an unused file name.
    for(i = 0; i < 1e6; i++)    // Stop eventually...
    {
        sprintf(numPos, "%03i.tga", i);
        if(!M_FileExists(name))
            break;
    }
    M_ScreenShot(name, 24);
    Con_Message("Wrote %s.\n", name);
}

#if __JHEXEN__ || __JSTRIFE__
void G_StartNewInit(void)
{
    SV_HxInitBaseSlot();
    SV_HxClearRebornSlot();

#   if __JHEXEN__
    P_ACSInitNewGame();
#   endif

    // Default the player start spot group to 0
    RebornPosition = 0;
}

void G_StartNewGame(skill_t skill)
{
    int     realMap = 1;

    G_StartNewInit();
#   if __JHEXEN__
    realMap = P_TranslateMap(1);
#   elif __JSTRIFE__
    realMap = 1;
#   endif
    if(realMap == -1)
    {
        realMap = 1;
    }
    G_InitNew(TempSkill, 1, realMap);
}

/*
 * Only called by the warp cheat code.  Works just like normal map to map
 * teleporting, but doesn't do any interlude stuff.
 */
void G_TeleportNewMap(int map, int position)
{
    gameaction = ga_leavemap;
    LeaveMap = map;
    LeavePosition = position;
}

void G_DoTeleportNewMap(void)
{
    // Clients trust the server in these things.
    if(IS_CLIENT)
    {
        gameaction = ga_nothing;
        return;
    }

    SV_HxMapTeleport(LeaveMap, LeavePosition);
    gamestate = GS_LEVEL;
    gameaction = ga_nothing;
    RebornPosition = LeavePosition;

    // Is there a briefing before this map?
    FI_Briefing(gameepisode, gamemap);
}
#endif

void G_ExitLevel(void)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

    secretexit = false;
    gameaction = ga_completed;
}

void G_SecretExitLevel(void)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

#if __JDOOM__
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if((gamemode == commercial) && (W_CheckNumForName("map31") < 0))
        secretexit = false;
    else
#endif
        secretexit = true;
    gameaction = ga_completed;
}

#if __JHEXEN__ || __JSTRIFE__
/*
 * Starts intermission routine, which is used only during hub exits,
 * and DeathMatch games.
 */
void G_Completed(int map, int position)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

    if(shareware && map > 4)
    {
        // Not possible in the 4-level demo.
        P_SetMessage(&players[consoleplayer], "PORTAL INACTIVE -- DEMO");
        return;
    }

    gameaction = ga_completed;
    LeaveMap = map;
    LeavePosition = position;
}
#endif

/*
 * @return boolean  (True) If the game has been completed
 */
boolean G_IfVictory(void)
{
#if __JDOOM__
    if((gamemap == 8) && (gamemode != commercial))
    {
        gameaction = ga_victory;
        return true;
    }

#elif __JHERETIC__
    if(gamemap == 8)
    {
        gameaction = ga_victory;
        return true;
    }

#elif __JHEXEN__ || __JSTRIFE__
    if(LeaveMap == -1 && LeavePosition == -1)
    {
        gameaction = ga_victory;
        return true;
    }
#endif

    return false;
}

void G_DoCompleted(void)
{
    int     i;

#if __JHERETIC__
    static int afterSecret[5] = { 7, 5, 5, 5, 4 };
#endif

    // Clear the currently playing script, if any.
    FI_Reset();

    // Is there a debriefing for this map?
    if(FI_Debriefing(gameepisode, gamemap))
        return;

    gameaction = ga_nothing;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame)
        {
#if __JHEXEN__ || __JSTRIFE__
            G_PlayerExitMap(i);
#else
            G_PlayerFinishLevel(i); // take away cards and stuff
#endif
            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS,
                                  PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    if(automapactive)
        AM_Stop();

    // Has the player completed the game?
    if(G_IfVictory())
        return; // Victorious!

#if __JHERETIC__
    prevmap = gamemap;
    if(secretexit == true)
    {
        gamemap = 9;
    }
    else if(gamemap == 9)
    {                           // Finished secret level
        gamemap = afterSecret[gameepisode - 1];
    }
    else
    {
        // Is there an overide for nextmap? (eg from an XG line)
        if(nextmap > 0)
            gamemap = nextmap;

        gamemap++;
    }
#endif

#if __JDOOM__
    if(gamemode != commercial && gamemap == 9)
    {
        for(i = 0; i < MAXPLAYERS; i++)
            players[i].didsecret = true;
    }

    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.last = gamemap - 1;

    // wminfo.next is 0 biased, unlike gamemap
    if(gamemode == commercial)
    {
        if(secretexit)
            switch (gamemap)
            {
            case 15:
                wminfo.next = 30;
                break;
            case 31:
                wminfo.next = 31;
                break;
            }
        else
            switch (gamemap)
            {
            case 31:
            case 32:
                wminfo.next = 15;
                break;
            default:
                wminfo.next = gamemap;
            }
    }
    else
    {
        if(secretexit)
            wminfo.next = 8;    // go to secret level
        else if(gamemap == 9)
        {
            // returning from secret level
            switch (gameepisode)
            {
            case 1:
                wminfo.next = 3;
                break;
            case 2:
                wminfo.next = 5;
                break;
            case 3:
                wminfo.next = 6;
                break;
            case 4:
                wminfo.next = 2;
                break;
            }
        }
        else
            wminfo.next = gamemap;  // go to next level
    }

    // Is there an overide for wminfo.next? (eg from an XG line)
    if(nextmap > 0)
    {
        wminfo.next = nextmap -1;   // wminfo is zero based
        nextmap = 0;
    }

    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;

    G_PrepareWIData();

    // Tell the clients what's going on.
    NetSv_Intermission(IMF_BEGIN, 0, 0);
    gamestate = GS_INTERMISSION;
    viewactive = false;
    automapactive = false;
    WI_Start(&wminfo);

#elif __JHERETIC__
    // Let the clients know the next level.
    NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
    gamestate = GS_INTERMISSION;
    IN_Start();
#endif

#if __JHEXEN__ || __JSTRIFE__
    NetSv_Intermission(IMF_BEGIN, LeaveMap, LeavePosition);
    gamestate = GS_INTERMISSION;
    IN_Start();
#endif
}

#if __JDOOM__
void G_PrepareWIData(void)
{
    int     i;
    ddmapinfo_t minfo;
    char    levid[10];

    wminfo.epsd = gameepisode - 1;
    wminfo.maxfrags = 0;

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.partime > 0)
        wminfo.partime = 35 * (int) minfo.partime;

    wminfo.pnum = consoleplayer;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        wminfo.plyr[i].in = players[i].plr->ingame;
        wminfo.plyr[i].skills = players[i].killcount;
        wminfo.plyr[i].sitems = players[i].itemcount;
        wminfo.plyr[i].ssecret = players[i].secretcount;
        wminfo.plyr[i].stime = leveltime;
        memcpy(wminfo.plyr[i].frags, players[i].frags,
               sizeof(wminfo.plyr[i].frags));
    }
}
#endif

void G_WorldDone(void)
{
    gameaction = ga_worlddone;

#if __JDOOM__
    if(secretexit)
        players[consoleplayer].didsecret = true;
#endif
}

void G_DoWorldDone(void)
{
    gamestate = GS_LEVEL;
#if __JDOOM__
    gamemap = wminfo.next + 1;
#endif
    G_DoLoadLevel();
    gameaction = ga_nothing;
    viewactive = true;
}

#if __JHEXEN__ || __JSTRIFE__
/*
 * Called by G_Ticker based on gameaction.  Loads a game from the reborn
 * save slot.
 */
void G_DoSingleReborn(void)
{
    gameaction = ga_nothing;
    SV_HxLoadGame(SV_HxGetRebornSlot());
    SB_SetClassData();
}
#endif

/*
 * Can be called by the startup code or the menu task.
 */
#if __JHEXEN__ || __JSTRIFE__
void G_LoadGame(int slot)
{
    GameLoadSlot = slot;
#else
void G_LoadGame(char *name)
{
    strcpy(savename, name);
#endif
    gameaction = ga_loadgame;
}

/*
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
    G_StopDemo();
    FI_Reset();
    gameaction = ga_nothing;

#if __JHEXEN__ || __JSTRIFE__

    Draw_LoadIcon();

    SV_HxLoadGame(GameLoadSlot);
    if(!IS_NETGAME)
    {                           // Copy the base slot to the reborn slot
        SV_HxUpdateRebornSlot();
    }
    SB_SetClassData();
#else
    SV_LoadGame(savename);
#endif
}

/*
 * Called by the menu task.
 * Description is a 24 byte text string
 */
void G_SaveGame(int slot, char *description)
{
    savegameslot = slot;
    strcpy(savedescription, description);
    gameaction = ga_savegame;
}

/*
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
#if __JHEXEN__ || __JSTRIFE__
    Draw_SaveIcon();

    SV_HxSaveGame(savegameslot, savedescription);
#else
    char    name[100];

    SV_SaveGameFile(savegameslot, name);
    SV_SaveGame(name, savedescription);
#endif

    gameaction = ga_nothing;
    savedescription[0] = 0;

    P_SetMessage(players + consoleplayer, TXT_GAMESAVED);
}

#if __JHEXEN__ || __JSTRIFE__
void G_DeferredNewGame(skill_t skill)
{
    TempSkill = skill;
    gameaction = ga_newgame;
}

void G_DoInitNew(void)
{
    SV_HxInitBaseSlot();
    G_InitNew(TempSkill, TempEpisode, TempMap);
    gameaction = ga_nothing;
}
#endif

/*
 * Can be called by the startup code or the menu task,
 * consoleplayer, displayplayer, playeringame[] should be set.
 */
void G_DeferedInitNew(skill_t skill, int episode, int map)
{
#if __JHEXEN__ || __JSTRIFE__
    TempSkill = skill;
    TempEpisode = episode;
    TempMap = map;
    gameaction = ga_initnew;
#else
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    gameaction = ga_newgame;
#endif
}

void G_DoNewGame(void)
{
    G_StopDemo();
#if __JDOOM__ || __JHERETIC__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnparm = false;
        nomonsters = ArgExists("-nomonsters");  //false;
    }
    G_InitNew(d_skill, d_episode, d_map);
#else
    G_StartNewGame(TempSkill);
#endif
    gameaction = ga_nothing;
}

#if __JDOOM__
/*
 * Returns true if the specified ep/map exists in a WAD.
 */
boolean P_MapExists(int episode, int map)
{
    char    buf[20];

    P_GetMapLumpName(episode, map, buf);
    return W_CheckNumForName(buf) >= 0;
}
#endif

/*
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(int *episode, int *map)
{
    boolean ok = true;

    if(*episode < 1)
    {
        *episode = 1;
        ok = false;
    }
    if(*map < 1)
    {
        *map = 1;
        ok = false;
    }

#ifdef __JDOOM__
    if(gamemode == shareware)
    {
        // only start episode 1 on shareware
        if(*episode > 1)
        {
            *episode = 1;
            ok = false;
        }
    }
    else
    {
        // Allow episodes 1-9.
        if(*episode > 9)
        {
            *episode = 9;
            ok = false;
        }
    }
    if((*map > 9) && (gamemode != commercial))
    {
        *map = 9;
        ok = false;
    }
    // Check that the map truly exists.
    if(!P_MapExists(*episode, *map))
    {
        // (1,1) should exist always?
        *episode = 1;
        *map = 1;
        ok = false;
    }

#elif __JHERETIC__
    // Up to 9 episodes for testing
    if(*episode > 9)
    {
        *episode = 9;
        ok = false;
    }
    if(*map > 9)
    {
        *map = 9;
        ok = false;
    }

#elif __JHEXEN__ || __JSTRIFE__
    if(*map > 99)
    {
        *map = 99;
        ok = false;
    }
#endif

    return ok;
}

/*
 * Start a new game.
 */
void G_InitNew(skill_t skill, int episode, int map)
{
    int     i;

#if __JDOOM__ || __JHERETIC__
    int     speed;
#endif

    // If there are any InFine scripts running, they must be stopped.
    FI_Reset();

    if(paused)
    {
        paused = false;
    }

    if(skill < sk_baby)
        skill = sk_baby;
    if(skill > sk_nightmare)
        skill = sk_nightmare;

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    M_ClearRandom();

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
    if(respawnparm)
        respawnmonsters = true;
    else
        respawnmonsters = false;
#endif

#if __JDOOM__
    // Is respawning enabled at all in nightmare skill?
    if(skill == sk_nightmare)
    {
        respawnmonsters = cfg.respawnMonstersNightmare;
    }

    // Fast monsters?
    if(fastparm || (skill == sk_nightmare && gameskill != sk_nightmare))
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; i++)
            states[i].tics = 1;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; i++)
            states[i].tics = 4;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; i++)
            states[i].tics = 1;
    }
    else
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; i++)
            states[i].tics = 2;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; i++)
            states[i].tics = 8;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; i++)
            states[i].tics = 2;
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__
#   if __JDOOM__
    speed = (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare));
#   else
    speed = skill == sk_nightmare;
#   endif

    for(i = 0; MonsterMissileInfo[i].type != -1; i++)
    {
        mobjinfo[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[speed] << FRACBITS;
    }
#endif

    if(!IS_CLIENT)
    {
        // force players to be initialized upon first level load
        for(i = 0; i < MAXPLAYERS; i++)
        {
            players[i].playerstate = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
            players[i].worldTimer = 0;
#else
            players[i].didsecret = false;
#endif
        }
    }

    usergame = true;            // will be set false if a demo
    paused = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;
    GL_Update(DDUF_BORDER);

    NetSv_UpdateGameConfig();

    // Tell the engine if we want that all players know
    // where everybody else is.
    Set(DD_SEND_ALL_PLAYERS, !deathmatch);

    G_DoLoadLevel();

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(map);
#endif
}

void G_DeferedPlayDemo(char *name)
{
    strcpy(defdemoname, name);
    gameaction = ga_playdemo;
}

void G_DoPlayDemo(void)
{
    int     lnum = W_CheckNumForName(defdemoname);
    char   *lump;
    char    buf[128];

    gameaction = ga_nothing;

#if __JHEXEN__ || __JSTRIFE__
    if(demoDisabled)
        return;
#endif

    // The lump should contain the path of the demo file.
    if(lnum < 0 || W_LumpLength(lnum) != 64)
    {
        Con_Message("G_DoPlayDemo: invalid demo lump \"%s\".\n", defdemoname);
        return;
    }

    lump = W_CacheLumpNum(lnum, PU_CACHE);
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "playdemo ");
    strncat(buf, lump, 64);

    // Start playing the demo.
    if(DD_Execute(buf, false))
        gamestate = GS_WAITING; // The demo will begin momentarily.
}

/*
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute("stopdemo", true);
}

void G_DemoEnds(void)
{
    gamestate = GS_WAITING;
    if(singledemo)
        Sys_Quit();
    FI_DemoEnds();
}

void G_DemoAborted(void)
{
    gamestate = GS_WAITING;
    FI_DemoEnds();
}

void P_Thrust3D(player_t *player, angle_t angle, float lookdir,
                int forwardmove, int sidemove)
{
    angle_t pitch = LOOKDIR2DEG(lookdir) / 360 * ANGLE_MAX;
    angle_t sideangle = angle - ANG90;
    mobj_t *mo = player->plr->mo;
    int     zmul;
    int     x, y, z;

    angle >>= ANGLETOFINESHIFT;
    sideangle >>= ANGLETOFINESHIFT;
    pitch >>= ANGLETOFINESHIFT;

    x = FixedMul(forwardmove, finecosine[angle]);
    y = FixedMul(forwardmove, finesine[angle]);
    z = FixedMul(forwardmove, finesine[pitch]);

    zmul = finecosine[pitch];
    x = FixedMul(x, zmul) + FixedMul(sidemove, finecosine[sideangle]);
    y = FixedMul(y, zmul) + FixedMul(sidemove, finesine[sideangle]);

    mo->momx += x;
    mo->momy += y;
    mo->momz += z;
}

boolean P_IsCamera(mobj_t *mo)
{
    // Client mobjs do not have thinkers and thus cannot be cameras.
    return (mo->thinker.function && mo->player &&
            mo->player->plr->flags & DDPF_CAMERA);
}

int P_CameraXYMovement(mobj_t *mo)
{
    if(!P_IsCamera(mo))
        return false;
#if __JDOOM__
    if(mo->flags & MF_NOCLIP
       // This is a very rough check!
       // Sometimes you get stuck in things.
       || P_CheckPosition2(mo, mo->x + mo->momx, mo->y + mo->momy, mo->z))
    {
#endif

        P_UnsetThingPosition(mo);
        mo->x += mo->momx;
        mo->y += mo->momy;
        P_SetThingPosition(mo);
        P_CheckPosition(mo, mo->x, mo->y);
        mo->floorz = tmfloorz;
        mo->ceilingz = tmceilingz;

#if __JDOOM__
    }
#endif
    // Friction.
    mo->momx = FixedMul(mo->momx, 0xe800);
    mo->momy = FixedMul(mo->momy, 0xe800);
    return true;
}

int P_CameraZMovement(mobj_t *mo)
{
    if(!P_IsCamera(mo))
        return false;
    mo->z += mo->momz;
    mo->momz = FixedMul(mo->momz, 0xe800);
    if(mo->z < mo->floorz + 6 * FRACUNIT)
        mo->z = mo->floorz + 6 * FRACUNIT;
    if(mo->z > mo->ceilingz - 6 * FRACUNIT)
        mo->z = mo->ceilingz - 6 * FRACUNIT;
    return true;
}

/*
 * Set appropriate parameters for a camera.
 */
void P_CameraThink(player_t *player)
{
    angle_t angle;
    int     tp, full, dist;
    mobj_t *mo, *target;

    // If this player is not a camera, get out of here.
    if(!(player->plr->flags & DDPF_CAMERA))
        return;

    mo = player->plr->mo;
    player->cheats |= CF_GODMODE;

    if(cfg.cameraNoClip)
        player->cheats |= CF_NOCLIP;

    player->plr->viewheight = 0;
    mo->flags &= ~(MF_SOLID | MF_SHOOTABLE | MF_PICKUP);

    // How about viewlock?
    if(player->viewlock & 0xff)
    {
        full = (player->viewlock & LOCKF_FULL) != 0;
        tp = (player->viewlock & LOCKF_MASK) - 1;
        target = players[tp].plr->mo;
        if(players[tp].plr->ingame && target)
        {
            angle = R_PointToAngle2(mo->x, mo->y, target->x, target->y);
            player->plr->clAngle = angle;
            if(full)
            {
                dist = P_ApproxDistance(mo->x - target->x, mo->y - target->y);
                angle =
                    R_PointToAngle2(0, 0,
                                    target->z + target->height / 2 - mo->z,
                                    dist);
                player->plr->clLookDir =
                    -(angle / (float) ANGLE_MAX * 360.0f - 90);
                if(player->plr->clLookDir > 180)
                    player->plr->clLookDir -= 360;
                player->plr->clLookDir *= 110.0f / 85.0f;
                if(player->plr->clLookDir > 110)
                    player->plr->clLookDir = 110;
                if(player->plr->clLookDir < -110)
                    player->plr->clLookDir = -110;
            }
        }
    }
}

DEFCC(CCmdMakeLocal)
{
    int     p;
    char    buf[20];

    if(argc < 2)
        return false;
    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }
    if(players[p].plr->ingame)
    {
        Con_Printf("Player %i is already in the game.\n", p);
        return false;
    }
    players[p].playerstate = PST_REBORN;
    players[p].plr->ingame = true;
    sprintf(buf, "conlocp %i", p);
    DD_Execute(buf, false);
    P_DealPlayerStarts();
    return true;
}

DEFCC(CCmdSetCamera)
{
    int     p;

    if(argc < 2)
        return false;
    p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        Con_Printf("Invalid console number %i.\n", p);
        return false;
    }
    players[p].plr->flags ^= DDPF_CAMERA;
    return true;
}

DEFCC(CCmdSetViewLock)
{
    int     pl = consoleplayer, lock;

    if(!stricmp(argv[0], "lockmode"))
    {
        if(argc < 2)
            return false;
        lock = atoi(argv[1]);
        if(lock)
            players[pl].viewlock |= LOCKF_FULL;
        else
            players[pl].viewlock &= ~LOCKF_FULL;
        return true;
    }
    if(argc < 2)
        return false;
    if(argc >= 3)
        pl = atoi(argv[2]);     // Console number.
    lock = atoi(argv[1]);
    if(lock == pl || lock < 0 || lock >= MAXPLAYERS)
        lock = -1;
    players[pl].viewlock &= ~LOCKF_MASK;
    players[pl].viewlock |= lock + 1;
    return true;
}

/*
 * CCmdSpawnMobj
 *  Spawns a mobj of the given type at (x,y,z).
 */
DEFCC(CCmdSpawnMobj)
{
    int     type;
    int     x, y, z;
    mobj_t *mo;

    if(argc != 5 && argc != 6)
    {
        Con_Printf("Usage: %s (type) (x) (y) (z) (angle)\n", argv[0]);
        Con_Printf("Type must be a defined Thing ID or Name.\n");
        Con_Printf("Z is an offset from the floor, 'floor' or 'ceil'.\n");
        Con_Printf("Angle (0..360) is optional.\n");
        return true;
    }

    if(IS_CLIENT)
    {
        Con_Printf("%s can't be used by clients.\n", argv[0]);
        return false;
    }

    // First try to find the thing by ID.
    if((type = Def_Get(DD_DEF_MOBJ, argv[1], 0)) < 0)
    {
        // Try to find it by name instead.
        if((type = Def_Get(DD_DEF_MOBJ_BY_NAME, argv[1], 0)) < 0)
        {
            Con_Printf("Undefined thing type %s.\n", argv[1]);
            return false;
        }
    }

    // The coordinates.
    x = strtod(argv[2], 0) * FRACUNIT;
    y = strtod(argv[3], 0) * FRACUNIT;
    if(!stricmp(argv[4], "floor"))
        z = ONFLOORZ;
    else if(!stricmp(argv[4], "ceil"))
        z = ONCEILINGZ;
    else
    {
        z = strtod(argv[4], 0) * FRACUNIT +
            P_GetFixedp(R_PointInSubsector(x, y), DMU_FLOOR_HEIGHT);
    }

    if((mo = P_SpawnMobj(x, y, z, type)) && argc == 6)
    {
        mo->angle = ((int) (strtod(argv[5], 0) / 360 * FRACUNIT)) << 16;
    }
    return true;
}

/*
 * Print the console player's coordinates.
 */
DEFCC(CCmdPrintPlayerCoords)
{
    mobj_t *mo = players[consoleplayer].plr->mo;

    if(!mo || gamestate != GS_LEVEL)
        return false;
    Con_Printf("Console %i: X=%g Y=%g\n", consoleplayer, FIX2FLT(mo->x),
               FIX2FLT(mo->y));
    return true;
}

/*
 * Display a local game message.
 */
DEFCC(CCmdLocalMessage)
{
    if(argc != 2)
    {
        Con_Printf("%s (msg)\n", argv[0]);
        return true;
    }
    D_NetMessageNoSound(argv[1]);
    return true;
}

DEFCC(CCmdCycleSpy)
{
    // FIXME: The engine should do this.
    Con_Printf("Spying not allowed.\n");
#if 0
    if(gamestate == GS_LEVEL && !deathmatch)
    {                           // Cycle the display player
        do
        {
            Set(DD_DISPLAYPLAYER, displayplayer + 1);
            if(displayplayer == MAXPLAYERS)
            {
                Set(DD_DISPLAYPLAYER, 0);
            }
        } while(!players[displayplayer].plr->ingame &&
                displayplayer != consoleplayer);
    }
#endif
    return true;
}

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
/*
 * Move the inventory selector
 */
DEFCC(CCmdInventory)
{
    inventoryMove(players + consoleplayer, !stricmp(argv[0], "invright"));
    return true;
}
#endif

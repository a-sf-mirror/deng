/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2003 Randy Heit <rheit@iastate.edu> (Zdoom)
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
 * In Zdoom, from which this code is based, it was originally under the
 * 3 clause BSD license as described here:
 * http://www.opensource.org/licenses/bsd-license.php
 */


/**
 * dehmain.c: Dehacked Reader Plugin for Doomsday
 *
 * Much of this has been taken from or is based on ZDoom's DEH reader.
 * Unsupported Dehacked features have been commented out.
 *
 * Put the Doomsday Include directory in your path.
 * This DLL also serves as an example of a Doomsday plugin.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

#include "doomsday.h"
#include "dd_api.h"

// This plugin accesses the internal definition arrays. This dependency
// should be removed entirely, either by making the plugin modify the
// definitions via an API or by integrating the plugin into the engine.
#include "../../../engine/portable/include/def_data.h"

// MACROS ------------------------------------------------------------------

#define OFF_STATE   0x01000000
#define OFF_SOUND   0x02000000
#define OFF_FLOAT   0x04000000
#define OFF_SPRITE  0x08000000
#define OFF_FIXED   0x10000000
#define OFF_MASK    0x00ffffff

#define myoffsetof(type,identifier,fl) (((size_t)&((type*)0)->identifier)|fl)

#define LPrintf     Con_Message

//#define LINESIZE    2048
#define NUMSPRITES  138
#define NUMSTATES   968

#define CHECKKEY(a,b)       if (!stricmp (Line1, (a))) (b) = atoi(Line2);

// TYPES -------------------------------------------------------------------

typedef struct Key {
    char   *name;
    ptrdiff_t offset;
} Key_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

#ifdef UNIX
void    InitPlugin(void) __attribute__ ((constructor));
#endif

int     PatchThing(int);
int     PatchSound(int);
int     PatchFrame(int);
int     PatchSprite(int);
int     PatchAmmo(int);
int     PatchWeapon(int);
int     PatchPointer(int);
int     PatchCheats(int);
int     PatchMisc(int);
int     PatchText(int);
int     PatchStrings(int);
int     PatchPars(int);
int     PatchCodePtrs(int);
int     DoInclude(int);
void    ApplyDEH(char *patch, int length);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ded_t  *ded;

boolean verbose;
char   *PatchFile, *PatchPt;
char   *Line1, *Line2;
int     dversion, pversion;
boolean including, includenotext;

static const char* SpriteMap[] = {
    "TROO",
    "SHTG",
    "PUNG",
    "PISG",
    "PISF",
    "SHTF",
    "SHT2",
    "CHGG",
    "CHGF",
    "MISG",
    "MISF",
    "SAWG",
    "PLSG",
    "PLSF",
    "BFGG",
    "BFGF",
    "BLUD",
    "PUFF",
    "BAL1",
    "BAL2",
    "PLSS",
    "PLSE",
    "MISL",
    "BFS1",
    "BFE1",
    "BFE2",
    "TFOG",
    "IFOG",
    "PLAY",
    "POSS",
    "SPOS",
    "VILE",
    "FIRE",
    "FATB",
    "FBXP",
    "SKEL",
    "MANF",
    "FATT",
    "CPOS",
    "SARG",
    "HEAD",
    "BAL7",
    "BOSS",
    "BOS2",
    "SKUL",
    "SPID",
    "BSPI",
    "APLS",
    "APBX",
    "CYBR",
    "PAIN",
    "SSWV",
    "KEEN",
    "BBRN",
    "BOSF",
    "ARM1",
    "ARM2",
    "BAR1",
    "BEXP",
    "FCAN",
    "BON1",
    "BON2",
    "BKEY",
    "RKEY",
    "YKEY",
    "BSKU",
    "RSKU",
    "YSKU",
    "STIM",
    "MEDI",
    "SOUL",
    "PINV",
    "PSTR",
    "PINS",
    "MEGA",
    "SUIT",
    "PMAP",
    "PVIS",
    "CLIP",
    "AMMO",
    "ROCK",
    "BROK",
    "CELL",
    "CELP",
    "SHEL",
    "SBOX",
    "BPAK",
    "BFUG",
    "MGUN",
    "CSAW",
    "LAUN",
    "PLAS",
    "SHOT",
    "SGN2",
    "COLU",
    "SMT2",
    "GOR1",
    "POL2",
    "POL5",
    "POL4",
    "POL3",
    "POL1",
    "POL6",
    "GOR2",
    "GOR3",
    "GOR4",
    "GOR5",
    "SMIT",
    "COL1",
    "COL2",
    "COL3",
    "COL4",
    "CAND",
    "CBRA",
    "COL6",
    "TRE1",
    "TRE2",
    "ELEC",
    "CEYE",
    "FSKU",
    "COL5",
    "TBLU",
    "TGRN",
    "TRED",
    "SMBT",
    "SMGT",
    "SMRT",
    "HDB1",
    "HDB2",
    "HDB3",
    "HDB4",
    "HDB5",
    "HDB6",
    "POB1",
    "POB2",
    "BRS1",
    "TLMP",
    "TLP2",
    NULL
};

static const char* SoundMap[] = {
    "None",
    "pistol",
    "shotgn",
    "sgcock",
    "dshtgn",
    "dbopn",
    "dbcls",
    "dbload",
    "plasma",
    "bfg",
    "sawup",
    "sawidl",
    "sawful",
    "sawhit",
    "rlaunc",
    "rxplod",
    "firsht",
    "firxpl",
    "pstart",
    "pstop",
    "doropn",
    "dorcls",
    "stnmov",
    "swtchn",
    "swtchx",
    "plpain",
    "dmpain",
    "popain",
    "vipain",
    "mnpain",
    "pepain",
    "slop",
    "itemup",
    "wpnup",
    "oof",
    "telept",
    "posit1",
    "posit2",
    "posit3",
    "bgsit1",
    "bgsit2",
    "sgtsit",
    "cacsit",
    "brssit",
    "cybsit",
    "spisit",
    "bspsit",
    "kntsit",
    "vilsit",
    "mansit",
    "pesit",
    "sklatk",
    "sgtatk",
    "skepch",
    "vilatk",
    "claw",
    "skeswg",
    "pldeth",
    "pdiehi",
    "podth1",
    "podth2",
    "podth3",
    "bgdth1",
    "bgdth2",
    "sgtdth",
    "cacdth",
    "skldth",
    "brsdth",
    "cybdth",
    "spidth",
    "bspdth",
    "vildth",
    "kntdth",
    "pedth",
    "skedth",
    "posact",
    "bgact",
    "dmact",
    "bspact",
    "bspwlk",
    "vilact",
    "noway",
    "barexp",
    "punch",
    "hoof",
    "metal",
    "chgun",
    "tink",
    "bdopn",
    "bdcls",
    "itmbk",
    "flame",
    "flamst",
    "getpow",
    "bospit",
    "boscub",
    "bossit",
    "bospn",
    "bosdth",
    "manatk",
    "mandth",
    "sssit",
    "ssdth",
    "keenpn",
    "keendt",
    "skeact",
    "skesit",
    "skeatk",
    "radio",
    NULL
};

static const char* MusicMap[] = {
    "e1m1",
    "e1m2",
    "e1m3",
    "e1m4",
    "e1m5",
    "e1m6",
    "e1m7",
    "e1m8",
    "e1m9",
    "e2m1",
    "e2m2",
    "e2m3",
    "e2m4",
    "e2m5",
    "e2m6",
    "e2m7",
    "e2m8",
    "e2m9",
    "e3m1",
    "e3m2",
    "e3m3",
    "e3m4",
    "e3m5",
    "e3m6",
    "e3m7",
    "e3m8",
    "e3m9",
    "inter",
    "intro",
    "bunny",
    "victor",
    "introa",
    "runnin",
    "stalks",
    "countd",
    "betwee",
    "doom",
    "the_da",
    "shawn",
    "ddtblu",
    "in_cit",
    "dead",
    "stlks2",
    "theda2",
    "doom2",
    "ddtbl2",
    "runni2",
    "dead2",
    "stlks3",
    "romero",
    "shawn2",
    "messag",
    "count2",
    "ddtbl3",
    "ampie",
    "theda3",
    "adrian",
    "messg2",
    "romer2",
    "tense",
    "shawn3",
    "openin",
    "evil",
    "ultima",
    "read_m",
    "dm2ttl",
    "dm2int",
    NULL
};

static const struct {
    const char*     id;
    const char*     str;
} TextMap[] = {
    { "E1TEXT", "Once you beat the big badasses and\nclean out the moon base you're supposed\nto win, aren't you? Aren't you? Where's\nyour fat reward and ticket home? What\nthe hell is this? It's not supposed to\nend this way!\n\nIt stinks like rotten meat, but looks\nlike the lost Deimos base.  Looks like\nyou're stuck on The Shores of Hell.\nThe only way out is through.\n\nTo continue the DOOM experience, play\nThe Shores of Hell and its amazing\nsequel, Inferno!\n" },
    { "E2TEXT", "You've done it! The hideous cyber-\ndemon lord that ruled the lost Deimos\nmoon base has been slain and you\nare triumphant! But ... where are\nyou? You clamber to the edge of the\nmoon and look down to see the awful\ntruth.\n\nDeimos floats above Hell itself!\nYou've never heard of anyone escaping\nfrom Hell, but you'll make the bastards\nsorry they ever heard of you! Quickly,\nyou rappel down to  the surface of\nHell.\n\nNow, it's on to the final chapter of\nDOOM! -- Inferno." },
    { "E3TEXT", "The loathsome spiderdemon that\nmasterminded the invasion of the moon\nbases and caused so much death has had\nits ass kicked for all time.\n\nA hidden doorway opens and you enter.\nYou've proven too tough for Hell to\ncontain, and now Hell at last plays\nfair -- for you emerge from the door\nto see the green fields of Earth!\nHome at last.\n\nYou wonder what's been happening on\nEarth while you were battling evil\nunleashed. It's good that no Hell-\nspawn could have come through that\ndoor with you ..." },
    { "E4TEXT", "the spider mastermind must have sent forth\nits legions of hellspawn before your\nfinal confrontation with that terrible\nbeast from hell.  but you stepped forward\nand brought forth eternal damnation and\nsuffering upon the horde as a true hero\nwould in the face of something so evil.\n\nbesides, someone was gonna pay for what\nhappened to daisy, your pet rabbit.\n\nbut now, you see spread before you more\npotential pain and gibbitude as a nation\nof demons run amok among our cities.\n\nnext stop, hell on earth!" },
    { "C1TEXT", "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\nSTARPORT. BUT SOMETHING IS WRONG. THE\nMONSTERS HAVE BROUGHT THEIR OWN REALITY\nWITH THEM, AND THE STARPORT'S TECHNOLOGY\nIS BEING SUBVERTED BY THEIR PRESENCE.\n\nAHEAD, YOU SEE AN OUTPOST OF HELL, A\nFORTIFIED ZONE. IF YOU CAN GET PAST IT,\nYOU CAN PENETRATE INTO THE HAUNTED HEART\nOF THE STARBASE AND FIND THE CONTROLLING\nSWITCH WHICH HOLDS EARTH'S POPULATION\nHOSTAGE." },
    { "C2TEXT", "YOU HAVE WON! YOUR VICTORY HAS ENABLED\nHUMANKIND TO EVACUATE EARTH AND ESCAPE\nTHE NIGHTMARE.  NOW YOU ARE THE ONLY\nHUMAN LEFT ON THE FACE OF THE PLANET.\nCANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\nAND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\nYOU SIT BACK AND WAIT FOR DEATH, CONTENT\nTHAT YOU HAVE SAVED YOUR SPECIES.\n\nBUT THEN, EARTH CONTROL BEAMS DOWN A\nMESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\nTHE SOURCE OF THE ALIEN INVASION. IF YOU\nGO THERE, YOU MAY BE ABLE TO BLOCK THEIR\nENTRY.  THE ALIEN BASE IS IN THE HEART OF\nYOUR OWN HOME CITY, NOT FAR FROM THE\nSTARPORT.\" SLOWLY AND PAINFULLY YOU GET\nUP AND RETURN TO THE FRAY." },
    { "C3TEXT", "YOU ARE AT THE CORRUPT HEART OF THE CITY,\nSURROUNDED BY THE CORPSES OF YOUR ENEMIES.\nYOU SEE NO WAY TO DESTROY THE CREATURES'\nENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\nTEETH AND PLUNGE THROUGH IT.\n\nTHERE MUST BE A WAY TO CLOSE IT ON THE\nOTHER SIDE. WHAT DO YOU CARE IF YOU'VE\nGOT TO GO THROUGH HELL TO GET TO IT?" },
    { "C4TEXT", "THE HORRENDOUS VISAGE OF THE BIGGEST\nDEMON YOU'VE EVER SEEN CRUMBLES BEFORE\nYOU, AFTER YOU PUMP YOUR ROCKETS INTO\nHIS EXPOSED BRAIN. THE MONSTER SHRIVELS\nUP AND DIES, ITS THRASHING LIMBS\nDEVASTATING UNTOLD MILES OF HELL'S\nSURFACE.\n\nYOU'VE DONE IT. THE INVASION IS OVER.\nEARTH IS SAVED. HELL IS A WRECK. YOU\nWONDER WHERE BAD FOLKS WILL GO WHEN THEY\nDIE, NOW. WIPING THE SWEAT FROM YOUR\nFOREHEAD YOU BEGIN THE LONG TREK BACK\nHOME. REBUILDING EARTH OUGHT TO BE A\nLOT MORE FUN THAN RUINING IT WAS.\n" },
    { "C5TEXT", "CONGRATULATIONS, YOU'VE FOUND THE SECRET\nLEVEL! LOOKS LIKE IT'S BEEN BUILT BY\nHUMANS, RATHER THAN DEMONS. YOU WONDER\nWHO THE INMATES OF THIS CORNER OF HELL\nWILL BE." },
    { "C6TEXT", "CONGRATULATIONS, YOU'VE FOUND THE\nSUPER SECRET LEVEL!  YOU'D BETTER\nBLAZE THROUGH THIS ONE!\n" },
    { "CC_ZOMBIE", "ZOMBIEMAN" },
    { "CC_SHOTGUN", "SHOTGUN GUY" },
    { "CC_HEAVY", "HEAVY WEAPON DUDE" },
    { "CC_IMP", "IMP" },
    { "CC_DEMON", "DEMON" },
    { "CC_LOST", "LOST SOUL" },
    { "CC_CACO", "CACODEMON" },
    { "CC_HELL", "HELL KNIGHT" },
    { "CC_BARON", "BARON OF HELL" },
    { "CC_ARACH", "ARACHNOTRON" },
    { "CC_PAIN", "PAIN ELEMENTAL" },
    { "CC_REVEN", "REVENANT" },
    { "CC_MANCU", "MANCUBUS" },
    { "CC_ARCH", "ARCH-VILE" },
    { "CC_SPIDER", "THE SPIDER MASTERMIND" },
    { "CC_CYBER", "THE CYBERDEMON" },
    { "CC_HERO", "OUR HERO" },
    { "D_DEVSTR", "Development mode ON.\n" },
    { "D_CDROM", "CD-ROM Version: default.cfg from c:\\doomdata\n" },
    { "LOADNET", "you can't do load while in a net game!\n\npress a key." },
    { "SAVEDEAD", "you can't save if you aren't playing!\n\npress a key." },
    { "QSPROMPT", "quicksave over your game named\n\n'%s'?\n\npress y or n." },
    { "QLOADNET", "you can't quickload during a netgame!\n\npress a key." },
    { "QSAVESPOT", "you haven't picked a quicksave slot yet!\n\npress a key." },
    { "QLPROMPT", "do you want to quickload the game named\n\n'%s'?\n\npress y or n." },
    { "NEWGAME", "you can't start a new game\nwhile in a network game.\n\npress a key." },
    { "NIGHTMARE", "are you sure? this skill level\nisn't even remotely fair.\n\npress y or n." },
    { "SWSTRING", "this is the shareware version of doom.\n\nyou need to order the entire trilogy.\n\npress a key." },
    { "MSGOFF", "Messages OFF" },
    { "MSGON", "Messages ON" },
    { "NETEND", "you can't end a netgame!\n\npress a key." },
    { "ENDGAME", "are you sure you want to end the game?\n\npress y or n." },
    { "DOSY", "%s\n\n(press y to quit to dos.)" },
    { "DETAILHI", "High detail" },
    { "DETAILLO", "Low detail" },
    { "HUSTR_CHATMACRO0", "No" },
    { "HUSTR_CHATMACRO1", "I'm ready to kick butt!" },
    { "HUSTR_CHATMACRO2", "I'm OK." },
    { "HUSTR_CHATMACRO3", "I'm not looking too good!" },
    { "HUSTR_CHATMACRO4", "Help!" },
    { "HUSTR_CHATMACRO5", "You suck!" },
    { "HUSTR_CHATMACRO6", "Next time, scumbag..." },
    { "HUSTR_CHATMACRO7", "Come here!" },
    { "HUSTR_CHATMACRO8", "I'll take care of it." },
    { "HUSTR_CHATMACRO9", "Yes" },
    { "AMSTR_FOLLOWON", "Follow Mode ON" },
    { "AMSTR_FOLLOWOFF", "Follow Mode OFF" },
    { "AMSTR_GRIDON", "Grid ON" },
    { "AMSTR_GRIDOFF", "Grid OFF" },
    { "AMSTR_MARKEDSPOT", "Marked Spot" },
    { "AMSTR_MARKSCLEARED", "All Marks Cleared" },
    { "PD_BLUEO", "You need a blue key to activate this object" },
    { "PD_REDO", "You need a red key to activate this object" },
    { "PD_YELLOWO", "You need a yellow key to activate this object" },
    { "PD_BLUEK", "You need a blue key to open this door" },
    { "PD_REDK", "You need a yellow key to open this door" },
    { "PD_YELLOWK", "You need a red key to open this door" },
    { "EMPTYSTRING", "empty slot" },
    { "GGSAVED", "game saved." },
    { "GOTARMOR", "Picked up the armor." },
    { "GOTMEGA", "Picked up the MegaArmor!" },
    { "GOTHTHBONUS", "Picked up a health bonus." },
    { "GOTARMBONUS", "Picked up an armor bonus." },
    { "GOTSUPER", "Supercharge!" },
    { "GOTMSPHERE", "MegaSphere!" },
    { "GOTBLUECARD", "Picked up a blue keycard." },
    { "GOTYELWCARD", "Picked up a yellow keycard." },
    { "GOTREDCARD", "Picked up a red keycard." },
    { "GOTBLUESKUL", "Picked up a blue skull key." },
    { "GOTYELWSKUL", "Picked up a yellow skull key." },
    { "GOTREDSKULL", "Picked up a red skull key." },
    { "GOTSTIM", "Picked up a stimpack." },
    { "GOTMEDINEED", "Picked up a medikit that you REALLY need!" },
    { "GOTMEDIKIT", "Picked up a medikit." },
    { "GOTINVUL", "Invulnerability!" },
    { "GOTBERSERK", "Berserk!" },
    { "GOTINVIS", "Partial Invisibility" },
    { "GOTSUIT", "Radiation Shielding Suit" },
    { "GOTMAP", "Computer Area Map" },
    { "GOTVISOR", "Light Amplification Visor" },
    { "GOTCLIP", "Picked up a clip." },
    { "GOTCLIPBOX", "Picked up a box of bullets." },
    { "GOTROCKET", "Picked up a rocket." },
    { "GOTROCKBOX", "Picked up a box of rockets." },
    { "GOTCELL", "Picked up an energy cell." },
    { "GOTCELLBOX", "Picked up an energy cell pac" },
    { "GOTSHELLS", "Picked up 4 shotgun shells." },
    { "GOTSHELLBOX", "Picked up a box of shotgun shells." },
    { "GOTBACKPACK", "Picked up a backpack full of ammo!" },
    { "GOTBFG9000", "You got the BFG9000!  Oh, yes." },
    { "GOTCHAINGUN", "You got the chaingun!" },
    { "GOTCHAINSAW", "A chainsaw!  Find some meat!" },
    { "GOTLAUNCHER", "You got the rocket launcher!" },
    { "GOTPLASMA", "You got the plasma gun!" },
    { "GOTSHOTGUN", "You got the shotgun!" },
    { "GOTSHOTGUN2", "You got the super shotgun!" },
    { "STSTR_DQDON", "Degreelessness Mode On" },
    { "STSTR_DQDOFF", "Degreelessness Mode Off" },
    { "STSTR_FAADDED", "Ammo (no keys) Added" },
    { "STSTR_KFAADDED", "Very Happy Ammo Added" },
    { "STSTR_MUS", "Music Change" },
    { "STSTR_NOMUS", "IMPOSSIBLE SELECTION" },
    { "STSTR_NCON", "No Clipping Mode ON" },
    { "STSTR_NCOFF", "No Clipping Mode OFF" },
    { "STSTR_BEHOLDX", "Power-up Toggled" },
    { "STSTR_BEHOLD", "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp" },
    { "STSTR_CHOPPERS", "... doesn't suck - GM" },
    { "STSTR_CLEV", "Changing Level..." },
    { "HUSTR_PLRGREEN", "Green: " },
    { "HUSTR_PLRINDIGO", "Indigo: " },
    { "HUSTR_PLRBROWN", "Brown: " },
    { "HUSTR_PLRRED", "Red: " },
    { "HUSTR_MSGU", "[Message unsent]" },
    { "HUSTR_TALKTOSELF1", "You mumble to yourself" },
    { "HUSTR_TALKTOSELF2", "Who's there?" },
    { "HUSTR_TALKTOSELF3", "You scare yourself" },
    { "HUSTR_TALKTOSELF4", "You start to rave" },
    { "HUSTR_TALKTOSELF5", "You've lost it..." },
    { "HUSTR_E1M1", "E1M1: Hangar" },
    { "HUSTR_E1M2", "E1M2: Nuclear Plant" },
    { "HUSTR_E1M3", "E1M3: Toxin Refinery" },
    { "HUSTR_E1M4", "E1M4: Command Control" },
    { "HUSTR_E1M5", "E1M5: Phobos Lab" },
    { "HUSTR_E1M6", "E1M6: Central Processing" },
    { "HUSTR_E1M7", "E1M7: Computer Station" },
    { "HUSTR_E1M8", "E1M8: Phobos Anomaly" },
    { "HUSTR_E1M9", "E1M9: Military Base" },
    { "HUSTR_E2M1", "E2M1: Deimos Anomaly" },
    { "HUSTR_E2M2", "E2M2: Containment Area" },
    { "HUSTR_E2M3", "E2M3: Refinery" },
    { "HUSTR_E2M4", "E2M4: Deimos Lab" },
    { "HUSTR_E2M5", "E2M5: Command Center" },
    { "HUSTR_E2M6", "E2M6: Halls of the Damned" },
    { "HUSTR_E2M7", "E2M7: Spawning Vats" },
    { "HUSTR_E2M8", "E2M8: Tower of Babel" },
    { "HUSTR_E2M9", "E2M9: Fortress of Mystery" },
    { "HUSTR_E3M1", "E3M1: Hell Keep" },
    { "HUSTR_E3M2", "E3M2: Slough of Despair" },
    { "HUSTR_E3M3", "E3M3: Pandemonium" },
    { "HUSTR_E3M4", "E3M4: House of Pain" },
    { "HUSTR_E3M5", "E3M5: Unholy Cathedral" },
    { "HUSTR_E3M6", "E3M6: Mt. Erebus" },
    { "HUSTR_E3M7", "E3M7: Limbo" },
    { "HUSTR_E3M8", "E3M8: Dis" },
    { "HUSTR_E3M9", "E3M9: Warrens" },
    { "HUSTR_E4M1", "E4M1: Hell Beneath" },
    { "HUSTR_E4M2", "E4M2: Perfect Hatred" },
    { "HUSTR_E4M3", "E4M3: Sever The Wicked" },
    { "HUSTR_E4M4", "E4M4: Unruly Evil" },
    { "HUSTR_E4M5", "E4M5: They Will Repent" },
    { "HUSTR_E4M6", "E4M6: Against Thee Wickedly" },
    { "HUSTR_E4M7", "E4M7: And Hell Followed" },
    { "HUSTR_E4M8", "E4M8: Unto The Cruel" },
    { "HUSTR_E4M9", "E4M9: Fear" },
    { "HUSTR_1", "level 1: entryway" },
    { "HUSTR_2", "level 2: underhalls" },
    { "HUSTR_3", "level 3: the gantlet" },
    { "HUSTR_4", "level 4: the focus" },
    { "HUSTR_5", "level 5: the waste tunnels" },
    { "HUSTR_6", "level 6: the crusher" },
    { "HUSTR_7", "level 7: dead simple" },
    { "HUSTR_8", "level 8: tricks and traps" },
    { "HUSTR_9", "level 9: the pit" },
    { "HUSTR_10", "level 10: refueling base" },
    { "HUSTR_11", "level 11: 'o' of destruction!" },
    { "HUSTR_12", "level 12: the factory" },
    { "HUSTR_13", "level 13: downtown" },
    { "HUSTR_14", "level 14: the inmost dens" },
    { "HUSTR_15", "level 15: industrial zone" },
    { "HUSTR_16", "level 16: suburbs" },
    { "HUSTR_17", "level 17: tenements" },
    { "HUSTR_18", "level 18: the courtyard" },
    { "HUSTR_19", "level 19: the citadel" },
    { "HUSTR_20", "level 20: gotcha!" },
    { "HUSTR_21", "level 21: nirvana" },
    { "HUSTR_22", "level 22: the catacombs" },
    { "HUSTR_23", "level 23: barrels o' fun" },
    { "HUSTR_24", "level 24: the chasm" },
    { "HUSTR_25", "level 25: bloodfalls" },
    { "HUSTR_26", "level 26: the abandoned mines" },
    { "HUSTR_27", "level 27: monster condo" },
    { "HUSTR_28", "level 28: the spirit world" },
    { "HUSTR_29", "level 29: the living end" },
    { "HUSTR_30", "level 30: icon of sin" },
    { "HUSTR_31", "level 31: wolfenstein" },
    { "HUSTR_32", "level 32: grosse" },
    { NULL, NULL }
};

boolean BackedUpData = false;

// This is the original data before it gets replaced by a patch.
char    OrgSprNames[NUMSPRITES][5];
char    OrgActionPtrs[NUMSTATES][40];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *unknown_str = "Unknown key %s encountered in %s %d.\n";

// From DeHackEd source.
static int toff[] = { 129044, 129044, 129044, 129284, 129380 };

// A conversion array to convert from the 448 code pointers to the 966
// Frames that exist.
// Again taken from the DeHackEd source.
static short codepconv[448] = { 1, 2, 3, 4, 6, 9, 10, 11, 12, 14,
    16, 17, 18, 19, 20, 22, 29, 30, 31, 32,
    33, 34, 36, 38, 39, 41, 43, 44, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 65, 66, 67, 68, 69,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    119, 127, 157, 159, 160, 166, 167, 174, 175, 176,
    177, 178, 179, 180, 181, 182, 183, 184, 185, 188,
    190, 191, 195, 196, 207, 208, 209, 210, 211, 212,
    213, 214, 215, 216, 217, 218, 221, 223, 224, 228,
    229, 241, 242, 243, 244, 245, 246, 247, 248, 249,
    250, 251, 252, 253, 254, 255, 256, 257, 258, 259,
    260, 261, 262, 263, 264, 270, 272, 273, 281, 282,
    283, 284, 285, 286, 287, 288, 289, 290, 291, 292,
    293, 294, 295, 296, 297, 298, 299, 300, 301, 302,
    303, 304, 305, 306, 307, 308, 309, 310, 316, 317,
    321, 322, 323, 324, 325, 326, 327, 328, 329, 330,
    331, 332, 333, 334, 335, 336, 337, 338, 339, 340,
    341, 342, 344, 347, 348, 362, 363, 364, 365, 366,
    367, 368, 369, 370, 371, 372, 373, 374, 375, 376,
    377, 378, 379, 380, 381, 382, 383, 384, 385, 387,
    389, 390, 397, 406, 407, 408, 409, 410, 411, 412,
    413, 414, 415, 416, 417, 418, 419, 421, 423, 424,
    430, 431, 442, 443, 444, 445, 446, 447, 448, 449,
    450, 451, 452, 453, 454, 456, 458, 460, 463, 465,
    475, 476, 477, 478, 479, 480, 481, 482, 483, 484,
    485, 486, 487, 489, 491, 493, 502, 503, 504, 505,
    506, 508, 511, 514, 527, 528, 529, 530, 531, 532,
    533, 534, 535, 536, 537, 538, 539, 541, 543, 545,
    548, 556, 557, 558, 559, 560, 561, 562, 563, 564,
    565, 566, 567, 568, 570, 572, 574, 585, 586, 587,
    588, 589, 590, 594, 596, 598, 601, 602, 603, 604,
    605, 606, 607, 608, 609, 610, 611, 612, 613, 614,
    615, 616, 617, 618, 620, 621, 622, 631, 632, 633,
    635, 636, 637, 638, 639, 640, 641, 642, 643, 644,
    645, 646, 647, 648, 650, 652, 653, 654, 659, 674,
    675, 676, 677, 678, 679, 680, 681, 682, 683, 684,
    685, 686, 687, 688, 689, 690, 692, 696, 700, 701,
    702, 703, 704, 705, 706, 707, 708, 709, 710, 711,
    713, 715, 718, 726, 727, 728, 729, 730, 731, 732,
    733, 734, 735, 736, 737, 738, 739, 740, 741, 743,
    745, 746, 750, 751, 766, 774, 777, 779, 780, 783,
    784, 785, 786, 787, 788, 789, 790, 791, 792, 793,
    794, 795, 796, 797, 798, 801, 809, 811
};

static byte OrgHeights[] = {
    56, 56, 56, 56, 16, 56, 8, 16, 64, 8, 56, 56,
    56, 56, 56, 64, 8, 64, 56, 100, 64, 110, 56, 56,
    72, 16, 32, 32, 32, 16, 42, 8, 8, 8,
    8, 8, 8, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 68, 84, 84,
    68, 52, 84, 68, 52, 52, 68, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 88, 88, 64, 64, 64, 64,
    16, 16, 16
};

static const struct {
    char*           name;
    int           (*func) (int);
} Modes[] = {
    // These appear in .deh and .bex files
    { "Thing",      PatchThing },
    { "Sound",      PatchSound },
    { "Frame",      PatchFrame },
    { "Sprite",     PatchSprite },
    { "Ammo",       PatchAmmo },
    { "Weapon",     PatchWeapon },
    { "Pointer",    PatchPointer },
    { "Cheat",      PatchCheats },
    { "Misc",       PatchMisc },
    { "Text",       PatchText },
    // These appear in .bex files
    { "include",    DoInclude },
    { "[STRINGS]",  PatchStrings },
    { "[PARS]",     PatchPars },
    { "[CODEPTR]",  PatchCodePtrs },
    { NULL,         NULL },
};

// CODE --------------------------------------------------------------------

void BackupData(void)
{
    int     i;

    if(BackedUpData)
        return;

    for(i = 0; i < NUMSPRITES && i < ded->count.sprites.num; i++)
        strcpy(OrgSprNames[i], ded->sprites[i].id);

    for(i = 0; i < NUMSTATES && i < ded->count.states.num; i++)
        strcpy(OrgActionPtrs[i], ded->states[i].action);
}

char    com_token[8192];
boolean com_eof;

/**
 * Parse a token out of a string.
 * From ZDoom cmdlib.cpp
 */
char   *COM_Parse(char *data)
{
    int     c;
    int     len;

    len = 0;
    com_token[0] = 0;

    if(!data)
        return NULL;

    // skip whitespace
  skipwhite:
    while((c = *data) <= ' ')
    {
        if(c == 0)
        {
            com_eof = true;
            return NULL;        // end of file;
        }
        data++;
    }

    // skip // comments
    if(c == '/' && data[1] == '/')
    {
        while(*data && *data != '\n')
            data++;
        goto skipwhite;
    }

    // handle quoted strings specially
    if(c == '\"')
    {
        data++;

        while((c = *data++) != '\"')
        {
            com_token[len] = c;
            len++;
        }

        com_token[len] = 0;
        return data;
    }

    // parse single characters
    if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' ||
       /*[RH] */ c == '=')
    {
        com_token[len] = c;
        len++;
        com_token[len] = 0;
        return data + 1;
    }

    // parse a regular word
    do
    {
        com_token[len] = c;
        data++;
        len++;
        c = *data;
        if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' ||
           c == ':' || c == '=')
            break;
    } while(c > 32);

    com_token[len] = 0;
    return data;
}

boolean IsNum(char *str)
{
    char   *end;

    strtol(str, &end, 0);
    if(*end && !isspace(*end))
        return false;
    return true;
}

static boolean HandleKey(const struct Key *keys, void *structure,
                         const char *key, int value)
{
    int     off;
    void   *ptr;

    while(keys->name && stricmp(keys->name, key))
        keys++;

    if(keys->name)
    {
        //*((int *)(((byte *)structure) + keys->offset)) = value;
        off = keys->offset & OFF_MASK;
        ptr = (void *) (((byte *) structure) + off);
        // Apply value.
        if(keys->offset & OFF_STATE)
            strcpy((char *) ptr, ded->states[value].id);
        else if(keys->offset & OFF_SPRITE)
            strcpy((char *) ptr, ded->sprites[value].id);
        else if(keys->offset & OFF_SOUND)
            strcpy((char *) ptr, ded->sounds[value].id);
        else if(keys->offset & OFF_FLOAT)
        {
            if(value < 0x2000)
                *(float *) ptr = (float) value; // / (float) 0x10000;
            else
                *(float *) ptr = value / (float) 0x10000;
        }
        else if(keys->offset & OFF_FIXED)
            *(float *) ptr = value / (float) 0x10000;
        else
            *(int *) ptr = value;
        return false;
    }

    return true;
}

static boolean ReadChars(char **stuff, int size, boolean skipJunk)
{
    char   *str = *stuff;

    if(!size)
    {
        *str = 0;
        return true;
    }

    do
    {
        // Ignore carriage returns
        if(*PatchPt != '\r')
            *str++ = *PatchPt;
        else
            size++;

        PatchPt++;
    } while(--size);

    *str = 0;

    if(skipJunk)
    {
        // Skip anything else on the line.
        while(*PatchPt != '\n' && *PatchPt != 0)
            PatchPt++;
    }

    return true;
}

void ReplaceSpecialChars(char *str)
{
    char   *p = str, c;
    int     i;

    while((c = *p++))
    {
        if(c != '\\')
        {
            *str++ = c;
        }
        else
        {
            switch (*p)
            {
            case 'n':
            case 'N':
                *str++ = '\n';
                break;
            case 't':
            case 'T':
                *str++ = '\t';
                break;
            case 'r':
            case 'R':
                *str++ = '\r';
                break;
            case 'x':
            case 'X':
                c = 0;
                p++;
                for(i = 0; i < 2; i++)
                {
                    c <<= 4;
                    if(*p >= '0' && *p <= '9')
                        c += *p - '0';
                    else if(*p >= 'a' && *p <= 'f')
                        c += 10 + *p - 'a';
                    else if(*p >= 'A' && *p <= 'F')
                        c += 10 + *p - 'A';
                    else
                        break;
                    p++;
                }
                *str++ = c;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                c = 0;
                for(i = 0; i < 3; i++)
                {
                    c <<= 3;
                    if(*p >= '0' && *p <= '7')
                        c += *p - '0';
                    else
                        break;
                    p++;
                }
                *str++ = c;
                break;
            default:
                *str++ = *p;
                break;
            }
            p++;
        }
    }
    *str = 0;
}

char *skipwhite(char *str)
{
    if(str)
        while(*str && isspace(*str))
            str++;
    return str;
}

void stripwhite(char *str)
{
    char   *end = str + strlen(str) - 1;

    while(end >= str && isspace(*end))
        end--;

    end[1] = '\0';
}

char *igets(void)
{
    char   *line;

    if(*PatchPt == '\0')
        return NULL;

    line = PatchPt;

    while(*PatchPt != '\n' && *PatchPt != '\0')
        PatchPt++;

    if(*PatchPt == '\n')
        *PatchPt++ = 0;

    return line;
}

int GetLine(void)
{
    char   *line, *line2;

    do
    {
        while((line = igets()))
            if(line[0] != '#')  // Skip comment lines
                break;

        if(!line)
            return 0;

        Line1 = skipwhite(line);
    } while(Line1 && *Line1 == 0);  // Loop until we get a line with
    // more than just whitespace.
    line = strchr(Line1, '=');

    if(line)
    {                           // We have an '=' in the input line
        line2 = line;
        while(--line2 >= Line1)
            if(*line2 > ' ')
                break;

        if(line2 < Line1)
            return 0;           // Nothing before '='

        *(line2 + 1) = 0;

        line++;
        while(*line && *line <= ' ')
            line++;

        if(*line == 0)
            return 0;           // Nothing after '='

        Line2 = line;

        return 1;
    }
    else
    {                           // No '=' in input line
        line = Line1 + 1;
        while(*line > ' ')
            line++;             // Get beyond first word

        *line++ = 0;
        while(*line && *line <= ' ')
            line++;             // Skip white space

        //.bex files don't have this restriction
        //if (*line == 0)
        //  return 0;           // No second word

        Line2 = line;

        return 2;
    }
}

int HandleMode(const char *mode, int num)
{
    int     i = 0;

    while(Modes[i].name && stricmp(Modes[i].name, mode))
        i++;

    if(Modes[i].name)
        return Modes[i].func(num);

    // Handle unknown or unimplemented data
    LPrintf("Unknown chunk %s encountered. Skipping.\n", mode);
    do
        i = GetLine();
    while(i == 1);

    return i;
}

int PatchThing(int thingy)
{
    size_t          thingNum = (size_t) thingy;

    static const struct Key keys[] = {
        {"ID #", myoffsetof(ded_mobj_t, doomEdNum, 0)},
        {"Hit points", myoffsetof(ded_mobj_t, spawnHealth, 0)},
        {"Reaction time", myoffsetof(ded_mobj_t, reactionTime, 0)},
        {"Pain chance", myoffsetof(ded_mobj_t, painChance, 0)},
        {"Speed", myoffsetof(ded_mobj_t, speed, OFF_FLOAT)},
        {"Width", myoffsetof(ded_mobj_t, radius, OFF_FIXED)},
        {"Height", myoffsetof(ded_mobj_t, height, OFF_FIXED)},
        {"Mass", myoffsetof(ded_mobj_t, mass, 0)},
        {"Missile damage", myoffsetof(ded_mobj_t, damage, 0)},
        //{ "Translucency",     myoffsetof(ded_mobj_t,translucency,0) },
        {"Alert sound", myoffsetof(ded_mobj_t, seeSound, OFF_SOUND)},
        {"Attack sound", myoffsetof(ded_mobj_t, attackSound, OFF_SOUND)},
        {"Pain sound", myoffsetof(ded_mobj_t, painSound, OFF_SOUND)},
        {"Death sound", myoffsetof(ded_mobj_t, deathSound, OFF_SOUND)},
        {"Action sound", myoffsetof(ded_mobj_t, activeSound, OFF_SOUND)},
        {NULL,}
    };
    static const struct {
        const char*     label;
        size_t          labelLen;
        statename_t     name;
    } stateNames[] = {
        { "Initial", 7, SN_SPAWN },
        { "First moving", 12, SN_SEE },
        { "Injury", 6, SN_PAIN },
        { "Close attack", 12, SN_MELEE },
        { "Far attack", 10, SN_MISSILE },
        { "Death", 5, SN_DEATH },
        { "Exploding", 9, SN_XDEATH },
        { "Respawn", 7, SN_RAISE },
        { NULL, 0 }
    };
    // Flags can be specified by name (a .bex extension):
    static const struct {
        short           bit;
        short           whichflags;
        const char*     name;
    } bitnames[] =
    {
        {0, 0, "SPECIAL"},
        {1, 0, "SOLID"},
        {2, 0, "SHOOTABLE"},
        {3, 0, "NOSECTOR"},
        {4, 0, "NOBLOCKMAP"},
        {5, 0, "AMBUSH"},
        {6, 0, "JUSTHIT"},
        {7, 0, "JUSTATTACKED"},
        {8, 0, "SPAWNCEILING"},
        {9, 0, "NOGRAVITY"},
        {10, 0, "DROPOFF"},
        {11, 0, "PICKUP"},
        {12, 0, "NOCLIP"},
        {14, 0, "FLOAT"},
        {15, 0, "TELEPORT"},
        {16, 0, "MISSILE"},
        {17, 0, "DROPPED"},
        {18, 0, "SHADOW"},
        {19, 0, "NOBLOOD"},
        {20, 0, "CORPSE"},
        {21, 0, "INFLOAT"},
        {22, 0, "COUNTKILL"},
        {23, 0, "COUNTITEM"},
        {24, 0, "SKULLFLY"},
        {25, 0, "NOTDMATCH"},
        {26, 0, "TRANSLATION1"},
        {26, 0, "TRANSLATION"},  // BOOM compatibility
        {27, 0, "TRANSLATION2"},
        {27, 0, "UNUSED1"},      // BOOM compatibility
        {28, 0, "STEALTH"},
        {28, 0, "UNUSED2"},      // BOOM compatibility
        {29, 0, "TRANSLUC25"},
        {29, 0, "UNUSED3"},      // BOOM compatibility
        {30, 0, "TRANSLUC50"},
        {(29 << 8) | 30, 0, "TRANSLUC75"},
        {30, 0, "UNUSED4"},      // BOOM compatibility
        {30, 0, "TRANSLUCENT"},  // BOOM compatibility?
        {31, 0, "RESERVED"},
        // Names for flags2
        {0, 1, "LOGRAV"},
        {1, 1, "WINDTHRUST"},
        {2, 1, "FLOORBOUNCE"},
        {3, 1, "BLASTED"},
        {4, 1, "FLY"},
        {5, 1, "FLOORCLIP"},
        {6, 1, "SPAWNFLOAT"},
        {7, 1, "NOTELEPORT"},
        {8, 1, "RIP"},
        {9, 1, "PUSHABLE"},
        {10, 1, "CANSLIDE"},     // Avoid conflict with SLIDE from BOOM
        {11, 1, "ONMOBJ"},
        {12, 1, "PASSMOBJ"},
        {13, 1, "CANNOTPUSH"},
        {14, 1, "DROPPED"},
        {15, 1, "BOSS"},
        {16, 1, "FIREDAMAGE"},
        {17, 1, "NODMGTHRUST"},
        {18, 1, "TELESTOMP"},
        {19, 1, "FLOATBOB"},
        {20, 1, "DONTDRAW"},
        {21, 1, "IMPACT"},
        {22, 1, "PUSHWALL"},
        {23, 1, "MCROSS"},
        {24, 1, "PCROSS"},
        {25, 1, "CANTLEAVEFLOORPIC"},
        {26, 1, "NONSHOOTABLE"},
        {27, 1, "INVULNERABLE"},
        {28, 1, "DORMANT"},
        {29, 1, "ICEDAMAGE"},
        {30, 1, "SEEKERMISSILE"},
        {31, 1, "REFLECTIVE"}
    };
    int             result;
    ded_mobj_t*     info, dummy;
    boolean         hadHeight = false;
    boolean         checkHeight = false;

    thingNum--;
    if(thingNum < (unsigned) ded->count.mobjs.num)
    {
        info = ded->mobjs + thingNum;
        if(verbose)
            LPrintf("Thing %lu\n", (unsigned long) thingNum);
    }
    else
    {
        info = &dummy;
        LPrintf("Thing %lu out of range. Create more Thing defs!\n",
                (unsigned long) (thingNum + 1));
    }

    while((result = GetLine()) == 1)
    {
        int             value = atoi(Line2);
        size_t          len = strlen(Line1);
        size_t          sndmap;

        sndmap = value;
        if(sndmap >= sizeof(SoundMap)-1)
            sndmap = 0;

        if(HandleKey(keys, info, Line1, value))
        {
            if(!stricmp(Line1 + len - 6, " frame"))
            {
                uint                i;

                for(i = 0; stateNames[i].label; ++i)
                {
                    if(!strnicmp(stateNames[i].label, Line1,
                                 stateNames[i].labelLen))
                    {
                        strcpy(info->states[stateNames[i].name],
                               ded->states[value].id);
                        break;
                    }
                }
            }
            else if(!stricmp(Line1, "Bits"))
            {
                int             value = 0, value2 = 0;
                boolean         vchanged = false, v2changed = false;
                char           *strval;

                for(strval = Line2; (strval = strtok(strval, ",+| \t\f\r"));
                    strval = NULL)
                {
                    size_t  iy;

                    if(IsNum(strval))
                    {
                        // Force the top 4 bits to 0 so that the user is forced
                        // to use the mnemonics to change them.
                        value |= (atoi(strval) & 0x0fffffff);
                        vchanged = true;
                    }
                    else
                    {
                        for(iy = 0;
                            iy < sizeof(bitnames) / sizeof(bitnames[0]); iy++)
                        {
                            if(!stricmp(strval, bitnames[iy].name))
                            {
                                if(bitnames[iy].whichflags)
                                {
                                    v2changed = true;
                                    if(bitnames[iy].bit & 0xff00)
                                        value2 |= 1 << (bitnames[iy].bit >> 8);
                                    value2 |= 1 << (bitnames[iy].bit & 0xff);
                                }
                                else
                                {
                                    vchanged = true;
                                    if(bitnames[iy].bit & 0xff00)
                                        value |= 1 << (bitnames[iy].bit >> 8);
                                    value |= 1 << (bitnames[iy].bit & 0xff);
                                }

                                break;
                            }
                        }

                        if(iy >= sizeof(bitnames) / sizeof(bitnames[0]))
                            LPrintf("Unknown bit mnemonic %s\n", strval);
                    }
                }

                if(vchanged)
                {
                    if(value & 0x10) // Old MF_NOBLOCKMAP
                    {
                        // Ensure vanilla compatibility; clear incompatible flags.
                        value &= ~(0x2 /*=MF_SOLID*/| 0x4 /*=MF_SHOOTABLE*/);
                    }

                    info->flags[0] = value;

                    if(value & 0x100) // Spawnceiling?
                        checkHeight = true;

                    // Bit flags are no longer used to specify translucency.
                    // This is just a temporary hack.
                    /*if (info->flags & 0x60000000)
                       info->translucency = (info->flags & 0x60000000) >> 15; */
                }

                if(v2changed)
                    info->flags[1] = value2;

                if(verbose)
                    LPrintf("Bits: %d,%d (0x%08x,0x%08x)\n", value, value2,
                            value, value2);
            }
            else
                LPrintf(unknown_str, Line1, "Thing", thingNum);
        }
        else if(!stricmp(Line1, "Height"))
        {
            hadHeight = true;
        }
    }

    if(checkHeight && !hadHeight && thingNum < sizeof(OrgHeights))
    {
        info->height = OrgHeights[thingNum];
    }

    return result;
}

int PatchSound(int soundNum)
{
/*    static struct Key keys[] = {
        {"Offset", 0}, //pointer to a name string, changed in text
        {"Zero/One", 0},  //info->singularity
        {"Value", myoffsetof(ded_sound_t, priority, 0)}, //info->priority
        {"Zero 1", myoffsetof(ded_sound_t, link, 0)}, //info->link
        {"Zero 2", myoffsetof(ded_sound_t, link_pitch, 0)}, //info->pitch
        {"Zero 3", myoffsetof(ded_sound_t, link_volume, 0)}, //info->volume
        {"Zero 4", 0}, //info->data
        {"Neg. One 1", 0}, //info->usefulness
        {"Neg. One 2", myoffsetof(ded_sound_t, id, OFF_SOUND)}, //info->lumpnum
        {NULL,}
    };
    ded_sound_t *info, dummy;*/
    int     result;

    LPrintf("Sound %d (not supported)\n", soundNum);
    /*
    if (soundNum >= 0 && soundNum < ded->count.sounds.num)
    {
        info = ded->sounds + soundNum
        if(verbose)
            LPrintf("Sound %d\n", soundNum);
    }
    else
    {
        info = &dummy;
        LPrintf ("Sound %d out of range.\n", soundNum);
    }
     */
    while((result = GetLine()) == 1)
    {
        /*if(HandleKey(keys, info, Line1, atoi(Line2)))
            LPrintf(unknown_str, Line1, "Sound", soundNum);*/
    }
    /*
       if (offset) {
       // Calculate offset from start of sound names
       offset -= toff[dversion] + 21076;

       if (offset <= 64)            // pistol .. bfg
       offset >>= 3;
       else if (offset <= 260)      // sawup .. oof
       offset = (offset + 4) >> 3;
       else                     // telept .. skeatk
       offset = (offset + 8) >> 3;

       if (offset >= 0 && offset < NUMSFX) {
       S_sfx[soundNum].name = OrgSfxNames[offset + 1];
       } else {
       Printf ("Sound name %d out of range.\n", offset + 1);
       }
       }
     */
    return result;
}

int PatchFrame(int frameNum)
{
    static struct Key keys[] = {
        {"Sprite number", myoffsetof(ded_state_t, sprite, OFF_SPRITE)},
        {"Sprite subnumber", myoffsetof(ded_state_t, frame, 0)},
        {"Duration", myoffsetof(ded_state_t, tics, 0)},
        {"Next frame", myoffsetof(ded_state_t, nextState, OFF_STATE)},
        {"Unknown 1", 0 /*myoffsetof(ded_state_t,misc[0],0) */ },
        {"Unknown 2", 0 /*myoffsetof(ded_state_t,misc[1],0) */ },
        {NULL,}
    };
    int     result;
    ded_state_t *info, dummy;

    // C doesn't allow non-constant initializers.
    keys[4].offset = myoffsetof(ded_state_t, misc[0], 0);
    keys[5].offset = myoffsetof(ded_state_t, misc[1], 0);

    if(frameNum >= 0 && frameNum < ded->count.states.num)
    {
        info = ded->states + frameNum;
        if(verbose)
            LPrintf("Frame %d\n", frameNum);
    }
    else
    {
        info = &dummy;
        LPrintf("Frame %d out of range (Create more State defs!)\n", frameNum);
    }

    while((result = GetLine()) == 1)
        if(HandleKey(keys, info, Line1, atoi(Line2)))
            LPrintf(unknown_str, Line1, "Frame", frameNum);

    return result;
}

int PatchSprite(int sprNum)
{
    int     result;
    int     offset = 0;

    if(sprNum >= 0 && sprNum < NUMSPRITES)
    {
        if(verbose)
            LPrintf("Sprite %d\n", sprNum);
    }
    else
    {
        LPrintf("Sprite %d out of range. Create more Sprite defs!\n", sprNum);
        sprNum = -1;
    }

    while((result = GetLine()) == 1)
    {
        if(!stricmp("Offset", Line1))
            offset = atoi(Line2);
        else
            LPrintf(unknown_str, Line1, "Sprite", sprNum);
    }

    if(offset > 0 && sprNum != -1)
    {
        // Calculate offset from beginning of sprite names.
        offset = (offset - toff[dversion] - 22044) / 8;

        if(offset >= 0 && offset < ded->count.sprites.num)
        {
            //sprNames[sprNum] = OrgSprNames[offset];
            strcpy(ded->sprites[sprNum].id, OrgSprNames[offset]);
        }
        else
        {
            LPrintf("Sprite name %d out of range.\n", offset);
        }
    }

    return result;
}

/**
 * CRT's realloc can't access other modules' memory, so we must ask
 * Doomsday to reallocate memory for us.
 */
void *DD_Realloc(void *ptr, int newsize)
{
    ded_count_t cnt;

    cnt.max = 0;
    cnt.num = newsize;
    DED_NewEntries(&ptr, &cnt, 1, 0);
    return ptr;
}

void SetValueStr(const char *path, const char *id, char *str)
{
    int     i;
    char    realid[300];

    //CString realid = CString(path) + "|" + id;

    sprintf(realid, "%s|%s", path, id);

    for(i = 0; i < ded->count.values.num; i++)
        if(!stricmp(ded->values[i].id, realid))
        {
            ded->values[i].text =
                (char *) DD_Realloc(ded->values[i].text, strlen(str) + 1);
            strcpy(ded->values[i].text, str);
            return;
        }

    // Not found, create a new Value.
    i = DED_AddValue(ded, realid);
    // Must allocate memory using DD_Realloc.
    ded->values[i].text = 0;
    strcpy(ded->values[i].text =
           DD_Realloc(ded->values[i].text, strlen(str) + 1), str);
}

void SetValueInt(const char *path, const char *id, int val)
{
    char    buf[80];

    sprintf(buf, "%i", val);
    SetValueStr(path, id, buf);
}

int PatchAmmo(int ammoNum)
{
    int     result, max, per;
    char   *ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
    char   *theAmmo = NULL;

    //  CString str;

    if(ammoNum >= 0 && ammoNum < 4)
    {
        if(verbose)
            LPrintf("Ammo %d.\n", ammoNum);
        theAmmo = ammostr[ammoNum];
    }
    else
        LPrintf("Ammo %d out of range.\n", ammoNum);

    /*  int *max;
       int *per;
       int dummy;

       if (ammoNum >= 0 && ammoNum < NUM_AMMO_TYPES) {
       DPrintf ("Ammo %d.\n", ammoNum);
       max = &maxammo[ammoNum];
       per = &clipammo[ammoNum];
       } else {
       Printf (PRINT_HIGH, "Ammo %d out of range.\n", ammoNum);
       max = per = &dummy;
       }
     */
    while((result = GetLine()) == 1)
    {
        max = per = -1;
        CHECKKEY("Max ammo", max)
        else
        CHECKKEY("Per ammo", per)
        else
        LPrintf(unknown_str, Line1, "Ammo", ammoNum);
        if(!theAmmo)
            continue;

        if(max != -1)
            SetValueInt("Player|Max ammo", theAmmo, max);
        if(per != -1)
            SetValueInt("Player|Clip ammo", theAmmo, per);
    }

    return result;
}

int PatchNothing()
{
    int     result;

    while((result = GetLine()) == 1)
    {
    }

    return result;
}

int PatchWeapon(int weapNum)
{
    //  LPrintf("Weapon patches not supported.\n");
    //  return PatchNothing();

    char   *ammotypes[] =
        { "clip", "shell", "cell", "misl", "-", "noammo", 0 };
    char    buf[80];
    int     val;
    int     result;

    if(weapNum >= 0)
    {
        if(verbose)
            LPrintf("Weapon %d\n", weapNum);
        sprintf(buf, "Weapon Info|%d", weapNum);
    }
    else
    {
        LPrintf("Weapon %d out of range.\n", weapNum);
        return PatchNothing();
    }

    /*  static const struct Key keys[] = {
       { "Ammo type",           myoffsetof(weaponinfo_t,ammo) },
       { "Deselect frame",      myoffsetof(weaponinfo_t,upstate) },
       { "Select frame",        myoffsetof(weaponinfo_t,downstate) },
       { "Bobbing frame",       myoffsetof(weaponinfo_t,readystate) },
       { "Shooting frame",      myoffsetof(weaponinfo_t,atkstate) },
       { "Firing frame",        myoffsetof(weaponinfo_t,flashstate) },
       { NULL, }
       };

       weaponinfo_t *info, dummy;

       if (weapNum >= 0 && weapNum < NUM_WEAPON_TYPES) {
       info = &weaponinfo[weapNum];
       DPrintf ("Weapon %d\n", weapNum);
       } else {
       info = &dummy;
       Printf (PRINT_HIGH, "Weapon %d out of range.\n", weapNum);
       }
     */
    while((result = GetLine()) == 1)
    {
        /*
           if (HandleKey (keys, info, Line1, atoi (Line2)))
           Printf (PRINT_HIGH, unknown_str, Line1, "Weapon", weapNum); */

        //#define CHECKKEY2(a,b)        if (!stricmp (Line1, (a))) { (b) = atoi(Line2);

        val = atoi(Line2);

        if(!stricmp(Line1, "Ammo type"))
            SetValueStr(buf, "Type", ammotypes[val]);
        else if(!stricmp(Line1, "Deselect frame"))
            SetValueStr(buf, "Up", ded->states[val].id);
        else if(!stricmp(Line1, "Select frame"))
            SetValueStr(buf, "Down", ded->states[val].id);
        else if(!stricmp(Line1, "Bobbing frame"))
            SetValueStr(buf, "Ready", ded->states[val].id);
        else if(!stricmp(Line1, "Shooting frame"))
            SetValueStr(buf, "Atk", ded->states[val].id);
        else if(!stricmp(Line1, "Firing frame"))
            SetValueStr(buf, "Flash", ded->states[val].id);
        else
            LPrintf(unknown_str, Line1, "Weapon", weapNum);
    }
    return result;
}

int PatchPointer(int ptrNum)
{
    int     result;

    if(ptrNum >= 0 && ptrNum < 448)
    {
        if(verbose)
            LPrintf("Pointer %d\n", ptrNum);
    }
    else
    {
        LPrintf("Pointer %d out of range.\n", ptrNum);
        ptrNum = -1;
    }

    while((result = GetLine()) == 1)
    {
        if((ptrNum != -1) && (!stricmp(Line1, "Codep Frame")))
        {
            //states[codepconv[ptrNum]].action = OrgActionPtrs[atoi (Line2)];
            strcpy(ded->states[codepconv[ptrNum]].action,
                   OrgActionPtrs[atoi(Line2)]);
        }
        else
            LPrintf(unknown_str, Line1, "Pointer", ptrNum);
    }
    return result;
}

int PatchCheats(int dummy)
{
    /*  static const struct {
       char *name;
       byte *cheatseq;
       BOOL needsval;
       } keys[] = {
       { "Change music",        cheat_mus_seq,               true },
       { "Chainsaw",            cheat_choppers_seq,          false },
       { "God mode",            cheat_god_seq,               false },
       { "Ammo & Keys",     cheat_ammo_seq,              false },
       { "Ammo",                cheat_ammonokey_seq,         false },
       { "No Clipping 1",       cheat_noclip_seq,            false },
       { "No Clipping 2",       cheat_commercial_noclip_seq, false },
       { "Invincibility",       cheat_powerup_seq[0],        false },
       { "Berserk",         cheat_powerup_seq[1],        false },
       { "Invisibility",        cheat_powerup_seq[2],        false },
       { "Radiation Suit",      cheat_powerup_seq[3],        false },
       { "Auto-map",            cheat_powerup_seq[4],        false },
       { "Lite-Amp Goggles",    cheat_powerup_seq[5],        false },
       { "BEHOLD menu",     cheat_powerup_seq[6],        false },
       { "Level Warp",          cheat_clev_seq,              true },
       { "Player Position", cheat_mypos_seq,             false },
       { "Map cheat",           cheat_amap_seq,              false },
       { NULL, }
       };
       int result;

       DPrintf ("Cheats\n");

       while ((result = GetLine ()) == 1) {
       int i = 0;
       while (keys[i].name && stricmp (keys[i].name, Line1))
       i++;

       if (!keys[i].name)
       Printf (PRINT_HIGH, "Unknown cheat %s.\n", Line2);
       else
       ChangeCheat (Line2, keys[i].cheatseq, keys[i].needsval);
       }
       return result; */

    LPrintf("Cheat patches are not supported!\n");
    return PatchNothing();
}

int PatchMisc(int dummy)
{
    /*  static const struct Key keys[] = {
       { "Initial Health",          myoffsetof(struct DehInfo,StartHealth) },
       { "Initial Bullets",     myoffsetof(struct DehInfo,StartBullets) },
       { "Max Health",              myoffsetof(struct DehInfo,MaxHealth) },
       { "Max Armor",               myoffsetof(struct DehInfo,MaxArmor) },
       { "Green Armor Class",       myoffsetof(struct DehInfo,GreenAC) },
       { "Blue Armor Class",        myoffsetof(struct DehInfo,BlueAC) },
       { "Max Soulsphere",          myoffsetof(struct DehInfo,MaxSoulsphere) },
       { "Soulsphere Health",       myoffsetof(struct DehInfo,SoulsphereHealth) },
       { "Megasphere Health",       myoffsetof(struct DehInfo,MegasphereHealth) },
       { "God Mode Health",     myoffsetof(struct DehInfo,GodHealth) },
       { "IDFA Armor",              myoffsetof(struct DehInfo,FAArmor) },
       { "IDFA Armor Class",        myoffsetof(struct DehInfo,FAAC) },
       { "IDKFA Armor",         myoffsetof(struct DehInfo,KFAArmor) },
       { "IDKFA Armor Class",       myoffsetof(struct DehInfo,KFAAC) },
       { "BFG Cells/Shot",          myoffsetof(struct DehInfo,BFGCells) },
       { "Monsters Infight",        myoffsetof(struct DehInfo,Infight) },
       { NULL, }
       };
       gitem_t *item;
     */
    int     result;
    int     val;

    if(verbose)
        LPrintf("Misc\n");

    while((result = GetLine()) == 1)
    {
        val = atoi(Line2);

        if(!stricmp(Line1, "Initial Health"))
            SetValueInt("Player|Health", "Type", val);

        else if(!stricmp(Line1, "Initial Bullets"))
            SetValueInt("Player|Init ammo", "Clip", val);

        else if(!stricmp(Line1, "Max Health"))
            SetValueInt("Player|Health Limit", "Type", val);

        else if(!stricmp(Line1, "Max Armor"))
            SetValueInt("Player|Blue Armor", "Type", val);

        else if(!stricmp(Line1, "Green Armor Class"))
            SetValueInt("Player|Green Armor Class", "Type", val);

        else if(!stricmp(Line1, "Blue Armor Class"))
            SetValueInt("Player|Blue Armor Class", "Type", val);

        else if(!stricmp(Line1, "Max Soulsphere"))
            SetValueInt("SoulSphere|Give", "Health Limit", val);

        else if(!stricmp(Line1, "Soulsphere Health"))
            SetValueInt("SoulSphere|Give", "Health", val);

        else if(!stricmp(Line1, "Megasphere Health"))
            SetValueInt("MegaSphere|Give", "Health", val);

        else if(!stricmp(Line1, "God Mode Health"))
            SetValueInt("Player|God Health", "Type", val);

        else if(!stricmp(Line1, "IDFA Armor"))
            SetValueInt("Player|IDFA Armor", "Type", val);

        else if(!stricmp(Line1, "IDFA Armor Class"))
            SetValueInt("Player|IDFA Armor Class", "Type", val);

        else if(!stricmp(Line1, "IDKFA Armor"))
            SetValueInt("Player|IDKFA Armor", "Type", val);

        else if(!stricmp(Line1, "IDKFA Armor Class"))
            SetValueInt("Player|IDKFA Armor Class", "Type", val);

        else if(!stricmp(Line1, "BFG Cells/Shot"))
            SetValueInt("Weapon Info|6", "Per shot", val);

        else if(!stricmp(Line1, "Monsters Infight"))
            SetValueInt("AI", "Infight", val);
        else
            LPrintf("Unknown miscellaneous info %s = %s.\n", Line1, Line2);
    }

    return result;
}

int PatchPars(int dummy)
{
    char   *space, mapname[8], *moredata;
    ded_mapinfo_t *info;
    int     result, par, i;

    if(verbose)
        LPrintf("[Pars]\n");

    while((result = GetLine()))
    {
        // Argh! .bex doesn't follow the same rules as .deh
        if(result == 1)
        {
            LPrintf("Unknown key in [PARS] section: %s\n", Line1);
            continue;
        }
        if(stricmp("par", Line1))
            return result;

        space = strchr(Line2, ' ');

        if(!space)
        {
            LPrintf("Need data after par.\n");
            continue;
        }

        *space++ = '\0';

        while(*space && isspace(*space))
            space++;

        moredata = strchr(space, ' ');

        if(moredata)
        {
            // At least 3 items on this line, must be E?M? format
            sprintf(mapname, "E%cM%c", *Line2, *space);
            par = atoi(moredata + 1);
        }
        else
        {
            // Only 2 items, must be MAP?? format
            sprintf(mapname, "MAP%02d", atoi(Line2) % 100);
            par = atoi(space);
        }

        info = NULL;
        /*if (!(info = FindLevelInfo (mapname)) ) {
           Printf (PRINT_HIGH, "No map %s\n", mapname);
           continue;
           } */
        for(i = 0; i < ded->count.mapInfo.num; i++)
            if(!stricmp(ded->mapInfo[i].id, mapname))
            {
                info = ded->mapInfo + i;
                break;
            }

        if(info)
            info->parTime = (float) par;

        LPrintf("Par for %s changed to %d\n", mapname, par);
    }
    return result;
}

int PatchCodePtrs(int dummy)
{
    LPrintf("[CodePtr] patches not supported\n");
    return PatchNothing();

    /*  int result;

       LPrintf ("[CodePtr]\n");

       while ((result = GetLine()) == 1)
       {
       if (!strnicmp ("Frame", Line1, 5) && isspace(Line1[5]))
       {
       int frame = atoi (Line1 + 5);

       if (frame < 0 || frame >= NUMSTATES)
       {
       Printf (PRINT_HIGH, "Frame %d out of range\n", frame);
       }
       else
       {
       int i = 0;
       char *data;

       COM_Parse (Line2);

       if ((com_token[0] == 'A' || com_token[0] == 'a') && com_token[1] == '_')
       data = com_token + 2;
       else
       data = com_token;

       while (CodePtrs[i].name && stricmp (CodePtrs[i].name, data))
       i++;

       if (CodePtrs[i].name) {
       states[frame].action.acp1 = CodePtrs[i].func.acp1;
       DPrintf ("Frame %d set to %s\n", frame, CodePtrs[i].name);
       } else {
       states[frame].action.acp1 = NULL;
       DPrintf ("Unknown code pointer: %s\n", com_token);
       }
       }
       }
       }
       return result; */
}

static void replaceInString(char* str, size_t len, const char* occurance,
                            const char* replacewith)
{
    char*               buf, *out, *in;
    int                 oclen, replen;

    if(!str || !len || !occurance || !(oclen = strlen(occurance)) ||
       !replacewith || !(replen = strlen(replacewith)))
        return;

    out = buf = calloc(strlen(str) * 2, 1);
    in = str;

    for(; *in; in++)
    {
        if(!strncmp(in, occurance, oclen))
        {
            strcat(out, replacewith);
            out += replen;
            in += oclen - 1;
        }
        else
            *out++ = *in;
    }

    strncpy(str, buf, len);
    free(buf);
}

static void patchSpriteNames(const char* origName, const char* newName)
{
    size_t              i;
    char                old[5];

    if(strlen(origName) != 4)
        return;

    for(i = 0; i < 4; ++i)
        old[i] = (char) toupper((int) origName[i]);
    old[4] = '\0';

    i = 0;
    do
    {
        int                 num;

        if(!strcmp(SpriteMap[i], old) &&
           (num = Def_Get(DD_DEF_SPRITE, old, NULL)) != -1)
        {
            ded_sprid_t*        spr = &ded->sprites[num];

            strncpy(spr->id, newName, DED_SPRITEID_LEN);
        }
    } while(SpriteMap[++i] != NULL);
}

static void patchMusicLumpNames(const char* origName, const char* newName)
{
    char                buf[9];
    size_t              i;

    dd_snprintf(buf, 9, "d_%s", origName);

    i = 0;
    do
    {
        if(!strcmp(MusicMap[i], origName))
        {
            int                 j;

            for(j = 0; j < ded->count.music.num; ++j)
            {
                if(!stricmp(ded->music[j].lumpName, buf))
                {
                    dd_snprintf(ded->music[j].lumpName, 9, "D_%s", newName);
                }
            }
        }
    } while(MusicMap[++i] != NULL);
}

static const char* textIDForOrigString(const char* str)
{
    size_t              i;

    i = 0;
    do
    {
        if(!strcmp(TextMap[i].str, str))
            return TextMap[i].id;
    } while(TextMap[++i].id != NULL);

    return NULL;
}

static void patchText(const char* origStr, const char* newStr)
{
#define BUF_SIZE        4096

    int                 id;

    if(!((id = Def_Get(DD_DEF_TEXT, textIDForOrigString(origStr), NULL)) < 0))
    {
        char                buf[BUF_SIZE];

        strncpy(buf, newStr, 4096);
        replaceInString(buf, 4096, "\n", "\\n");

        Def_Set(DD_DEF_TEXT, id, 0, buf);
    }

#undef BUF_SIZE
}

int PatchText(int oldSize)
{
    int                 newSize;
    char*               oldStr, *newStr, *temp;
    boolean             parseError = false;

    temp = COM_Parse(Line2); // Skip old size, since we already have it
    if(!COM_Parse(temp))
    {
        LPrintf("Text chunk is missing size of new string.\n");
        return 2;
    }
    newSize = atoi(com_token);

    oldStr = malloc(oldSize + 1);
    newStr = malloc(newSize + 1);

    if(oldStr && newStr)
    {
        boolean             good;

        good = ReadChars(&oldStr, oldSize, false);
        good += ReadChars(&newStr, newSize, true);

        if(good)
        {
            if(!includenotext)
            {
                if(verbose)
                {
                    LPrintf("Searching for text:\n%s\n", oldStr);
                    LPrintf("<< TO BE REPLACED WITH:\n%s\n>>\n", newStr);
                }

                patchSpriteNames(oldStr, newStr);
                patchMusicLumpNames(oldStr, newStr);
                patchText(oldStr, newStr);
            }
            else
            {
                LPrintf("Skipping text chunk in included patch.\n");
            }
        }
        else
        {
            LPrintf("Unexpected end-of-file.\n");
            parseError = true;
        }
    }
    else
    {
        LPrintf("Out of memory.\n");
    }

    if(newStr)
        free(newStr);
    if(oldStr)
        free(oldStr);

    if(!parseError)
    {
        int                 result;

        // Fetch next identifier for main loop
        while((result = GetLine()) == 1);

        return result;
    }

    return 0;
}

int PatchStrings(int dummy)
{
    LPrintf("[Strings] patches not supported\n");
    return PatchNothing();

    /*  static size_t maxstrlen = 128;
       static char *holdstring;
       int result;

       DPrintf ("[Strings]\n");

       if (!holdstring)
       holdstring = Malloc (maxstrlen);

       while ((result = GetLine()) == 1) {
       int i;

       *holdstring = '\0';
       do {
       while (maxstrlen < strlen (holdstring) + strlen (Line2)) {
       maxstrlen += 128;
       holdstring = (char *)Realloc (holdstring, maxstrlen);
       }
       strcat (holdstring, skipwhite (Line2));
       stripwhite (holdstring);
       if (holdstring[strlen(holdstring)-1] == '\\') {
       holdstring[strlen(holdstring)-1] = '\0';
       Line2 = igets ();
       } else
       Line2 = NULL;
       } while (Line2 && *Line2);

       for (i = 0; i < NUMSTRINGS; i++)
       if (!stricmp (Strings[i].name, Line1))
       break;

       if (i == NUMSTRINGS) {
       Printf (PRINT_HIGH, "Unknown string: %s\n", Line1);
       } else {
       ReplaceSpecialChars (holdstring);
       ReplaceString (&Strings[i].string, copystring (holdstring));
       Strings[i].type = str_patched;
       Printf (PRINT_HIGH, "%s set to:\n%s\n", Line1, holdstring);
       }
       }
       return result;
     */
}

int DoInclude(int dummy)
{
    char   *data;
    int     savedversion, savepversion;
    char   *savepatchfile, *savepatchpt;
    int     len;
    FILE   *file;
    char   *patch;

    if(including)
    {
        LPrintf("Sorry, can't nest includes\n");
        goto endinclude;
    }

    data = COM_Parse(Line2);
    if(!stricmp(com_token, "notext"))
    {
        includenotext = true;
        data = COM_Parse(data);
    }

    if(!com_token[0])
    {
        includenotext = false;
        LPrintf("Include directive is missing filename\n");
        goto endinclude;
    }

    if(verbose)
        LPrintf("Including %s\n", com_token);
    savepatchfile = PatchFile;
    savepatchpt = PatchPt;
    savedversion = dversion;
    savepversion = pversion;
    including = true;

    file = fopen(com_token, "rt");
    if(!file)
    {
        LPrintf("Can't include %s, it can't be found.\n", com_token);
        goto endinclude;
    }
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    patch = calloc(len + 1, 1);
    rewind(file);
    fread(patch, len, 1, file);
    patch[len] = 0;
    fclose(file);
    ApplyDEH(patch, len);
    free(patch);

    if(verbose)
        LPrintf("Done with include\n");
    PatchFile = savepatchfile;
    PatchPt = savepatchpt;
    dversion = savedversion;
    pversion = savepversion;

  endinclude:
    including = false;
    includenotext = false;
    return GetLine();
}

void ApplyDEH(char *patch, int length)
{
    int     cont;

    BackupData();
    PatchFile = patch;
    dversion = pversion = -1;

    cont = 0;
    if(!strncmp(PatchFile, "Patch File for DeHackEd v", 25))
    {
        PatchPt = strchr(PatchFile, '\n');
        while((cont = GetLine()) == 1)
        {
            CHECKKEY("Doom version", dversion)
            else
        CHECKKEY("Patch format", pversion)}
        if(!cont || dversion == -1 || pversion == -1)
        {
            Con_Message("This is not a DeHackEd patch file!");
            return;
        }
    }
    else
    {
        LPrintf("Patch does not have DeHackEd signature. Assuming .bex\n");
        dversion = 19;
        pversion = 6;
        PatchPt = PatchFile;
        while((cont = GetLine()) == 1);
    }

    if(pversion != 6)
    {
        LPrintf
            ("DeHackEd patch version is %d.\nUnexpected results may occur.\n",
             pversion);
    }

    if(dversion == 16)
        dversion = 0;
    else if(dversion == 17)
        dversion = 2;
    else if(dversion == 19)
        dversion = 3;
    else if(dversion == 20)
        dversion = 1;
    else if(dversion == 21)
        dversion = 4;
    else
    {
        LPrintf
            ("Patch created with unknown DOOM version.\nAssuming version 1.9.\n");
        dversion = 3;
    }

    do
    {
        if(cont == 1)
        {
            LPrintf("Key %s encountered out of context\n", Line1);
            cont = 0;
        }
        else if(cont == 2)
            cont = HandleMode(Line1, atoi(Line2));
    } while(cont);

}

/**
 * Reads and applies the given lump as a DEH patch.
 */
void ReadDehackedLump(int lumpnum)
{
    size_t      len;
    byte       *lump;

    Con_Message("Applying Dehacked: lump %i...\n", lumpnum);
    len = W_LumpLength(lumpnum);
    lump = calloc(len + 1, 1);
    memcpy(lump, W_CacheLumpNum(lumpnum, PU_CACHE), len);
    ApplyDEH((char*)lump, len);
    free(lump);
}

/**
 * Reads and applies the given Dehacked patch file.
 */
void ReadDehacked(char *filename)
{
    FILE   *file;
    char   *deh;
    int     len;

    Con_Message("Applying Dehacked: %s...\n", filename);

    // Read in the patch.
    if((file = fopen(filename, "rt")) == NULL)
        return;                 // Shouldn't happen.
    // How long is it?
    fseek(file, 0, SEEK_END);
    len = ftell(file) + 1;
    // Allocate enough memory and read it.
    deh = calloc(len + 1, 1);
    rewind(file);
    fread(deh, len, 1, file);
    fclose(file);
    // Process it!
    ApplyDEH(deh, len);
    free(deh);
}

/**
 * This will be called after the engine has loaded all definitions but
 * before the data they contain has been initialized.
 */
int DefsHook(int hook_type, int parm, void *data)
{
    int                 i;

    verbose = ArgExists("-verbose");
    ded = (ded_t *) data;

    // Check for a DEHACKED lumps.
    for(i = DD_GetInteger(DD_NUMLUMPS) - 1; i >= 0; i--)
        if(!strnicmp(W_LumpName(i), "DEHACKED", 8))
        {
            ReadDehackedLump(i);
            // We'll only continue this if the -alldehs option is given.
            if(!ArgCheck("-alldehs"))
                break;
        }

    // How about the -deh option?
    if(ArgCheckWith("-deh", 1))
    {
        filename_t          temp;
        const char*         fn;

        // Aha! At least one DEH specified. Let's read all of 'em.
        while((fn = ArgNext()) != NULL && fn[0] != '-')
        {
            M_TranslatePath(temp, fn, FILENAME_T_MAXLEN);
            if(!M_FileExists(temp))
                continue;

            ReadDehacked(temp);
        }
    }
    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_DEFS, DefsHook);
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Register our hooks.
        DP_Initialize();
        break;
    }
    return TRUE;
}
#endif

#if 0
/**
 * Here follows the remainder of the text strings in DOOM2.EXE which can
 * be changed by patching with DeHackED 3.0
 *
 * \todo There are a few interesting strings amongst this lot which I am
 * pretty sure have been used in some of the older mods such as Aliens TC.
 */
"CODEC: Testing I/O address: %04x\n"
"CODEC: Starting the \"busy\" test.\n"
"CODEC: The \"busy\" test failed.\n"
"CODEC: Passed the \"busy\" test.\n"
"CODEC: Starting the version test.\n"
"CODEC: Failed the version inspection.\n"
"CODEC: Passed the version inspection.\n"
"CODEC: Starting the misc. write test.\n"
"CODEC: Failed the misc. write test.\n"
"CODEC: Passed all I/O port inspections.\n"
"="
"SNDSCAPE"
"\\SNDSCAPE.INI"
"r"
"Port"
"DMA"
"IRQ"
"WavePort"
" out of GUS RAM:"
"%d%c"
"GF1PATCH110"
"110"
",%d"
"Ultrasound @ Port:%03Xh, IRQ:%d,  DMA:%d\n"
"Sizing GUS RAM:"
"BANK%d:OK "
"BANK%d:N/A "
"\n"
"ULTRADIR"
"\\midi\\"
"\r\n"
"Loading GUS Patches\n"
".pat"
"%s"
" - FAILED\n"
" - OK\n"
"ULTRASND"
"NO MVSOUND.SYS\n"
"PAS IRQ: %d  DMA:%d\n"
"Pro Audio Spectrum is NOT version 1.01.\n"
"ECHO Personal Sound System Enabled.\n"
"BLASTER"
"%x"
"Pro Audio Spectrum 3D JAZZ\n"
"\nDSP Version: %x.%02x\n"
"        IRQ: %d\n"
"        DMA: %d\n"
"GENMIDI.OP2"
"MixPg: "
"WAVE"
"fmt "
"data"
"MThd"
"MTrk"
"MUS"
"DMXTRACE"
"DMXOPTION"
"-opl3"
"-phase"
"Bad I_UpdateBox (%i, %i, %i, %i)"
"PLAYPAL"
"0x%x\n"
"-nomouse"
"Mouse: not present\n"
"Mouse: detected\n"
"-nojoy"
"joystick not found\n"
"joystick found\n"
"CENTER the joystick and press button 1:"
"\nPush the joystick to the UPPER LEFT corner and press button 1:"
"\nPush the joystick to the LOWER RIGHT corner and press button 1:"
"\n"
"INT_SetTimer0: %i is a bad value"
"novideo"
"-control"
"Using external control API\n"
"I_StartupDPMI\n"
"I_StartupMouse\n"
"I_StartupJoystick\n"
"I_StartupKeyboard\n"
"I_StartupSound\n"
"ENDOOM"
"DPMI memory: 0x%x"
", 0x%x allocated for zone\n"
"Insufficient memory!  You need to have at least 3.7 megabytes of total\n"
"free memory available for DOOM to execute.  Reconfigure your CONFIG.SYS\n"
"or AUTOEXEC.BAT to load fewer device drivers or TSR's.  We recommend\n"
"creating a custom boot menu item in your CONFIG.SYS for optimum DOOMing.\n"
"Please consult your DOS manual (\"Making more memory available\") for\n"
"information on how to free up more memory for DOOM.\n\n"
"DOOM aborted.\n"
"-cdrom"
"STCDROM"
"STDISK"
"I_AllocLow: DOS alloc of %i failed, %i free"
"-net"
"malloc() in I_InitNetwork() failed"
"I_NetCmd when not in netgame"
"I_StartupTimer()\n"
"Can't register 35 Hz timer w/ DMX library"
"d%c%s"
"-nosound"
"-nosfx"
"-nomusic"
"ENSONIQ\n"
"Dude.  The ENSONIQ ain't responding.\n"
"CODEC p=0x%x, d=%d\n"
"CODEC.  The CODEC ain't responding.\n"
"GUS\n"
"GUS1\n"
"Dude.  The GUS ain't responding.\n"
"GUS2\n"
"dmxgusc"
"dmxgus"
"cfg p=0x%x, i=%d, d=%d\n"
"SB isn't responding at p=0x%x, i=%d, d=%d\n"
"SB_Detect returned p=0x%x,i=%d,d=%d\n"
"Adlib\n"
"Dude.  The Adlib isn't responding.\n"
"genmidi"
"Midi\n"
"cfg p=0x%x\n"
"The MPU-401 isn't reponding @ p=0x%x.\n"
"I_StartupSound: Hope you hear a pop.\n"
"  Music device #%d & dmxCode=%d\n"
"  Sfx device #%d & dmxCode=%d\n"
"  calling DMX_Init\n"
"  DMX_Init() returned %d\n"
"CyberMan: Wrong mouse driver - no SWIFT support (AX=%04x).\n"
"CyberMan: no SWIFT device connected.\n"
"CyberMan: SWIFT device is not a CyberMan! (type=%d)\n"
"CyberMan: CyberMan %d.%02d connected.\n"
"SLIME16"
"RROCK14"
"RROCK07"
"RROCK17"
"RROCK13"
"RROCK19"
"FLOOR4_8"
"SFLR6_1"
"MFLR8_4"
"MFLR8_3"
"BOSSBACK"
"PFUB2"
"PFUB1"
"END0"
"END%i"
"CREDIT"
"VICTORY2"
"ENDPIC"
"map01"
"PLAYPAL"
"M_PAUSE"
"-debugfile"
"debug%i.txt"
"debug output to: %s\n"
"w"
"TITLEPIC"
"demo1"
"CREDIT"
"demo2"
"demo3"
"demo4"
"default.cfg"
"doom1.wad"
"doom2f.wad"
"doom2.wad"
"doom.wad"
"-shdev"
"c:/localid/doom1.wad"
"f:/doom/data_se/data_se/texture1.lmp"
"f:/doom/data_se/data_se/pnames.lmp"
"c:/localid/default.cfg"
"-regdev"
"c:/localid/doom.wad"
"f:/doom/data_se/data_se/texture1.lmp"
"f:/doom/data_se/data_se/texture2.lmp"
"f:/doom/data_se/data_se/pnames.lmp"
"-comdev"
"c:/localid/doom2.wad"
"f:/doom/data_se/cdata/texture1.lmp"
"f:/doom/data_se/cdata/pnames.lmp"
"French version\n"
"Game mode indeterminate.\n"
"rb"
"\nNo such response file!"
"Found response file %s!\n"
"%d command-line args:\n"
"%s\n"
"-nomonsters"
"-respawn"
"-fast"
"-devparm"
"-altdeath"
"-deathmatch"
"                         The Ultimate DOOM Startup v%i.%i                "
"                         DOOM 2: Hell on Earth v%i.%i                    "
"\nP_Init: Checking cmd-line parameters...\n"
"-cdrom"
"c:\\doomdata"
"c:/doomdata/default.cfg"
"-turbo"
"turbo scale: %i%%\n"
"-wart"
"~f:/doom/data_se/cdata/map0%i.wad"
"~f:/doom/data_se/cdata/map%i.wad"
"~f:/doom/data_se/E%cM%c.wad"
"Warping to Episode %s, Map %s.\n"
"-file"
"-playdemo"
"-timedemo"
"%s.lmp"
"Playing demo %s.lmp.\n"
"-skill"
"-episode"
"-timer"
"Levels will end after %d minute"
"s"
".\n"
"-avg"
"Austin Virtual Gaming: Levels will end after 20 minutes\n"
"-warp"
"V_Init: allocate screens.\n"
"M_LoadDefaults: Load system defaults.\n"
"Z_Init: Init zone memory allocation daemon. \n"
"W_Init: Init WADfiles.\n"
"\nYou cannot -file with the shareware version. Register!"
"\nThis is not the registered version."
"===========================================================================\nATTENTION:  This version of DOOM has been modified.  If you would like to\nget a copy of the original game, call 1-800-IDGAMES or see the readme file.\n        You will not receive technical support for modified games.\n                      press enter to continue\n===========================================================================\n"
"\tregistered version.\n"
"===========================================================================\n             This version is NOT SHAREWARE, do not distribute!\n         Please report software piracy to the SPA: 1-800-388-PIR8\n===========================================================================\n"
"\tshareware version.\n"
"\tcommercial version.\n"
"===========================================================================\n                            Do not distribute!\n         Please report software piracy to the SPA: 1-800-388-PIR8\n===========================================================================\n"
"M_Init: Init miscellaneous info.\n"
"R_Init: Init DOOM refresh daemon - "
"\nP_Init: Init Playloop state.\n"
"I_Init: Setting up machine state.\n"
"D_CheckNetGame: Checking network game status.\n"
"S_Init: Setting up sound.\n"
"HU_Init: Setting up heads up display.\n"
"ST_Init: Init status bar.\n"
"-statcopy"
"External statistics registered.\n"
"-record"
"-loadgame"
"c:\\doomdata\\doomsav%c.dsg"
"doomsav%c.dsg"
"ExpandTics: strange value %i at maketic %i"
"Tried to transmit to another node"
"send (%i + %i, R %i) [%i] "
"%i "
"\n"
"bad packet length %i\n"
"bad packet checksum\n"
"setup packet\n"
"get %i = (%i + %i, R %i)[%i] "
"Player 1 left the game"
"Killed by network driver"
"retransmit from %i\n"
"out of order packet (%i + %i)\n"
"missed tics from %i (%i - %i)\n"
"NetUpdate: netbuffer->numtics > BACKUPTICS"
"Network game synchronization aborted."
"listening for network start info...\n"
"Different DOOM versions cannot play a net game!"
"sending network start info...\n"
"Doomcom buffer invalid!"
"startskill %i  deathmatch: %i  startmap: %i  startepisode: %i\n"
"player %i of %i (%i nodes)\n"
"=======real: %i  avail: %i  game: %i\n"
"TryRunTics: lowtic < gametic"
"gametic>lowtic"
"%s is turbo!"
"consistency failure (%i should be %i)"
"NET GAME"
"Only %i deathmatch spots, 4 required"
"map31"
"version %i"
"Bad savegame"
"-cdrom"
"c:\\doomdata\\doomsav%d.dsg"
"doomsav%d.dsg"
"Savegame buffer overrun"
"SKY3"
"SKY1"
"SKY2"
"SKY4"
".lmp"
"-maxdemo"
"Demo is from a different game version!"
"-nodraw"
"-noblit"
"timed %i gametics in %i realtics"
"Z_CT at g_game.c:%i"
"Demo %s recorded"
"-cdrom"
"c:\\doomdata\\doomsav%d.dsg"
"doomsav%d.dsg"
"M_LOADG"
"M_LSLEFT"
"M_LSCNTR"
"M_LSRGHT"
"M_SAVEG"
"_"
"HELP2"
"HELP1"
"HELP"
"M_SVOL"
"M_DOOM"
"M_NEWG"
"M_SKILL"
"M_EPISOD"
"M_OPTTTL"
"M_THERML"
"M_THERMM"
"M_THERMR"
"M_THERMO"
"M_CELL1"
"M_CELL2"
"PLAYPAL"
"Couldn't read file %s"
"mouse_sensitivity"
"sfx_volume"
"music_volume"
"show_messages"
"key_right"
"key_left"
"key_up"
"key_down"
"key_strafeleft"
"key_straferight"
"key_fire"
"key_use"
"key_strafe"
"key_speed"
"use_mouse"
"mouseb_fire"
"mouseb_strafe"
"mouseb_forward"
"use_joystick"
"joyb_fire"
"joyb_strafe"
"joyb_use"
"joyb_speed"
"screenblocks"
"detaillevel"
"snd_channels"
"snd_musicdevice"
"snd_sfxdevice"
"snd_sbport"
"snd_sbirq"
"snd_sbdma"
"snd_mport"
"usegamma"
"chatmacro0"
"chatmacro1"
"chatmacro2"
"chatmacro3"
"chatmacro4"
"chatmacro5"
"chatmacro6"
"chatmacro7"
"chatmacro8"
"chatmacro9"
"w"
"%s\t\t%i\n"
"%s\t\t\"%s\"\n"
"-config"
"\tdefault file: %s\n"
"r"
"%79s %[^\n]\n"
"%x"
"%i"
"DOOM00.pcx"
"M_ScreenShot: Couldn't create a PCX"
"PLAYPAL"
"screen shot"
"AMMNUM%d"
"Z_CT at am_map.c:%i"
"%s %d"
"fuck %d \r"
"Weird actor->movedir!"
"P_NewChaseDir: called with no target"
"P_GiveAmmo: bad type %i"
"P_SpecialThing: Unknown gettable thing"
"PTR_SlideTraverse: not a line?"
"P_AddActivePlat: no more plats!"
"P_RemoveActivePlat: can't find plat!"
"P_GroupLines: miscounted"
"map0%i"
"map%i"
"P_CrossSubsector: ss %i with numss = %i"
"P_InitPicAnims: bad cycle from %s to %s"
"P_PlayerInSpecialSector: unknown special %i"
"texture2"
"-avg"
"-timer"
"P_StartButton: no button slots left!"
"P_SpawnMapThing: Unknown type %i at (%i, %i)"
"Unknown tclass %i in savegame"
"P_UnarchiveSpecials:Unknown tclass %i in savegame"
"R_Subsector: ss %i with numss = %i"
"Z_CT at r_data.c:%i"
"R_GenerateLookup: column without a patch (%s)\n"
"R_GenerateLookup: texture %i is >64k"
"PNAMES"
"TEXTURE1"
"TEXTURE2"
"S_START"
"S_END"
"["
" "
"         ]"
""
""
"."
"R_InitTextures: bad texture directory"
"R_InitTextures: Missing patch in texture %s"
"F_START"
"F_END"
"COLORMAP"
"R_FlatNumForName: %s not found"
"R_TextureNumForName: %s not found"
"R_DrawFuzzColumn: %i to %i at %i"
"R_DrawColumn: %i to %i at %i"
"brdr_t"
"brdr_b"
"brdr_l"
"brdr_r"
"brdr_tl"
"brdr_tr"
"brdr_bl"
"brdr_br"
"."
"F_SKY1"
"R_MapPlane: %i, %i at %i"
"R_FindPlane: no more visplanes"
"R_DrawPlanes: drawsegs overflow (%i)"
"R_DrawPlanes: visplane overflow (%i)"
"R_DrawPlanes: opening overflow (%i)"
"Z_CT at r_plane.c:%i"
"Bad R_RenderWallRange: %i to %i"
"R_InstallSpriteLump: Bad frame characters in lump %i"
"R_InitSprites: Sprite %s frame %c has multip rot=0 lump"
"R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
"R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
"R_InitSprites: Sprite %s : %c : %c has two lumps mapped to it"
"R_InitSprites: No patches found for %s frame %c"
"R_InitSprites: Sprite %s frame %c is missing rotations"
"R_DrawSpriteRange: bad texturecolumn"
"R_ProjectSprite: invalid sprite number %i "
"R_ProjectSprite: invalid sprite frame %i : %i "
"R_ProjectSprite: invalid sprite number %i "
"R_ProjectSprite: invalid sprite frame %i : %i "
"Filename base of %s >8 chars"
"\tcouldn't open %s\n"
"\tadding %s\n"
"wad"
"IWAD"
"PWAD"
"Wad file %s doesn't have IWAD or PWAD id\n"
"Couldn't realloc lumpinfo"
"W_Reload: couldn't open %s"
"W_InitFiles: no files found"
"Couldn't allocate lumpcache"
"W_GetNumForName: %s not found!"
"W_LumpLength: %i >= numlumps"
"W_ReadLump: %i >= numlumps"
"W_ReadLump: couldn't open %s"
"W_ReadLump: only read %i of %i on lump %i"
"W_CacheLumpNum: %i >= numlumps"
"Z_CT at w_wad.c:%i"
"w"
"waddump.txt"
"%s "
"    %c"
"\n"
"Bad V_CopyRect"
"Bad V_DrawPatch"
"Bad V_DrawPatchDirect"
"Bad V_DrawBlock"
"Z_Free: freed a pointer without ZONEID"
"Z_Malloc: failed on allocation of %i bytes"
"Z_Malloc: an owner is required for purgable blocks"
"zone size: %i  location: %p\n"
"tag range: %i to %i\n"
"block:%p    size:%7i    user:%p    tag:%3i\n"
"ERROR: block size does not touch the next block\n"
"ERROR: next block doesn't have proper back link\n"
"ERROR: two consecutive free blocks\n"
"block:%p    size:%7i    user:%p    tag:%3i\n"
"ERROR: block size does not touch the next block\n"
"ERROR: next block doesn't have proper back link\n"
"ERROR: two consecutive free blocks\n"
"Z_CheckHeap: block size does not touch the next block\n"
"Z_CheckHeap: next block doesn't have proper back link\n"
"Z_CheckHeap: two consecutive free blocks\n"
"Z_ChangeTag: freed a pointer without ZONEID"
"Z_ChangeTag: an owner is required for purgable blocks"
"ang=0x%x;x,y=(0x%x,0x%x)"
"STTNUM%d"
"STYSNUM%d"
"STTPRCNT"
"STKEYS%d"
"STARMS"
"STGNUM%d"
"STFB%d"
"STBAR"
"STFST%d%d"
"STFTR%d0"
"STFTL%d0"
"STFOUCH%d"
"STFEVL%d"
"STFKILL%d"
"STFGOD0"
"STFDEAD0"
"PLAYPAL"
"Z_CT at st_stuff.c:%i"
"STTMINUS"
"drawNum: n->y - ST_Y < 0"
"updateMultIcon: y - ST_Y < 0"
"updateBinIcon: y - ST_Y < 0"
"NEWLEVEL"
"STCFN%.3d"
"Could not place patch on level %d"
"INTERPIC"
"WIMAP%d"
"CWILV%2.2d"
"WILV%d%d"
"WIURH0"
"WIURH1"
"WISPLAT"
"WIA%d%.2d%.2d"
"WIMINUS"
"WINUM%d"
"WIPCNT"
"WIF"
"WIENTER"
"WIOSTK"
"WIOSTS"
"WISCRT2"
"WIOBJ"
"WIOSTI"
"WIFRGS"
"WICOLON"
"WITIME"
"WISUCKS"
"WIPAR"
"WIKILRS"
"WIVCTMS"
"WIMSTT"
"STFST01"
"STFDEAD0"
"STPB%d"
"WIBP%d"
"Z_CT at wi_stuff.c:%i"
"wi_stuff.c"
"wbs->epsd"
"%s=%d in %s:%d"
"wbs->last"
"wbs->next"
"wbs->pnum"
"Attempt to set music volume at %d"
"Z_CT at s_sound.c:%i"
"Bad music number %d"
"d_%s"
"Attempt to set sfx volume at %d"
"Bad sfx #: %d"
"Bad list in dll_AddEndNode"
"Bad list in dll_AddStartNode"
"Bad list in dll_DelNode"
"Empty list in dll_DelNode"
"Floating-point support not loaded\r\n"
"TZ"
#endif

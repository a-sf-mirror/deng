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
 * net_main.h: Network Subsystem
 */

#ifndef __DOOMSDAY_NETWORK_H__
#define __DOOMSDAY_NETWORK_H__

#include <stdio.h>

#include "lzss.h"
#include "dd_share.h"
#include "sys_network.h"
#include "net_msg.h"
#include "con_decl.h"

#define BIT(x)              (1 << (x))

#define NSP_BROADCAST       -1     // For Net_SendBuffer.

// Flags for console text from the server.
// Change with server version?
#define SV_CONSOLE_FLAGS    (CBLF_WHITE|CBLF_LIGHT|CBLF_GREEN)

#define PING_TIMEOUT        1000   // Ping timeout (ms).
#define MAX_PINGS           10

// The default bandwidth rating for new clients.
#define BWR_DEFAULT         40

// A modest acktime used by default for new clients (1 sec ping).
#define ACK_DEFAULT         1000

#define MONITORTICS         7

#define LOCALTICS           10     // Built ticcmds are stored here.
#define BACKUPTICS          70     // Two seconds worth of tics.

// The number of mobjs that can be stored in the input/visible buffer.
// The server won't send more mobjs than this.
#define MAX_CLMOBJS         80

// Packet types.
// PKT = sent by anybody
// PSV = sent by server
// PCL = sent by client
enum {
    // Messages and responses.
    PCL_HELLO = 0,
    PKT_OK = 1,
    PKT_CANCEL = 2,                 // unused?
    PKT_PLAYER_INFO = 3,
    PKT_CHAT = 4,
    PKT_TICCMD = 5,                 // unused?
    PKT_PING = 6,
    PSV_HANDSHAKE = 7,
    PSV_SERVER_CLOSE = 8,
    PSV_FRAME = 9,                  // obsolete
    PSV_PLAYER_EXIT = 10,
    PSV_CONSOLE_TEXT = 11,
    PCL_ACK_SHAKE = 12,
    PSV_SYNC = 13,
    PSV_FILTER = 14,                // unused?
    PKT_COMMAND = 15,               // obsolete
    PKT_LOGIN = 16,
    PCL_ACK_SETS = 17,
    PKT_COORDS = 18,
    PKT_DEMOCAM = 19,
    PKT_DEMOCAM_RESUME = 20,
    PCL_HELLO2 = 21,                // Includes game ID
    PSV_FRAME2 = 22,                // Frame packet v2
    PSV_FIRST_FRAME2 = 23,          // First PSV_FRAME2 after map change
    PSV_SOUND2 = 24,                // unused?
    PSV_STOP_SOUND = 25,
    PCL_ACKS = 26,
    PSV_PLAYER_FIX = 27,            // Fix angles/pos/mom.
    PCL_ACK_PLAYER_FIX = 28,        // Acknowledge player fix. /* 28 */
    PKT_COMMAND2 = 29,

    PCL_COMMANDS = DDPT_COMMANDS,   // 32; ticcmds (handled by game)

    // Game specific events.
    PKT_GAME_MARKER = DDPT_FIRST_GAME_EVENT // 64
};

// Use the number defined in dd_share.h for sound packets.
// This is for backwards compatibility.
#define PSV_SOUND           71     /* DDPT_SOUND */

#define RESENDCOUNT         10
#define HANDSHAKECOUNT      17
#define UPDATECOUNT         20

// These dd-flags are packed (i.e. included in mobj deltas).
#define DDMF_PACK_MASK      0x3cfff1ff

// A client's acknowledgement threshold depends on the average of his
// acknowledgement times.
#define NUM_ACK_TIMES       8

// The consolePlayer's camera position is written to the demo file
// every 3rd tic.
#define LOCALCAM_WRITE_TICS 3

//---------------------------------------------------------------------------
// Types
//---------------------------------------------------------------------------

typedef struct {
    // High tics when ping was sent (0 if pinger not used).
    int             sent;

    // A record of the pings (negative time: no response).
    float           times[MAX_PINGS];

    // Total number of pings and the current one.
    int             total;
    int             current;
} pinger_t;

// Network information for a player. Corresponds the players array.
typedef struct {
    // ID number. Each client has a unique ID number.
    ident_t         id;

    // Ticcmd buffer. The server uses this when clients send it ticcmds.
    byte           *ticCmds;

    // Number of tics in the buffer.
    int             numTics;

    // Index of the first tic in the buffer.
    int             firstTic;

    // Last command executed, reused if a new one isn't found.
    ticcmd_t       *lastCmd;

    // Clients merge several ticcmds into this one, which is them sent
    // periodically to the server.
    ticcmd_t       *aggregateCmd;

    int             lastTransmit;

    // If >0, the server will send the next world frame to the client.
    // This is set when input is received from the client.
    int             updateCount;

    // Gametic when the client entered the game.
    int             enterTime;

    // Client-reported time of the last processed ticcmd.
    // Older or as old tics than this are discarded.
    int             runTime;

    // Bandwidth rating for connection. Determines how much information
    // can be sent to the client. Determined dynamically.
    int             bandwidthRating;

    // During the adjust period, raising the BWR is allowed (hitting max
    // frame size).
    int             bwrAdjustTime;

    // A record of the past few acknowledgement times.
    uint            ackTimes[NUM_ACK_TIMES];
    int             ackIdx;

    // Clients use this to determine how long ago they received the
    // last update of this client.
    int             age;

    // Is this client connected? (Might not be in the game yet.) Only
    // used by the server.
    int             connected;

    // Clients are pinged by the server when they join the game.
    // This is the ping in milliseconds for this client. For the server.
    unsigned int    shakePing;

    // If true, the server will send the player a handshake. The client must
    // acknowledge it before this flag is turned off.
    int             handshake;

    // Server uses this to determine whether it's OK to send game packets
    // to the client.
    int             ready;

    // The name of the player.
    char            name[PLAYERNAMELEN];

    // Field of view. Used in determining visible mobjs (default: 90).
    float           fov;

    // The DirectPlay player that represents this client.
    unsigned int    nodeID;        // DP player ID.

    // Ping tracker for this client.
    pinger_t        ping;

    // Demo recording file (being recorded if not NULL).
    LZFILE         *demo;
    boolean         recording;
    boolean         recordPaused;

    // View console. Which player this client is viewing?
    int             viewConsole;
} client_t;

// Packets.
typedef struct {
    byte            version;
    unsigned short  playerMask;    // 16 players.
    byte            yourConsole;   // Which one's you?
    int             gameTime;
} handshake_packet_t;

typedef struct {
    byte            console;
    char            name[PLAYERNAMELEN];
} playerinfo_packet_t;

//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------
extern boolean  firstNetUpdate;
extern int      resendStart;      // set when server needs our tics
extern int      resendCount;
extern int      oldEnterTics;
extern int      numClMobjs;
extern boolean  masterAware;
extern int      netGame;
extern int      realTics, availableTics;
extern int      isServer, isClient;
extern boolean  allowNetTraffic; // Should net traffic be allowed?
extern byte     netDontSleep, netTicSync;
extern client_t clients[DDMAXPLAYERS];

//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------
void            Net_Register(void);
void            Net_Init(void);
void            Net_Shutdown(void);
void            Net_AllocArrays(void);
void            Net_DestroyArrays(void);
void            Net_SendPacket(int to_player, int type, void *data,
                               size_t length);
boolean         Net_GetPacket(void);
void            Net_SendBuffer(int to_player, int sp_flags);
void            Net_InitGame(void);
void            Net_StartGame(void);
void            Net_StopGame(void);
void            Net_BuildLocalCommands(timespan_t time);
void            Net_SendCommands(void);
void            Net_SendPing(int player, int count);
void            Net_PingResponse(void);
void            Net_ShowPingSummary(int player);
void            Net_ShowChatMessage(void);
int             Net_TimeDelta(byte now, byte then);
int             Net_GetTicCmd(void *cmd, int player);
void            Net_Update(void);
void            Net_Ticker(void/*timespan_t time*/);
void            Net_Drawer(void);

boolean         Net_IsLocalPlayer(int pNum);
void            Net_SetInitialAckTime(int clientNumber, uint period);
void            Net_SetAckTime(int clientNumber, uint period);
uint            Net_GetAckTime(int clientNumber);
uint            Net_GetAckThreshold(int clientNumber);

const char*     Net_GetPlayerName(int player);
ident_t         Net_GetPlayerID(int player);

void            Net_PrintServerInfo(int index, serverinfo_t *info);

#endif

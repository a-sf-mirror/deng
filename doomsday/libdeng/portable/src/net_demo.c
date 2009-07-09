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
 *
 * \bug - Demo playback is broken -> http://sourceforge.net/tracker/index.php?func=detail&aid=1693198&group_id=74815&atid=542099
 */

/**
 * net_demo.c: Demos
 *
 * Handling of demo recording and playback.
 * Opening of, writing to, reading from and closing of demo files.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_misc.h"
#include "de_play.h"

#include "r_main.h"
#include "gl_main.h" // For r_framecounter.

// MACROS ------------------------------------------------------------------

#define DEMOTIC SECONDS_TO_TICKS(demoTime)

// Local Camera flags.
#define LCAMF_ONGROUND      0x1
#define LCAMF_FOV           0x2 // FOV has changed (short).
#define LCAMF_CAMERA        0x4 // Camera mode.

// TYPES -------------------------------------------------------------------

#pragma pack(1)

typedef struct {
    //ushort        angle;
    //short         lookdir;
    ushort          length;
} demopacket_header_t;

#pragma pack()

typedef struct {
    boolean         first;
    int             begintime;
    boolean         canwrite;           // False until Handshake packet.
    int             cameratimer;
    int             pausetime;
    float           fov;
} demotimer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(DemoLump);
D_CMD(PauseDemo);
D_CMD(PlayDemo);
D_CMD(RecordDemo);
D_CMD(StopDemo);

void Demo_WriteLocalCamera(int plnum);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float netConnectTime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char demoPath[128] = "demo\\";

//int demotic = 0;
LZFILE *playdemo = 0;
int playback = false;
int viewangleDelta = 0;
float lookdirDelta = 0;
float posDelta[3];
float demoFrameZ, demoZ;
boolean demoOnGround;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static demotimer_t writeInfo[DDMAXPLAYERS];
static demotimer_t readInfo;
static float startFOV;
static int demoStartTic;

// CODE --------------------------------------------------------------------

void Demo_Register(void)
{
    // Ccmds
    C_CMD("demolump", "ss", DemoLump);
    C_CMD("pausedemo", NULL, PauseDemo);
    C_CMD("playdemo", "s", PlayDemo);
    C_CMD("recorddemo", NULL, RecordDemo);
    C_CMD("stopdemo", NULL, StopDemo);
}

void Demo_Init(void)
{
    // Make sure the demo path is there.
    M_CheckPath(demoPath);
}

/**
 * Open a demo file and begin recording.
 * Returns false if the recording can't be begun.
 */
boolean Demo_BeginRecording(const char* fileName, int plrNum)
{
#if 0
    filename_t          buf;
    client_t*           cl = &clients[plrNum];
    player_t*           plr = &ddPlayers[plrNum];

    // Is a demo already being recorded for this client?
    if(cl->recording || playback || (isDedicated && !plrNum) ||
       !plr->shared.inGame)
        return false;

    // Compose the real file name.
    strncpy(buf, demoPath, FILENAME_T_MAXLEN);
    strncat(buf, fileName, FILENAME_T_MAXLEN);
    M_TranslatePath(buf, buf, FILENAME_T_MAXLEN);

    // Open the demo file.
    cl->demo = lzOpen(buf, "wp");
    if(!cl->demo)
        return false; // Couldn't open it!

    cl->recording = true;
    cl->recordPaused = false;
    writeInfo[plrNum].first = true;
    writeInfo[plrNum].canwrite = false;
    writeInfo[plrNum].cameratimer = 0;
    writeInfo[plrNum].fov = -1; // Must be written in the first packet.

    if(isServer)
    {
        // Playing demos alters gametic. This'll make sure we're going to
        // get updates.
        clients[0].lastTransmit = -1;
        // Servers need to send a handshake packet.
        // It only needs to recorded in the demo file, though.
        allowSending = false;
        Sv_Handshake(plrNum, false);
        // Enable sending to network.
        allowSending = true;
    }
    else
    {
        // Clients need a Handshake packet.
        // Request a new one from the server.
        Cl_SendHello();
    }
#endif

    // The operation is a success.
    return true;
}

void Demo_PauseRecording(int playerNum)
{
#if 0
    client_t       *cl = clients + playerNum;

    // A demo is not being recorded?
    if(!cl->recording || cl->recordPaused)
        return;
    // All packets will be written for the same tic.
    writeInfo[playerNum].pausetime = SECONDS_TO_TICKS(demoTime);
    cl->recordPaused = true;
#endif
}

/**
 * Resumes a paused recording.
 */
void Demo_ResumeRecording(int playerNum)
{
#if 0
    client_t       *cl = clients + playerNum;

    // Not recording or not paused?
    if(!cl->recording || !cl->recordPaused)
        return;
    Demo_WriteLocalCamera(playerNum);
    cl->recordPaused = false;
    // When the demo is read there can't be a jump in the timings, so we
    // have to make it appear the pause never happened; begintime is
    // moved forwards.
    writeInfo[playerNum].begintime += DEMOTIC - writeInfo[playerNum].pausetime;
#endif
}

/**
 * Stop recording a demo.
 */
void Demo_StopRecording(int playerNum)
{
#if 0
    client_t       *cl = clients + playerNum;

    // A demo is not being recorded?
    if(!cl->recording)
        return;

    // Close demo file.
    lzClose(cl->demo);
    cl->demo = 0;
    cl->recording = false;
#endif
}

void Demo_WritePacket(int playerNum)
{
#if 0
    LZFILE         *file;
    demopacket_header_t hdr;
    demotimer_t    *inf = writeInfo + playerNum;
    byte            ptime;

    if(playerNum == NSP_BROADCAST)
    {
        Demo_BroadcastPacket();
        return;
    }

    // Is this client recording?
    if(!clients[playerNum].recording)
        return;

    if(!inf->canwrite)
    {
        if(netBuffer.msg.type != PSV_HANDSHAKE)
            return;
        // The handshake has arrived. Now we can begin writing.
        inf->canwrite = true;
    }

    if(clients[playerNum].recordPaused)
    {
        // Some types of packet are not written in record-paused mode.
        if(netBuffer.msg.type == PSV_SOUND ||
           netBuffer.msg.type == DDPT_MESSAGE)
            return;
    }

    // This counts as an update. (We know the client is alive.)
    clients[playerNum].updateCount = UPDATECOUNT;

    file = clients[playerNum].demo;

#if _DEBUG
    if(!file)
        Con_Error("Demo_WritePacket: No demo file!\n");
#endif

    if(!inf->first)
    {
        ptime =
            (clients[playerNum].recordPaused ? inf->pausetime : DEMOTIC) -
            inf->begintime;
    }
    else
    {
        ptime = 0;
        inf->first = false;
        inf->begintime = DEMOTIC;
    }
    lzWrite(&ptime, 1, file);

    // The header.

#if _DEBUG
    if(netBuffer.length >= sizeof(hdr.length))
        Con_Error("Demo_WritePacket: Write buffer too large!\n");
#endif

    hdr.length = (ushort) 1 + netBuffer.length;
    lzWrite(&hdr, sizeof(hdr), file);

    // Write the packet itself.
    lzPutC(netBuffer.msg.type, file);
    lzWrite(netBuffer.msg.data, (long) netBuffer.length, file);
#endif
}

void Demo_BroadcastPacket(void)
{
#if 0
    int                 i;

    // Write packet to all recording demo files.
    for(i = 0; i < DDMAXPLAYERS; ++i)
        Demo_WritePacket(i);
#endif
}

boolean Demo_BeginPlayback(const char* fileName)
{
#if 0
    filename_t          buf;
    int                 i;

    if(playback)
        return false; // Already in playback.
    if(netGame || isClient)
        return false; // Can't do it.

    // Check that we aren't recording anything.
    for(i = 0; i < DDMAXPLAYERS; ++i)
        if(clients[i].recording)
            return false;

    // Open the demo file.
    snprintf(buf, FILENAME_T_MAXLEN, "%s%s",
             Dir_IsAbsolute(fileName) ? "" : demoPath, fileName);
    M_TranslatePath(buf, buf, FILENAME_T_MAXLEN);
    playdemo = lzOpen(buf, "rp");
    if(!playdemo)
        return false; // Failed to open the file.

    // OK, let's begin the demo.
    playback = true;
    isServer = false;
    isClient = true;
    readInfo.first = true;
    viewangleDelta = 0;
    lookdirDelta = 0;
    demoFrameZ = 1;
    demoZ = 0;
    startFOV = fieldOfView;
    demoStartTic = DEMOTIC;
    memset(posDelta, 0, sizeof(posDelta));
    // Start counting frames from here.
    if(ArgCheck("-timedemo"))
        r_framecounter = 0;

#endif
    return true;
}

void Demo_StopPlayback(void)
{
#if 0
    float           diff;

    if(!playback)
        return;

    Con_Message("Demo was %.2f seconds (%i tics) long.\n",
                (DEMOTIC - demoStartTic) / (float) TICSPERSEC,
                DEMOTIC - demoStartTic);

    playback = false;
    lzClose(playdemo);
    playdemo = 0;
    fieldOfView = startFOV;
    Net_StopGame();

    if(ArgCheck("-timedemo"))
    {
        diff = Sys_GetSeconds() - netConnectTime;
        if(!diff)
            diff = 1;
        // Print summary and exit.
        Con_Message("Timedemo results: ");
        Con_Message("%i game tics in %.1f seconds\n", r_framecounter, diff);
        Con_Message("%f FPS\n", r_framecounter / diff);
        Sys_Quit();
    }

    // "Play demo once" mode?
    if(ArgCheck("-playdemo"))
        Sys_Quit();
#endif
}

boolean Demo_ReadPacket(void)
{
#if 0
    static byte     ptime;
    int             nowtime = DEMOTIC;
    demopacket_header_t hdr;

    if(!playback)
        return false;

    if(lzEOF(playdemo))
    {
        Demo_StopPlayback();
        // Tell the Game the demo has ended.
        if(gx.NetWorldEvent)
            gx.NetWorldEvent(DDWE_DEMO_END, 0, 0);
        return false;
    }

    if(readInfo.first)
    {
        readInfo.first = false;
        readInfo.begintime = nowtime;
        ptime = lzGetC(playdemo);
    }

    // Check if the packet can be read.
    if(Net_TimeDelta(nowtime - readInfo.begintime, ptime) < 0)
        return false; // Can't read yet.

    // Read the packet.
    lzRead(&hdr, sizeof(hdr), playdemo);

    // Get the packet.
    netBuffer.length = hdr.length - 1;
    netBuffer.player = 0; // From the server.
    netBuffer.msg.id = 0;
    netBuffer.msg.type = lzGetC(playdemo);
    lzRead(netBuffer.msg.data, (long) netBuffer.length, playdemo);
    netBuffer.cursor = netBuffer.msg.data;

    // Read the next packet time.
    ptime = lzGetC(playdemo);

#endif
    return true;
}

/**
 * Writes a view angle and coords packet. Doesn't send the packet outside.
 */
void Demo_WriteLocalCamera(int plrNum)
{
#if 0
    player_t           *plr = &ddPlayers[plrNum];
    ddplayer_t         *ddpl = &plr->shared;
    mobj_t             *mo = ddpl->mo;
    fixed_t             x, y, z;
    byte                flags;
    boolean             incfov = (writeInfo[plrNum].fov != fieldOfView);

    if(!mo)
        return;

    Msg_Begin(clients[plrNum].recordPaused ? PKT_DEMOCAM_RESUME : PKT_DEMOCAM);
    // Flags.
    flags = (mo->pos[VZ] <= mo->floorZ ? LCAMF_ONGROUND : 0)  // On ground?
        | (incfov ? LCAMF_FOV : 0);
    if(ddpl->flags & DDPF_CAMERA)
    {
        flags &= ~LCAMF_ONGROUND;
        flags |= LCAMF_CAMERA;
    }
    Msg_WriteByte(flags);

    // Coordinates.
    x = FLT2FIX(mo->pos[VX]);
    y = FLT2FIX(mo->pos[VY]);
    Msg_WriteShort(x >> 16);
    Msg_WriteByte(x >> 8);
    Msg_WriteShort(y >> 16);
    Msg_WriteByte(y >> 8);

    //z = mo->pos[VZ] + ddpl->viewheight;
    z = FLT2FIX(ddpl->viewZ);
    Msg_WriteShort(z >> 16);
    Msg_WriteByte(z >> 8);

    Msg_WriteShort(mo->angle /*ddpl->clAngle*/ >> 16); /* $unifiedangles */
    Msg_WriteShort(ddpl->lookDir / 110 * DDMAXSHORT /* $unifiedangles */);
    // Field of view is optional.
    if(incfov)
    {
        Msg_WriteShort(fieldOfView / 180 * DDMAXSHORT);
        writeInfo[plrNum].fov = fieldOfView;
    }
    Net_SendBuffer(plrNum, SPF_DONT_SEND);
#endif
}

/**
 * Read a view angle and coords packet. NOTE: The Z coordinate of the
 * camera is the real eye Z coordinate, not the player mobj's Z coord.
 */
void Demo_ReadLocalCamera(void)
{
#if 0
    ddplayer_t         *pl = &ddPlayers[consolePlayer].shared;
    mobj_t             *mo = pl->mo;
    int                 flags;
    float               z;
    int                 intertics = LOCALCAM_WRITE_TICS;
    int                 dang;
    float               dlook;

    if(!mo)
        return;

    if(netBuffer.msg.type == PKT_DEMOCAM_RESUME)
    {
        intertics = 1;
    }

    // Framez keeps track of the current camera Z.
    demoFrameZ += demoZ;

    flags = Msg_ReadByte();
    demoOnGround = (flags & LCAMF_ONGROUND) != 0;
    if(flags & LCAMF_CAMERA)
        pl->flags |= DDPF_CAMERA;
    else
        pl->flags &= ~DDPF_CAMERA;

    // X and Y coordinates are easy. Calculate deltas to the new coords.
    posDelta[VX] =
        (FIX2FLT((Msg_ReadShort() << 16) + (Msg_ReadByte() << 8)) - mo->pos[VX]) / intertics;
    posDelta[VY] =
        (FIX2FLT((Msg_ReadShort() << 16) + (Msg_ReadByte() << 8)) - mo->pos[VY]) / intertics;

    // The Z coordinate is a bit trickier. We are tracking the *camera's*
    // Z coordinate (z+viewheight), not the player mobj's Z.
    z = FIX2FLT((Msg_ReadShort() << 16) + (Msg_ReadByte() << 8));
    posDelta[VZ] = (z - demoFrameZ) / LOCALCAM_WRITE_TICS;

    // View angles.
    dang = Msg_ReadShort() << 16;
    dlook = Msg_ReadShort() * 110.0f / DDMAXSHORT;

    // FOV included?
    if(flags & LCAMF_FOV)
        fieldOfView = Msg_ReadShort() * 180.0f / DDMAXSHORT;

    if(intertics == 1 || demoFrameZ == 1)
    {
        // Immediate change.
        /*pl->clAngle = dang;
        pl->clLookDir = dlook;*/
        pl->mo->angle = dang;
        pl->lookDir = dlook;
        /* $unifiedangles */
        viewangleDelta = 0;
        lookdirDelta = 0;
    }
    else
    {
        viewangleDelta = (dang - pl->mo->angle) / intertics;
        lookdirDelta = (dlook - pl->lookDir) / intertics;
        /* $unifiedangles */
    }

    // The first one gets no delta.
    if(demoFrameZ == 1)
    {
        // This must be the first democam packet.
        // Initialize framez to the height we just read.
        demoFrameZ = z;
        posDelta[VZ] = 0;
    }
    // demo_z is the offset to demo_framez for the current tic.
    // It is incremented by pos_delta[VZ] every tic.
    demoZ = 0;

    if(intertics == 1)
    {
        // Instantaneous move.
        R_ResetViewer();
        demoFrameZ = z;
        Cl_MoveLocalPlayer(posDelta[VX], posDelta[VY], z, demoOnGround);
        pl->viewZ = z; // Might get an unsynced frame is not set right now.
        posDelta[VX] = posDelta[VY] = posDelta[VZ] = 0;
    }
#endif
}

/**
 * Called once per tic.
 */
void Demo_Ticker(timespan_t time)
{
#if 0
    static trigger_t        fixed = { 1 / 35.0, 0 };

    if(!M_RunTrigger(&fixed, time))
        return;

    // Only playback is handled.
    if(playback)
    {
        player_t               *plr = &ddPlayers[consolePlayer];
        ddplayer_t             *ddpl = &plr->shared;

        ddpl->mo->angle += viewangleDelta;
        ddpl->lookDir += lookdirDelta;
        /* $unifiedangles */
        // Move player (i.e. camera).
        Cl_MoveLocalPlayer(posDelta[VX], posDelta[VY], demoFrameZ + demoZ,
                           demoOnGround);
        // Interpolate camera Z offset (to framez).
        demoZ += posDelta[VZ];
    }
    else
    {
        int                     i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t               *plr = &ddPlayers[i];
            ddplayer_t             *ddpl = &plr->shared;
            client_t               *cl = &clients[i];

            if(ddpl->inGame && cl->recording && !cl->recordPaused &&
               ++writeInfo[i].cameratimer >= LOCALCAM_WRITE_TICS)
            {
                // It's time to write local view angles and coords.
                writeInfo[i].cameratimer = 0;
                Demo_WriteLocalCamera(i);
            }
        }
    }
#endif
}

D_CMD(PlayDemo)
{
    Con_Printf("Playing demo \"%s\"...\n", argv[1]);
    return Demo_BeginPlayback(argv[1]);
}

D_CMD(RecordDemo)
{
#if 0
    int                 plnum = consolePlayer;

    if(argc == 3 && isClient)
    {
        Con_Printf("Clients can only record the consolePlayer.\n");
        return true;
    }

    if(isClient && argc != 2)
    {
        Con_Printf("Usage: %s (fileName)\n", argv[0]);
        return true;
    }

    if(isServer && (argc < 2 || argc > 3))
    {
        Con_Printf("Usage: %s (fileName) (plnum)\n", argv[0]);
        Con_Printf("(plnum) is the player which will be recorded.\n");
        return true;
    }

    if(argc == 3)
        plnum = atoi(argv[2]);

    Con_Printf("Recording demo of player %i to \"%s\".\n", plnum, argv[1]);

    return Demo_BeginRecording(argv[1], plnum);
#endif
    return false;
}

D_CMD(PauseDemo)
{
#if 0
    int         plnum = consolePlayer;

    if(argc >= 2)
        plnum = atoi(argv[1]);

    if(!clients[plnum].recording)
    {
        Con_Printf("Not recording for player %i.\n", plnum);
        return false;
    }
    if(clients[plnum].recordPaused)
    {
        Demo_ResumeRecording(plnum);
        Con_Printf("Demo recording of player %i resumed.\n", plnum);
    }
    else
    {
        Demo_PauseRecording(plnum);
        Con_Printf("Demo recording of player %i paused.\n", plnum);
    }
#endif
    return true;
}

D_CMD(StopDemo)
{
#if 0
    int         plnum = consolePlayer;

    if(argc > 2)
    {
        Con_Printf("Usage: stopdemo (plrnum)\n");
        return true;
    }
    if(argc == 2)
        plnum = atoi(argv[1]);

    if(!playback && !clients[plnum].recording)
        return true;

    Con_Printf("Demo %s of player %i stopped.\n",
               clients[plnum].recording ? "recording" : "playback", plnum);

    if(playback)
    {
        Demo_StopPlayback();
        // Tell the Game DLL that the playback was aborted.
        gx.NetWorldEvent(DDWE_DEMO_END, true, 0);
    }
    else
        Demo_StopRecording(plnum);
#endif
    return true;
}

/**
 * Make a demo lump.
 */
D_CMD(DemoLump)
{
    char        buf[64];

    memset(buf, 0, sizeof(buf));
    strncpy(buf, argv[1], 64);
    return M_WriteFile(argv[2], buf, 64);
}

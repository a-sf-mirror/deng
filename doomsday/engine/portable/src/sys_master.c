/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/*
 * sys_master.c: Communication with the Master Server
 *
 * Communication with the master server, using TCP and HTTP.
 * The HTTP requests run in their own threads.
 * Sockets were initialized by sys_network.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#ifdef WIN32
#   include <winsock.h>
#endif

#ifdef UNIX
#   include <sys/types.h>
#   include <sys/socket.h>
#endif

#include "de_base.h"
#include "de_network.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"

#include "r_world.h"

// MACROS ------------------------------------------------------------------

// Communication with the master is done at 'below normal' priority.
#define MST_PRIORITY    -1

// Maximum time from the first received line of the response (seconds).
#define RESPONSE_TIMEOUT    2

// TYPES -------------------------------------------------------------------

typedef struct serverlist_s {
    struct serverlist_s *next;
    serverinfo_t info;
} serverlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void N_MasterClearList(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Master server info. Hardcoded defaults.
char   *masterAddress = "www.dengine.net";
int     masterPort = 0;         // Uses 80 by default.
char   *masterPath = "/master.php";
boolean masterAware = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *responseOK = "HTTP/1.1 200";

// This variable will be true while a communication is in progress.
static volatile boolean communicating;

// A linked list of servers created from info retrieved from the master.
static serverlist_t *servers;
static int numServers;

// CODE --------------------------------------------------------------------

/**
 * Called by N_Init() while initializing the low-level network subsystem.
 */
void N_MasterInit(void)
{
    communicating = false;
}

/**
 * Called by N_Shutdown() during engine shutdown.
 */
void N_MasterShutdown(void)
{
    // Free the server list. (What if communicating?)
    N_MasterClearList();
}

/**
 * Clears our copy of the server list returned by the master.
 */
static void N_MasterClearList(void)
{
    numServers = 0;
    while(servers)
    {
        serverlist_t *next = servers->next;

        M_Free(servers);
        servers = next;
    }
}

/**
 * Creates a new server and links it into our copy of the server list.
 *
 * @return                  Ptr to the newly created server.
 */
static serverlist_t *N_MasterNewServer(void)
{
    serverlist_t *node;

    numServers++;
    node = M_Calloc(sizeof(serverlist_t));
    node->next = servers;
    servers = node;
    return node;
}

/**
 * NOTE: The info must be allocated from the heap. We will free it when it's
 * no longer needed.
 *
 * @param parm              The announcement info to be sent.
 *
 * @return                  <code>true</code> if the announcement was sent
 *                          successfully and the master responds "OK".
 */
static int C_DECL N_MasterSendAnnouncement(void *parm)
{
    serverinfo_t *info = parm;
    socket_t    s;
    struct hostent *host;
    ddstring_t  msg;
    char        buf[256];
    unsigned int length;

    // Get host information.
    if((host = N_SockGetHost(masterAddress)) == NULL)
    {
        // Could not find the host.
        return false;
    }

    // Create a socket.
    if((s = N_SockNewStream()) == INVALID_SOCKET)
    {
        // Couldn't create a new socket.
        return false;
    }

    // Connect.
    if(!N_SockConnect(s, host, masterPort ? masterPort : 80))
    {
        // Connection failed.
        return false;
    }

    // Convert the serverinfo into plain text.
    Str_Init(&msg);
    length = Sv_InfoToString(info, &msg);

    // Free the serverinfo, it's no longer needed.
    M_Free(info);

    // Write an HTTP POST request with our info.
    N_SockPrintf(s, "POST %s HTTP/1.1\n", masterPath);
    N_SockPrintf(s, "Host: %s\n", masterAddress);
    N_SockPrintf(s, "Connection: close\n");
    N_SockPrintf(s, "Content-Type: application/x-deng-announce\n");
    N_SockPrintf(s, "Content-Length: %i\n\n", length);
    send(s, Str_Text(&msg), length, 0);
    Str_Free(&msg);

    // Wait for a response.
    length = recv(s, buf, sizeof(buf), 0);
    /*
       memset(buf, 0, sizeof(buf));
       while(recv(s, buf, sizeof(buf) - 1, 0) > 0)
       {
       Con_Printf("%s", buf);
       memset(buf, 0, sizeof(buf));
       }
     */
    N_SockClose(s);

    // The communication ends.
    communicating = false;

    // If the master responds "OK" return true, otherwise false.
    return length >= strlen(responseOK) &&
        !strncmp(buf, responseOK, strlen(responseOK));
}

/**
 * Decodes a master server response into a plain format for futher parsing.
 *
 * @param response          Is an HTTP response with the chunked transfer
 *                          encoding.
 * @param out               The ouput buffer where the decoded body of the
 *                          response will be written to.
 */
static void N_MasterDecodeChunked(ddstring_t *response, ddstring_t *out)
{
    ddstring_t  line;
    const char *pos = Str_Text(response);
    boolean     foundChunked = true;
    boolean     done;
    int         length;

    // Skip to the body.
    Str_Init(&line);
    if(*pos)
    {
        done = false;
        while(!done)
        {
            pos = Str_GetLine(&line, pos);

            // Let's make sure the encoding is chunked.
            // RFC 2068 says that HTTP/1.1 clients must ignore encodings
            // they don't understand.
            if(!stricmp(Str_Text(&line), "Transfer-Encoding: chunked"))
                foundChunked = true;

            // The first break indicates the end of the headers.
            if(!Str_Length(&line))
                done = true;

            if(!*pos)
                done = true;
        }
    }

    if(foundChunked && *pos)
    {
        // Decode the body.
        done = false;
        while(!done)
        {
            // The first line of the chunk is the length.
            pos = Str_GetLine(&line, pos);
            length = strtol(Str_Text(&line), 0, 16);
            if(length)
            {
                // Append the chunk data to the output.
                Str_PartAppend(out, pos, 0, length);
                pos += length;

                // A newline ends the chunk.
                pos = (const char *) M_SkipLine((char *) pos);
                if(!*pos)
                    done = true;
            }
            else // No more chunks.
            {
                done = true;
            }
        }
    }
    Str_Free(&line);
}

/**
 * Attempts to parses a list of servers from the given text string.
 *
 * @param response          The string to be parsed.
 *
 * @return                  <code>true</code> if successful.
 */
static int N_MasterParseResponse(ddstring_t *response)
{
    ddstring_t  msg, line;
    const char *pos;
    serverinfo_t *info = NULL;

    Str_Init(&msg);
    Str_Init(&line);

    // Clear the list of servers.
    N_MasterClearList();

    // The response must be OK.
    if(strncmp(Str_Text(response), responseOK, strlen(responseOK)))
    {
        // This is not valid.
        return false;
    }

    // Extract the body of the response.
    N_MasterDecodeChunked(response, &msg);

    // The syntax of the response is simple:
    // label:value
    // One or more empty lines separate servers.
    pos = Str_Text(&msg);
    if(*pos)
    {
        boolean isDone = false;
        while(!isDone)
        {
            pos = Str_GetLine(&line, pos);

            if(Str_Length(&line) && !info)
            {
                // A new server begins.
                info = &N_MasterNewServer()->info;
            }
            else if(!Str_Length(&line) && info)
            {
                // No more current server.
                info = NULL;
            }

            if(info)
                Sv_StringToInfo(Str_Text(&line), info);

            if(!*pos)
                isDone = true;
        }
    }

    Str_Free(&line);
    Str_Free(&msg);
    return true;
}

/**
 * @param parm              Not used.
 *
 * @return                  <code>true</code> if the request was sent
 *                          successfully.
 */
static int C_DECL N_MasterSendRequest(void *parm)
{
    struct hostent *host;
    socket_t    s;
    ddstring_t  response;
    char        buf[128];
    int         result;
    double      startTime = 0;

    // Get host information.
    if((host = N_SockGetHost(masterAddress)) == NULL)
    {
        // Could not find the host.
        return false;
    }

    // Create a TCP stream socket.
    if((s = N_SockNewStream()) == INVALID_SOCKET)
    {
        // Couldn't create a new socket.
        return false;
    }

    // Connect.
    if(!N_SockConnect(s, host, masterPort ? masterPort : 80))
    {
        // Connection failed.
        return false;
    }

    // Write a HTTP GET request.
    N_SockPrintf(s, "GET %s?list HTTP/1.1\r\n", masterPath);
    N_SockPrintf(s, "Host: %s\r\n", masterAddress);
    // We want the connection to close as soon as everything has been
    // received.
    N_SockPrintf(s, "Connection: close\r\n\r\n\r\n");

    // Receive the entire list.
    Str_Init(&response);
    memset(buf, 0, sizeof(buf));
    while((result = recv(s, buf, sizeof(buf) - 1, 0)) >= 0)
    {
        if(!result && Str_Length(&response) > 0 && 
           Sys_GetSeconds() - startTime > RESPONSE_TIMEOUT)
        {        
#ifdef _DEBUG
            fprintf(outFile, "timed out!\n", startTime);
#endif
            break;
        }
        
        if(!Str_Length(&response))
        {
            startTime = Sys_GetSeconds();
#ifdef _DEBUG
            fprintf(outFile, "startTime = %lf\n", startTime);
#endif
        }
        if(result)
        {
#ifdef _DEBUG
            fprintf(outFile, "received: >>>%s<<<\n", buf);
#endif
            Str_Append(&response, buf);
            memset(buf, 0, sizeof(buf));
        }
    }
    N_SockClose(s);
    
    // Let's parse the message.
    N_MasterParseResponse(&response);

    // We're done with the parsing.
    Str_Free(&response);
    communicating = false;
    return true;
}

/**
 * Sends a server announcement to the master. The announcement includes our
 * IP address and other information.
 *
 * @param isOpen            If <code>true</code> then the server will be
 *                          visible on the server list for other clients to
 *                          find by querying the server list.
 */
void N_MasterAnnounceServer(boolean isOpen)
{
    serverinfo_t *info;

    if(isClient)
        return;                 // Must be a server.

    // Are we already communicating with the master at the moment?
    if(communicating)
    {
        if(verbose)
            Con_Printf("N_MasterAnnounceServer: Request already in "
                       "progress.\n");
        return;
    }

    // The communication begins.
    communicating = true;

    // The info is not freed in this function, but in
    // N_MasterSendAnnouncement().
    info = M_Calloc(sizeof(serverinfo_t));

    // Let's figure out what we want to tell about ourselves.
    Sv_GetInfo(info);

    if(!isOpen)
        info->canJoin = false;

    Sys_StartThread(N_MasterSendAnnouncement, info);
}

/**
 * Requests the list of open servers from the master.
 */
void N_MasterRequestList(void)
{
    // Are we already communicating with the master at the moment?
    if(communicating)
    {
        if(verbose)
            Con_Printf("N_MasterRequestList: Request already "
                       "in progress.\n");
        return;
    }

    // The communication begins. Will be cleared when the list is ready
    // for use.
    communicating = true;

    // Start a new thread for the request.
    Sys_StartThread(N_MasterSendRequest, NULL);
}

/**
 * Returns information about the server #N.
 *
 * @return                  <code>0</code> if communication with the master
 *                          is currently in progress.
 *                          If param info is <code>NULL</code>, will return
 *                          the number of known servers ELSE, will return
 *                          <code>not zero</code> if param index was valid
 *                          and the master returned info on the requested
 *                          server.
 */
int N_MasterGet(int index, serverinfo_t *info)
{
    serverlist_t *it;

    if(communicating)
        return -1;
    if(!info)
        return numServers;

    // Find the index'th entry.
    for(it = servers; index > 0 && it; index--, it = it->next);

    // Was the index valid?
    if(!it)
    {
        memset(info, 0, sizeof(*info));
        return false;
    }

    // Make a copy of the info.
    memcpy(info, &it->info, sizeof(*info));
    return true;
}

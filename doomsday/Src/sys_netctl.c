/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_netctl.c: Network Control Connections
 */

// HEADER FILES ------------------------------------------------------------

#include <SDL_net.h>

#include "de_base.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_TCP_PORT	13209
#define MAX_CTL_SOCKS		64
#define CTL_BUFFER_SIZE		4096

// TYPES -------------------------------------------------------------------

typedef struct ctlsocket_s {
	struct ctlsocket_s *next, *prev;
	TCPsocket sock;
	char buffer[CTL_BUFFER_SIZE];
	int head, tail;
	int node;					// -1 if not connected to a node
} ctlsocket_t;

typedef enum clientstate_e {
	CST_IDLE = 0,
	CST_QUIT,
	CST_INFO,
	CST_INFO_AND_QUIT
} clientstate_t;

typedef struct foundhost_s {
	boolean valid;
	serverinfo_t info;
	IPaddress addr;
} foundhost_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void N_IPToString(char *buf, IPaddress *ip);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void N_DeleteAllControlSockets(void);
ctlsocket_t *N_GetClientControlSocket(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int	defaultTCPPort = DEFAULT_TCP_PORT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// This socket accepts new connections from clients.
static TCPsocket serverSock;

static SDLNet_SocketSet sockSet;

// Control connections.
static ctlsocket_t *firstCts, *lastCts;
static int numCts;

// The default client state.
static clientstate_t clientState = CST_IDLE;

static foundhost_t located;

// CODE --------------------------------------------------------------------

/*
 * Initialize the network control service.  This is what enabled
 * clients to connect to a server.
 */
boolean N_InitControl(boolean inServerMode, ushort userPort)
{
	IPaddress ip;
	Uint16 port;

	// Let's forget about servers found earlier.
	located.valid = false;
	
	if(inServerMode)
	{
		// Which TCP port to use for listening?
		port = userPort;
		if(!port) port = defaultTCPPort;
		
		VERBOSE( Con_Message("N_InitService: Listening TCP socket on "
							 "port %i.\n", port) );
		
		// Open a listening TCP socket. It will accept client
		// connections.
		if(SDLNet_ResolveHost(&ip, NULL, port))
		{
			Con_Message("N_InitService: %s\n", SDLNet_GetError());
			return false;
		}
		if(!(serverSock = SDLNet_TCP_Open(&ip)))
		{
			Con_Message("N_InitService: %s\n", SDLNet_GetError());
			return false;
		}
	}
	else
	{
		serverSock = NULL;
	}

	// Initialize the control sockets.
	firstCts = lastCts = NULL;
	numCts = 0;
	sockSet = SDLNet_AllocSocketSet(MAX_CTL_SOCKS);
	return true;
}

/*
 * Shut down the network control service.
 */
void N_ShutdownControl(void)
{
	if(serverSock)
	{
		// Close the listening socket.
		SDLNet_TCP_Close(serverSock);
		serverSock = NULL;
	}

	N_DeleteAllControlSockets();
	SDLNet_FreeSocketSet(sockSet);
	sockSet = NULL;

	// Let's forget about servers found earlier.
	located.valid = false;
}

/*
 * Add a new control connection to the list of connections.  Returns
 * NULL if no more control connections are allowed.
 */
ctlsocket_t *N_NewControlSocket(TCPsocket sock)
{
	ctlsocket_t *c;
	char buf[80];
	
	if(numCts >= MAX_CTL_SOCKS)
	{
		// No more is allowed.  The caller should close the socket.
		return NULL;
	}

	c = calloc(sizeof(ctlsocket_t), 1);
	c->sock = sock;

	// No node associated with this one yet.
	c->node = -1;

	// Link it to the list.
	if(!firstCts)
	{
		firstCts = lastCts = c;
	}
	else
	{
		c->prev = lastCts;
		lastCts->next = c;
		lastCts = c;
	}

	// One more connection.
	numCts++;

	// Also add it to the socket set, which contains all the sockets
	// we listen to.
	SDLNet_TCP_AddSocket(sockSet, sock);

	N_IPToString(buf, SDLNet_TCP_GetPeerAddress(sock));
	VERBOSE( Con_Message("N_NewControlSocket: IP %s.\n", buf) );
	
	return c;
}

/*
 * Close and delete the control connection.
 */
void N_DeleteControlSocket(ctlsocket_t *c)
{
	char buf[80];
	
	if(!numCts)
	{
		Con_Error("N_DeleteControlSocket: No connections to delete.\n");
	}

	N_IPToString(buf, SDLNet_TCP_GetPeerAddress(c->sock));
	VERBOSE( Con_Message("N_DeleteControlSocket: IP %s.\n", buf) );

	// Reset client state.
	if(isClient && N_GetClientControlSocket() == c)
	{
		clientState = CST_IDLE;
	}
	// The associated node should be closed, too.
	if(c->node >= 0) N_TerminateNode(c->node);
	
	SDLNet_TCP_DelSocket(sockSet, c->sock);
	SDLNet_TCP_Close(c->sock);

	// Unlink from the list.
	if(c == firstCts) firstCts = firstCts->next;
	if(c == lastCts) lastCts = lastCts->prev;
	if(c->prev) c->prev->next = c->next;
	if(c->next) c->next->prev = c->prev;

	// All gone?
	if(!--numCts) firstCts = lastCts = NULL;
	
	free(c);
}

void N_DeleteAllControlSockets(void)
{
	while(numCts > 0) N_DeleteControlSocket(firstCts);
}

/*
 * Insert a character into the control connection's incoming buffer.
 * Returns true if successful.
 */
static boolean N_PutByte(ctlsocket_t *s, byte ch)
{
	int next = (s->head + 1) % CTL_BUFFER_SIZE;
	
	if(next == s->tail)
	{
		// The buffer would overflow.
		return false;
	}

	s->buffer[s->head] = ch;
	s->head = next;
	return true;
}

/*
 * Returns true if the control connection's buffer is empty.
 */
static boolean N_IsEmpty(ctlsocket_t *s)
{
	return s->head == s->tail;
}

/*
 * Returns the number of bytes waiting in the connection's buffer.
 */
static int N_GetByteCount(ctlsocket_t *s)
{
	if(s->head > s->tail)
		return s->head - s->tail;
	else
		return CTL_BUFFER_SIZE - s->tail + s->head;
}

/*
 * Retrieve a byte from the control connection's buffer.  Returns zero
 * if no more bytes.
 */
static byte N_GetByte(ctlsocket_t *s)
{
	byte ch;
	
	if(N_IsEmpty(s)) return 0;

	ch = s->buffer[s->tail];
	s->tail = (s->tail + 1) % CTL_BUFFER_SIZE;
	return ch;
}

/*
 * Returns true if the connection's buffer contains the given string.
 */
static boolean N_FindString(ctlsocket_t *s, const char *string)
{
	int oldTail = s->tail;
	int length  = strlen(string);
	int match   = 0;
	char ch;
	
	if(N_GetByteCount(s) < length) return false; // Not enough data.

	while(!N_IsEmpty(s))
	{
		ch = N_GetByte(s);
		if(ch != string[match]) match = 0; // Start over.
		if(ch == string[match]) // Matching char?
		{
			if(++match == length) break;
		}
	}
	s->tail = oldTail;
	return match == length;
}

/*
 * Retrieve a line of text from the connection's buffer.  Returns the
 * length of the retrieved string.
 */
static int N_GetLine(ctlsocket_t *s, char *buffer, int maxLen)
{
	int i;
	
	// If there is no more data, return -1 to indicate error.
	if(N_IsEmpty(s))
	{
		buffer[0] = 0;
		return -1;
	}

	for(i = 0; i < maxLen - 1 && !N_IsEmpty(s); i++)
	{
		buffer[i] = N_GetByte(s);
		if(buffer[i] == '\n' || buffer[i] == '\r') break;
	}
	// A newline will be removed.
	buffer[i] = 0;
	return i;
}

/*
 * Validate and process the command that has been sent by a remote
 * agent.  Note that anyone can connect to a server via telnet and
 * issue queries.
 *
 * If the command is invalid, the node is immediately closed.  We
 * don't have time to fool around with badly behaving clients.
 */
static boolean N_ExecuteCommand(ctlsocket_t *c)
{
/*	char command[80], *ch, buf[256];
	const char *in;
	TCPsocket sock = netNodes[node].sock;
	Uint16 port;*/
#define MAX_COMMAND_LEN 81
	byte command[MAX_COMMAND_LEN], buf[256];
	serverinfo_t info;
	ddstring_t *msg;
	int length;

	// If the command is malformed, the connection will be terminated.
	length = N_GetLine(c, command, MAX_COMMAND_LEN);
	if(length <= 0 || length == MAX_COMMAND_LEN - 1)
		return false;
	
	VERBOSE2( Con_Message("N_ExecuteCommand: \"%s\"\n", command) );
	
	// Status query?
	if(!strcmp(command, "INFO"))
	{
		Sv_GetInfo(&info);
		msg = Str_New();
		Str_Appendf(msg, "BEGIN\n");
		Sv_InfoToString(&info, msg);
		Str_Appendf(msg, "END\n");
		SDLNet_TCP_Send(c->sock, Str_Text(msg), Str_Length(msg));
		Str_Delete(msg);
	}
/*	else if(!strncmp(command, "JOIN ", 5) && length > 10)
	{
		// Which UDP port does the client use?
		memset(buf, 0, 5);
		strncpy(buf, command + 5, 4);
		port = strtol(buf, &ch, 16);
		if(*ch || !port)
		{
			N_TerminateNode(node);
			return false;
		}
		
		// Read the client's name and convert the network node into a
		// real client network node (which has a transmitter).
		if(N_JoinNode(node, port, command + 10))
		{
			// Successful! Send a reply.
			sprintf(buf, "ENTER %04x\n", recvUDPPort);
			SDLNet_TCP_Send(sock, buf, strlen(buf));
		}
		else
		{
			// Couldn't join the game, so close the connection.
			SDLNet_TCP_Send(sock, "BYE\n", 4);
			N_TerminateNode(node);
		}
		}*/
	else if(!strcmp(command, "TIME"))
	{
		sprintf(buf, "%.3f\n", Sys_GetSeconds());
		SDLNet_TCP_Send(c->sock, buf, strlen(buf));
	}
/*	else if(!strcmp(command, "BYE"))
	{
		// Request for the server to terminate the connection.
		N_TerminateNode(node);
		}*/
	else
	{
		// Too bad, scoundrel! Goodbye.
		SDLNet_TCP_Send(c->sock, "Huh?\n", 5);
		return false;
	}

	// Everything was OK.
	return true;
}

/*
 * Clients will frequently need to wait for a response from the
 * server.  In the state responder we check if we have received a
 * response, and when that happens the client's state changes.
 */  
void N_ClientStateResponder(void)
{
	char line[256];
	
	// Clients only use one control connection at a time.
	ctlsocket_t *ctl = firstCts;
	
	if(!ctl || serverSock) return; // Not available for servers.

	switch(clientState)
	{
	case CST_IDLE:
		// Ignore anything the server sends.
		while(!N_IsEmpty(ctl)) N_GetByte(ctl);
		break;

	case CST_QUIT:
		// Close the connection.
		SDLNet_TCP_Send(ctl->sock, "BYE\n", 4);
		N_DeleteControlSocket(ctl);
		return;

	case CST_INFO:
	case CST_INFO_AND_QUIT:
		// Wait for a "BEGIN\n...\nEND\n" response.
		if(N_FindString(ctl, "\nEND\n"))
		{
			located.valid = true;
			N_GetLine(ctl, line, sizeof(line));
			
			// Did we receive what we expected to receive?
			if(!strcmp(line, "BEGIN"))
			{
				// Convert the string into a serverinfo_s.
				while(N_GetLine(ctl, line, sizeof(line)) > 0)
				{
					Sv_StringToInfo(line, &located.info);
				}

				// Show the information in the console.
				Con_Printf("%i server%s been found.\n", N_GetHostCount(),
						   N_GetHostCount() != 1? "s have" : " has");
				Net_PrintServerInfo(0, NULL);
				Net_PrintServerInfo(0, &located.info);
			}
			else
			{
				Con_Message("N_LookForHosts: Reply from server "
							"was invalid.\n");
			}

			if(clientState == CST_INFO_AND_QUIT)
				clientState = CST_QUIT;
			else
				clientState = CST_IDLE;
		}			   
		break;
	}
}

/*
 * Poll all TCP sockets for activity.
 */
void N_Listen(void)
{
	TCPsocket client;
	ctlsocket_t *ctl, *next;
	byte ch;

	// The server will listen for new connections.
	if(serverSock)
	{
		// Any incoming connections on the listening socket?
		while((client = SDLNet_TCP_Accept(serverSock)) != NULL)
		{
			// A new client is attempting to connect.  Open a new
			// control socket so the client can issue commands.
			if(!N_NewControlSocket(client))
			{
				// There was a failure, close the socket.
				SDLNet_TCP_Close(client);
				client = NULL;
			}
		}
	}

	while(sockSet)
	{
		// Poll all control sockets for incoming data.
		if(SDLNet_CheckSockets(sockSet, 0) <= 0) break;
		
		// Receive characters from all the control connections.
		for(ctl = firstCts; ctl; ctl = next)
		{
			next = ctl->next;
			if(!SDLNet_SocketReady(ctl->sock)) continue;

			if(SDLNet_TCP_Recv(ctl->sock, &ch, 1) <= 0)
				goto delSocket;	// Close on failure.

			// Newlines trigger command execution on the server.  They
			// are not inserted into the buffer.
			if(ch == '\n' && serverSock)
			{
				// Invalid commands won't be tolerated.
				if(!N_ExecuteCommand(ctl)) goto delSocket;
			}
			else
			{
				// Insert the character into the incoming buffer.  If
				// the buffer would overflow, we close the connection.
				if(!N_PutByte(ctl, ch)) goto delSocket;
			}
			continue;

		delSocket:
			N_DeleteControlSocket(ctl);
		}
	}

	// Clients will frequently need to wait for a response from the
	// server.  In the state responder we check if we have received a
	// response, and when that happens the client's state changes.
	N_ClientStateResponder();
}

/*
 * Returns the control connection that a client uses.
 */
ctlsocket_t *N_GetClientControlSocket(void)
{
	return firstCts;
}

/*
 * Try to open a control connection to the destination.  Returns true
 * if the connection has been opened.  This can be used only by
 * clients.
 */
boolean N_OpenControlSocket(const char *address, ushort port)
{
	TCPsocket sock;
	char buf[80];
	
	if(!isClient)
	{
		Con_Message("N_OpenControlSocket: Only allowed for clients.\n");
		return false;
	}

	// We can't open a connection if one is already oped.
	if(N_GetClientControlSocket())
	{
		Con_Message("A control connection is already open.\n");
		return false;
	}

	// Get rid of previous findings.
	memset(&located, 0, sizeof(located));
	
	// Let's determine the address we will be looking into.
	SDLNet_ResolveHost(&located.addr, address, port);

	// I say, anyone there?
	if((sock = SDLNet_TCP_Open(&located.addr)) == NULL)
	{
		Con_Message("N_OpenControlSocket: Can't connect to %s (port %i).\n",
					address, port);
		Con_Message("  %s\n", SDLNet_GetError());
		return false;
	}

	// Register the socket for control communications.
	N_NewControlSocket(sock);

	N_IPToString(buf, &located.addr);
	Con_Message("Connected to %s.\n", buf);
	
	return true;
}

/*
 * Send an INFO query over the control connection.
 */ 
void N_AskInfo(boolean quit)
{
	ctlsocket_t *c = N_GetClientControlSocket();
	
	if(!c) return;

	SDLNet_TCP_Send(c->sock, "INFO\n", 5);
	
	// The connection will be closed after we receive a reply.
	clientState = (quit? CST_INFO_AND_QUIT : CST_INFO);
}

/*
 * Retrieve information about the located host(s).
 */
boolean	N_GetHostInfo(int index, struct serverinfo_s *info)
{
	if(!located.valid || index != 0) return false;
	memcpy(info, &located.info, sizeof(*info));
	return true;
}

/*
 * How many hosts have been located?
 */
int N_GetHostCount(void)
{
	return located.valid? 1 : 0;
}

/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_network.c: Low-Level Sockets Networking
 *
 * TCP sockets are periodically polled for activity (Net_Update ->
 * N_Listen).
 */

// HEADER FILES ------------------------------------------------------------

#include <SDL_net.h>

#include "de_base.h"
#include "de_network.h"
#include "de_console.h"
#include "de_system.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_TCP_PORT	13209
#define DEFAULT_UDP_PORT	13209
#define MAX_NODES 			32
#define MAX_PACKET_SIZE		1600

// TYPES -------------------------------------------------------------------

/*
 * Each network node has a send queue. The queue contains a number of
 * sqpacks.
 */
typedef struct sqpack_s {
	struct sqpack_s *next;
	UDPpacket *packet;
} sqpack_t;

/*
 * On serverside, each client has its own network node. A node
 * represents the TCP connection between the client and the server. On
 * clientside, the node zero is used always.
 */
typedef struct netnode_s {
	TCPsocket sock;
	char name[128];

	// The node is owned by a client in the game.  This becomes true
	// when the client issues the JOIN request.
	boolean isClient;

	// This is the UDP address that the client listens to.
	IPaddress addr; 

	// The send queue.
	int mutex;
	semaphore_t waiting;
	sqpack_t *first, *last;
	uint numWaiting;
	uint bytesWaiting;
} netnode_t;

typedef struct foundhost_s {
	boolean valid;
	serverinfo_t info;
	IPaddress addr;
} foundhost_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Settings for the various network protocols.
// Most recently used provider: 0=TCP/IP, 1=IPX, 2=Modem, 3=Serial.
int			nptActive;
// TCP/IP:
char		*nptIPAddress = "";
int 		nptIPPort = 0;	// This is the port *we* use to communicate.
int			nptUDPPort = 0;
int			nptUDPOutPort = 0;
int			defaultTCPPort = DEFAULT_TCP_PORT;
int 		defaultUDPPort = DEFAULT_UDP_PORT;
// Modem:
int			nptModem = 0;	// Selected modem device index.
char		*nptPhoneNum = "";
// Serial:
int			nptSerialPort = 0;
int			nptSerialBaud = 57600;
int			nptSerialStopBits = 0;
int			nptSerialParity = 0;
int			nptSerialFlowCtrl = 4;

// Operating mode of the currently active service provider.
serviceprovider_t netCurrentProvider = NSP_NONE;
boolean		netServerMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TCPsocket serverSock;
static volatile UDPsocket inSock, outSock;
static Uint16 actualRecvUDPPort; 
static int mutexInSock;
static netnode_t netNodes[MAX_NODES];
static SDLNet_SocketSet sockSet;
static int hReceiver;

static foundhost_t located;

// CODE --------------------------------------------------------------------

/*
 * A UDP transmitter thread takes messages off a network node's send
 * queue and sends them one by one. On serverside, each client has its
 * own transmitter thread.
 *
 * 'parm' is a pointer to the network node of this transmitter.
 */
static int N_UDPTransmitter(void *parm)
{
	netnode_t *node = parm;
	sqpack_t *pack;

	while(node->sock != NULL)
	{
		// We will wait until there are messages to send.  The
		// semaphore is incremented when a new message is added to the
		// queue.  Waiting for this semaphore causes us to sleep
		// unless there are messages to send.
		Sem_P(node->waiting);

		// Lock the send queue.
		Sem_P(node->mutex);

		// If there are no messages waiting, we realize it's time to
		// quit.
		if(!node->numWaiting || node->sock == NULL)
		{
			Sys_Unlock(node->mutex);
			break;
		}

		// Extract the next message from the FIFO queue.
		pack = node->first;
		node->first = node->first->next;
		if(!node->first) node->last = NULL;
		node->numWaiting--;
		node->bytesWaiting -= pack->packet->len;

		Sem_V(node->mutex);

		// Commence sending.
		SDLNet_UDP_Send(outSock, -1, pack->packet);

		// Now that the packet has been sent, we can discard the data.
		SDLNet_FreePacket(pack->packet);
		free(pack);
	}
	return 0;
}

/*
 * The UDP receiver thread waits for UDP packets and places them into
 * the incoming message buffer. The UDP receiver is started when the
 * TCP/IP service is initialized. The thread is stopped when the
 * service is shut down.
 */
static int N_UDPReceiver(void *parm)
{
	SDLNet_SocketSet set;
	UDPpacket *packet = NULL;

	// Put the UDP socket in our socket set so we can wait for it.
	set = SDLNet_AllocSocketSet(1);
	SDLNet_UDP_AddSocket(set, inSock);

	while(inSock)
	{
		// Most of the time we will be sleeping here, waiting for
		// incoming packets.
		if(SDLNet_CheckSockets(set, 750) <= 0) continue;

		// There is activity on the socket. Allocate a new packet
		// to store the data into. The packet will be released later,
		// in N_ReturnBuffer.
		if(!packet)
		{
			// Allocate a new packet.
			packet = SDLNet_AllocPacket(MAX_PACKET_SIZE);
		}
		
		// The mutex will prevent problems when new channels are
		// bound to the socket.
		Sys_Lock(mutexInSock);
		if(SDLNet_UDP_Recv(inSock, packet) > 0)
		{
			Sys_Unlock(mutexInSock);

			// If we don't know the sender, discard the packet.
			if(packet->channel < 0) continue;
			
			// Successfully received a packet.
			netmessage_t *msg = (netmessage_t*)
				calloc(sizeof(netmessage_t), 1);
			
			msg->sender = packet->channel;
			msg->data   = packet->data;
			msg->size   = packet->len;
			msg->handle = packet;
			
			// The message queue will handle the message from now on.
			N_PostMessage(msg);
			
			// This packet has now been used.
			packet = NULL;
		}
		else
		{
			Sys_Unlock(mutexInSock);
		}
	}

	if(packet) SDLNet_FreePacket(packet);
	SDLNet_FreeSocketSet(set);
	return 0;
}

/*
 * Send the buffer to the destination. For clients, the server is the
 * only possible destination (doesn't depend on the value of
 * 'destination').
 */
void N_SendDataBuffer(void *data, uint size, nodeid_t destination)
{
}

/*
 * Returns the number of messages waiting in the player's send queue.
 */
uint N_GetSendQueueCount(int player)
{
	return 0;
}

/*
 * Returns the number of bytes waiting in the player's send queue.
 */
uint N_GetSendQueueSize(int player)
{
	return 0;
}

/*
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
	if(!SDLNet_Init())
	{
		VERBOSE( Con_Message("N_SystemInit: OK\n") );
	}
	else
	{
		Con_Message("N_SystemInit: %s\n", SDLNet_GetError());
	}
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
	SDLNet_Quit();
}

unsigned int N_GetServiceProviderCount(serviceprovider_t type)
{
	// Only TCP/IP.
	return type == NSP_TCPIP? 1 : 0;
}

boolean	N_GetServiceProviderName(serviceprovider_t type,
								 unsigned int index, char *name,
								 int maxLength)
{
	if(type != NSP_TCPIP || index != 0) return false;
	if(name) strncpy(name, "TCP/IP", maxLength);
	return true;
}

void N_IPToString(char *buf, IPaddress *ip)
{
	uint host = SDLNet_Read32(&ip->host);
		
	sprintf(buf, "%i.%i.%i.%i:%i\n", host >> 24, (host >> 16) & 0xff,
			(host >> 8) & 0xff, host & 0xff, SDLNet_Read16(&ip->port));
}

/*
 * Initialize the chosen service provider each in server or client
 * mode.  If a service provider has already been initialized, it will
 * be shut down first.  Returns true if successful.
 */
boolean N_InitService(serviceprovider_t provider, boolean inServerMode)
{
	IPaddress ip;
	Uint16 port;
	int i;
	
	if(netCurrentProvider == provider && netServerMode == inServerMode)
	{
		// Nothing to change.
		return true;
	}

	// Get rid of the currently active service provider.
	N_ShutdownService();

	if(provider == NSP_NONE)
	{
		// Nothing further to do.
		return true;
	}
	if(provider != NSP_TCPIP)
	{
		// Only TCP/IP is supported.
		Con_Message("N_InitService: Provider not supported.\n");
		return false;
	}

	if(inServerMode)
	{
		port = nptIPPort;
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

		// Allocate a socket set, which we'll use for listening to the
		// client sockets.
		if(!(sockSet = SDLNet_AllocSocketSet(MAX_NODES)))
		{
			Con_Message("N_InitService: %s\n", SDLNet_GetError());
			return false;
		}

	}
	else
	{
/*		// On clientside, specifying port zero means that we don't
		// care which port will be used.
		inSock = SDLNet_UDP_Open((Uint16)nptUDPPort);

		if(verbose)
		{
			Con_Message("N_InitService: ");
			if(nptUDPPort)
				Con_Message("Using UDP port %i.\n", nptUDPPort);
			else
				Con_Message("Using a UDP port chosen by the OS.\n");
		}
*/
		// Let's forget about servers found earlier.
		located.valid = false;
	}

	// FIXME: When entering server mode, why must be close and re-open
	// the incoming UDP port? It's the same either way...

	// Open the socket that will be used for UDP communications.
	// Port zero means that we will use the default port.
	port = nptUDPPort;
	if(!port) port = defaultUDPPort;
	inSock = SDLNet_UDP_Open(port);
	for(i = 0; !inSock && i < MAX_NODES; i++)
	{
		port = defaultUDPPort + i;
		VERBOSE( Con_Message("N_InitService: Trying alternative incoming "
							 "UDP port (%i).\n", port) );
		inSock = SDLNet_UDP_Open(port);
	}
	actualRecvUDPPort = port;
	VERBOSE( Con_Message("N_InitService: Incoming UDP port %i.\n", port) );

	// Success.
	nptActive = provider - 1; // -1 to match old values: 0=TCP/IP...
	netServerMode = inServerMode;
	netCurrentProvider = provider;

	// Did we fail in opening the UDP port?
	if(!inSock)
	{
		N_ShutdownService();
		return false;
	}

	// Open the port for outgoing UDP communications.
	port = nptUDPOutPort;
	if(!port && inServerMode) port = defaultUDPPort + 1;
	if(port)
	{
		VERBOSE( Con_Message("N_InitService: Outgoing UDP port %i.\n", port) );
	}
	if(!(outSock = SDLNet_UDP_Open(port)))
	{
		Con_Message("N_InitService: Couldn't open outgoing UDP socket.\n");
		N_ShutdownService();
		return false;
	}

	// Start the UDP receiver right away.
	mutexInSock = Sys_CreateMutex("UDPIncomingMutex");
	hReceiver = Sys_StartThread(N_UDPReceiver, NULL, 0);

	return true;
}

void N_ShutdownService(void)
{
	int i;
	UDPsocket sock;
	
	if(!N_IsAvailable()) return; // Nothing to do.

	if(netgame)
	{
		// We seem to be shutting down while a netgame is running.
		Con_Execute(isServer? "net server close" : "net disconnect", true);
	}

/*	N_StopLookingForHosts();

	// The list of found hosts can be deleted.
	N_ClearHosts();

	if(netServerMode)
		gServer->Close(0);
	else
		gClient->Close(0);
*/
	// Any queued messages will be destroyed.
	N_ClearMessages();

	// Close the outgoing UDP socket.
	sock = outSock;
	outSock = NULL;
	SDLNet_UDP_Close(sock);
	
	// Close the incoming UDP socket.
	sock = inSock;
	inSock = NULL;
	SDLNet_UDP_Close(sock);
	Sys_WaitThread(hReceiver);
	hReceiver = 0;

	Sys_DestroyMutex(mutexInSock);
	mutexInSock = 0;
	
	if(netServerMode)
	{
		// Close the listening socket.
		SDLNet_TCP_Close(serverSock);
		serverSock = NULL;

		// Clear the client nodes.
		for(i = 0; i < MAX_NODES; i++) N_TerminateNode(i);

		// Free the socket set.
		SDLNet_FreeSocketSet(sockSet);
		sockSet = NULL;
	}
	else
	{
		// Let's forget about servers found earlier.
		located.valid  = false;
	}
	
	// Reset the current provider's info.
	netCurrentProvider = NSP_NONE;
	netServerMode      = false;
}

/*
 * Free a message buffer.
 */
void N_ReturnBuffer(void *handle)
{
	if(!handle) return;
	SDLNet_FreePacket(handle);
}

/*
 * Returns true if the low-level network routines have been initialized
 * and are expected to be working.
 */
boolean	N_IsAvailable(void)
{
	return netCurrentProvider != NSP_NONE;
}

/*
 * Returns true if the internet is available.
 */
boolean N_UsingInternet(void)
{
	return netCurrentProvider == NSP_TCPIP;
}

boolean	N_GetHostInfo(int index, struct serverinfo_s *info)
{
	if(!located.valid || index != 0) return false;
	memcpy(info, &located.info, sizeof(*info));
	return true;
}

int N_GetHostCount(void)
{
	return located.valid? 1 : 0;
}

const char *N_GetProtocolName(void)
{
	return "TCP/IP";
}

/*
 * Returns the player name associated with the given network node.
 */
boolean N_GetNodeName(nodeid_t id, char *name)
{
	if(!netNodes[id].sock)
	{
		strcpy(name, "-unknown-");
		return false;
	}
	strcpy(name, netNodes[id].name);
	return true;
}

/*
 * The client is removed from the game immediately. This is used when
 * the server needs to terminate a client's connection abnormally.
 */
void N_TerminateNode(nodeid_t id)
{
	netnode_t *node = &netNodes[id];
	netevent_t netEvent;

	if(!node->sock) return; // There is nothing here...

	if(netServerMode && node->isClient)
	{
		// This causes a network event.
		netEvent.type = NE_CLIENT_EXIT;
		netEvent.id = id;
		N_NEPost(&netEvent);
	}
	
	// Remove the node from the set of active sockets.
	SDLNet_TCP_DelSocket(sockSet, node->sock);

	// Close the socket and forget everything about the node.
	SDLNet_TCP_Close(node->sock);

	// Cancel all packets waiting in the send queue.
	Sem_P(node->mutex);
	while(node->first)
	{
		sqpack_t *sqp = node->first;
		node->first = sqp->next;
		free(sqp);
	}
	node->sock = NULL;
	node->first = node->last = NULL;
	node->numWaiting = 0;
	Sem_V(node->mutex);
	// Also cancel the wait.
	Sem_V(node->waiting);
	memset(node, 0, sizeof(*node));
}

/*
 * Registers a new TCP socket as a client node. There can only be a
 * limited number of nodes at a time.
 */
static boolean N_RegisterNewSocket(TCPsocket sock)
{
	int i;
	netnode_t *node;

	// Find a free node.
	for(i = 0, node = netNodes; i < MAX_NODES; i++, node++)
		if(!node->sock)
		{
			// This'll do.
			node->sock = sock;

			// We don't know the name yet.
			memset(node->name, 0, sizeof(node->name));

			// Add this socket to the set of client sockets.
			SDLNet_TCP_AddSocket(sockSet, sock);

			return true;
		}
	// There were no free nodes.
	return false;
}

/*
 * It would probably be enough to have one transmitter thread...?
 */
static void N_StartTransmitter(netnode_t *node)
{
	node->numWaiting   = 0;
	node->bytesWaiting = 0;
	node->mutex        = Sem_Create(1);
	node->waiting      = Sem_Create(0);
	Sys_StartThread(N_UDPTransmitter, node, 0);
}

/*
 * A network node wishes to become a real client. Returns true if we
 * allow this.
 */
static boolean N_JoinNode(nodeid_t id, Uint16 port, const char *name)
{
	netnode_t *node;
	netevent_t netEvent;
	IPaddress *ip;
	
	// If the server is full, attempts to connect are canceled.
	if(Sv_GetNumConnected() >= sv_maxPlayers) return false;

	node = &netNodes[id];
	
	// The UDP address where we should be sending data.
	if(!(ip = SDLNet_TCP_GetPeerAddress(node->sock)))
	{
		// This is a strange situation...
		return false;
	}
	memcpy(&node->addr, ip, sizeof(IPaddress));
	SDLNet_Write16(port, &node->addr.port);

	if(verbose)
	{
		char buf[80];
		N_IPToString(buf, &node->addr);
		Con_Message("N_JoinNode: Node %i listens at %s (UDP).\n", id, buf);
	}
	
	// Convert the network node into a real client node.
	node->isClient = true;

	// FIXME: We should use more discretion with the name. It has
	// been provided by an untrusted source.
	strncpy(node->name, name, sizeof(node->name) - 1);

	// Prepare a UDP transmitter for the node.
	N_StartTransmitter(node);

	// Bind the address to the incoming UDP socket, so we'll recognize
	// the sender.
	Sys_Lock(mutexInSock);
	SDLNet_UDP_Bind(inSock, id, &node->addr);
	Sys_Unlock(mutexInSock);					
	
	// Inform the higher levels of this occurence.
	netEvent.type = NE_CLIENT_ENTRY;
	netEvent.id   = id;
	N_NEPost(&netEvent);
	
	return true;
}

/*
 * Maybe it would be wisest to run this in a separate thread?
 */
boolean N_LookForHosts(void)
{
	TCPsocket sock;
	char buf[256];
	ddstring_t *response;
	
	// We must be a client.
	if(!N_IsAvailable() || netServerMode) return false;

	// Get rid of previous findings.
	memset(&located, 0, sizeof(located));

	// Let's determine the address we will be looking into.
	SDLNet_ResolveHost(&located.addr, nptIPAddress, nptIPPort);

	// I say, anyone there?
	sock = SDLNet_TCP_Open(&located.addr);
	if(!sock)
	{
		Con_Message("N_LookForHosts: No reply from %s (port %i).\n",
					nptIPAddress, nptIPPort);
		return false;
	}

	// Send an INFO query.
	SDLNet_TCP_Send(sock, "INFO\n", 5);

	// Let's listen to the reply.
	memset(buf, 0, sizeof(buf));
	response = Str_New();
	while(!strstr(Str_Text(response), "END\n"))
	{
		int result = SDLNet_TCP_Recv(sock, buf, sizeof(buf) - 1);
		if(result <= 0) break; // Terminated?
		Str_Appendf(response, buf);
	}

	// Close the connection; that was all the information we need.
	SDLNet_TCP_Close(sock);
	
	// Did we receive what we expected to receive?
	if(strstr(Str_Text(response), "BEGIN\n"))
	{
		const char *ch;
		ddstring_t *line;

		located.valid = true;

		// Convert the string into a serverinfo_s.
		line = Str_New();
		ch = Str_Text(response);
		do
		{
			ch = Str_GetLine(line, ch);
			Sv_StringToInfo(Str_Text(line), &located.info);
		}
		while(*ch);
		Str_Delete(line);
		Str_Delete(response);

		// Show the information in the console.
		Con_Printf("%i server%s been found.\n", N_GetHostCount(),
				   N_GetHostCount() != 1? "s have" : " has");
		Net_PrintServerInfo(0, NULL);
		Net_PrintServerInfo(0, &located.info);
		return true;
	}
	else
	{
		Str_Delete(response);
		Con_Message("N_LookForHosts: Reply from %s (port %i) was invalid.\n",
					nptIPAddress, nptIPPort);
		return false;
	}
}

boolean N_Connect(int index)
{
	netnode_t *node;
	foundhost_t *host;
	char buf[128], *pName;
	
	if(!N_IsAvailable() || netServerMode || index != 0) return false;

	Demo_StopPlayback();

	// Call game DLL's NetConnect.
	gx.NetConnect(true);

	host = &located;
	
	// We'll use node number zero for all communications.
	node = &netNodes[0];
	if(!(node->sock = SDLNet_TCP_Open(&host->addr)))
	{
		N_IPToString(buf, &host->addr);
		Con_Message("N_Connect: No reply from %s.\n", buf);
		return false;
	}
	memcpy(&node->addr, &located.addr, sizeof(IPaddress));
	
	// Connect by issuing: "JOIN (my-udp) (myname)"
	pName = playerName;
	if(!pName || !pName[0]) pName = "Anonymous";
	sprintf(buf, "JOIN %04x %s\n", actualRecvUDPPort, pName);
	SDLNet_TCP_Send(node->sock, buf, strlen(buf));

	// What is the reply?
	memset(buf, 0, sizeof(buf));
	if(SDLNet_TCP_Recv(node->sock, buf, 64) <= 0
		|| strncmp(buf, "ENTER ", 6))
	{
		SDLNet_TCP_Close(node->sock);
		memset(node, 0, sizeof(node));
		Con_Message("N_Connect: Server refused connection.\n");
		if(buf[0]) Con_Message("  Reply: %s", buf);
		return false;
	}

	VERBOSE( Con_Message("Server responds: %s", buf) );

	// The server tells us which UDP port we should send packets to.
	SDLNet_Write16(strtol(buf + 6, NULL, 16), &node->addr.port);

	// Start the transmitter.
	N_StartTransmitter(node);

	handshake_received = false;
	netgame = true; // Allow sending/receiving of packets.
	isServer = false;
	isClient = true;
	
	// Call game's NetConnect.
	gx.NetConnect(false);
	
	// G'day mate!
	Cl_SendHello();
	return true;
}

/*
 * Disconnect from the server.
 */
boolean N_Disconnect(void)
{
	if(!N_IsAvailable()) return false;
	
	// Tell the Game that a disconnecting is about to happen.
	if(gx.NetDisconnect) gx.NetDisconnect(true);
	
	Net_StopGame();
	N_ClearMessages();

/*	
	// Exit the session, but reinit the client interface.
	gClient->Close(0);
	N_InitDPObject(false);
*/

	// Tell the Game that disconnecting is now complete.
	if(gx.NetDisconnect) gx.NetDisconnect(false);

	return true;
}

boolean N_ServerOpen(void)
{
	if(!N_IsAvailable()) return false;

	Demo_StopPlayback();
	
	// Let's make sure the correct service provider is initialized
	// in server mode.
	if(!N_InitService(netCurrentProvider, true))
	{
		Con_Message("N_ServerOpen: Failed to initialize server mode.\n");
		return false;
	}

	// The game module may have something that needs doing before we
	// actually begin.
	if(gx.NetServerStart) gx.NetServerStart(true);

	Sv_StartNetGame();

	// The game DLL might want to do something now that the
	// server is started.
	if(gx.NetServerStart) gx.NetServerStart(false);

	if(masterAware && N_UsingInternet())
	{
		// Let the master server know that we are running a public server.
		N_MasterAnnounceServer(true);
	}
	return true;
}

boolean N_ServerClose(void)
{
	if(!N_IsAvailable()) return false;

	if(masterAware && N_UsingInternet())
	{
		// Bye-bye, master server.
		N_MAClear();
		N_MasterAnnounceServer(false);
	}
	if(gx.NetServerStop) gx.NetServerStop(true);
	Net_StopGame();

	// Exit server mode.
	N_InitService(netCurrentProvider, false);

	if(gx.NetServerStop) gx.NetServerStop(false);
	return true;
}

/*
 * Validate and process the command, which has been sent by a remote
 * agent. Anyone is free to connect to a server using telnet and issue
 * queries.
 *
 * If the command is invalid, the node is immediately closed. We don't
 * have time to fool around with badly behaving clients.
 */
static boolean N_DoNodeCommand(nodeid_t node, const char *input, int length)
{
	char command[80], *ch, buf[256];
	const char *in;
	TCPsocket sock = netNodes[node].sock;
	serverinfo_t info;
	ddstring_t msg;
	Uint16 port;
	
	// If the command is too long, it'll be considered invalid.
	if(length >= 80)
	{
		N_TerminateNode(node);
		return false;
	}

	// Make a copy of the command.
	memset(command, 0, sizeof(command));
	for(ch = command, in = input;
		*in && *in != '\r' && *in != '\n' && in - input < length;)
		*ch++ = *in++;

	//Con_Message("N_DoNodeCommand: %s\n", command);
	
	// Status query?
	if(!strcmp(command, "INFO"))
	{
		Sv_GetInfo(&info);
		Str_Init(&msg);
		Str_Appendf(&msg, "BEGIN\n");
		Sv_InfoToString(&info, &msg);
		Str_Appendf(&msg, "END\n");
		SDLNet_TCP_Send(sock, Str_Text(&msg), Str_Length(&msg));
		Str_Free(&msg);
	}
	else if(!strncmp(command, "JOIN ", 5) && length > 10)
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
			sprintf(buf, "ENTER %04x\n", actualRecvUDPPort);
			SDLNet_TCP_Send(sock, buf, strlen(buf));
		}
		else
		{
			// Couldn't join the game, so close the connection.
			SDLNet_TCP_Send(sock, "BYE\n", 4);
			N_TerminateNode(node);
		}
	}
	else if(!strcmp(command, "TIME"))
	{
		sprintf(buf, "%.3f\n", Sys_GetSeconds());
		SDLNet_TCP_Send(sock, buf, strlen(buf));
	}
	else if(!strcmp(command, "BYE"))
	{
		// Request for the server to terminate the connection.
		N_TerminateNode(node);
	}
	else
	{
		// Too bad, scoundrel! Goodbye.
		SDLNet_TCP_Send(sock, "Huh?\n", 5);
		N_TerminateNode(node);
		return false;
	}

	// Everything was OK.
	return true;
}

/*
 * Poll all TCP sockets for activity. Client commands are processed.
 */
void N_Listen(void)
{
	TCPsocket sock;
	int i, result;
	char buf[256];

	if(netServerMode)
	{
		// Any incoming connections on the listening socket?
		// FIXME: Include this in the set of sockets?
		while((sock = SDLNet_TCP_Accept(serverSock)) != NULL)
		{
			// A new client is attempting to connect. Let's try to
			// register the new socket as a network node.
			if(!N_RegisterNewSocket(sock))
			{
				// There was a failure, close the socket.
				SDLNet_TCP_Close(sock);
			}
		}

		// Any activity on the client sockets? (Don't wait.)
		if(SDLNet_CheckSockets(sockSet, 0) > 0)
		{
			for(i = 0; i < MAX_NODES; i++)
			{
				// Does this socket have got any activity?
				if(!SDLNet_SocketReady(netNodes[i].sock)) continue;

				result = SDLNet_TCP_Recv(netNodes[i].sock, buf, sizeof(buf));
				if(result <= 0)
				{
					// Close this socket & node.
					VERBOSE2( Con_Message("N_Listen: Connection closed on "
										  "node %i.\n", i) );
					N_TerminateNode(i);
				}
				else
				{
					// FIXME: Read into a buffer, execute when newline
					// received.
					
					// Process the command; we will need to answer, or
					// do something else.
					N_DoNodeCommand(i, buf, result);
				}
			}
		}				
	}
   	else
	{
		
	}
}

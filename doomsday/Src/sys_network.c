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
 * sys_network.c: Network Control Services
 *
 * Network control means tasks like client connections, server
 * management and server control/status commands.  Everything is done
 * via character streams (over TCP).
 *
 * Packet communications are not handled here (but in sys_netlink.c).
 *
 * TCP control sockets are periodically polled for activity
 * (Net_Update -> N_Listen).
 */

// HEADER FILES ------------------------------------------------------------

#include <SDL_net.h>

#include "de_base.h"
#include "de_network.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_NODES 			DDMAXPLAYERS

// TYPES -------------------------------------------------------------------

/*
 * Each network node has a send queue. The queue contains a number of
 * sqpacks.
 */
/*typedef struct sqpack_s {
	struct sqpack_s *next;
	struct netnode_s *node;
	UDPpacket *packet;
	} sqpack_t;*/

/*
 * On serverside, each client has its own network node. A node
 * represents the TCP connection between the client and the server. On
 * clientside, the node zero is used always.
 */
/*typedef struct netnode_s {
	TCPsocket sock;
	char name[128];

	// The node is owned by a client in the game.  This becomes true
	// when the client issues the JOIN request.
	boolean hasJoined;

	// This is the UDP address that the client listens to.
	IPaddress addr; 

	// Send queue statistics.
	int mutex;
	uint numWaiting;
	uint bytesWaiting;
} netnode_t;
*/

typedef struct netnode_s {
//	ctlsocket_t *control;		// Control connection.
	char name[128];				// Client name.
} netnode_t;

/*typedef struct sendqueue_s {
	semaphore_t waiting;
	int mutex;
	sqpack_t *first, *last;
	boolean online; // Set to false to make transmitter stop.
} sendqueue_t;
*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void N_IPToString(char *buf, IPaddress *ip);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//uint		maxDatagramSize = MAX_DATAGRAM_SIZE;

// Settings for the various network protocols.
// Most recently used provider: 0=TCP/IP, 1=IPX, 2=Modem, 3=Serial.
int			nptActive;
// TCP/IP:
char		*nptIPAddress = "";
int			nptIPPort = 0;
//int			nptUDPPort = 0;
//int			defaultTCPPort = DEFAULT_TCP_PORT;
//int 		defaultUDPPort = DEFAULT_UDP_PORT;
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

static netnode_t netNodes[DDMAXPLAYERS];

// Client's control connection to the server.
//static TCPsocket serverSock;

//static volatile UDPsocket inSock;
//static Uint16 recvUDPPort; 
//static int mutexInSock;

//static SDLNet_SocketSet sockSet;
//static int hReceiver, hTransmitter;
//static sendqueue_t sendQ;


// CODE --------------------------------------------------------------------

/*
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_Init(void)
{
/*	// Create a mutex for the message queue.
	msgMutex = Sys_CreateMutex();
*/
	N_SockInit();
	N_MasterInit();
	N_SystemInit(); // Platform dependent stuff.
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_Shutdown(void)
{
	N_SystemShutdown();
	N_MasterShutdown();
	N_SockShutdown();

/*	// Close the handle of the message queue mutex.
	Sys_DestroyMutex(msgMutex);
	msgMutex = 0;*/
	
	if(ArgExists("-huffavg"))
	{
		Con_Execute("huffman", false);
	}
}

/*
 * Send the buffer to the destination. For clients, the server is the
 * only possible destination (doesn't depend on the value of
 * 'destination').
 */
void N_SendDataBuffer(void *data, uint size, nodeid_t destination)
{
/*	sqpack_t *pack;
	UDPpacket *p;
	netnode_t *node;

	// If the send queue is not active, we can't send anything.
	if(!sendQ.online) return;

//#ifdef _DEBUG
	if(size > maxDatagramSize)
	{
		Con_Error("N_SendDataBuffer: Too large packet (%i), risk of "
				  "fragmentation (MTU=%i).\n", size, maxDatagramSize);
	}
//#endif*/

/*	// This memory is freed after the packet is sent.
	pack = malloc(sizeof(sqpack_t));
	p = pack->packet = SDLNet_AllocPacket(size);

	// The destination node.
	pack->node = node = netNodes + destination;

	// Init the packet's data.
	p->channel = -1;
	memcpy(p->data, data, size);
	p->len = size;
	memcpy(&p->address, &node->addr, sizeof(p->address));

	// Add the packet to the send queue.
	Sem_P(sendQ.mutex);
	if(!sendQ.first)
	{
		sendQ.first = sendQ.last = pack;
	}
	else
	{
		sendQ.last->next = pack;
		sendQ.last = pack;
	}
	pack->next = NULL;
	Sem_V(sendQ.mutex);*/

	// Increment the statistics.
/*	Sem_P(node->mutex);
	node->numWaiting++;
	node->bytesWaiting += size;
	Sem_V(node->mutex);

	// Signal the transmitter to start working.
	Sem_V(sendQ.waiting);*/
}

/*
 * Returns the number of messages waiting in the player's send queue.
 */
/*uint N_GetSendQueueCount(int player)
{
	netnode_t *node = netNodes + player;
	uint count;

	Sem_P(node->mutex);
	count = node->numWaiting;
	Sem_V(node->mutex);
	return count;
}*/

/*
 * Returns the number of bytes waiting in the player's send queue.
 */
/*uint N_GetSendQueueSize(int player)
{
	netnode_t *node = netNodes + player;
	uint bytes;

	Sem_P(node->mutex);
	bytes = node->bytesWaiting;
	Sem_V(node->mutex);
	return bytes;
}*/

/*
 * Initialize the transmitter thread and the send queue.
 */
/*static void N_StartTransmitter(sendqueue_t *q)
{
	q->online  = true;
	q->waiting = Sem_Create(0);
	q->mutex   = Sem_Create(1);
	q->first   = NULL;
	q->last    = NULL;
	
	hTransmitter = Sys_StartThread(N_UDPTransmitter, q, 0);
}*/

/* 
 * Blocks until the transmitter thread has been exited.
 */
/*static void N_StopTransmitter(sendqueue_t *q)
{
	if(!hTransmitter) return;
	
	// Tell the transmitter to stop sending.
	q->online = false;

	// Increment the semaphore without adding a new message: this'll
	// make the transmitter "run dry."
	Sem_V(q->waiting);

	// Wait until the transmitter thread finishes.
	Sys_WaitThread(hTransmitter);
	hTransmitter = 0;

	// Destroy the semaphores.
	Sem_Destroy(q->waiting);
	Sem_Destroy(q->mutex);
}*/

/*
 * Start the UDP receiver thread.
 */
/*static void N_StartReceiver(void)
{
	mutexInSock = Sys_CreateMutex("UDPIncomingMutex");
	hReceiver = Sys_StartThread(N_UDPReceiver, NULL, 0);
}
*/
/*
 * Blocks until the UDP receiver thread has exited.
 */
/*static void N_StopReceiver(void)
{
	UDPsocket sock = inSock;

	// Close the incoming UDP socket.
	inSock = NULL;
	SDLNet_UDP_Close(sock);

	// Wait for the receiver thread the stop.
	Sys_WaitThread(hReceiver);
	hReceiver = 0;

	Sys_DestroyMutex(mutexInSock);
	mutexInSock = 0;
}*/

/*
 * Bind or unbind the address to/from the incoming UDP socket.  When
 * the address is bound, packets from it will be accepted.
 */
/*void N_BindIncoming(IPaddress *addr, nodeid_t id)
{
	Sys_Lock(mutexInSock);
	if(addr)
	{
		SDLNet_UDP_Bind(inSock, id, addr);
	}
	else
	{
		SDLNet_UDP_Unbind(inSock, id);
	}
	Sys_Unlock(mutexInSock);
}*/

/*	
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
/*	// The MTU can be customized.
	if(ArgCheckWith("-mtu", 1))
	{
		maxDatagramSize = strtol(ArgNext(), NULL, 0);
		Con_Message("N_SystemInit: Custom MTU: %i bytes.\n", maxDatagramSize);
		}*/
	
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

/*
 * Convert an IPaddress to a string.
 */
void N_IPToString(char *buf, IPaddress *ip)
{
	uint host;

	if(!ip)
	{
		strcpy(buf, "-?-");
	}
	else
	{
		host = SDLNet_Read32(&ip->host);
		sprintf(buf, "%i.%i.%i.%i:%i", host >> 24, (host >> 16) & 0xff,
				(host >> 8) & 0xff, host & 0xff, SDLNet_Read16(&ip->port));
	}
}

/* 
 * Opens an UDP socket.  The used port number is returned.  If the
 * socket cannot be opened, 'sock' is set to NULL.  'defaultPort'
 * should never be zero.
 */
/*Uint16 N_OpenUDPSocket(UDPsocket *sock, Uint16 preferPort, Uint16 defaultPort)
{
	Uint16 port = preferPort;
	int tries = 1000;
	
	if(!preferPort) port = defaultPort;
	*sock = NULL;

	// Try opening the port, advance to next one if the opening fails.
	for(; tries > 0; tries--)
	{
		if((*sock = SDLNet_UDP_Open(port)) == NULL)
			port++;
		else
			return port;
	}
	// Failure!
	return 0;
}*/

/*
 * Initialize the chosen service provider each in server or client
 * mode.  If a service provider has already been initialized, it will
 * be shut down first.  Returns true if successful.
 */
boolean N_InitService(serviceprovider_t provider, boolean inServerMode)
{
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

	if(!N_InitControl(inServerMode, nptIPPort)) return false;
	
	// Success.
	nptActive = provider - 1; // -1 to match old values: 0=TCP/IP...
	netServerMode = inServerMode;
	netCurrentProvider = provider;

	return true;
}

/*
 * Shut down the TCP/IP network services.
 */
void N_ShutdownService(void)
{
	int i;
	
	if(!N_IsAvailable()) return; // Nothing to do.

	if(netgame)
	{
		// We seem to be shutting down while a netgame is running.
		Con_Execute(isServer? "net server close" : "net disconnect", true);
	}

	// Any queued messages will be destroyed.
	//N_ClearMessages();

	// Kill the UDP transmitter thread.
//	N_StopTransmitter(&sendQ);

/*	
	// Close the outgoing UDP socket.
	sock = outSock;
	outSock = NULL;
	SDLNet_UDP_Close(sock);
*/

	//N_StopReceiver();

	N_ShutdownControl();
	
	if(netServerMode)
	{
		// Clear the client nodes.
		for(i = 0; i < MAX_NODES; i++) N_TerminateNode(i);
	}

	// Reset the current provider's info.
	netCurrentProvider = NSP_NONE;
	netServerMode      = false;
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

const char *N_GetProtocolName(void)
{
	return "TCP/IP";
}

/*
 * Returns the player name associated with the given network node.
 */
boolean N_GetNodeName(nodeid_t id, char *name)
{
/*	if(!netNodes[id].sock)
	{*/
		strcpy(name, "-unknown-");
		return false;
/*	}
	strcpy(name, netNodes[id].name);
	return true;*/
}

/*
 * The client is removed from the game immediately. This is used when
 * the server needs to terminate a client's connection abnormally.
 */
void N_TerminateNode(nodeid_t id)
{
	/*netnode_t *node = &netNodes[id];
	sqpack_t *i;*/
	netevent_t netEvent;

//	if(!node->sock) return; // There is nothing here...

	// Close the client's link.
	
/*	if(netServerMode && node->hasJoined)
	{
		// This causes a network event.
		netEvent.type = NE_CLIENT_EXIT;
		netEvent.id = id;
		N_NEPost(&netEvent);
		}*/
	
/*	// Remove the node from the set of active sockets.
	SDLNet_TCP_DelSocket(sockSet, node->sock);

	// Close the socket and forget everything about the node.
	SDLNet_TCP_Close(node->sock);

	// Remove the address binding from the incoming UDP socket.  This
	// means we'll reject all packets from the address.
	N_BindIncoming(NULL, id);

	// Cancel this node's packets in the send queue by setting their
	// node pointers to NULL.
	Sem_P(sendQ.mutex);
	for(i = sendQ.first; i; i = i->next)
		if(i->node == node) i->node = NULL;
	Sem_V(sendQ.mutex);

	// Clear the node's data.
	Sem_Destroy(node->mutex);
	memset(node, 0, sizeof(*node));*/
}

/*
 * Registers a new TCP socket as a client node.  There can only be a
 * limited number of nodes at a time.  This is only used by a server.
 */
static boolean N_RegisterNewSocket(TCPsocket sock)
{
/*	int i;
	netnode_t *node;

	// Find a free node.
	for(i = 1, node = netNodes + 1; i < MAX_NODES; i++, node++)
		if(!node->sock)
		{
			// This'll do.
			node->sock = sock;

			// We don't know the name yet.
			memset(node->name, 0, sizeof(node->name));

			// Add this socket to the set of client sockets.
			SDLNet_TCP_AddSocket(sockSet, sock);

			return true;
			}*/
	// There were no free nodes.
	return false;
}

/*
 * A network node wishes to become a real client. Returns true if we
 * allow this.
 */
static boolean N_JoinNode(nodeid_t id, Uint16 port, const char *name)
{
/*	netnode_t *node;
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
	node->hasJoined = true;

	// FIXME: We should use more discretion with the name. It has
	// been provided by an untrusted source.
	strncpy(node->name, name, sizeof(node->name) - 1);

	// Prepare the transmission stats for the node.
	node->numWaiting   = 0;
	node->bytesWaiting = 0;
	node->mutex        = Sem_Create(1);

	// Bind the address to the incoming UDP socket, so we'll recognize
	// the sender.
	N_BindIncoming(&node->addr, id);
	
	// Inform the higher levels of this occurence.
	netEvent.type = NE_CLIENT_ENTRY;
	netEvent.id   = id;
	N_NEPost(&netEvent);
*/
	return true;
}

/*
 * Start the search for a host.  Any responses are parsed at a later
 * point in time.  Returns true if there was a server that answered.
 */
boolean N_LookForHosts(void)
{
/*	TCPsocket sock;
	char buf[256];
	ddstring_t *response;
*/
	// We must be a client.
	if(!N_IsAvailable() || netServerMode) return false;

	// Try to connect to the server at the defined address.
	if(!N_OpenControlSocket(nptIPAddress, nptIPPort))
		return false;

	// Send a status information query, closing the socket afterwards.
	N_AskInfo(true);
	
	return true;
	
	// Get rid of previous findings.
	/*memset(&located, 0, sizeof(located));

	// Let's determine the address we will be looking into.
	SDLNet_ResolveHost(&located.addr, nptIPAddress, nptIPPort);

	// I say, anyone there?
	sock = SDLNet_TCP_Open(&located.addr);
	if(!sock)
	{
		Con_Message("N_LookForHosts: No reply from %s (port %i).\n",
					nptIPAddress, nptIPPort);
		return false;
		}*/

/*	// Send an INFO query.
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
		}*/
}

/*
 * Connect a client to the server identified with 'index'.  We enter
 * clientside mode during this routine.
 */
boolean N_Connect(int index)
{
/*	netnode_t *svNode;
	foundhost_t *host;
	char buf[128], *pName;

	if(!N_IsAvailable() || netServerMode || index != 0) return false;

	Demo_StopPlayback();

	// Call game DLL's NetConnect.
	gx.NetConnect(true);

	host = &located;
	
	// We'll use node number zero for all communications.
	svNode = &netNodes[0];
	if(!(svNode->sock = SDLNet_TCP_Open(&host->addr)))
	{
		N_IPToString(buf, &host->addr);
		Con_Message("N_Connect: No reply from %s.\n", buf);
		return false;
	}
	memcpy(&svNode->addr, &located.addr, sizeof(IPaddress));*/
	/*
	// Connect by issuing: "JOIN (myname)"
	pName = playerName;
	if(!pName || !pName[0]) pName = "Anonymous";
	sprintf(buf, "JOIN %s\n", pName);
	SDLNet_TCP_Send(svNode->sock, buf, strlen(buf));

	VERBOSE( Con_Printf("N_Connect: %s", buf) );
*/
	// What is the reply?
/*	memset(buf, 0, sizeof(buf));
	if(SDLNet_TCP_Recv(svNode->sock, buf, 64) <= 0 ||
	   strncmp(buf, "ENTER ", 6))
	{
		SDLNet_TCP_Close(svNode->sock);
		memset(svNode, 0, sizeof(svNode));
		Con_Message("N_Connect: Server refused connection.\n");
		if(buf[0]) Con_Message("  Reply: %s", buf);
		return false;
	}

	VERBOSE( Con_Message("  Server responds: %s", buf) );

	// The server tells us which UDP port we should send packets to.
	SDLNet_Write16(strtol(buf + 6, NULL, 16), &svNode->addr.port);

	// Bind the server's address to our incoming UDP port, so we'll
	// recognize the packets from the server.
	N_BindIncoming(&svNode->addr, 0);
	
	// Clients are allowed to send packets to the server.
	svNode->hasJoined = true;
*/
	handshakeReceived = false;
	netgame = true; // Allow sending/receiving of packets.
	isServer = false;
	isClient = true;
	
	// Call game's NetConnect.
	gx.NetConnect(false);
	
	// G'day mate!  The client is responsible for beginning the
	// handshake.
	Cl_SendHello();
	return true;
}

/*
 * Disconnect from the server.
 */
boolean N_Disconnect(void)
{
/*	netnode_t *svNode;*/
	
	if(!N_IsAvailable()) return false;
	
	// Tell the Game that a disconnecting is about to happen.
	if(gx.NetDisconnect) gx.NetDisconnect(true);
	
	Net_StopGame();
	//N_ClearMessages();

	// Tell the Game that disconnecting is now complete.
	if(gx.NetDisconnect) gx.NetDisconnect(false);

	// This'll prevent the sending of further packets.
/*	svNode = &netNodes[0];
	svNode->hasJoined = false;
	N_BindIncoming(NULL, 0);*/

	// Close the control connection.  This will let the server know
	// that we are no more.
/*	SDLNet_TCP_Close(svNode->sock);*/
	
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


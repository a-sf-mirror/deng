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
 * sys_netlink.c: Network Communications
 *
 * The lowest layer of the network code.  Multi-Channel Reliable
 * Communications.  All communication between the server and the
 * clients is carried out with these.
 *
 * A link (netlink_t) is composed of two or more channels
 * (netchannel_t).  A server maintains a separate link for each
 * client.  Each client has a single link for communicating with the
 * server.
 *
 * There are several threads running here.
 *
 * Assumes Little Endian! (i.e. Intel)
 */

// HEADER FILES ------------------------------------------------------------

#include <SDL_net.h>
#include <string.h>
#include <errno.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

#define STREAM_BUFFER_LEN 	0x10000
#define MAX_CHANNELS		4
#define NUM_LINKS			DDMAXPLAYERS
#define SEND_BUFFER_SIZE	0x10000	// 64 KB buffer for data to be sent.
#define RECV_BUFFER_SIZE 	0x10000	// 64 KB receive buffer.

// TYPES -------------------------------------------------------------------

typedef struct bytestream_s {
	byte buffer[STREAM_BUFFER_LEN];
	unsigned int head, tail;
	int mutex;					// Lock before accessing the buffer.
} bytestream_t;

/*
 * Each network channel represents a TCP connection.
 */
typedef struct netchannel_s
{
	// When this is set to false, the channel thread will exit.
	volatile boolean enabled;
	
	// Each channel is maintained by two threads.  These are zero if
	// the channel is not being used.
	int sender;
	int receiver;
	
	// Outgoing data stream, waiting to be sent.
	bytestream_t *out;
	
	TCPsocket socket;

	// A large(ish) buffers for storing transmitted data.
	byte *sendBuffer;
	byte *recvBuffer;

	// Pointer of the link that own this channel.
	struct netlink_s *link;
}
netchannel_t;

/*
 * Links connect the server and the clients.
 */
typedef struct netlink_s
{
	// Incoming data streams (aggregated from all channels).
	bytestream_t in;

	// There are always at least two channels per link.
	netchannel_t channels[MAX_CHANNELS];
}
netlink_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void NC_CloseLink(int idx);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int NC_SendThread(void *parm);
static int NC_RecvThread(void *parm);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static netlink_t *netLinks[NUM_LINKS];

// CODE --------------------------------------------------------------------

/*
 * Initialize the MCRC system.
 */
void NC_Init(void)
{
}

/*
 * Shutdown the MCRC system.
 */
void NC_Shutdown(void)
{
	int i;
	
	// Shut down all the links.
	for(i = 0; i < NUM_LINKS; i++)
	{
		NC_CloseLink(i);
	}			
}

/*
 * Initialize a byte stream.
 */
void NC_StreamInit(bytestream_t *s)
{
	memset(s, 0, sizeof(*s));

	// Create a mutex for protecting the buffer.
	s->mutex = Sys_CreateMutex();
}

void NC_StreamFree(bytestream_t *s)
{
	Sys_DestroyMutex(s->mutex);
}

/*
 * Read data from the stream buffer.  Lock the stream before calling!
 */
void NC_StreamBufferRead(bytestream_t *s, byte *data, ushort length)
{
	// Are the bytes to be read in a contiguous memory area?
	if(s->tail + length <= STREAM_BUFFER_LEN)
	{
		memcpy(data, s->buffer + s->tail, length);
	}
	else
	{
		uint breakPoint = STREAM_BUFFER_LEN - s->tail;

		// Copy the beginning.
		memcpy(data, s->buffer + s->tail, breakPoint);

		// ...and the end.
		memcpy(data + breakPoint, s->buffer, length - breakPoint);
	}

	// Advance the tail.
	s->tail = (s->tail + length) % STREAM_BUFFER_LEN;
}

/* 
 * Write data to the stream buffer.  Lock the stream before calling!
 */
void NC_StreamBufferWrite(bytestream_t *s, byte *data, ushort length)
{
	// Is there enough room in the buffer to fit the data in a
	// contiguous memory area?
	if(s->head + length <= STREAM_BUFFER_LEN)
	{
		// Just copy it, then.
		memcpy(s->buffer + s->head, data, length);
	}
	else
	{
		uint breakPoint = STREAM_BUFFER_LEN - s->head;
		
		// Copy the beginning.
		memcpy(s->buffer + s->head, data, breakPoint);

		// ...and the end.
		memcpy(s->buffer, data + breakPoint, length - breakPoint);
	}

	// Advance the head.
	s->head = (s->head + length) % STREAM_BUFFER_LEN;
}

/*
 * Returns the size of the unused area of the byte stream buffer.
 * Locking the stream before calling this function is advised!
 */
uint NC_StreamGetFree(bytestream_t *s)
{
	if(s->head == s->tail)
	{
		// The entire buffer is empty.
		return STREAM_BUFFER_LEN - 1;
	}

	// Count bytes from head to tail.
	if(s->head > s->tail)
	{
		return STREAM_BUFFER_LEN - s->head + s->tail - 1;
	}
	else
	{
		return STREAM_BUFFER_LEN - s->tail + s->head - 1;
	}		
}

/*
 * Gets the next packet from the byte stream, if there is one available.
 * 'data' should be large enough for 64 KB packets.
 */
ushort NC_StreamGetPacket(bytestream_t *s, byte *data)
{
	ushort length = 0;

	// Streams are protected by mutexes because they are accessed from
	// both the channel threads and the main thread.
	Sys_Lock(s->mutex);

	// Is there something in the buffer?
	if(s->head != s->tail)
	{
		byte header[2];

		// The length of the packet is restricted to 16 bits.
		NC_StreamBufferRead(s, header, 2);
		length = header[0] + (header[1] << 8);

		// Read the data of the packet.
		NC_StreamBufferRead(s, data, length);
	}

	Sys_Unlock(s->mutex);

	return length;
}

/*
 * Write a packet to the byte stream.
 */
void NC_StreamPutPacket(bytestream_t *s, byte *data, ushort length)
{
	byte header[2];
	
	// Streams are protected by mutexes because they are accessed from
	// both the channel threads and the main thread.
	Sys_Lock(s->mutex);

	// We don't want the stream buffer to overflow.
	if(NC_StreamGetFree(s) < length + 2)
	{
		Sys_Unlock(s->mutex);
		
		// We must die!  (Or block until there is enough room?)
		Con_Error("NC_StreamPutPacket: Out of buffer space.\n");
		return;
	}

	// First write the length of the packet.
	header[0] = length & 0xff;
	header[1] = (length >> 8) & 0xff;
	NC_StreamBufferWrite(s, header, 2);

	// Then write the rest of the packet.
	NC_StreamBufferWrite(s, data, length);
	
	Sys_Unlock(s->mutex);
}

/*
 * Returns true if the byte stream is empty.
 */
boolean NC_StreamIsEmpty(bytestream_t *s)
{
	boolean isEmpty;
	
	Sys_Lock(s->mutex);
	isEmpty = (s->head == s->tail);
	Sys_Unlock(s->mutex);

	return isEmpty;
}

/*
 * Create a new channel that will be associated with the given socket.
 * If we return false ("DENIED!"), the caller should close the socket
 * and forget about it.
 */
boolean NC_OpenChannel(int linkIdx, void *socket)
{
	netlink_t *link = netLinks[linkIdx];
	netchannel_t *chan = NULL;
	int i;

	// Sanity check.
	if(!link)
	{
		Con_Error("NC_OpenChannel: Link %i is not open.\n", linkIdx);
	}
	
	// Look for an unused channel.
	for(i = 0; i < MAX_CHANNELS; i++)
	{
		if(!link->channels[i].sender)
		{
			// This is unused.
			chan = link->channels + i;
			break;
		}
	}
	if(!chan) return false;

	chan->socket = socket;

	// Allocate a buffer for outgoing data.
	chan->sendBuffer = malloc(SEND_BUFFER_SIZE);
	
	// Allocate a buffer for incoming data.
	chan->recvBuffer = malloc(RECV_BUFFER_SIZE);

	// A stream for outgoing data.
	chan->out = malloc(sizeof(bytestream_t));
	NC_StreamInit(chan->out);

	// Enable the channel and start the threads.
	chan->enabled  = true;
	chan->receiver = Sys_StartThread(NC_RecvThread, chan, 0);
	chan->sender   = Sys_StartThread(NC_SendThread, chan, 0);

	VERBOSE( Con_Message("NC_OpenChannel: Link %i, channel %i.\n",
						 linkIdx, chan - link->channels) );
	
	// We're in business.
	return true;
}

/*
 * Blocks until the channel's threads have been stopped.
 */
void NC_CloseChannel(netchannel_t *chan)
{
	// Tell the threads to stop.
	chan->enabled = false;

	// Wait for both threads to exit.
	Sys_WaitThread(chan->sender);
	Sys_WaitThread(chan->receiver);
	chan->sender = chan->receiver = 0;

	// Outgoing data buffer.
	NC_StreamFree(chan->out);
	free(chan->out);
	chan->out = NULL;

	SDLNet_TCP_Close(chan->socket);
	chan->socket = NULL;

	// Buffer for outgoing data.
	free(chan->sendBuffer);
	chan->sendBuffer = NULL;
	
	// Buffer for received data.
	free(chan->recvBuffer);
	chan->recvBuffer = NULL;
}

/*
 * Opening a link initializes the link data structures, but doesn't
 * open any channels.  Clients are responsible for opening as many
 * channels as they want (within our limits, of course).
 */
void NC_OpenLink(int idx)
{
	netlink_t *link;
	int i;

	if(netLinks[idx])
	{
		// A link has already been opened!
		Con_Error("NC_OpenLink: Link %i already open.\n", idx);
	}

	// Allocate a new link.
	link = netLinks[idx] = calloc(sizeof(netlink_t), 1);

	// Initialize the incoming data stream.
	NC_StreamInit(&link->in);

	// Set the channels' pointers back to the link.
	for(i = 0; i < MAX_CHANNELS; i++)
	{
		link->channels[i].link = link;
	}
}

/*
 * Closing a link shuts down all its open channels, too.
 */
void NC_CloseLink(int idx)
{
	netlink_t *link = netLinks[idx];
	netchannel_t *chan;
	int i;
	
	if(link == NULL)
	{
		// No links here.
		return;
	}

	// Disable all channels.
	for(i = 0; i < MAX_CHANNELS; i++)
		link->channels[i].enabled = false;
	
	// Wait for running channels to stop.
	for(i = 0, chan = link->channels; i < MAX_CHANNELS; i++, chan++)
	{
		if(!chan->sender) continue;
		NC_CloseChannel(chan);
	}
	
	NC_StreamFree(&link->in);

	// The link has been shut down.
	netLinks[idx] = NULL;
}

/*
 * Sends the length of a packet.  Returns true if no errors occur.
 */
static boolean NC_SendLength(TCPsocket sock, ushort length)
{
	byte normalSize;
	byte extended[3];
	
	if(length < 0xff)
	{
		// A single byte is enough.
		normalSize = length;
		if(SDLNet_TCP_Send(sock, &normalSize, 1) < 1)
			return false;
	}
	else
	{
		// Send an extended length instead.
		extended[0] = 0xff;
		extended[1] = length & 0xff;
		extended[2] = (length >> 8) & 0xff;
		if(SDLNet_TCP_Send(sock, extended, 3) < 3)
			return false;
	}
	return true;
}

/*
 * Reads the length of an incoming packet.  It is either a short 8-bit
 * length or an extended 16-bit length (3 bytes).
 */
static uint NC_RecvLength(TCPsocket sock)
{
	byte normalSize = 0;
	
	// Most packets are short enough to have an 8-bit length.
	if(SDLNet_TCP_Recv(sock, &normalSize, 1) < 1)
		return 0;

	// The length 255 is reserved for extra long packets.
	if(normalSize == 0xff)
	{
		byte extended[2];
		
		// The actual length is now an unsigned 16-bit interer.
		if(SDLNet_TCP_Recv(sock, extended, 2) < 2)
			return 0;
		
		return extended[0] + (extended[1] << 8);
	}
	return normalSize;
}

/*
 * Channel send thread.  There is one instance of this thread running
 * per each active communications channel.
 */
static int NC_SendThread(void *parm)
{
	netchannel_t *chan = parm;
	int size;

	while(chan->enabled)
	{
		// Retrieve the next packet from the the outgoing buffer and
		// send it over the TCP socket.
		if((size = NC_StreamGetPacket(chan->out, chan->sendBuffer)) > 0)
		{
			if(!NC_SendLength(chan->socket, size))
			{
				// There was an error!
				fprintf(outFile, "NC_ChannelThread: %s\n", SDLNet_GetError());
				// FIXME: Close down this channel.
			}

			// Send the data of the packet.
			if(SDLNet_TCP_Send(chan->socket, chan->sendBuffer, size) < size)
			{
				// FIXME: Close down this channel.
			}
		}

		if(NC_StreamIsEmpty(chan->out))
		{
			// Suck a delta from our client's pool.  A delta can be
			// added to the outgoing buffer only if there is room for
			// it.  We don't want to saturate the channel, that would
			// increase latency.
			
			{
				// If we did get a new delta, send it immediately.
				continue;
			}
		}

		// If the outgoing stream is empty, let's take a little nap so
		// others can do something effective.
		if(NC_StreamIsEmpty(chan->out))
		{
			Sys_Sleep(5);
		}
	}

	// Exit from the thread.
	return 0;
}

/*
 * Channel receive thread.  There is one instance of this thread
 * running per each active communications channel.
 */
static int NC_RecvThread(void *parm)
{
#define SELECT_TIMEOUT 100 // ms
	netchannel_t *chan = parm;
	int size, numReady;
	SDLNet_SocketSet set;

	// Create a socket set for our socket.
	set = SDLNet_AllocSocketSet(1);
	SDLNet_TCP_AddSocket(set, chan->socket);

	while(chan->enabled)
	{
		// Don't wait for activity.  
		if(!(numReady = SDLNet_CheckSockets(set, SELECT_TIMEOUT)))
			continue;
			
		if(numReady == -1)
		{
			// Some kind of an error has occured.
			fprintf(outFile, "NC_ChannelThread: %s\n", SDLNet_GetError());
			fprintf(outFile, "  System error message: %s\n", strerror(errno));

			// FIXME: Close down this channel.
			break;
		}
			
		if((size = NC_RecvLength(chan->socket)) == 0)
		{
			// FIXME: Close down this channel.
			break;
		}

		// Read the data of the packet.
		if(SDLNet_TCP_Recv(chan->socket, chan->recvBuffer, size) < size)
		{
			// The transmission was interrupted.
			// FIXME: Close down this channel.
			Con_Message("NC_RecvThread: Interrupted packet.\n");
			break;
		}

		// Add the received data to the incoming data stream
		// buffer.
		NC_StreamPutPacket(&chan->link->in, chan->recvBuffer, size);
	}

	// Free the resources we allocated when the thread was started.
	SDLNet_FreeSocketSet(set);

	// Exit from the thread.
	return 0;
}

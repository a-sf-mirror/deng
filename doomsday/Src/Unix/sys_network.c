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
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

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
serviceprovider_t gCurrentProvider = NSP_NONE;
boolean		netServerMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
}

unsigned int N_GetServiceProviderCount(serviceprovider_t type)
{
	return 0;
}

boolean	N_GetServiceProviderName(serviceprovider_t type,
								 unsigned int index, char *name,
								 int maxLength)
{
	return false;
}

/*
 * Initialize the chosen service provider each in server or client
 * mode.  If a service provider has already been initialized, it will
 * be shut down first.  Returns true if successful.
 */
boolean N_InitService(serviceprovider_t provider, boolean inServerMode)
{
	return false;
}

void N_ShutdownService(void)
{
}

/*
 * Free a message buffer.
 */
void N_ReturnBuffer(void *handle)
{
	// This is analogous the DirectPlay buffer release, but what was
	// it that it did, again?
}

/*
 * Returns true if the low-level network routines have been initialized
 * and are expected to be working.
 */
boolean	N_IsAvailable(void)
{
	return false;
}

/*
 * Returns true if the internet is available.
 */
boolean N_UsingInternet(void)
{
	return false;
}

boolean	N_GetHostInfo(int index, struct serverinfo_s *info)
{
	return false;
}

int N_GetHostCount(void)
{
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

const char *N_GetProtocolName(void)
{
	return "TCP/IP";
}

/*
 * Returns the player name associated with the given network node.
 */
boolean N_GetNodeName(nodeid_t id, char *name)
{
	strcpy(name, "-unknown-");
	return false;
}

/*
 * The client is removed from the game without delay. This is used
 * when the server needs to terminate a client's connection
 * abnormally.
 */
void N_TerminateNode(nodeid_t id)
{
}

boolean N_LookForHosts(void)
{
	return false;
}

boolean N_Connect(int index)
{
	return false;
}

boolean N_Disconnect(void)
{
	return false;
}

boolean N_ServerOpen(void)
{
	return false;
}

boolean N_ServerClose(void)
{
	return false;
}

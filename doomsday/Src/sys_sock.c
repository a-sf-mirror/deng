
//**************************************************************************
//**
//** $Id$
//**
//** Handling of sockets.
//**
//** $Log$
//** Revision 1.1.2.1  2004/05/16 10:01:35  skyjake
//** Merged good stuff from branch-nix for the final 1.7.15
//**
//** Revision 1.1.4.1  2003/11/22 18:01:04  skyjake
//** Compiles with gcc and -DUNIX
//**
//** Revision 1.1  2003/03/09 16:03:51  skyjake
//** New master server mechanism
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#	include <winsock.h>
#endif

#ifdef UNIX
#	include "de_platform.h"
#	include <unistd.h>
#	include <sys/socket.h>
#	include <sys/types.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include "sys_sock.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * N_SockInit
 *	Called from N_Init().
 */
void N_SockInit(void)
{
#ifdef WIN32
	WSADATA wsaData;
	WSAStartup( MAKEWORD(1,1), &wsaData );
	// FIXME: Check the result... (who cares?)
#endif
}

/*
 * N_SockShutdown
 *	Called from N_Shutdown().
 */
void N_SockShutdown(void)
{
#ifdef WIN32
	WSACleanup();
#endif
}

/*
 * N_SockPrintf
 *	Don't print too long messages with one call.
 */
void N_SockPrintf(socket_t s, const char *format, ...)
{
	char buf[512];
	int length;
	va_list args;

	// Print the message into the buffer.
	va_start(args, format);
	length = vsprintf(buf, format, args);
	va_end(args);

	if(length > sizeof(buf)) 
	{
		// Oops... Something important may have been overwritten in memory.
		length = sizeof(buf);
	}

	// Send it.
	send(s, buf, length, 0);
}

/*
 * N_SockGetHost
 */
struct hostent *N_SockGetHost(const char *hostName)
{
	struct sockaddr_in addr;
	struct hostent *host;

	// Clear the socket address.
    memset(&addr, 0, sizeof(addr));

	// Is it an IP address or name?
    addr.sin_addr.s_addr = inet_addr(hostName);

	// Get host information.
    if(addr.sin_addr.s_addr == INADDR_NONE) 
	{
		// Try to get using DNS.
        if((host = gethostbyname(hostName)) == NULL) 
		{
            //perror("Could not get host by name");
            return NULL;
        }
    } 
	else 
	{
		// Try to get using the IP address.
        if((host = gethostbyaddr((const char*)&addr.sin_addr, 
			sizeof(struct sockaddr_in), AF_INET)) == NULL)
        {
            //perror("Could not get host by address");
            return NULL;
        }
    }
	return host;
}

/*
 * N_SockNewStream
 */
socket_t N_SockNewStream(void)
{
	return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

/*
 * N_SockConnect
 */
boolean N_SockConnect(socket_t s, struct hostent *host, unsigned short port)
{
	struct sockaddr_in addr;

	// The address to connect to.
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr   = *((struct in_addr*) *host->h_addr_list);
	// 80 is the default port.
	addr.sin_port   = htons(port);

	// Let's try connecting.
	return connect(s, (struct sockaddr*) &addr, sizeof(struct sockaddr)) 
		!= SOCKET_ERROR;
}

/*
 * N_SockClose
 */
void N_SockClose(socket_t s)
{
#ifdef WIN32
	closesocket(s);
#elif defined UNIX
	close(s);
#endif
}


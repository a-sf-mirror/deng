//===========================================================================
// SYS_NETWORK.H
//===========================================================================
#ifndef __DOOMSDAY_SYSTEM_NETWORK_H__
#define __DOOMSDAY_SYSTEM_NETWORK_H__

#include "dd_share.h"
#include "net_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "con_decl.h"

typedef enum serviceprovider_e {
	NSP_NONE,
	NSP_TCPIP,
	NSP_IPX,
	NSP_MODEM,
	NSP_SERIAL,
	NUM_NSP
} serviceprovider_t;

// If a master action fails, the action queue is emptied.
typedef enum {
	MAC_REQUEST,	// Retrieve the list of servers from the master.
	MAC_WAIT,		// Wait for the server list to arrive.
	MAC_LIST		// Print the server list in the console.
} masteraction_t;

extern boolean	allowSending;
extern int		maxQueuePackets;

extern serviceprovider_t gCurrentProvider;
extern boolean	netServerMode;   

extern int		nptActive;
extern char		*nptIPAddress;
extern int		nptIPPort;
extern int		nptModem;	
extern char		*nptPhoneNum;
extern int		nptSerialPort;
extern int		nptSerialBaud;
extern int		nptSerialStopBits;
extern int		nptSerialParity;
extern int		nptSerialFlowCtrl;

extern char		*serverName, *serverInfo, *playerName;
extern int		serverData[];

extern char		*masterAddress;
extern int		masterPort;
extern char		*masterPath;

void	N_SystemInit(void);
void	N_SystemShutdown(void);
boolean	N_InitService(serviceprovider_t provider, boolean inServerMode);
void	N_ShutdownService(void);
boolean	N_IsAvailable(void);
boolean N_UsingInternet(void);
boolean N_LookForHosts(void);

boolean N_Connect(int index);
boolean N_Disconnect(void);
boolean N_ServerOpen(void);
boolean N_ServerClose(void);
	
void 	N_SendDataBuffer(void *data, uint size, nodeid_t destination);
void	N_ReturnBuffer(void *handle);
uint	N_GetSendQueueCount(int player);
uint	N_GetSendQueueSize(int player);
void 	N_TerminateNode(nodeid_t id);

boolean N_GetNodeName(nodeid_t id, char *name);
const char* N_GetProtocolName(void);

int		N_GetHostCount(void);
boolean	N_GetHostInfo(int index, struct serverinfo_s *info);
unsigned int N_GetServiceProviderCount(serviceprovider_t type);
boolean	N_GetServiceProviderName(serviceprovider_t type,
								 unsigned int index, char *name,
								 int maxLength);

#ifdef __cplusplus
}
#endif

#endif 


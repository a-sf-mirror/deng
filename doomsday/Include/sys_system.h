#ifndef __DOOMSDAY_SYSTEM_H__
#define __DOOMSDAY_SYSTEM_H__

#include "dd_types.h"

typedef int (*systhreadfunc_t)(void *parm);

extern boolean novideo;
extern int systics;

void	Sys_Init(void);
void	Sys_Shutdown(void);
void	Sys_Quit(void);
int		Sys_CriticalMessage(char *msg);
void	Sys_Sleep(int millisecs);
void	Sys_ShowCursor(boolean show);
void	Sys_HideMouse(void);
void	Sys_MessageBox(const char *msg, boolean iserror);
void	Sys_OpenTextEditor(const char *filename);
void	Sys_ShowWindow(boolean hide);
int		Sys_StartThread(systhreadfunc_t startpos, void *parm, int priority);
void	Sys_SuspendThread(int handle, boolean dopause);
int 	Sys_CreateMutex(const char *name); // returns the mutex handle
void 	Sys_DestroyMutex(int mutexHandle);
void 	Sys_Lock(int mutexHandle);
void 	Sys_Unlock(int mutexHandle);

#endif

//===========================================================================
// SV_FRAME.H
//===========================================================================
#ifndef __DOOMSDAY_SERVER_FRAME_H__
#define __DOOMSDAY_SERVER_FRAME_H__

extern int bwrAdjustTime;

void	Sv_TransmitFrame(void);
int		Sv_GetMaxFrameSize(int playerNumber);

#endif 

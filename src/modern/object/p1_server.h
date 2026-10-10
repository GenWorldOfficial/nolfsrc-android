// ----------------------------------------------------------------------- //
//
// MODULE  : p1_server.h
//
// PURPOSE : The modern build's package P1 (MODERN.md), server side:
//           M-P1-02, the 60 fps limiter on multiplayer servers.
//
//           Compiled only in the modern build (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P1_SERVER_H__
#define __P1_SERVER_H__

void	P1Server_Init();										// a new server shell
void	P1Server_OnDontLockMessage(HCLIENT hSender, HMESSAGEREAD hMessage);	// MID_DONT_LOCK_SERVER_FPS
void	P1Server_EndUpdate(bool bMultiplayer);					// end of each server update

#endif // __P1_SERVER_H__

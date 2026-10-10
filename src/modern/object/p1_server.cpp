// ----------------------------------------------------------------------- //
//
// MODULE  : p1_server.cpp
//
// PURPOSE : Package P1 (MODERN.md), M-P1-02: the server's frame limiter.
//
//           A multiplayer server running unlimited updates too fast for
//           NOLF's timing, as the client does (M-P1-01), so each server
//           update in a multiplayer game ends by waiting until 1/60 s has
//           passed since the previous one.
//
//           A listen server shares its loop with the hosting client,
//           which limits it already; that client sends
//           MID_DONT_LOCK_SERVER_FPS, and the server stops limiting.  Only
//           the local client (the host itself) is listened to: a remote
//           player cannot unlock a server.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include <mmsystem.h>
#include "object/p1_server.h"

namespace
{
	const int		kUpdatesPerSecond	= 60;

	bool			s_bTimerInit		= false;
	LARGE_INTEGER	s_liFreq;			// counts per second (0: no counter)
	bool			s_bLock				= true;
	bool			s_bHaveLast			= false;
	LARGE_INTEGER	s_liLast;			// when the previous update was let go
}

void P1Server_Init()
{
	if (!s_bTimerInit)
	{
		s_bTimerInit = true;
		if (!QueryPerformanceFrequency(&s_liFreq) || s_liFreq.QuadPart <= 0)
			s_liFreq.QuadPart = 0;

		// 1 ms sleeps (a dedicated server has no client to have asked)
		timeBeginPeriod(1);
	}

	s_bLock = true;
	s_bHaveLast = false;
}

void P1Server_OnDontLockMessage(HCLIENT hSender, HMESSAGEREAD hMessage)
{
	uint8 nUnlock = g_pLTServer->ReadFromMessageByte(hMessage);

	if (!hSender || !(g_pLTServer->GetClientInfoFlags(hSender) & CIF_LOCAL))
		return;

	if (nUnlock)
	{
		s_bLock = false;
		s_bHaveLast = false;
	}
}

void P1Server_EndUpdate(bool bMultiplayer)
{
	if (!bMultiplayer || !s_bLock || !s_liFreq.QuadPart)
	{
		s_bHaveLast = false;
		return;
	}

	LARGE_INTEGER liNow;
	QueryPerformanceCounter(&liNow);

	if (!s_bHaveLast)
	{
		s_liLast = liNow;
		s_bHaveLast = true;
		return;
	}

	LONGLONG nPeriod = s_liFreq.QuadPart / kUpdatesPerSecond;
	LONGLONG nTarget = s_liLast.QuadPart + nPeriod;

	while (liNow.QuadPart < nTarget)
	{
		LONGLONG nLeftMs = ((nTarget - liNow.QuadPart) * 1000) / s_liFreq.QuadPart;
		Sleep(nLeftMs > 2 ? 1 : 0);
		QueryPerformanceCounter(&liNow);
	}

	// keep the cadence unless far behind
	if (liNow.QuadPart - nTarget > nPeriod)
		s_liLast = liNow;
	else
		s_liLast.QuadPart = nTarget;
}

// ----------------------------------------------------------------------- //
//
// MODULE  : p1_window.cpp
//
// PURPOSE : Package P1 (MODERN.md), M-P1-06: the game window.
//
//           The retail game only sizes its window once (HookWindow, full
//           screen) and leaves windowed mode to the renderer, which
//           centres the window wrongly (its vertical offset is computed
//           from the widths) and sizes it for a frame style the window
//           does not have.  After every mode change, and whenever the
//           engine may have moved the window (renderer re-initialised,
//           focus regained), this puts it right:
//
//             full screen          what HookWindow does: no frame, topmost,
//                                  the whole screen (only needed after
//                                  having been windowed)
//             windowed             a title bar and the mode's size as the
//                                  client area, centred in the work area
//             windowed at the      no frame at all, covering the screen
//             desktop's size       ("borderless windowed")
//
//           A window already right (the user may have moved it) is left
//           where it is.
//
//           Also the client-event hook for all of P1 (P1_OnClientEvent).
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "ClientUtilities.h"
#include "GameClientShell.h"
#include "cshell/p1_modern.h"

extern HWND		g_hMainWnd;		// GameClientShell.cpp
extern RECT*	g_prcClip;		// GameClientShell.cpp: the retail full-screen clip
extern CGameClientShell* g_pGameClientShell;

namespace
{
	const DWORD kFramedStyle		= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
	const DWORD kBorderlessStyle	= WS_POPUP | WS_VISIBLE;
	const DWORD kFullScreenStyle	= WS_VISIBLE;		// as HookWindow sets it

	// What the window was last made, so that a full-screen window that was
	// never windowed is left to the retail code.
	enum Placement
	{
		PLACED_NONE,
		PLACED_FULLSCREEN,
		PLACED_FRAMED,
		PLACED_BORDERLESS
	};

	Placement	s_ePlaced	= PLACED_NONE;
	uint32		s_nWidth	= 0;		// the mode it was placed for
	uint32		s_nHeight	= 0;
}

void P1Window_Apply()
{
	if (!g_hMainWnd)
		return;

	RMode mode;
	if (g_pLTClient->GetRenderMode(&mode) != LT_OK || !mode.m_Width || !mode.m_Height)
		return;

	bool bModeChanged = (mode.m_Width != s_nWidth || mode.m_Height != s_nHeight);
	s_nWidth  = mode.m_Width;
	s_nHeight = mode.m_Height;

	if (GetConsoleInt("Windowed", 0) != 1)
	{
		if (s_ePlaced == PLACED_FRAMED || s_ePlaced == PLACED_BORDERLESS)
		{
			// back from a window: what HookWindow does at start-up
			SetWindowLong(g_hMainWnd, GWL_STYLE, kFullScreenStyle);
			SetWindowPos(g_hMainWnd, HWND_TOPMOST, 0, 0, mode.m_Width, mode.m_Height, SWP_FRAMECHANGED);
			if (g_prcClip)
			{
				GetWindowRect(g_hMainWnd, g_prcClip);
				ClipCursor(g_prcClip);
			}
		}
		if (s_ePlaced != PLACED_NONE)
			s_ePlaced = PLACED_FULLSCREEN;
		return;
	}

	// Windowed: the display is at the desktop's own mode.
	int nDeskW = GetSystemMetrics(SM_CXSCREEN);
	int nDeskH = GetSystemMetrics(SM_CYSCREEN);
	bool bBorderless = ((int)mode.m_Width >= nDeskW && (int)mode.m_Height >= nDeskH);
	DWORD dwStyle = bBorderless ? kBorderlessStyle : kFramedStyle;
	Placement ePlace = bBorderless ? PLACED_BORDERLESS : PLACED_FRAMED;

	RECT rcWant;
	SetRect(&rcWant, 0, 0, mode.m_Width, mode.m_Height);
	AdjustWindowRect(&rcWant, dwStyle, FALSE);
	int cx = rcWant.right - rcWant.left;
	int cy = rcWant.bottom - rcWant.top;

	RECT rcNow;
	GetWindowRect(g_hMainWnd, &rcNow);
	DWORD dwNow = (DWORD)GetWindowLong(g_hMainWnd, GWL_STYLE);
	bool bRight = !bModeChanged && s_ePlaced == ePlace &&
		(dwNow & (WS_POPUP | WS_CAPTION)) == (dwStyle & (WS_POPUP | WS_CAPTION)) &&
		rcNow.right - rcNow.left == cx && rcNow.bottom - rcNow.top == cy;
	if (bRight)
		return;

	int x = 0, y = 0;
	if (!bBorderless)
	{
		RECT rcWork;
		if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0))
			SetRect(&rcWork, 0, 0, nDeskW, nDeskH);

		x = rcWork.left + ((rcWork.right - rcWork.left) - cx) / 2;
		y = rcWork.top + ((rcWork.bottom - rcWork.top) - cy) / 2;
		if (x < rcWork.left) x = rcWork.left;
		if (y < rcWork.top) y = rcWork.top;
	}

	SetWindowLong(g_hMainWnd, GWL_STYLE, dwStyle);
	SetWindowPos(g_hMainWnd, HWND_NOTOPMOST, x, y, cx, cy, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	// The retail full-screen clip rectangle, which some interface code
	// still applies, must not hold the cursor in a window.
	if (g_prcClip)
	{
		SetRect(g_prcClip, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
			GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
			GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN));
		ClipCursor(NULL);
	}

	s_ePlaced = ePlace;
}

// CGameClientShell::OnEvent, after the retail handling and before the
// interface's.
void P1_OnClientEvent(uint32 dwEventID)
{
	switch (dwEventID)
	{
		case LTEVENT_RENDERINIT:
		{
			// the renderer (re)sized the window, and dropped our surfaces
			P1Window_Apply();
			if (g_pGameClientShell)
				g_pGameClientShell->GetScreenTintMgr()->P1Invalidate();
		}
		break;

		case LTEVENT_GAINEDFOCUS:
		{
			P1Window_Apply();
			P1Input_OnFocus(true);
		}
		break;

		case LTEVENT_LOSTFOCUS:
		{
			P1Input_OnFocus(false);
		}
		break;
	}
}

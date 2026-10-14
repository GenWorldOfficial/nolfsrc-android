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
//             borderless window    no frame at all, covering the screen,
//                                  at the desktop's own resolution
//
//           The window mode (Display options) is the engine's Windowed
//           (0 full screen, 1 a window: what the renderers read) and our
//           WindowMode (2: that window borderless).  A borderless window
//           is always at the desktop's resolution: when the render mode is
//           another (a command line's ++screenwidth, say), the mode is set
//           to the desktop's on the next frame (P1Window_Update).
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
#include "InterfaceMgr.h"
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
	bool		s_bWantDesktop = false;	// borderless at another mode: set the desktop's
	uint32		s_nTriedWidth	= 0;	// ... once for each mode it is at
	uint32		s_nTriedHeight	= 0;
}

int P1Window_GetMode()
{
	if (GetConsoleInt("Windowed", 0) != 1)
		return P1_WINDOW_FULLSCREEN;

	return GetConsoleInt("WindowMode", P1_WINDOW_WINDOWED) == P1_WINDOW_BORDERLESS ?
		P1_WINDOW_BORDERLESS : P1_WINDOW_WINDOWED;
}

void P1Window_SetMode(int nMode)
{
	WriteConsoleInt("Windowed", nMode != P1_WINDOW_FULLSCREEN ? 1 : 0);
	WriteConsoleInt("WindowMode", nMode);
}

// The desktop's own resolution -- not the screen's, which in full screen
// is the game's mode.
void P1Window_DesktopSize(int* pWidth, int* pHeight)
{
	DEVMODE dm;
	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);

	if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &dm) && dm.dmPelsWidth && dm.dmPelsHeight)
	{
		*pWidth  = (int)dm.dmPelsWidth;
		*pHeight = (int)dm.dmPelsHeight;
		return;
	}

	*pWidth  = GetSystemMetrics(SM_CXSCREEN);
	*pHeight = GetSystemMetrics(SM_CYSCREEN);
}

// CGameClientShell::Update, every frame: a borderless window's mode.
void P1Window_Update()
{
	if (!s_bWantDesktop)
		return;

	s_bWantDesktop = false;

	RMode mode;
	if (g_pLTClient->GetRenderMode(&mode) != LT_OK)
		return;

	int nDeskW, nDeskH;
	P1Window_DesktopSize(&nDeskW, &nDeskH);

	// once per mode, so a desktop mode the renderer refuses is not asked
	// for every frame
	if (mode.m_Width == s_nTriedWidth && mode.m_Height == s_nTriedHeight)
		return;
	s_nTriedWidth  = mode.m_Width;
	s_nTriedHeight = mode.m_Height;

	mode.m_Width  = (uint32)nDeskW;
	mode.m_Height = (uint32)nDeskH;

	// as Display options sets a mode (CFolderDisplay::SetRenderer)
	g_pInterfaceMgr->SetSwitchingRenderModes(LTTRUE);
	LTRESULT result = g_pLTClient->SetRenderMode(&mode);
	g_pInterfaceMgr->SetSwitchingRenderModes(LTFALSE);
	if (result != LT_OK)
		return;
	g_pInterfaceMgr->ScreenDimsChanged();

	char szCmd[64];
	sprintf(szCmd, "+screenwidth %d", nDeskW);
	g_pLTClient->RunConsoleString(szCmd);
	sprintf(szCmd, "+screenheight %d", nDeskH);
	g_pLTClient->RunConsoleString(szCmd);
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

	int nMode = P1Window_GetMode();

	if (nMode == P1_WINDOW_FULLSCREEN)
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
	int nDeskW, nDeskH;
	P1Window_DesktopSize(&nDeskW, &nDeskH);
	bool bBorderless = (nMode == P1_WINDOW_BORDERLESS);
	DWORD dwStyle = bBorderless ? kBorderlessStyle : kFramedStyle;

	// A borderless window covers the desktop; its mode is the desktop's.
	if (bBorderless && ((int)mode.m_Width != nDeskW || (int)mode.m_Height != nDeskH))
		s_bWantDesktop = true;
	Placement ePlace = bBorderless ? PLACED_BORDERLESS : PLACED_FRAMED;

	RECT rcWant;
	if (bBorderless)
		SetRect(&rcWant, 0, 0, nDeskW, nDeskH);
	else
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

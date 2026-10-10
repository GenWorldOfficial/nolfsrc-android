// ----------------------------------------------------------------------- //
//
// MODULE  : p1_input.cpp
//
// PURPOSE : Package P1 (MODERN.md), M-P1-03: raw mouse look.
//
//           The retail mouse look reads DirectInput axis offsets while the
//           engine re-centres the cursor every frame (CursorCenter 1),
//           which stutters on modern systems.  Here the camera reads the
//           mouse's own relative motion from raw input (WM_INPUT on the
//           game window, which the client shell already subclasses), and
//           while the game has the mouse the cursor is hidden and confined
//           to the window; menus get it back.
//
//             OldMouseLook 1   the retail path, unchanged
//             NoRawInput 1     no raw input: the cursor's own motion,
//                              warped back to the window's centre each
//                              frame (also the fallback when raw input is
//                              not available)
//
//           Scale: the retail sensitivity formula (GameSettings.cpp,
//           ImplementMouseSensitivity), halved, per mouse count.
//
//           Only the mouse is read here: in the new mode, joystick look
//           (which the retail axis offsets included) is not read.
//
//           VC6's headers predate raw input, so its types are declared
//           here and its two functions are looked up in user32.dll.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "ClientUtilities.h"
#include "cshell/p1_modern.h"

extern HWND		g_hMainWnd;		// GameClientShell.cpp
extern RECT*	g_prcClip;		// GameClientShell.cpp: the retail full-screen clip

namespace
{
	// ------------------------------------------------------------------ //
	// Raw input, as winuser.h (Windows XP and later) declares it.

	#define P1_WM_INPUT					0x00FF
	#define P1_RID_INPUT				0x10000003
	#define P1_RIM_TYPEMOUSE			0
	#define P1_RIDEV_REMOVE				0x00000001
	#define P1_MOUSE_MOVE_ABSOLUTE		0x01
	#define P1_MOUSE_VIRTUAL_DESKTOP	0x02
	#define P1_HID_USAGE_PAGE_GENERIC	0x01
	#define P1_HID_USAGE_GENERIC_MOUSE	0x02

	struct P1RawInputDevice				// RAWINPUTDEVICE
	{
		USHORT	usUsagePage;
		USHORT	usUsage;
		DWORD	dwFlags;
		HWND	hwndTarget;
	};

	struct P1RawInputHeader				// RAWINPUTHEADER
	{
		DWORD	dwType;
		DWORD	dwSize;
		HANDLE	hDevice;
		WPARAM	wParam;
	};

	struct P1RawMouse					// RAWMOUSE
	{
		USHORT	usFlags;
		USHORT	usPad;					// the union below is 4-aligned
		ULONG	ulButtons;				// usButtonFlags, usButtonData
		ULONG	ulRawButtons;
		LONG	lLastX;
		LONG	lLastY;
		ULONG	ulExtraInformation;
	};

	struct P1RawInputMouse				// RAWINPUT, mouse arm
	{
		P1RawInputHeader	header;
		P1RawMouse			mouse;
	};

	typedef BOOL (WINAPI *PFNREGISTERRAWINPUTDEVICES)(const P1RawInputDevice* pDevices, UINT nDevices, UINT cbSize);
	typedef UINT (WINAPI *PFNGETRAWINPUTDATA)(HANDLE hRawInput, UINT uiCommand, LPVOID pData, UINT* pcbSize, UINT cbSizeHeader);

	PFNREGISTERRAWINPUTDEVICES	s_pfnRegisterRawInputDevices	= NULL;
	PFNGETRAWINPUTDATA			s_pfnGetRawInputData			= NULL;
	bool						s_bLookedUp						= false;
	HWND						s_hRawWnd						= NULL;	// registered for

	// ------------------------------------------------------------------ //

	enum Mode
	{
		MODE_OLD,		// OldMouseLook: retail axis offsets
		MODE_RAW,		// WM_INPUT
		MODE_CURSOR		// cursor motion, warped back to the centre
	};

	bool	s_bCaptured		= false;	// the game has the mouse (no cursor)
	Mode	s_eMode			= MODE_OLD;	// how it is read while captured
	LONG	s_nDX			= 0;		// raw counts since the last read
	LONG	s_nDY			= 0;
	bool	s_bHaveAbs		= false;	// absolute devices (tablets, remote
	LONG	s_nAbsX			= 0;		// desktops): the last position, to
	LONG	s_nAbsY			= 0;		// turn into motion

	bool LookUpRawInput()
	{
		if (!s_bLookedUp)
		{
			s_bLookedUp = true;

			HMODULE hUser = GetModuleHandle("user32.dll");
			if (hUser)
			{
				s_pfnRegisterRawInputDevices = (PFNREGISTERRAWINPUTDEVICES)GetProcAddress(hUser, "RegisterRawInputDevices");
				s_pfnGetRawInputData = (PFNGETRAWINPUTDATA)GetProcAddress(hUser, "GetRawInputData");
			}
		}

		return s_pfnRegisterRawInputDevices && s_pfnGetRawInputData;
	}

	// Ask for the mouse's WM_INPUT on the game window (once per window).
	bool RegisterMouse()
	{
		if (!g_hMainWnd || !LookUpRawInput())
			return false;
		if (s_hRawWnd == g_hMainWnd)
			return true;

		P1RawInputDevice rid;
		rid.usUsagePage	= P1_HID_USAGE_PAGE_GENERIC;
		rid.usUsage		= P1_HID_USAGE_GENERIC_MOUSE;
		rid.dwFlags		= 0;			// keep the legacy mouse messages (menus)
		rid.hwndTarget	= g_hMainWnd;

		if (!s_pfnRegisterRawInputDevices(&rid, 1, sizeof(rid)))
			return false;

		s_hRawWnd = g_hMainWnd;
		return true;
	}

	void UnregisterMouse()
	{
		if (!s_hRawWnd || !LookUpRawInput())
			return;

		P1RawInputDevice rid;
		rid.usUsagePage	= P1_HID_USAGE_PAGE_GENERIC;
		rid.usUsage		= P1_HID_USAGE_GENERIC_MOUSE;
		rid.dwFlags		= P1_RIDEV_REMOVE;
		rid.hwndTarget	= NULL;

		s_pfnRegisterRawInputDevices(&rid, 1, sizeof(rid));
		s_hRawWnd = NULL;
	}

	// The game window's client area, in screen coordinates.
	bool GetClientScreenRect(RECT* pRect)
	{
		if (!g_hMainWnd || !GetClientRect(g_hMainWnd, pRect))
			return false;

		MapWindowPoints(g_hMainWnd, NULL, (POINT*)pRect, 2);
		return pRect->right > pRect->left && pRect->bottom > pRect->top;
	}

	void GetClientCentre(POINT* pPt)
	{
		RECT rc;
		if (GetClientScreenRect(&rc))
		{
			pPt->x = (rc.left + rc.right) / 2;
			pPt->y = (rc.top + rc.bottom) / 2;
		}
		else
		{
			pPt->x = GetSystemMetrics(SM_CXSCREEN) / 2;
			pPt->y = GetSystemMetrics(SM_CYSCREEN) / 2;
		}
	}

	bool IsWindowed()
	{
		return GetConsoleInt("Windowed", 0) == 1;
	}

	// Hide the cursor, keep it inside the window, and park it in the
	// middle so the first warp (cursor mode) measures nothing.
	void Confine()
	{
		RECT rc;
		if (GetClientScreenRect(&rc))
			ClipCursor(&rc);

		POINT pt;
		GetClientCentre(&pt);
		SetCursorPos(pt.x, pt.y);
		SetCursor(NULL);
	}

	// What the retail game would have: the full-screen clip, or none in a
	// window.
	void Release()
	{
		if (!IsWindowed() && g_prcClip)
			ClipCursor(g_prcClip);
		else
			ClipCursor(NULL);
	}

	void ResetMotion()
	{
		s_nDX = 0;
		s_nDY = 0;
		s_bHaveAbs = false;
	}

	void OnRawInput(LPARAM lParam)
	{
		union
		{
			P1RawInputMouse	raw;
			BYTE			ab[64];
		} buf;
		UINT nSize = sizeof(buf);

		if (s_pfnGetRawInputData((HANDLE)lParam, P1_RID_INPUT, &buf, &nSize, sizeof(P1RawInputHeader)) == (UINT)-1)
			return;
		if (nSize < sizeof(P1RawInputMouse) || buf.raw.header.dwType != P1_RIM_TYPEMOUSE)
			return;

		const P1RawMouse& m = buf.raw.mouse;

		if (m.usFlags & P1_MOUSE_MOVE_ABSOLUTE)
		{
			// 0..65535 across the (virtual) screen: turn into pixels
			bool bVirtual = (m.usFlags & P1_MOUSE_VIRTUAL_DESKTOP) != 0;
			int nW = GetSystemMetrics(bVirtual ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
			int nH = GetSystemMetrics(bVirtual ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
			LONG nX = MulDiv(m.lLastX, nW, 65535);
			LONG nY = MulDiv(m.lLastY, nH, 65535);

			if (s_bHaveAbs)
			{
				s_nDX += nX - s_nAbsX;
				s_nDY += nY - s_nAbsY;
			}
			s_nAbsX = nX;
			s_nAbsY = nY;
			s_bHaveAbs = true;
		}
		else
		{
			s_nDX += m.lLastX;
			s_nDY += m.lLastY;
		}
	}
}

// CInterfaceMgr::UseCursor: TRUE in menus (the cursor is shown), FALSE
// while playing.
void P1Input_UseCursor(bool bUseCursor)
{
	if (s_bCaptured && s_eMode != MODE_OLD)
		Release();

	if (bUseCursor)
	{
		g_pLTClient->RunConsoleString("CursorCenter 0");
		s_bCaptured = false;
		return;
	}

	if (P1Config_OldMouseLook())
	{
		s_eMode = MODE_OLD;
		g_pLTClient->RunConsoleString("CursorCenter 1");
	}
	else
	{
		s_eMode = (!P1Config_NoRawInput() && RegisterMouse()) ? MODE_RAW : MODE_CURSOR;
		g_pLTClient->RunConsoleString("CursorCenter 0");
		Confine();
	}

	ResetMotion();
	s_bCaptured = true;
}

// CGameClientShell::CalculateCameraRotation: offsets[0] yaw, [1] pitch, as
// ILTClient::GetAxisOffsets gives them.
void P1Input_GetAxisOffsets(float* pOffsets)
{
	if (P1Config_OldMouseLook())
	{
		g_pLTClient->GetAxisOffsets(pOffsets);
		return;
	}

	pOffsets[0] = pOffsets[1] = pOffsets[2] = 0.0f;

	LONG nDX = 0, nDY = 0;

	if (s_bCaptured && s_eMode == MODE_CURSOR)
	{
		POINT ptCentre, pt;
		GetClientCentre(&ptCentre);
		if (GetCursorPos(&pt))
		{
			nDX = pt.x - ptCentre.x;
			nDY = pt.y - ptCentre.y;
			if (nDX || nDY)
				SetCursorPos(ptCentre.x, ptCentre.y);
		}
	}
	else if (s_bCaptured && s_eMode == MODE_RAW)
	{
		nDX = s_nDX;
		nDY = s_nDY;
	}
	s_nDX = s_nDY = 0;

	if (!nDX && !nDY)
		return;

	// The retail scale (per DirectInput count), halved: at the same
	// setting the retail look was too fast for raw counts.
	float fSensitivity = GetConsoleFloat("MouseSensitivity", 5.0f);
	float fScale = (0.00125f + fSensitivity * 0.001125f) * 0.5f;

	pOffsets[0] = (float)nDX * fScale;
	pOffsets[1] = (float)nDY * fScale;
}

// HookedWindowProc, before its own handling; the message still goes on to
// the engine's procedure (which lets DefWindowProc clean up WM_INPUT).
void P1Input_OnWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == P1_WM_INPUT)
	{
		if (s_bCaptured && s_eMode == MODE_RAW && s_pfnGetRawInputData)
			OnRawInput(lParam);
	}
	else if (uMsg == WM_SETCURSOR)
	{
		// keep it hidden over the window while the game has the mouse
		if (s_bCaptured && s_eMode != MODE_OLD && LOWORD(lParam) == HTCLIENT)
			SetCursor(NULL);
	}
}

void P1Input_OnWindowUnhooked()
{
	UnregisterMouse();
	if (s_bCaptured && s_eMode != MODE_OLD)
		ClipCursor(NULL);
	s_bCaptured = false;
}

// LTEVENT_LOSTFOCUS / LTEVENT_GAINEDFOCUS: let the cursor go while another
// window is in front, take it again on return.
void P1Input_OnFocus(bool bFocus)
{
	if (!s_bCaptured || s_eMode == MODE_OLD)
		return;

	if (bFocus)
		Confine();
	else
		ClipCursor(NULL);

	ResetMotion();
}

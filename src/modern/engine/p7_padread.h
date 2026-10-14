// ----------------------------------------------------------------------- //
//
// MODULE  : p7_padread.h
//
// PURPOSE : Package P7 (MODERN.md): reading the gamepad, for the two
//           modules that do -- lithtech.exe's ##gamepad device
//           (p7_gamepad.cpp: the buttons the bindings see) and CShell.dll
//           (cshell/p7_pad.cpp: the sticks, and the menus' navigation,
//           which must not depend on what a button is bound to).  Each
//           includes this once and gets its own copy of the state.
//
//           The first connected XInput pad (slot 0..3; an empty slot is
//           slow to ask about, so they are looked through at most once a
//           second while none is connected), from whichever of xinput1_4
//           / 1_3 / 9_1_0 loads.  VC6's headers predate XInput, so its two
//           types are declared here.
//
//           The test pad: with NOLFSRC_PADTEST naming a file, XInput is
//           not used and the pad's state is that file's one line,
//           "<buttons> <lx> <ly> <rx> <ry> <lt> <rt>" in XInput's units
//           (XINPUT_GAMEPAD's wButtons; thumbs -32768..32767 with Y up
//           positive; triggers 0..255), re-read every 15 ms; no file (or
//           an unreadable one) is no pad.  tools/playtest.sh's `pad`
//           command writes it.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P7_PADREAD_H__
#define __P7_PADREAD_H__

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// XINPUT_GAMEPAD
struct P7PadState
{
	WORD	wButtons;
	BYTE	bLeftTrigger;
	BYTE	bRightTrigger;
	SHORT	sThumbLX;
	SHORT	sThumbLY;
	SHORT	sThumbRX;
	SHORT	sThumbRY;
};

// XINPUT_GAMEPAD's button bits
enum
{
	P7PAD_DPAD_UP			= 0x0001,
	P7PAD_DPAD_DOWN			= 0x0002,
	P7PAD_DPAD_LEFT			= 0x0004,
	P7PAD_DPAD_RIGHT		= 0x0008,
	P7PAD_START				= 0x0010,
	P7PAD_BACK				= 0x0020,
	P7PAD_LEFT_THUMB		= 0x0040,
	P7PAD_RIGHT_THUMB		= 0x0080,
	P7PAD_LEFT_SHOULDER		= 0x0100,
	P7PAD_RIGHT_SHOULDER	= 0x0200,
	P7PAD_A					= 0x1000,
	P7PAD_B					= 0x2000,
	P7PAD_X					= 0x4000,
	P7PAD_Y					= 0x8000,
	P7PAD_TRIGGER_THRESHOLD	= 30		// XINPUT_GAMEPAD_TRIGGER_THRESHOLD
};

namespace
{
	struct P7XInputState				// XINPUT_STATE
	{
		DWORD		dwPacketNumber;
		P7PadState	Gamepad;
	};

	typedef DWORD (WINAPI* P7XInputGetState)(DWORD dwUserIndex, P7XInputState* pState);

	int					s_nPadAvailable = -1;	// -1: not decided yet
	P7XInputGetState	s_pPadGetState;
	int					s_nPadSlot = -1;		// the pad being read, -1: none
	DWORD				s_dwPadLastScan;
	bool				s_bPadScanned;

	const char*			s_pPadTestFile;			// NOLFSRC_PADTEST
	bool				s_bPadTestRead;
	DWORD				s_dwPadTestRead;
	bool				s_bPadTestPad;
	P7PadState			s_PadTestState;

	// Can there be a pad at all: XInput loads, or the test pad is on.
	// Decided once.
	bool P7PadRead_Available()
	{
		static const char* const dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
		int i;

		if (s_nPadAvailable >= 0)
			return s_nPadAvailable != 0;

		s_pPadTestFile = getenv("NOLFSRC_PADTEST");

		if (s_pPadTestFile && !*s_pPadTestFile)
			s_pPadTestFile = NULL;

		if (s_pPadTestFile)
		{
			s_nPadAvailable = 1;
			return true;
		}

		for (i = 0; i < 3 && !s_pPadGetState; i++)
		{
			HMODULE hXInput = LoadLibrary(dlls[i]);

			if (hXInput)
			{
				s_pPadGetState = (P7XInputGetState)GetProcAddress(hXInput, "XInputGetState");

				if (!s_pPadGetState)
					FreeLibrary(hXInput);
			}
		}

		s_nPadAvailable = s_pPadGetState != NULL;
		return s_nPadAvailable != 0;
	}

	// Is this the test pad (no XInput)?
	bool P7PadRead_IsTest()
	{
		P7PadRead_Available();
		return s_pPadTestFile != NULL;
	}

	// Re-read at most every 15 ms (a file's time is only to the second).
	bool P7PadRead_Test(P7PadState* pPad)
	{
		DWORD dwNow = GetTickCount();

		if (!s_bPadTestRead || dwNow - s_dwPadTestRead >= 15)
		{
			FILE* pFile = fopen(s_pPadTestFile, "r");
			int   n[7];

			s_bPadTestRead  = true;
			s_dwPadTestRead = dwNow;
			s_bPadTestPad   = false;

			if (pFile)
			{
				if (fscanf(pFile, "%i %i %i %i %i %i %i",
				           &n[0], &n[1], &n[2], &n[3], &n[4], &n[5], &n[6]) == 7)
				{
					s_PadTestState.wButtons      = (WORD)n[0];
					s_PadTestState.sThumbLX      = (SHORT)n[1];
					s_PadTestState.sThumbLY      = (SHORT)n[2];
					s_PadTestState.sThumbRX      = (SHORT)n[3];
					s_PadTestState.sThumbRY      = (SHORT)n[4];
					s_PadTestState.bLeftTrigger  = (BYTE)n[5];
					s_PadTestState.bRightTrigger = (BYTE)n[6];
					s_bPadTestPad = true;
				}

				fclose(pFile);
			}
		}

		*pPad = s_PadTestState;
		return s_bPadTestPad;
	}

	// The pad's state; false (and everything released) when none is
	// connected.
	bool P7PadRead_State(P7PadState* pPad)
	{
		P7XInputState state;
		DWORD         dwNow;
		int           i;

		memset(pPad, 0, sizeof(*pPad));

		if (!P7PadRead_Available())
			return false;

		if (s_pPadTestFile)
			return P7PadRead_Test(pPad);

		if (s_nPadSlot >= 0)
		{
			if (s_pPadGetState(s_nPadSlot, &state) == ERROR_SUCCESS)
			{
				*pPad = state.Gamepad;
				return true;
			}

			s_nPadSlot = -1;
		}

		dwNow = GetTickCount();

		if (s_bPadScanned && dwNow - s_dwPadLastScan < 1000)
			return false;

		s_bPadScanned   = true;
		s_dwPadLastScan = dwNow;

		for (i = 0; i < 4; i++)
		{
			if (s_pPadGetState(i, &state) == ERROR_SUCCESS)
			{
				s_nPadSlot = i;
				*pPad      = state.Gamepad;
				return true;
			}
		}

		return false;
	}
}

#endif // __P7_PADREAD_H__

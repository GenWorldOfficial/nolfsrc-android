// ----------------------------------------------------------------------- //
//
// MODULE  : p1_config.cpp
//
// PURPOSE : Package P1 (MODERN.md): the console variables P1 adds, and
//           the steps it takes when the client shell starts.
//
//             FramerateLock        1  M-P1-01  cap the client at 60 fps
//             OldMouseLook         0  M-P1-03  the retail mouse look
//             NoRawInput           0  M-P1-03  cursor motion, not raw input
//             EnableScreenTinting  1  M-P1-10  0: tint with a 2D overlay
//             EnableLightScale     1  M-P1-10  0: no light scaling
//             ModPatchNum          0  M-P1-12  config version last seen
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VarTrack.h"
#include "ClientUtilities.h"
#include "cshell/p1_modern.h"
#include "cshell/p7_pad.h"

namespace
{
	VarTrack	s_vtFramerateLock;
	VarTrack	s_vtOldMouseLook;
	VarTrack	s_vtNoRawInput;
	VarTrack	s_vtScreenTinting;
	VarTrack	s_vtLightScale;
	VarTrack	s_vtModPatchNum;

	// A variable read before OnEngineInitialized created it reads as its
	// default.
	bool ReadFlag(VarTrack& vt, bool bDefault)
	{
		if (!vt.IsInitted())
			return bDefault;
		return vt.GetFloat() != 0.0f;
	}

	// Unbind every control of every device, so that reading defctrls.cfg
	// afterwards leaves exactly the default bindings (what Controls' own
	// "restore defaults" does).
	void UnbindAll()
	{
		static const uint32 s_aDevices[3] =
		{
			DEVICETYPE_KEYBOARD,
			DEVICETYPE_MOUSE,
			DEVICETYPE_JOYSTICK
		};

		for (int i = 0; i < 3; i++)
		{
			DeviceBinding* pBindings = g_pLTClient->GetDeviceBindings(s_aDevices[i]);
			DeviceBinding* pBinding;

			for (pBinding = pBindings; pBinding; pBinding = pBinding->pNext)
			{
				char szCmd[160];

				// ';' cannot appear in a console string: name it by code
				if (pBinding->strTriggerName[0] == ';')
					_snprintf(szCmd, sizeof(szCmd), "rangebind \"%s\" \"##39\" 0 0 \"\"",
						pBinding->strDeviceName);
				else
					_snprintf(szCmd, sizeof(szCmd), "rangebind \"%s\" \"%s\" 0 0 \"\"",
						pBinding->strDeviceName, pBinding->strTriggerName);
				szCmd[sizeof(szCmd) - 1] = '\0';

				g_pLTClient->RunConsoleString(szCmd);
			}

			if (pBindings)
				g_pLTClient->FreeDeviceBindings(pBindings);
		}
	}
}

// Called from OnEngineInitialized with the shell's other variables, before
// the first render mode is set.
void P1Config_InitVars()
{
	s_vtFramerateLock.Init(g_pLTClient, "FramerateLock", LTNULL, 1.0f);
	s_vtOldMouseLook.Init(g_pLTClient, "OldMouseLook", LTNULL, 0.0f);
	s_vtNoRawInput.Init(g_pLTClient, "NoRawInput", LTNULL, 0.0f);
	s_vtScreenTinting.Init(g_pLTClient, "EnableScreenTinting", LTNULL, 1.0f);
	s_vtLightScale.Init(g_pLTClient, "EnableLightScale", LTNULL, 1.0f);
	s_vtModPatchNum.Init(g_pLTClient, "ModPatchNum", LTNULL, 0.0f);
	P7Pad_InitVars();

	// M-P1-08: with surface optimisation off, text is drawn in black boxes,
	// and the setting can end up off in a config.
	g_pLTClient->RunConsoleString("optimizesurfaces 1");
}

// Called at the end of OnEngineInitialized.
void P1Config_AfterInit()
{
	// M-P1-08: a CursorCenter left on by the retail mouse look fights the
	// new one.  P1Input_UseCursor turns it back on when OldMouseLook asks.
	g_pLTClient->RunConsoleString("CursorCenter 0");

	// M-P1-12: one-time changes for configs older than this build.
	float fSeen = s_vtModPatchNum.IsInitted() ? s_vtModPatchNum.GetFloat() : 0.0f;
	if (fSeen >= P1_CONFIG_VERSION)
		return;

	if (fSeen < 1.0f)
	{
		// More bullet holes stay on the walls.
		if (GetConsoleFloat("BulletHoles", 0.0f) < 100.0f)
			WriteConsoleFloat("BulletHoles", 100.0f);

		// The modern build's default controls (defctrls.cfg) replace the
		// bindings, as Controls' "restore defaults" would.
		if (g_pLTClient->ReadConfigFile("defctrls.cfg") != LT_ERROR)
		{
			UnbindAll();
			g_pLTClient->ReadConfigFile("defctrls.cfg");
		}
	}

	if (fSeen < 1.1f)
	{
		// The modern build's d3d.ren is d3d7.ren now, and its default
		// renderer d3d9.ren (which the engine already loaded for a config
		// naming d3d.ren): say so in the config.
		HCONSOLEVAR hDll = g_pLTClient->GetConsoleVar("RenderDll");
		const char* sDll = hDll ? g_pLTClient->GetVarValueString(hDll) : LTNULL;
		if (sDll && !_stricmp(sDll, "d3d.ren"))
			g_pLTClient->RunConsoleString("RenderDll d3d9.ren");
	}

	if (fSeen >= 1.0f && fSeen < 1.2f)
	{
		// M-P7-02: the controller's default bindings, alone (a config
		// from before 1.0 has just had all of defctrls.cfg): the
		// keyboard's and the mouse's are the player's by now.
		P7Pad_ApplyDefaultBindings();
	}

	if (fSeen < 1.3f)
	{
		// M-P1-22: a window as large as the desktop was borderless by
		// itself; it is the window mode "borderless window" now.
		int nDeskW, nDeskH;
		P1Window_DesktopSize(&nDeskW, &nDeskH);
		if (GetConsoleInt("Windowed", 0) == 1 &&
			GetConsoleInt("screenwidth", 0) >= nDeskW && GetConsoleInt("screenheight", 0) >= nDeskH)
			P1Window_SetMode(P1_WINDOW_BORDERLESS);
	}

	WriteConsoleFloat("ModPatchNum", P1_CONFIG_VERSION);
	g_pLTClient->WriteConfigFile("autoexec.cfg");
}

bool P1Config_FramerateLock()	{ return ReadFlag(s_vtFramerateLock, true); }
bool P1Config_OldMouseLook()	{ return ReadFlag(s_vtOldMouseLook, false); }
bool P1Config_NoRawInput()		{ return ReadFlag(s_vtNoRawInput, false); }
bool P1Config_ScreenTinting()	{ return ReadFlag(s_vtScreenTinting, true); }
bool P1Config_LightScale()		{ return ReadFlag(s_vtLightScale, true); }

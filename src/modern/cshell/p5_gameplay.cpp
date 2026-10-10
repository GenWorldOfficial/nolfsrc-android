// ----------------------------------------------------------------------- //
//
// MODULE  : p5_gameplay.cpp
//
// PURPOSE : The modern build's gameplay and crash fixes and features in
//           CShell.dll (package P5 of MODERN.md). Compiled only with
//           NOLFSRC_MODERN.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include <stdio.h>
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "PlayerStats.h"
#include "WeaponModel.h"
#include "WeaponMgr.h"
#include "ModelButeMgr.h"
#include "ClientUtilities.h"
#include "CommonUtilities.h"
#include "CommandIDs.h"
#include "VarTrack.h"
#include "cshell/p5_gameplay.h"
#include "cshell/p5_movement.h"

extern CGameClientShell* g_pGameClientShell;

// QuickSwitch (M-P5-12): next/previous weapon change the weapon at once.
static VarTrack s_vtQuickSwitch;

// How long the weapon chooser stays up after a quick switch.
static const float kfQuickSwitchChooserTime = 2.5f;

// ----------------------------------------------------------------------- //

void P5_OnEngineInitialized()
{
	if (!s_vtQuickSwitch.IsInitted())
	{
		s_vtQuickSwitch.Init(g_pLTClient, "QuickSwitch", NULL, 0.0f);
	}

	// M-P5-13: the engine only binds actions it knows. It writes the ones it
	// knows into autoexec.cfg, so after the first run this repeats what the
	// config already said.
	char szCmd[128];
	sprintf(szCmd, "AddAction %s %d", CommandName(COMMAND_ID_TOGGLE_ZOOM), COMMAND_ID_TOGGLE_ZOOM);
	g_pLTClient->RunConsoleString(szCmd);
}

// ----------------------------------------------------------------------- //
//
//	M-P5-05: standing on the world across seams
//
// ----------------------------------------------------------------------- //

// Frames the last surface is carried, at most; and how far the player may
// have dropped meanwhile (UpdateOnGround counts more than 1 unit as a fall).
static const int		kP5MaxStandingOnFrames	= 10;
static const LTFLOAT	kfP5MaxStandingOnDrop	= 1.0f;

void P5_ResetStandingOn(P5StandingOn & s)
{
	memset(&s.m_Info, 0, sizeof(s.m_Info));
	s.m_bValid	= LTFALSE;
	s.m_fY		= 0.0f;
	s.m_nFrames	= 0;
}

void P5_CarryStandingOn(P5StandingOn & s, CollisionInfo & Info, const LTVector & vPos, LTBOOL bAirborne)
{
	if (Info.m_hObject)
	{
		// Only the world is carried: it cannot go away in the meantime,
		// as a door or lift object could.
		s.m_bValid	= IsMainWorld(Info.m_hObject);
		s.m_Info	= Info;
		s.m_fY		= vPos.y;
		s.m_nFrames	= 0;
		return;
	}

	if (!s.m_bValid || bAirborne || s.m_nFrames >= kP5MaxStandingOnFrames ||
		s.m_fY - vPos.y > kfP5MaxStandingOnDrop)
	{
		s.m_bValid = LTFALSE;
		return;
	}

	s.m_nFrames++;
	Info = s.m_Info;
}

// ----------------------------------------------------------------------- //
//
//	M-P5-06: an invalid model style
//
// ----------------------------------------------------------------------- //

ModelStyle P5_ValidModelStyle(ModelStyle ms)
{
	if (!g_pModelButeMgr) return ms;

	if (ms == eModelStyleInvalid || (int)ms >= g_pModelButeMgr->GetNumStyles())
	{
		return eModelStyleDefault;
	}

	return ms;
}

// ----------------------------------------------------------------------- //
//
//	M-P5-09: console strings without a copy
//
// ----------------------------------------------------------------------- //

const char* P5_GetConsoleStr(char* szKey)
{
	if (g_pLTClient)
	{
		HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar(szKey);
		if (hVar)
		{
			const char* szValue = g_pLTClient->GetVarValueString(hVar);
			if (szValue) return szValue;
		}
	}

	return "";
}

// ----------------------------------------------------------------------- //
//
//	M-P5-10: the ammo HUD surface
//
//	It is as wide as a full clip of ammo icons, and the camera disabler's
//	clip is 10000 shots. It is drawn right-aligned at the ammo position,
//	so nothing beyond the screen's width can ever show.
//
// ----------------------------------------------------------------------- //

LTBOOL P5_FitAmmoHUD(int & nWidth, int nHeight, const LTRect & rcHUD)
{
	uint32 dwScreenWidth = 0, dwScreenHeight = 0;
	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (hScreen)
	{
		g_pLTClient->GetSurfaceDims(hScreen, &dwScreenWidth, &dwScreenHeight);
	}

	int nMax = (int)dwScreenWidth;
	if (nMax > 0 && nWidth > nMax)
	{
		nWidth = nMax;
	}

	if (nWidth > rcHUD.right || nHeight > rcHUD.bottom) return LTTRUE;

	// One made for a bigger clip (or a bigger screen) is made again.
	return (nMax > 0 && rcHUD.right > nMax) ? LTTRUE : LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	M-P5-12: quick switch
//
// ----------------------------------------------------------------------- //

LTBOOL P5_QuickSwitchOn()
{
	return (s_vtQuickSwitch.GetFloat() != 0.0f) ? LTTRUE : LTFALSE;
}

void P5_QuickSwitchSelect(uint8 nWeaponId)
{
	if (!P5_QuickSwitchOn() || !g_pGameClientShell || !g_pWeaponMgr) return;

	CWeaponModel* pWeaponModel = g_pGameClientShell->GetWeaponModel();
	if (!pWeaponModel || pWeaponModel->IsDisabled()) return;

	if (!g_pWeaponMgr->GetWeapon(nWeaponId)) return;

	// Change now, but leave the chooser up: it is how the player sees
	// where he is in the list while stepping through it.
	pWeaponModel->ChangeWeapon(g_pWeaponMgr->GetCommandId(nWeaponId), LTTRUE, LTTRUE);
}

float P5_ChooserDelay(float fDelay)
{
	return P5_QuickSwitchOn() ? kfQuickSwitchChooserTime : fDelay;
}

// ----------------------------------------------------------------------- //
//
//	M-P5-13: toggle zoom
//
// ----------------------------------------------------------------------- //

int P5_ToggleZoomCommand(int nZoomView)
{
	if (g_pGameClientShell && g_pWeaponMgr)
	{
		CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
		if (pStats)
		{
			uint8 nScopeId = pStats->GetScope();
			if (nScopeId != WMGR_INVALID_ID)
			{
				MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
				if (pMod && nZoomView >= pMod->nZoomLevel)
				{
					return COMMAND_ID_ZOOM_OUT;
				}
			}
		}
	}

	return COMMAND_ID_ZOOM_IN;
}

// ----------------------------------------------------------------------- //
//
//	M-P5-14: model shadows after cinematics
//
//	TurnOnAlternativeCamera runs every frame a cinematic camera is live, and
//	each time saves MaxModelShadows before setting it to 0 -- so from the
//	second frame on it saves the 0, and the end of the cinematic "restores"
//	0. The display options set MaxModelShadows and DrawShadows together, so
//	DrawShadows is what it should come back to.
//
//	Written, but not switched on: with model shadows left on, this
//	project's reconstructed d3d.ren crashes in its shadow pass (a null
//	geometry pointer reaching 0x1000a700); the retail d3d.ren does not.
//	The retail bug has been hiding that. Set this to 1 once d3d.ren is
//	fixed; until then the cinematic code behaves as retail.
//
// ----------------------------------------------------------------------- //

#define P5_RESTORE_MODEL_SHADOWS	0

extern int g_nCinSaveModelShadows;		// GameClientShell.cpp

static void P5_SetModelShadowsFromSetting()
{
	if (!g_pLTClient) return;

	HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("DrawShadows");
	if (!hVar) return;

	WriteConsoleInt("MaxModelShadows", (g_pLTClient->GetVarValueFloat(hVar) > 0.0f) ? 1 : 0);
}

void P5_OnCinematicCameraOff()
{
#if P5_RESTORE_MODEL_SHADOWS
	P5_SetModelShadowsFromSetting();
#else
	WriteConsoleInt("MaxModelShadows", g_nCinSaveModelShadows);
#endif
}

void P5_OnWorldStarted()
{
#if P5_RESTORE_MODEL_SHADOWS
	P5_SetModelShadowsFromSetting();
#endif
}

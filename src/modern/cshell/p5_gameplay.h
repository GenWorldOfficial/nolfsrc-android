// ----------------------------------------------------------------------- //
//
// MODULE  : p5_gameplay.h
//
// PURPOSE : The modern build's gameplay and crash fixes and features in
//           CShell.dll (package P5 of MODERN.md). Compiled only with
//           NOLFSRC_MODERN.
//
// ----------------------------------------------------------------------- //

#ifndef __P5_GAMEPLAY_H__
#define __P5_GAMEPLAY_H__

#include "ModelButeMgr.h"

// Called once from CGameClientShell::OnEngineInitialized: creates P5's
// console variables and the ToggleZoom action.
void		P5_OnEngineInitialized();

// M-P5-06: ms if it names a style of the model bute file, else the
// default style.
ModelStyle	P5_ValidModelStyle(ModelStyle ms);

// M-P5-09: the value of a console variable, "" if there is none.
const char*	P5_GetConsoleStr(char* szKey);

// M-P5-10: fit the ammo HUD surface (nWidth x nHeight wanted) to the
// screen; TRUE if the current one (rcHUD) has to be made again.
LTBOOL		P5_FitAmmoHUD(int & nWidth, int nHeight, const LTRect & rcHUD);

// M-P5-12: quick switch.
LTBOOL		P5_QuickSwitchOn();
void		P5_QuickSwitchSelect(uint8 nWeaponId);	// the chooser's new selection
float		P5_ChooserDelay(float fDelay);			// how long the weapon chooser stays open

// M-P5-13: the command a ToggleZoom press stands for (zoom in a step, or
// out from the last step), given the current zoom step.
int			P5_ToggleZoomCommand(int nZoomView);

// M-P5-14: model shadows, at the end of a cinematic (in place of the
// retail restore) and when a world has started.
void		P5_OnCinematicCameraOff();
void		P5_OnWorldStarted();

#endif // __P5_GAMEPLAY_H__

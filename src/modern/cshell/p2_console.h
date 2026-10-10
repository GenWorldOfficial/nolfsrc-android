// ----------------------------------------------------------------------- //
//
// MODULE  : p2_console.h
//
// PURPOSE : M-P2-01..06, the drop-down developer console (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// A console over the top half of the screen, single player only: the
// engine's and the game's console output (through ILTClient::
// SetConsoleOutputFn, M-P0-02), an edit line, command recall and
// scroll-back. It pauses the game while it is open, and while it is open
// no game command, camera movement or other key reaches the game.
//
// Every entry point is a call-out from a retail file, inside that file's
// #ifdef NOLFSRC_MODERN block:
//
//   GameClientShell.cpp  OnEngineInitialized  P2Console_Init()
//                        OnEngineTerm         P2Console_Term()
//                        OnCommandOn          P2Console_OnCommandOn()
//   InterfaceMgr.cpp     OnKeyDown            P2Console_OnKeyDown()
//                        OnChar               P2Console_OnChar()
//                        AllowCameraMovement  P2Console_IsVisible()
//                        PostUpdate           P2Console_Draw() (before the flip)
//   FolderCustomControls.cpp OnFocus          P2Console_SetLock(CONTROLS)
//   LoadingScreen.cpp    Show / Hide          P2Console_SetLock(LOADING) (p2_speedrun.cpp)

#ifndef __P2_CONSOLE_H__
#define __P2_CONSOLE_H__

// Why the console cannot open right now (bits; any set = locked).
#define P2CONSOLE_LOCK_CONTROLS		(1<<0)	// Custom Controls is waiting for keys
#define P2CONSOLE_LOCK_LOADING		(1<<1)	// a loading screen is up

void   P2Console_Init();
void   P2Console_Term();

LTBOOL P2Console_IsVisible();
void   P2Console_SetLock(uint32 nReason, LTBOOL bLock);

// LTTRUE: the console has taken the key / character / command.
LTBOOL P2Console_OnKeyDown(int nKey, int nRep);
LTBOOL P2Console_OnChar(char c);
LTBOOL P2Console_OnCommandOn(int nCommand);

void   P2Console_Draw();

#endif // __P2_CONSOLE_H__

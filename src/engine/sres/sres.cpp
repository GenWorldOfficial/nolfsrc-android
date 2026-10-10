// *********************************************************************** //
//
//  MODULE:   SRES (.CPP)  -- sres.dll, the server resource DLL
//
//  The server half of the engine (server.dll, and lithtech.exe's copy of the
//  same code) loads sres.dll out of the rez stack after object.lto
//  (cb_LoadServerObjects, src/engine/server/counter.cpp) and fails the level
//  load with LT_ERRORCOPYINGFILE if it is missing. It is the server's
//  counterpart of CRes.dll: a module the engine can pull string resources
//  from (ILTServer::FormatString).
//
//  The shipped one (SRES.dll in nolf2.rez, 155,702 bytes) is a debug-CRT
//  build of an empty DLL: its only code of its own is a DllMain that returns
//  TRUE, and its resource section is empty -- NOLF keeps every server string
//  elsewhere. This is that DLL.
//
// *********************************************************************** //

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pReserved)
{
	return TRUE;
}

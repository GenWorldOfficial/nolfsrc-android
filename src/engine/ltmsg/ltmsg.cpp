// ltmsg.dll — resource-only DLL carrying the engine's error message strings
// (and two leftover dev-era network dialogs). lithtech.exe loads it with
// LoadLibrary and pulls messages by string ID; there are no exports, so the
// code section is just the CRT's DLL startup around this empty DllMain.
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	return TRUE;
}

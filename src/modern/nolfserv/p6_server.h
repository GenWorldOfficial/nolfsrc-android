// ----------------------------------------------------------------------- //
//
// MODULE  : p6_server.h
//
// PURPOSE : The modern build's dedicated server, NolfServ.exe (package P6,
//           MODERN.md; NOLFSRC_MODERN only)
//
//           M-P6-11  the server console in the modern log
//           M-P6-12  unattended start: -nowiz anywhere, -NetLevel adds levels
//           M-P6-13  the master server: shown, and -MasterServer
//
// ----------------------------------------------------------------------- //

#ifndef __P6_SERVER_H__
#define __P6_SERVER_H__

class CServerDlg;

// M-P6-11: the server's log is modern-server.log, not the game's
// modern.log: its lines, and those of the game DLLs it loads, go there.
// Call it first thing.
#define P6_SERVER_LOG_FILE "modern-server.log"
void P6_InitServerLog();

// M-P6-11: one line of the server's console into the log, as
// "server: <line>".
void P6_LogConsole(const char* pLine);

// M-P6-12: the level options. -NetLevel <world> (deathmatch) and
// -NetCALevel <world> (H.A.R.M. vs UNITY) each add a world to the game
// type's level list, in order; the first of each replaces the list saved
// in NetHost.txt. Returns TRUE when szArg was one of them (its value then
// used); nLevels and nCALevels count them and start at 0.
BOOL P6_LevelOption(CServerDlg* pDlg, const char* szArg, const char* szValue,
                    int& nLevels, int& nCALevels);

// M-P6-13: tell the console (and so the log) where the game is: the
// address and port it is hosted on, and the master server it is listed on.
void P6_ShowMasterServer(CServerDlg* pDlg);

#endif // __P6_SERVER_H__

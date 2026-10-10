// *********************************************************************** //
//
//  MODULE:   GAMESPYCLIENTMGR (.CPP)  -- our replacement for
//            GameSpyClientMgr.lib
//
//  PURPOSE:  The client-side server browser CShell.dll links
//            (CGameSpyClientMgr, CGameSpyServer, CGameSpyPlayer; headers in
//            src/LT2/lithshared/incs and src/LT2/lithshared/gamespy). Only the
//            multiplayer "Join" folder (FolderJoin.cpp) uses it.
//
//  WHY THIS IS NOT A RECONSTRUCTION.  The prebuilt library is the GameSpy
//  C Engine SDK plus thin Monolith wrappers -- middleware, deliberately not
//  byte-matched. The build compiles
//  every binary from our own source, so this file stands in for it: the
//  same interface, written from the headers alone.
//
//  WHAT IT DOES.  The SDK filled its list from master.gamespy.com, which
//  was shut down in 2013. With no master there is nothing to list, so the
//  manager initialises, refreshes to an empty list and stays idle; the Join
//  folder shows no servers. The server and player wrappers are complete
//  but are never handed a server.
//
// *********************************************************************** //

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "GameSpyClientMgr.h"


// ----------------------------------------------------------------------- //
//  CGameSpyClientMgr
// ----------------------------------------------------------------------- //

BOOL CGameSpyClientMgr::Init(const char* sGameName, const char* sSecretKey)
{
	Term();

	strncpy(m_sGameName, sGameName ? sGameName : "", GSCM_MAX_STRING - 1);
	m_sGameName[GSCM_MAX_STRING - 1] = '\0';
	strncpy(m_sSecretKey, sSecretKey ? sSecretKey : "", GSCM_MAX_STRING - 1);
	m_sSecretKey[GSCM_MAX_STRING - 1] = '\0';

	if (!CreateServerList()) return(FALSE);

	m_bInit = TRUE;
	return(TRUE);
}

void CGameSpyClientMgr::Term()
{
	FreeServerList();
	Clear();
}

void CGameSpyClientMgr::Clear()
{
	m_bInit           = FALSE;
	m_pServerList     = NULL;
	m_sGameName[0]    = '\0';
	m_sSecretKey[0]   = '\0';
	m_nGetServerIndex = 0;
	m_nProgress       = 0;
	m_bNewServer      = FALSE;
}

BOOL CGameSpyClientMgr::CreateServerList()
{
	// There is no master to create a list against; the (always empty) list
	// needs no state of its own.
	m_pServerList = NULL;
	return(TRUE);
}

void CGameSpyClientMgr::FreeServerList()
{
	m_pServerList = NULL;
}

int CGameSpyClientMgr::GetNumServers()
{
	return(0);
}

int CGameSpyClientMgr::GetState()
{
	return(sl_idle);
}

void CGameSpyClientMgr::ClearServers()
{
	m_nGetServerIndex = 0;
	m_bNewServer      = FALSE;
}

BOOL CGameSpyClientMgr::RefreshServers(BOOL bAsync)
{
	if (!m_bInit) return(FALSE);

	ClearServers();
	m_nProgress = 100;
	return(TRUE);
}

CGameSpyServer* CGameSpyClientMgr::GetServer(int nIndex)
{
	return(NULL);
}

CGameSpyServer* CGameSpyClientMgr::GetServerFromHandle(void* pHandle)
{
	return(NULL);
}

CGameSpyServer* CGameSpyClientMgr::GetFirstServer()
{
	m_nGetServerIndex = 0;
	return(GetServer(m_nGetServerIndex));
}

CGameSpyServer* CGameSpyClientMgr::GetNextServer()
{
	m_nGetServerIndex++;
	return(GetServer(m_nGetServerIndex));
}

BOOL CGameSpyClientMgr::ExistNewServer()
{
	BOOL bNew = m_bNewServer;
	m_bNewServer = FALSE;
	return(bNew);
}

void CGameSpyClientMgr::SortServersByName(BOOL bAscending)     { SortServers(bAscending, "hostname", cm_stricase); }
void CGameSpyClientMgr::SortServersByPing(BOOL bAscending)     { SortServers(bAscending, "ping", cm_int); }
void CGameSpyClientMgr::SortServersByPlayers(BOOL bAscending)  { SortServers(bAscending, "numplayers", cm_int); }
void CGameSpyClientMgr::SortServersByMap(BOOL bAscending)      { SortServers(bAscending, "mapname", cm_stricase); }
void CGameSpyClientMgr::SortServersByGameType(BOOL bAscending) { SortServers(bAscending, "gametype", cm_stricase); }

void CGameSpyClientMgr::SortServers(BOOL bAscending, char* sSortKey, GCompareMode eCompareMode)
{
	// an empty list is sorted
}

void CGameSpyClientMgr::Update()
{
}

void CGameSpyClientMgr::HandleCallback(GServerList pServerList, int nMsg, void* pParam1, void* pParam2)
{
	if (nMsg == LIST_PROGRESS)
	{
		m_nProgress  = (int)pParam2;
		m_bNewServer = TRUE;
	}
}

CGameSpyServer* CGameSpyClientMgr::CreateGameSpyServer(GServer pGServer)
{
	if (!pGServer) return(NULL);
	if (!m_CurServer.Init(pGServer)) return(NULL);
	return(&m_CurServer);
}


// ----------------------------------------------------------------------- //
//  CGameSpyServer -- one browser row. Never handed a server (see above).
// ----------------------------------------------------------------------- //

BOOL CGameSpyServer::Init(GServer pServer)
{
	Term();
	if (!pServer) return(FALSE);
	m_pGServer = pServer;
	m_bInit    = TRUE;
	return(TRUE);
}

void CGameSpyServer::Term()
{
	for (int i = 0; i < m_nNumPlayers; i++) m_aPlayers[i].Term();
	Clear();
}

void CGameSpyServer::Clear()
{
	m_pGServer     = NULL;
	m_bInit        = FALSE;
	m_nNumPlayers  = 0;
	m_nPlayerIndex = 0;
}

char* CGameSpyServer::GetName()       { return(GetStringValue("hostname")); }
char* CGameSpyServer::GetGameType()   { return(GetStringValue("gametype")); }
char* CGameSpyServer::GetMap()        { return(GetStringValue("mapname")); }
int   CGameSpyServer::GetPing()       { return(GetIntValue("ping", 9999)); }
int   CGameSpyServer::GetNumPlayers() { return(GetIntValue("numplayers")); }
int   CGameSpyServer::GetMaxPlayers() { return(GetIntValue("maxplayers")); }
char* CGameSpyServer::GetAddress()    { return(GetStringValue("hostaddr")); }

char* CGameSpyServer::GetStringValue(char* sKey, char* sDefault)
{
	return(sDefault);
}

int CGameSpyServer::GetIntValue(char* sKey, int nDefault)
{
	return(nDefault);
}

float CGameSpyServer::GetFloatValue(char* sKey, float fDefault)
{
	return(fDefault);
}

int CGameSpyServer::UpdatePlayers()
{
	for (int i = 0; i < m_nNumPlayers; i++) m_aPlayers[i].Term();
	m_nNumPlayers  = 0;
	m_nPlayerIndex = 0;

	if (!m_bInit) return(0);

	int nPlayers = GetNumPlayers();
	if (nPlayers > GSS_MAX_PLAYERS) nPlayers = GSS_MAX_PLAYERS;

	for (int j = 0; j < nPlayers; j++)
	{
		if (m_aPlayers[m_nNumPlayers].Init(m_pGServer, j)) m_nNumPlayers++;
	}

	return(m_nNumPlayers);
}

CGameSpyPlayer* CGameSpyServer::GetPlayer(int nIndex)
{
	if (nIndex < 0 || nIndex >= m_nNumPlayers) return(NULL);
	return(&m_aPlayers[nIndex]);
}

CGameSpyPlayer* CGameSpyServer::GetFirstPlayer()
{
	m_nPlayerIndex = 0;
	return(GetPlayer(m_nPlayerIndex));
}

CGameSpyPlayer* CGameSpyServer::GetNextPlayer()
{
	m_nPlayerIndex++;
	return(GetPlayer(m_nPlayerIndex));
}

void CGameSpyServer::SortPlayers(BOOL bAscending, int nKey)
{
	// insertion sort by the requested key
	for (int i = 1; i < m_nNumPlayers; i++)
	{
		for (int j = i; j > 0; j--)
		{
			CGameSpyPlayer* a = &m_aPlayers[j - 1];
			CGameSpyPlayer* b = &m_aPlayers[j];
			int nCmp;
			switch (nKey)
			{
				case GSS_SORT_PING:  nCmp = a->GetPing() - b->GetPing(); break;
				case GSS_SORT_FRAGS: nCmp = a->GetFrags() - b->GetFrags(); break;
				default:             nCmp = stricmp(a->GetName(), b->GetName()); break;
			}
			if (!bAscending) nCmp = -nCmp;
			if (nCmp <= 0) break;

			char tmp[sizeof(CGameSpyPlayer)];
			memcpy(tmp, a, sizeof(tmp));
			memcpy(a, b, sizeof(tmp));
			memcpy(b, tmp, sizeof(tmp));
		}
	}
}


// ----------------------------------------------------------------------- //
//  CGameSpyPlayer
// ----------------------------------------------------------------------- //

BOOL CGameSpyPlayer::Init(GServer pGServer, int nIndex)
{
	Term();
	if (!pGServer) return(FALSE);
	m_pGServer = pGServer;
	m_nIndex   = nIndex;
	m_bInit    = TRUE;
	return(TRUE);
}

void CGameSpyPlayer::Term()
{
	Clear();
}

void CGameSpyPlayer::Clear()
{
	m_pGServer = NULL;
	m_nIndex   = 0;
	m_bInit    = FALSE;
}

char* CGameSpyPlayer::GetName()  { return(GetStringValue("player")); }
int   CGameSpyPlayer::GetFrags() { return(GetIntValue("frags")); }
int   CGameSpyPlayer::GetPing()  { return(GetIntValue("ping")); }

char* CGameSpyPlayer::GetStringValue(char* sKey, char* sDefault)
{
	return(sDefault);
}

int CGameSpyPlayer::GetIntValue(char* sKey, int nDefault)
{
	return(nDefault);
}

float CGameSpyPlayer::GetFloatValue(char* sKey, float fDefault)
{
	return(fDefault);
}

// *********************************************************************** //
//
//  MODULE:   GAMESPYMGR (.CPP)  -- our replacement for GameSpyMgr.lib
//
//  PURPOSE:  The server-side GameSpy query responder Object.lto links
//            (CGameSpyMgr, src/LT2/lithshared/incs/GameSpyMgr.h; the game
//            derives CMyGameSpyMgr from it).
//
//  WHY THIS IS NOT A RECONSTRUCTION.  The library the source release
//  ships prebuilt is GameSpy middleware, not Monolith code, and it is
//  deliberately not byte-matched. The
//  build compiles every binary from our own source, so this file stands
//  in for it: the same interface, written from the header alone.
//
//  WHAT IT DOES.  It answers GameSpy-protocol status queries that reach
//  the game port (GameServerShell::ProcessPacket hands every packet that
//  starts with a backslash to OnQuery): \basic\ \info\ \rules\ \players\
//  \status\ and \echo\, each answered as `\key\value...` pairs closed by
//  `\queryid\<id>.<packet>` and, on the last packet, `\final\`, sent
//  through the game's CGameSpySender. A LAN query tool still sees the
//  server. What it does NOT do is talk to master.gamespy.com: that
//  service was shut down in 2013, so there is no heartbeat, and \secure\
//  (the master's challenge) gets no answer.
//
// *********************************************************************** //

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "GameSpyMgr.h"

#define GSM_PACKET_LIMIT	1300	// flush before a UDP datagram grows past this


BOOL CGameSpyMgr::Init(const char* sGameName, const char* sGameVer, const char* sGameKey,
                       int nGameLoc, int nGamePort, int nSpyPort, DWORD dwFlags)
{
	Term();

	if (!sGameName || !sGameName[0]) return(FALSE);

	m_sGameName = sGameName;
	m_sGameVer  = sGameVer ? sGameVer : "";
	m_sGameKey  = sGameKey ? sGameKey : "";
	m_sGameLoc.Format("%i", nGameLoc);

	m_dwGamePort = (DWORD)nGamePort;
	m_dwSpyPort  = (DWORD)nSpyPort;
	m_dwFlags    = dwFlags;
	m_bInit      = TRUE;

	return(TRUE);
}

void CGameSpyMgr::Term()
{
	m_bInit = FALSE;
}

void CGameSpyMgr::Update()
{
	if (!m_bInit) return;
	UpdateHeartbeat();
}

BOOL CGameSpyMgr::UpdateHeartbeat()
{
	// The master server this used to report to no longer exists.
	return(FALSE);
}


// ----------------------------------------------------------------------- //
//  Queries
// ----------------------------------------------------------------------- //

BOOL CGameSpyMgr::OnQuery(CGameSpyQueryInfo* pQueryInfo)
{
	if (!pQueryInfo || !pQueryInfo->IsValid()) return(FALSE);

	CString sData = pQueryInfo->GetData();
	CString sAddr = pQueryInfo->GetAddr();

	return(OnQuery((char*)(LPCTSTR)sAddr, pQueryInfo->GetPort(),
	               (char*)(LPCTSTR)sData, sData.GetLength()));
}

BOOL CGameSpyMgr::OnQuery(char* sAddr, DWORD dwPort, char* sData, int nLen)
{
	if (!m_bInit || !sAddr || !sData || nLen <= 0) return(FALSE);

	// The packet is not NUL-terminated on the wire.
	char sQuery[MAX_RESPONSE];
	if (nLen >= (int)sizeof(sQuery)) nLen = sizeof(sQuery) - 1;
	memcpy(sQuery, sData, nLen);
	sQuery[nLen] = '\0';

	StartResponse(sAddr, dwPort);

	BOOL bAnswered = FALSE;
	CString sToken = GetFirstQueryToken(sQuery);

	while (!sToken.IsEmpty())
	{
		if (sToken.CompareNoCase("basic") == 0)
		{
			bAnswered |= OnBasicQuery();
		}
		else if (sToken.CompareNoCase("info") == 0)
		{
			bAnswered |= OnInfoQuery();
		}
		else if (sToken.CompareNoCase("rules") == 0)
		{
			bAnswered |= OnRulesQuery();
		}
		else if (sToken.CompareNoCase("players") == 0)
		{
			bAnswered |= OnPlayersQuery();
		}
		else if (sToken.CompareNoCase("status") == 0)
		{
			bAnswered |= OnStatusQuery();
		}
		else if (sToken.CompareNoCase("echo") == 0)
		{
			CString sValue = GetNextQueryToken();
			bAnswered |= OnEchoQuery(sValue);
		}
		else if (sToken.CompareNoCase("secure") == 0)
		{
			CString sValue = GetNextQueryToken();
			bAnswered |= OnSecureQuery(sValue);
		}

		sToken = GetNextQueryToken();
	}

	if (bAnswered) SendResponse();

	return(bAnswered);
}

BOOL CGameSpyMgr::OnEchoQuery(const char* sQuery)
{
	SendResponseInfo("echo", sQuery ? sQuery : "");
	return(TRUE);
}

BOOL CGameSpyMgr::OnSecureQuery(const char* sValue)
{
	// The master's validation challenge: nothing left to validate against.
	return(FALSE);
}

BOOL CGameSpyMgr::OnBasicQuery()
{
	SendResponseInfo("gamename", m_sGameName);
	SendResponseInfo("gamever", m_sGameVer);
	SendResponseInfo("location", m_sGameLoc);
	return(TRUE);
}

BOOL CGameSpyMgr::OnStatusQuery()
{
	OnBasicQuery();
	OnInfoQuery();
	OnRulesQuery();
	OnPlayersQuery();
	return(TRUE);
}


// ----------------------------------------------------------------------- //
//  Responses
// ----------------------------------------------------------------------- //

void CGameSpyMgr::StartResponse(const char* sAddr, DWORD dwPort)
{
	m_sResponseAddr  = sAddr;
	m_dwResponsePort = dwPort;
	m_sResponse[0]   = '\0';
	m_nPacket        = 0;
	m_dwQueryID++;
}

void CGameSpyMgr::SendResponseInfo(const char* sKey, const char* sValue)
{
	if (!sKey) return;
	if (!sValue) sValue = "";

	int nAdd = 2 + (int)strlen(sKey) + (int)strlen(sValue);
	if ((int)strlen(m_sResponse) + nAdd > GSM_PACKET_LIMIT) FlushResponse();
	if ((int)strlen(m_sResponse) + nAdd > MAX_RESPONSE) return;

	strcat(m_sResponse, "\\");
	strcat(m_sResponse, sKey);
	strcat(m_sResponse, "\\");
	strcat(m_sResponse, sValue);
}

void CGameSpyMgr::SendResponseInfo(const char* sKey, int nValue)
{
	char sValue[32];
	sprintf(sValue, "%i", nValue);
	SendResponseInfo(sKey, sValue);
}

void CGameSpyMgr::AddResponseQueryID()
{
	char sID[64];
	sprintf(sID, "\\queryid\\%i.%i", (int)m_dwQueryID, m_nPacket + 1);
	strcat(m_sResponse, sID);
}

// Sends what has accumulated as one packet and starts the next.
void CGameSpyMgr::FlushResponse()
{
	if (!m_sResponse[0]) return;
	AddResponseQueryID();
	SendSocketString(m_sResponseAddr, m_dwResponsePort, m_sResponse);
	m_sResponse[0] = '\0';
	m_nPacket++;
}

// Sends the last packet of a response.
void CGameSpyMgr::SendResponse()
{
	strcat(m_sResponse, "\\final\\");
	AddResponseQueryID();
	SendSocketString(m_sResponseAddr, m_dwResponsePort, m_sResponse);
	m_sResponse[0] = '\0';
	m_nPacket = 0;
}

BOOL CGameSpyMgr::SendSocketString(const char* sAddr, DWORD dwPort, const char* sText, int nLength)
{
	if (!m_pSender || !sAddr || !sText) return(FALSE);
	if (nLength < 0) nLength = (int)strlen(sText);
	m_pSender->SendTo(sText, (unsigned long)nLength, sAddr, dwPort);
	return(TRUE);
}


// ----------------------------------------------------------------------- //
//  Query tokens: "\basic\\info\" -> "basic", "info"; empty when done.
// ----------------------------------------------------------------------- //

static char s_sQueryBuf[MAX_RESPONSE];
static char* s_pQueryPos = NULL;

CString CGameSpyMgr::GetFirstQueryToken(const char* sQuery)
{
	strncpy(s_sQueryBuf, sQuery ? sQuery : "", sizeof(s_sQueryBuf) - 1);
	s_sQueryBuf[sizeof(s_sQueryBuf) - 1] = '\0';
	s_pQueryPos = s_sQueryBuf;
	return(GetNextQueryToken());
}

CString CGameSpyMgr::GetNextQueryToken()
{
	CString sToken;
	if (!s_pQueryPos) return(sToken);

	while (*s_pQueryPos == '\\') s_pQueryPos++;
	if (!*s_pQueryPos) { s_pQueryPos = NULL; return(sToken); }

	char* pStart = s_pQueryPos;
	while (*s_pQueryPos && *s_pQueryPos != '\\') s_pQueryPos++;

	char c = *s_pQueryPos;
	*s_pQueryPos = '\0';
	sToken = pStart;
	*s_pQueryPos = c;

	return(sToken);
}

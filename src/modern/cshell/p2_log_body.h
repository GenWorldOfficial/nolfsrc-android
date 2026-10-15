// ----------------------------------------------------------------------- //
//
// MODULE  : p2_log_body.h
//
// PURPOSE : M-P2-07, the body of ModernLog() (see p2_log.h)
//
// ----------------------------------------------------------------------- //
//
// Compiled once into each game DLL, by cshell/p2_log.cpp and
// object/p2_log.cpp, and into the dedicated server, NolfServ.exe
// (nolfserv/p6_server.cpp): the copies share nothing but the file and the
// process, so "first line of this run" is a process-wide flag kept in an
// environment variable all of them see. Another process-wide variable can
// name another file: the dedicated server sets it (M-P6-11), so that its
// log and that of a game on the same machine do not empty each other.

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "cshell/p2_log.h"

#define MODERN_LOG_STARTED_VAR "NOLFSRC_MODERN_LOG_STARTED"
#define MODERN_LOG_FILE_VAR    "NOLFSRC_MODERN_LOG_FILE"

void ModernLog(const char* pFormat, ...)
{
	char    szLine[1024];
	char    szFlag[4];
	char    szFile[MAX_PATH];
	va_list ap;
	FILE*   pFile;
	int     nLen;
	LTBOOL  bFirst;

	if (!pFormat) return;

	va_start(ap, pFormat);
	nLen = _vsnprintf(szLine, sizeof(szLine) - 1, pFormat, ap);
	va_end(ap);

	if (nLen < 0 || nLen >= (int)sizeof(szLine) - 1)
		nLen = sizeof(szLine) - 1;
	szLine[nLen] = '\0';

	// Two threads writing at the same instant could interleave their lines;
	// for a log of rare events that is good enough.
	bFirst = (GetEnvironmentVariable(MODERN_LOG_STARTED_VAR, szFlag, sizeof(szFlag)) == 0);
	if (bFirst)
	{
		SetEnvironmentVariable(MODERN_LOG_STARTED_VAR, "1");
	}

	if (!GetEnvironmentVariable(MODERN_LOG_FILE_VAR, szFile, sizeof(szFile)) ||
	    strlen(szFile) >= sizeof(szFile))
	{
		strcpy(szFile, MODERN_LOG_FILE);
	}

	pFile = fopen(szFile, bFirst ? "w" : "a");
	if (!pFile) return;

	if (bFirst)
	{
		time_t now = time(NULL);
		fprintf(pFile, "NOLF modern build log, %s", ctime(&now));
	}

	fprintf(pFile, "%s\n", szLine);
	fclose(pFile);
}

/*
 * nolfsrc_debug.h -- diagnostics for the debug build, and nothing in release.
 *
 * The release build has to compile to the retail bytes, so nothing here may
 * reach it: without NOLFSRC_DEBUG (which only the debug build defines, see
 * build/variant.sh) every macro below expands to nothing and this header
 * includes nothing. Arguments are not evaluated in release either, so never
 * put a side effect in them.
 *
 * Any file of any target can include it: src/nolfsrc/ is on every
 * compiler's include path (build/env.sh).
 *
 *   #include "nolfsrc_debug.h"
 *
 *   NOLFSRC_LOG(("loading world %s, %d objects", pName, nObjects));
 *   NOLFSRC_ASSERT(pObj != NULL);
 *
 *   #ifdef NOLFSRC_DEBUG
 *       ... any other debug-only code ...
 *   #endif
 *
 * NOLFSRC_LOG takes printf arguments in a second pair of parentheses (VC6
 * has no variadic macros). Each line is written, with the time in
 * milliseconds and the source file and line, to nolfsrc.log in the current
 * directory (the game directory; $NOLFSRC_LOG names another file) and to
 * OutputDebugString (shown by Wine with WINEDEBUG=+debugstr). NOLFSRC_ASSERT
 * logs a failed condition and carries on.
 *
 * Plain C and C++, VC6 and Intel C/C++ 4.0, MFC or not: it declares the two
 * Win32 functions it calls rather than including <windows.h>, which MFC
 * files must not see before their own headers.
 */
#ifndef NOLFSRC_DEBUG_H
#define NOLFSRC_DEBUG_H

#ifdef NOLFSRC_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *lpOutputString);
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
#ifdef __cplusplus
}
#endif

/* where the next NolfsrcLogf line comes from (set by NOLFSRC_LOG) */
static const char *g_NolfsrcLogFile;
static int g_NolfsrcLogLine;

static void NolfsrcLogAt(const char *file, int line)
{
    g_NolfsrcLogFile = file;
    g_NolfsrcLogLine = line;
}

static void __cdecl NolfsrcLogf(const char *fmt, ...)
{
    char buf[1024];
    int n;
    va_list ap;
    FILE *f;
    const char *path;

    if (g_NolfsrcLogLine > 0)
        n = _snprintf(buf, sizeof(buf) - 2, "[%8lu] %s(%d): ", GetTickCount(),
                      g_NolfsrcLogFile ? g_NolfsrcLogFile : "?", g_NolfsrcLogLine);
    else            /* no source position: just a tag */
        n = _snprintf(buf, sizeof(buf) - 2, "[%8lu] %s: ", GetTickCount(),
                      g_NolfsrcLogFile ? g_NolfsrcLogFile : "?");
    if (n < 0)
        n = 0;
    va_start(ap, fmt);
    if (_vsnprintf(buf + n, sizeof(buf) - 2 - n, fmt, ap) < 0)
        n = sizeof(buf) - 2;            /* truncated: keep what fits */
    else
        n += (int)strlen(buf + n);
    va_end(ap);
    buf[n++] = '\n';
    buf[n] = '\0';

    OutputDebugStringA(buf);
    path = getenv("NOLFSRC_LOG");
    f = fopen(path && *path ? path : "nolfsrc.log", "a");
    if (f) {
        fputs(buf, f);
        fclose(f);
    }
}

#define NOLFSRC_LOG(args) (NolfsrcLogAt(__FILE__, __LINE__), NolfsrcLogf args)
#define NOLFSRC_ASSERT(expr) \
    ((expr) ? (void)0 : (NolfsrcLogAt(__FILE__, __LINE__), \
                         NolfsrcLogf("assertion failed: %s", #expr)))

#else /* release: nothing */

#define NOLFSRC_LOG(args) ((void)0)
#define NOLFSRC_ASSERT(expr) ((void)0)

#endif /* NOLFSRC_DEBUG */

#endif /* NOLFSRC_DEBUG_H */

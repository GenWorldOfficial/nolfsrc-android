// ----------------------------------------------------------------------- //
//
// MODULE  : p2_log.h
//
// PURPOSE : M-P2-07, the modern build's own log file (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// ModernLog() appends one line to modern.log in the game's working
// directory. The file is emptied by the first line written in each run of
// the game, whichever DLL writes it, and every line is flushed at once (the
// file is opened and closed per line), so nothing is lost if the game then
// crashes. Both CShell.dll and Object.lto have it: include
// "cshell/p2_log.h" from either.
//
// Unlike src/nolfsrc/nolfsrc_debug.h's NOLFSRC_LOG, which exists only in the
// debug build, this is always on in the modern build: keep it for things
// worth knowing after the fact (a fix that had to step in, an address a tool
// needs), not per-frame chatter.

#ifndef __P2_LOG_H__
#define __P2_LOG_H__

// printf-style; a trailing newline is added.
void ModernLog(const char* pFormat, ...);

// The log's file name (for messages that point the player at it).
#define MODERN_LOG_FILE "modern.log"

#endif // __P2_LOG_H__

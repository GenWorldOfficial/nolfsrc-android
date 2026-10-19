// mfcs_misc.h - Misc. types and defines for the MFC stub

#ifndef __MFCS_MISC_H__
#define __MFCS_MISC_H__

// Note : Trace would be much better off with an actual implementation...
inline void MFCStubTrace(const char *pFormat, ...) {}
#ifdef NOLF_NATIVE
// (the native build: the arguments are never looked at, and a CString
// cannot go through '...' there)
#define TRACE(...) ((void)0)
#else
#define TRACE MFCStubTrace
#endif

#define DEBUG_NEW new

#endif // __MFCS_MISC_H__
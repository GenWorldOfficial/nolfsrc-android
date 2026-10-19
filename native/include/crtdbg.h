/* crtdbg.h -- the debug C runtime's checks: nothing in a release build */
#include "nolfwin.h"
#define _ASSERT(e)      ((void)0)
#define _ASSERTE(e)     ((void)0)
#define _RPT0(t, m)     ((void)0)
#define _RPT1(t, m, a)  ((void)0)
#define _CrtSetDbgFlag(f) (0)
#define _CrtCheckMemory() (1)
#define _CrtDumpMemoryLeaks() (0)
typedef int (*_CRT_REPORT_HOOK)(int nReportType, char* pMessage, int* pReturn);
#define _CRT_WARN   0
#define _CRT_ERROR  1
#define _CRT_ASSERT 2
#define _CRTDBG_MODE_FILE   0x1
#define _CRTDBG_MODE_DEBUG  0x2
#define _CRTDBG_MODE_WNDW   0x4
#define _CRTDBG_REPORT_MODE -1
#define _CRTDBG_FILE_STDERR ((void*)(intptr_t)-5)
static inline _CRT_REPORT_HOOK _CrtSetReportHook(_CRT_REPORT_HOOK h) { (void)h; return 0; }
static inline int _CrtSetReportMode(int t, int m) { (void)t; (void)m; return 0; }
static inline void* _CrtSetReportFile(int t, void* f) { (void)t; return f; }

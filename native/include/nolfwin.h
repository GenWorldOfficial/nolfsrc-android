/* nolfwin.h -- the part of the Windows API the game uses, for the native
 * build.
 *
 * Every Windows header the source includes (windows.h, io.h, mmsystem.h,
 * winsock.h, ...) is a file in native/include/ that includes this one. The
 * types keep Windows' sizes whatever the host's (DWORD and LONG are 32 bits
 * even where long is 64), the C runtime's Microsoft names map onto POSIX
 * ones, and the functions with no POSIX twin are implemented in
 * native/plat/ (nolfplat_win32.cpp), on SDL2 where they touch the screen,
 * the keyboard or the clock.
 */

#ifndef NOLFWIN_H
#define NOLFWIN_H
/* windows.h's own guard, which headers test to know RECT and POINT exist */
#define _WINDOWS_

#include "nolfcrt.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
#define NOLFWIN_EXTERN extern "C"
#else
#define NOLFWIN_EXTERN extern
#endif

/* ---- types ---------------------------------------------------------------- */

typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
#if ULONG_MAX == 0xffffffffUL
/* Windows' own types where long is 32 bits (x86, 32-bit ARM): the same C++
   names (and overloads) as the Windows build */
typedef unsigned long       DWORD;
typedef long                LONG;
typedef unsigned long       ULONG;
#else
/* where long is 64 bits, the sizes Windows has */
typedef uint32_t            DWORD;
typedef int32_t             LONG;
typedef uint32_t            ULONG;
#endif
/* as macros too: the LithTech headers define them only #ifndef */
#define BYTE  BYTE
#define WORD  WORD
#define DWORD DWORD
typedef unsigned int        UINT;
typedef int                 INT;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef float               FLOAT;
typedef int64_t             LONGLONG;
typedef uint64_t            ULONGLONG;
typedef uint64_t            DWORDLONG;
typedef intptr_t            INT_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           ULONG_PTR;
typedef uintptr_t           DWORD_PTR;
typedef void                VOID;
typedef void*               PVOID;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef char*               LPSTR;
typedef char*               PSTR;
typedef const char*         LPCSTR;
typedef const char*         PCSTR;
typedef char                TCHAR;
typedef char*               LPTSTR;
typedef const char*         LPCTSTR;
typedef BYTE*               LPBYTE;
typedef BYTE*               PBYTE;
typedef WORD*               LPWORD;
typedef DWORD*              LPDWORD;
typedef DWORD*              PDWORD;
typedef LONG*               LPLONG;
typedef BOOL*               LPBOOL;
typedef int*                LPINT;
typedef UINT*               LPUINT;
typedef LONG                HRESULT;
typedef intptr_t            LPARAM;
typedef uintptr_t           WPARAM;
typedef intptr_t            LRESULT;
typedef DWORD               COLORREF;
typedef WORD                ATOM;
typedef unsigned short      WCHAR;

typedef void*               HANDLE;
#define NOLFWIN_HANDLE(name) typedef struct name##__ { int unused; } *name
NOLFWIN_HANDLE(HWND);
NOLFWIN_HANDLE(HINSTANCE);
NOLFWIN_HANDLE(HDC);
NOLFWIN_HANDLE(HKEY);
NOLFWIN_HANDLE(HMENU);
NOLFWIN_HANDLE(HICON);
NOLFWIN_HANDLE(HBITMAP);
NOLFWIN_HANDLE(HBRUSH);
NOLFWIN_HANDLE(HFONT);
NOLFWIN_HANDLE(HPEN);
NOLFWIN_HANDLE(HPALETTE);
NOLFWIN_HANDLE(HGLRC);
NOLFWIN_HANDLE(HRGN);
NOLFWIN_HANDLE(HACCEL);
typedef HINSTANCE           HMODULE;
typedef HICON               HCURSOR;
typedef void*               HGDIOBJ;
typedef void*               HGLOBAL;
typedef void*               HLOCAL;
typedef HKEY*               PHKEY;
typedef DWORD               REGSAM;
/* a function pointer, as Windows has it (what GetProcAddress returns) */
typedef intptr_t (*FARPROC)(void);

typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT;
typedef const RECT*         LPCRECT;
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE;
typedef struct _GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID, IID, CLSID;
#ifdef __cplusplus
typedef const GUID&         REFGUID;
#else
typedef const GUID*         REFGUID;
#endif
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

/* ---- constants and macros ------------------------------------------------------- */

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define MAX_PATH        260
#define INFINITE        0xFFFFFFFFu
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define S_OK            ((HRESULT)0)
#define S_FALSE         ((HRESULT)1)
#define E_FAIL          ((HRESULT)0x80004005L)
#define E_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define E_INVALIDARG    ((HRESULT)0x80070057L)
#define E_NOTIMPL       ((HRESULT)0x80004001L)
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
#define ERROR_SUCCESS   0L

#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define far
#define near
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#define UNREFERENCED_PARAMETER(p) ((void)(p))

#define LOBYTE(w)       ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w)       ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xff))
#define LOWORD(l)       ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l)       ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define MAKEWORD(a, b)  ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)  ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define RGB(r, g, b)    ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(c)    ((BYTE)(c))
#define GetGValue(c)    ((BYTE)(((WORD)(c)) >> 8))
#define GetBValue(c)    ((BYTE)((c) >> 16))
#ifndef __cplusplus
#ifndef min
#define min(a, b)       (((a) < (b)) ? (a) : (b))
#define max(a, b)       (((a) > (b)) ? (a) : (b))
#endif
#endif
#define ZeroMemory(p, n)        memset((p), 0, (n))
#define FillMemory(p, n, c)     memset((p), (c), (n))
#define CopyMemory(d, s, n)     memcpy((d), (s), (n))
#define MoveMemory(d, s, n)     memmove((d), (s), (n))

/* MessageBox */
#define MB_OK               0x00000000L
#define MB_OKCANCEL         0x00000001L
#define MB_YESNO            0x00000004L
#define MB_ICONHAND         0x00000010L
#define MB_ICONSTOP         MB_ICONHAND
#define MB_ICONERROR        MB_ICONHAND
#define MB_ICONQUESTION     0x00000020L
#define MB_ICONEXCLAMATION  0x00000030L
#define MB_ICONWARNING      MB_ICONEXCLAMATION
#define MB_ICONINFORMATION  0x00000040L
#define MB_ICONASTERISK     MB_ICONINFORMATION
#define MB_TASKMODAL        0x00002000L
#define MB_SETFOREGROUND    0x00010000L
#define MB_TOPMOST          0x00040000L
#define IDOK                1
#define IDCANCEL            2
#define IDYES               6
#define IDNO                7

/* virtual-key codes (what GetAsyncKeyState takes) */
#define VK_LBUTTON  0x01
#define VK_RBUTTON  0x02
#define VK_CANCEL   0x03
#define VK_MBUTTON  0x04
#define VK_BACK     0x08
#define VK_TAB      0x09
#define VK_CLEAR    0x0C
#define VK_RETURN   0x0D
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_MENU     0x12
#define VK_PAUSE    0x13
#define VK_CAPITAL  0x14
#define VK_ESCAPE   0x1B
#define VK_SPACE    0x20
#define VK_PRIOR    0x21
#define VK_NEXT     0x22
#define VK_END      0x23
#define VK_HOME     0x24
#define VK_LEFT     0x25
#define VK_UP       0x26
#define VK_RIGHT    0x27
#define VK_DOWN     0x28
#define VK_SELECT   0x29
#define VK_PRINT    0x2A
#define VK_EXECUTE  0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT   0x2D
#define VK_DELETE   0x2E
#define VK_HELP     0x2F
#define VK_LWIN     0x5B
#define VK_RWIN     0x5C
#define VK_APPS     0x5D
#define VK_NUMPAD0  0x60
#define VK_NUMPAD1  0x61
#define VK_NUMPAD2  0x62
#define VK_NUMPAD3  0x63
#define VK_NUMPAD4  0x64
#define VK_NUMPAD5  0x65
#define VK_NUMPAD6  0x66
#define VK_NUMPAD7  0x67
#define VK_NUMPAD8  0x68
#define VK_NUMPAD9  0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD      0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL  0x6E
#define VK_DIVIDE   0x6F
#define VK_F1       0x70
#define VK_F2       0x71
#define VK_F3       0x72
#define VK_F4       0x73
#define VK_F5       0x74
#define VK_F6       0x75
#define VK_F7       0x76
#define VK_F8       0x77
#define VK_F9       0x78
#define VK_F10      0x79
#define VK_F11      0x7A
#define VK_F12      0x7B
#define VK_NUMLOCK  0x90
#define VK_SCROLL   0x91
#define VK_LSHIFT   0xA0
#define VK_RSHIFT   0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU    0xA4
#define VK_RMENU    0xA5

/* ---- the system ------------------------------------------------------------------------ */

NOLFWIN_EXTERN DWORD GetTickCount(void);
NOLFWIN_EXTERN DWORD timeGetTime(void);
NOLFWIN_EXTERN DWORD timeBeginPeriod(UINT n);
NOLFWIN_EXTERN DWORD timeEndPeriod(UINT n);
NOLFWIN_EXTERN BOOL  QueryPerformanceCounter(LARGE_INTEGER* p);
NOLFWIN_EXTERN BOOL  QueryPerformanceFrequency(LARGE_INTEGER* p);
NOLFWIN_EXTERN void  Sleep(DWORD nMilliseconds);
NOLFWIN_EXTERN void  GetLocalTime(SYSTEMTIME* p);
NOLFWIN_EXTERN void  GetSystemTime(SYSTEMTIME* p);
NOLFWIN_EXTERN void  OutputDebugString(const char* p);
NOLFWIN_EXTERN void  OutputDebugStringA(const char* p);
NOLFWIN_EXTERN int   MessageBox(HWND hWnd, const char* pText, const char* pCaption, UINT nType);
NOLFWIN_EXTERN int   MessageBoxA(HWND hWnd, const char* pText, const char* pCaption, UINT nType);
NOLFWIN_EXTERN SHORT GetAsyncKeyState(int nVirtKey);
NOLFWIN_EXTERN SHORT GetKeyState(int nVirtKey);
NOLFWIN_EXTERN DWORD GetLastError(void);
NOLFWIN_EXTERN void  SetLastError(DWORD n);
NOLFWIN_EXTERN DWORD GetCurrentDirectory(DWORD nSize, char* pBuf);
NOLFWIN_EXTERN BOOL  SetCurrentDirectory(const char* pDir);
NOLFWIN_EXTERN BOOL  CreateDirectory(const char* pDir, void* pSecurity);
NOLFWIN_EXTERN BOOL  RemoveDirectory(const char* pDir);
NOLFWIN_EXTERN BOOL  DeleteFile(const char* pFile);
NOLFWIN_EXTERN BOOL  CopyFile(const char* pFrom, const char* pTo, BOOL bFailIfExists);
NOLFWIN_EXTERN BOOL  MoveFile(const char* pFrom, const char* pTo);
NOLFWIN_EXTERN DWORD GetFileAttributes(const char* pFile);
NOLFWIN_EXTERN DWORD GetModuleFileName(HMODULE hModule, char* pBuf, DWORD nSize);
NOLFWIN_EXTERN HMODULE GetModuleHandle(const char* pName);
#define FILE_ATTRIBUTE_READONLY     0x00000001
#define FILE_ATTRIBUTE_DIRECTORY    0x00000010
#define FILE_ATTRIBUTE_NORMAL       0x00000080
#define INVALID_FILE_ATTRIBUTES     ((DWORD)-1)

/* module loading: the native build links the game modules in and answers
   these from a table (nolfplat_modules.cpp), or dlopens a library */
NOLFWIN_EXTERN HMODULE LoadLibrary(const char* pName);
NOLFWIN_EXTERN HMODULE LoadLibraryA(const char* pName);
NOLFWIN_EXTERN BOOL    FreeLibrary(HMODULE hModule);
NOLFWIN_EXTERN FARPROC GetProcAddress(HMODULE hModule, const char* pName);

/* the old ini-file API, and the registry the game keeps its settings in,
   both kept in files beside the game (nolfplat_registry.cpp) */
NOLFWIN_EXTERN DWORD GetPrivateProfileString(const char* pApp, const char* pKey, const char* pDefault,
                                             char* pOut, DWORD nSize, const char* pFile);
NOLFWIN_EXTERN UINT  GetPrivateProfileInt(const char* pApp, const char* pKey, int nDefault, const char* pFile);
NOLFWIN_EXTERN BOOL  WritePrivateProfileString(const char* pApp, const char* pKey, const char* pValue,
                                               const char* pFile);

/* the registry, kept in a file beside the game (nolfplat_registry.cpp) */
#define HKEY_CLASSES_ROOT       ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER       ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE      ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS              ((HKEY)(uintptr_t)0x80000003)
#define REG_NONE                0
#define REG_SZ                  1
#define REG_EXPAND_SZ           2
#define REG_BINARY              3
#define REG_DWORD               4
#define REG_OPTION_NON_VOLATILE 0x00000000L
#define REG_CREATED_NEW_KEY     0x00000001L
#define REG_OPENED_EXISTING_KEY 0x00000002L
#define KEY_QUERY_VALUE         0x0001
#define KEY_SET_VALUE           0x0002
#define KEY_CREATE_SUB_KEY      0x0004
#define KEY_ENUMERATE_SUB_KEYS  0x0008
#define KEY_READ                0x20019
#define KEY_WRITE               0x20006
#define KEY_ALL_ACCESS          0xF003F
#define ERROR_FILE_NOT_FOUND    2L
#define ERROR_MORE_DATA         234L
#define ERROR_NO_MORE_ITEMS     259L
NOLFWIN_EXTERN LONG RegOpenKeyEx(HKEY hKey, const char* pSubKey, DWORD nOptions, REGSAM nSam, PHKEY phResult);
NOLFWIN_EXTERN LONG RegOpenKey(HKEY hKey, const char* pSubKey, PHKEY phResult);
NOLFWIN_EXTERN LONG RegCreateKeyEx(HKEY hKey, const char* pSubKey, DWORD nReserved, char* pClass, DWORD nOptions,
                                   REGSAM nSam, void* pSecurity, PHKEY phResult, LPDWORD pDisposition);
NOLFWIN_EXTERN LONG RegCreateKey(HKEY hKey, const char* pSubKey, PHKEY phResult);
NOLFWIN_EXTERN LONG RegCloseKey(HKEY hKey);
NOLFWIN_EXTERN LONG RegQueryValueEx(HKEY hKey, const char* pName, LPDWORD pReserved, LPDWORD pType, BYTE* pData,
                                    LPDWORD pcbData);
NOLFWIN_EXTERN LONG RegSetValueEx(HKEY hKey, const char* pName, DWORD nReserved, DWORD nType, const BYTE* pData,
                                  DWORD cbData);
NOLFWIN_EXTERN LONG RegDeleteValue(HKEY hKey, const char* pName);
NOLFWIN_EXTERN LONG RegDeleteKey(HKEY hKey, const char* pSubKey);
NOLFWIN_EXTERN LONG RegEnumKeyEx(HKEY hKey, DWORD nIndex, char* pName, LPDWORD pcName, LPDWORD pReserved,
                                 char* pClass, LPDWORD pcClass, FILETIME* pTime);
NOLFWIN_EXTERN LONG RegEnumValue(HKEY hKey, DWORD nIndex, char* pName, LPDWORD pcName, LPDWORD pReserved,
                                 LPDWORD pType, BYTE* pData, LPDWORD pcbData);
#define RegOpenKeyExA       RegOpenKeyEx
#define RegCreateKeyExA     RegCreateKeyEx
#define RegQueryValueExA    RegQueryValueEx
#define RegSetValueExA      RegSetValueEx
#define RegDeleteValueA     RegDeleteValue
#define RegDeleteKeyA       RegDeleteKey

/* ---- windows, the cursor and the screen (nolfplat_user32.cpp) ---------------------------
 * The engine's one window is SDL's. The game's own calls on windows (the
 * launcher hand-off, the cursor clip, listbox messages in its Windows-only
 * dialogs) find no window natively and do nothing. */

typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct _devicemode { char dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
                             DWORD dmFields, dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags,
                             dmDisplayFrequency; } DEVMODE, *LPDEVMODE;
typedef void* HRSRC;
typedef void* HKL;
typedef int   HFILE;
typedef struct _OFSTRUCT { BYTE cBytes, fFixedDisk; WORD nErrCode, Reserved1, Reserved2; char szPathName[128]; } OFSTRUCT;
#define HFILE_ERROR         ((HFILE)-1)
#define OF_EXIST            0x00004000
#define MAKEINTRESOURCE(i)  ((char*)(uintptr_t)(WORD)(i))
#define HWND_TOP            ((HWND)0)
#define HWND_TOPMOST        ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST      ((HWND)(intptr_t)-2)
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_MAXIMIZE         3
#define SW_RESTORE          9
#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_NOACTIVATE      0x0010
#define GWL_STYLE           (-16)
#define GWL_EXSTYLE         (-20)
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_VISIBLE          0x10000000L
#define WS_CAPTION          0x00C00000L
#define WS_SYSMENU          0x00080000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_THICKFRAME       0x00040000L
#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define SM_CXSCREEN         0
#define SM_CYSCREEN         1
#define SPI_GETWORKAREA     0x0030
#define WM_SETREDRAW        0x000B
#define WM_SETCURSOR        0x0020
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_ACTIVATEAPP      0x001C
#define WM_USER             0x0400
#define WM_MOUSEMOVE        0x0200
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MOUSEWHEEL       0x020A
#define WM_INPUT            0x00FF
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_PAINT            0x000F
#define WM_DESTROY          0x0002
#define WM_SIZE             0x0005
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113
#define MK_LBUTTON          0x0001
#define MK_RBUTTON          0x0002
#define MK_SHIFT            0x0004
#define MK_CONTROL          0x0008
#define MK_MBUTTON          0x0010
#define GWL_WNDPROC         (-4)
#define GWLP_WNDPROC        (-4)
#define GWL_USERDATA        (-21)
#define GWLP_USERDATA       (-21)
#define HTCLIENT            1
#define LB_ADDSTRING        0x0180
#define LB_DELETESTRING     0x0182
#define LB_RESETCONTENT     0x0184
#define LB_GETCURSEL        0x0188
#define LB_GETTEXT          0x0189
#define LB_GETCOUNT         0x018B
#define LB_ERR              (-1)
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000
#define DRIVE_UNKNOWN       0
#define DRIVE_NO_ROOT_DIR   1
#define DRIVE_REMOVABLE     2
#define DRIVE_FIXED         3
#define DRIVE_REMOTE        4
#define DRIVE_CDROM         5
#define MB_ABORTRETRYIGNORE 0x00000002L
#define MB_RETRYCANCEL      0x00000005L
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define ENUM_REGISTRY_SETTINGS ((DWORD)-2)
#define ENUM_CURRENT_SETTINGS  ((DWORD)-1)
#define CDS_FULLSCREEN      0x00000004
#define DISP_CHANGE_SUCCESSFUL 0
#define MAPVK_VK_TO_VSC     0
#define MAPVK_VSC_TO_VK     1
#define MAPVK_VK_TO_CHAR    2

NOLFWIN_EXTERN HWND    FindWindow(const char* pClass, const char* pTitle);
NOLFWIN_EXTERN HWND    GetForegroundWindow(void);
NOLFWIN_EXTERN HWND    GetFocus(void);
NOLFWIN_EXTERN HWND    SetFocus(HWND hWnd);
NOLFWIN_EXTERN BOOL    SetForegroundWindow(HWND hWnd);
NOLFWIN_EXTERN BOOL    DestroyWindow(HWND hWnd);
NOLFWIN_EXTERN BOOL    ShowWindow(HWND hWnd, int nCmd);
NOLFWIN_EXTERN BOOL    UpdateWindow(HWND hWnd);
NOLFWIN_EXTERN BOOL    InvalidateRect(HWND hWnd, const RECT* pRect, BOOL bErase);
NOLFWIN_EXTERN BOOL    GetWindowRect(HWND hWnd, RECT* pRect);
NOLFWIN_EXTERN BOOL    GetClientRect(HWND hWnd, RECT* pRect);
NOLFWIN_EXTERN BOOL    SetWindowPos(HWND hWnd, HWND hAfter, int x, int y, int w, int h, UINT nFlags);
NOLFWIN_EXTERN BOOL    MoveWindow(HWND hWnd, int x, int y, int w, int h, BOOL bRepaint);
NOLFWIN_EXTERN LONG    GetWindowLong(HWND hWnd, int nIndex);
NOLFWIN_EXTERN LONG_PTR GetWindowLongPtr(HWND hWnd, int nIndex);
NOLFWIN_EXTERN LONG_PTR SetWindowLongPtr(HWND hWnd, int nIndex, LONG_PTR nValue);
/* takes FARPROC as Windows' non-STRICT headers have it (the game's hook
   keeps the engine's procedure as one), and WNDPROC in C++ */
NOLFWIN_EXTERN LRESULT CallWindowProc(FARPROC pProc, HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
#ifdef __cplusplus
static inline LRESULT CallWindowProc(WNDPROC pProc, HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc((FARPROC)pProc, hWnd, nMsg, wParam, lParam);
}
#endif
NOLFWIN_EXTERN LRESULT DefWindowProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
NOLFWIN_EXTERN LONG    SetWindowLong(HWND hWnd, int nIndex, LONG nValue);
NOLFWIN_EXTERN int     GetWindowText(HWND hWnd, char* pBuf, int nMax);
NOLFWIN_EXTERN BOOL    SetWindowText(HWND hWnd, const char* pText);
NOLFWIN_EXTERN BOOL    AdjustWindowRect(RECT* pRect, DWORD nStyle, BOOL bMenu);
NOLFWIN_EXTERN int     MapWindowPoints(HWND hFrom, HWND hTo, POINT* pPoints, UINT nPoints);
NOLFWIN_EXTERN BOOL    ClientToScreen(HWND hWnd, POINT* pPoint);
NOLFWIN_EXTERN BOOL    ScreenToClient(HWND hWnd, POINT* pPoint);
NOLFWIN_EXTERN LRESULT SendMessage(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
NOLFWIN_EXTERN BOOL    PostMessage(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
NOLFWIN_EXTERN int     GetSystemMetrics(int nIndex);
NOLFWIN_EXTERN BOOL    SystemParametersInfo(UINT nAction, UINT nParam, void* pParam, UINT nWinIni);
NOLFWIN_EXTERN BOOL    EnumDisplaySettings(const char* pDevice, DWORD nMode, DEVMODE* pMode);
NOLFWIN_EXTERN LONG    ChangeDisplaySettings(DEVMODE* pMode, DWORD nFlags);
NOLFWIN_EXTERN BOOL    ClipCursor(const RECT* pRect);
NOLFWIN_EXTERN BOOL    GetCursorPos(POINT* pPoint);
NOLFWIN_EXTERN BOOL    SetCursorPos(int x, int y);
NOLFWIN_EXTERN HCURSOR SetCursor(HCURSOR hCursor);
NOLFWIN_EXTERN int     ShowCursor(BOOL bShow);
NOLFWIN_EXTERN BOOL    SetRect(RECT* pRect, int left, int top, int right, int bottom);
NOLFWIN_EXTERN BOOL    SetRectEmpty(RECT* pRect);
NOLFWIN_EXTERN BOOL    PtInRect(const RECT* pRect, POINT pt);
NOLFWIN_EXTERN BOOL    IntersectRect(RECT* pOut, const RECT* pA, const RECT* pB);
NOLFWIN_EXTERN BOOL    OffsetRect(RECT* pRect, int dx, int dy);
NOLFWIN_EXTERN BOOL    DeleteObject(HGDIOBJ hObject);
NOLFWIN_EXTERN int     MulDiv(int a, int b, int c);
NOLFWIN_EXTERN UINT    MapVirtualKey(UINT nCode, UINT nMapType);
NOLFWIN_EXTERN UINT    MapVirtualKeyEx(UINT nCode, UINT nMapType, HKL hKeyboard);
NOLFWIN_EXTERN void    DebugBreak(void);
NOLFWIN_EXTERN HKL     GetKeyboardLayout(DWORD nThread);
NOLFWIN_EXTERN UINT    SetErrorMode(UINT nMode);
NOLFWIN_EXTERN UINT    GetDriveType(const char* pRoot);
NOLFWIN_EXTERN DWORD   GetEnvironmentVariable(const char* pName, char* pBuf, DWORD nSize);
NOLFWIN_EXTERN BOOL    SetEnvironmentVariable(const char* pName, const char* pValue);
NOLFWIN_EXTERN HFILE   OpenFile(const char* pName, OFSTRUCT* pOf, UINT nStyle);
NOLFWIN_EXTERN HRSRC   FindResource(HMODULE hModule, const char* pName, const char* pType);
NOLFWIN_EXTERN HGLOBAL LoadResource(HMODULE hModule, HRSRC hRes);
NOLFWIN_EXTERN void*   LockResource(HGLOBAL hRes);
NOLFWIN_EXTERN DWORD   SizeofResource(HMODULE hModule, HRSRC hRes);
NOLFWIN_EXTERN int     LoadString(HINSTANCE hInstance, UINT nID, char* pBuf, int nMax);
#define LoadStringA         LoadString
#define FindWindowA         FindWindow
#define SendMessageA        SendMessage
#define GetWindowTextA      GetWindowText
#define SetWindowTextA      SetWindowText
#define GetEnvironmentVariableA GetEnvironmentVariable
#define SetEnvironmentVariableA SetEnvironmentVariable

/* ---- threads and synchronisation (nolfplat_thread.cpp) ------------------------------------- */

typedef struct _CRITICAL_SECTION { void* pImpl; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
NOLFWIN_EXTERN void  InitializeCriticalSection(CRITICAL_SECTION* p);
NOLFWIN_EXTERN void  DeleteCriticalSection(CRITICAL_SECTION* p);
NOLFWIN_EXTERN void  EnterCriticalSection(CRITICAL_SECTION* p);
NOLFWIN_EXTERN void  LeaveCriticalSection(CRITICAL_SECTION* p);
NOLFWIN_EXTERN LONG  InterlockedIncrement(LONG volatile* p);
NOLFWIN_EXTERN LONG  InterlockedDecrement(LONG volatile* p);
NOLFWIN_EXTERN LONG  InterlockedExchange(LONG volatile* p, LONG v);

/* events and threads */
#define WAIT_OBJECT_0       0x00000000L
#define WAIT_TIMEOUT        0x00000102L
#define WAIT_FAILED         0xFFFFFFFFL
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_LOWEST (-2)
#define THREAD_PRIORITY_IDLE (-15)
#define THREAD_PRIORITY_TIME_CRITICAL 15
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
NOLFWIN_EXTERN HANDLE CreateEvent(void* pSecurity, BOOL bManualReset, BOOL bInitialState, const char* pName);
NOLFWIN_EXTERN BOOL   SetEvent(HANDLE hEvent);
NOLFWIN_EXTERN BOOL   ResetEvent(HANDLE hEvent);
NOLFWIN_EXTERN DWORD  WaitForSingleObject(HANDLE hObject, DWORD nMilliseconds);
NOLFWIN_EXTERN HANDLE CreateThread(void* pSecurity, size_t nStack, LPTHREAD_START_ROUTINE pStart, LPVOID pParam,
                                   DWORD nFlags, LPDWORD pThreadId);
NOLFWIN_EXTERN BOOL   SetThreadPriority(HANDLE hThread, int nPriority);
NOLFWIN_EXTERN BOOL   TerminateThread(HANDLE hThread, DWORD nExitCode);
NOLFWIN_EXTERN BOOL   GetExitCodeThread(HANDLE hThread, LPDWORD pExitCode);
NOLFWIN_EXTERN BOOL   CloseHandle(HANDLE hObject);
NOLFWIN_EXTERN DWORD  GetCurrentThreadId(void);
NOLFWIN_EXTERN HANDLE GetCurrentThread(void);
#define STILL_ACTIVE 259

#endif /* NOLFWIN_H */

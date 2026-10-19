/* windowsx.h -- the message crackers (HANDLE_MSG) for the messages the game
   handles in its hook on the engine's window. Native, that window is the
   platform layer's (native/plat/), which turns SDL's events into these
   messages and calls the window procedure the game set. */
#ifndef NOLF_WINDOWSX_H
#define NOLF_WINDOWSX_H
#include "nolfwin.h"

#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))

#define HANDLE_MSG(hwnd, message, fn) \
    case (message): return HANDLE_##message((hwnd), (wParam), (lParam), (fn))

#define HANDLE_WM_LBUTTONDOWN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), FALSE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_LBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), TRUE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_LBUTTONUP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_RBUTTONDOWN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), FALSE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_RBUTTONDBLCLK(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), TRUE, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_RBUTTONUP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_MOUSEMOVE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)(wParam)), 0L)
#define HANDLE_WM_CHAR(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (char)(wParam), (int)(short)LOWORD(lParam)), 0L)
#define HANDLE_WM_SETCURSOR(hwnd, wParam, lParam, fn) \
    (LRESULT)(DWORD)(BOOL)(fn)((hwnd), (HWND)(wParam), (UINT)LOWORD(lParam), (UINT)HIWORD(lParam))
#endif

// glplatform.h -- what opengl.ren needs from the system under it.
//
// The display modes, the engine's window sized for a mode, the GL context,
// the buffer swap and extension entry points: the Mac renderer did these
// with AGL and DrawSprocket, gl_wgl.cpp does them with WGL and
// ChangeDisplaySettings. Everything else in the renderer is plain OpenGL
// 1.x and the engine's interface, so a native build for another system
// (Linux, under its own port of the engine) supplies these calls and
// leaves the rest as it is.

#ifndef GLPLATFORM_H
#define GLPLATFORM_H

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#endif
#include <GL/gl.h>
#include <stdint.h>

#ifndef APIENTRY
#define APIENTRY
#endif

// Every display mode at 16 and 32 bits, 640x480 and up, once each.
typedef void (*PlatModeFn)(uint32_t nWidth, uint32_t nHeight, uint32_t nBitDepth, void* pUser);
void plat_EnumModes(PlatModeFn fn, void* pUser);

// Size the engine's window (RenderStructInit::m_hWnd) for the mode: the
// whole display at that mode for full screen, else a centred window whose
// client area is the mode. *pbWindowed comes back true when the display
// could not be switched. plat_CloseWindow undoes it.
void plat_OpenWindow(void* hWnd, uint32_t nWidth, uint32_t nHeight, bool* pbWindowed);
void plat_CloseWindow(void);

// A double-buffered RGBA context with depth and stencil on that window,
// made current.
bool plat_CreateContext(void);
void plat_DestroyContext(void);
void plat_SwapBuffers(void);
void plat_SetSwapInterval(int nInterval);
void* plat_GetProcAddress(const char* pName);

#endif

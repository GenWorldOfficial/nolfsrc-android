// android_platform.h -- what the Android engine port hands the renderer.
//
// On Android there is one window, the activity's surface, and it is always
// the whole screen. The engine's port (the activity glue that replaces
// clientwin.cpp's Win32 window) passes it here when the surface is created
// and NULL when it is destroyed, and passes the same pointer as the
// engine's window (RenderStructInit::m_hWnd), which gles_device.cpp makes
// its EGL surface on.

#ifndef ANDROID_PLATFORM_H
#define ANDROID_PLATFORM_H

struct ANativeWindow;

#ifdef __cplusplus
extern "C" {
#endif

void nolf_SetNativeWindow(struct ANativeWindow* pWindow);
struct ANativeWindow* nolf_GetNativeWindow(void);

#ifdef __cplusplus
}
#endif

#endif

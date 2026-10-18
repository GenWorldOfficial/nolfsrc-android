// android_platform.cpp -- mrplatform.h on Android.
//
// There are no display modes to switch: the screen stays at its own
// resolution and the engine's mode is the size gles_device.cpp draws at,
// scaled onto the screen when presented. So the mode list is the screen's
// own size and a few fractions of it (cheaper to draw on a phone), plus the
// classic 4:3 modes the game's menus were made for, and opening the window
// for a mode changes nothing on the screen.

#include "../mrplatform.h"
#include "android_platform.h"

#include <android/native_window.h>

static ANativeWindow* s_pWindow;

void nolf_SetNativeWindow(ANativeWindow* pWindow) { s_pWindow = pWindow; }
ANativeWindow* nolf_GetNativeWindow(void) { return s_pWindow; }

void plat_EnumModes(PlatModeFn fn, void* pUser)
{
    // The game runs in landscape; a surface asked for before the rotation
    // settles can still be portrait.
    uint32_t nW = 1920, nH = 1080;
    if (s_pWindow)
    {
        int32_t w = ANativeWindow_getWidth(s_pWindow), h = ANativeWindow_getHeight(s_pWindow);
        if (w > 0 && h > 0)
        {
            nW = (uint32_t)(w > h ? w : h);
            nH = (uint32_t)(w > h ? h : w);
        }
    }

    uint32_t modes[16][2];
    int nModes = 0;
    static const uint32_t s_Classic[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 } };
    for (const auto& m : s_Classic)
    {
        modes[nModes][0] = m[0];
        modes[nModes][1] = m[1];
        nModes++;
    }
    // The screen's proportions at 1/2, 2/3, 3/4 and full size, even sizes
    // only, 480 lines and up.
    static const uint32_t s_Fractions[][2] = { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 1, 1 } };
    for (const auto& f : s_Fractions)
    {
        uint32_t w = (nW * f[0] / f[1]) & ~1u, h = (nH * f[0] / f[1]) & ~1u;
        if (w < 640 || h < 480)
            continue;
        bool bDup = false;
        for (int i = 0; i < nModes; i++)
            bDup |= modes[i][0] == w && modes[i][1] == h;
        if (!bDup && nModes < 16)
        {
            modes[nModes][0] = w;
            modes[nModes][1] = h;
            nModes++;
        }
    }
    for (int i = 0; i < nModes; i++)
    {
        fn(modes[i][0], modes[i][1], 16, pUser);
        fn(modes[i][0], modes[i][1], 32, pUser);
    }
}

void plat_OpenWindow(void* hWnd, uint32_t nWidth, uint32_t nHeight, bool* pbWindowed)
{
    (void)hWnd;
    (void)nWidth;
    (void)nHeight;
    // Always the whole screen; the mode is the size drawn at, not shown at.
    if (pbWindowed)
        *pbWindowed = false;
}

void plat_CloseWindow(void)
{
}

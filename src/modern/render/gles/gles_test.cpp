// gles_test.cpp -- draws known scenes through rdev.h on gles_device.cpp and
// checks the pixels that come back, without the engine.
//
// Runs on a desktop with EGL and OpenGL ES 3 (Mesa's llvmpipe under Xvfb
// is enough): see run_test.sh. Each check prints PASS or FAIL; the exit
// status is the number of failures.

#include "../rdev.h"

#include <X11/Xlib.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void mr_Print(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    vprintf(pFormat, args);
    va_end(args);
    printf("\n");
}

static const int W = 320, H = 240;
static int s_nFail;

static uint32_t Pixel(int x, int y, bool bFront = false)
{
    uint32_t v = 0;
    rd_ReadPixels(x, y, 1, 1, RD_PIX_BGRA, &v, bFront);
    return v;
}

static int Chan(uint32_t v, int nShift) { return (int)((v >> nShift) & 255); }

// Within nTol of each channel (0xAARRGGBB).
static void Check(const char* pName, uint32_t nGot, uint32_t nWant, int nTol = 3)
{
    bool ok = true;
    for (int s = 0; s < 32; s += 8)
        ok &= abs(Chan(nGot, s) - Chan(nWant, s)) <= nTol;
    printf("%s  %-52s got %08x want %08x\n", ok ? "PASS" : "FAIL", pName, nGot, nWant);
    s_nFail += ok ? 0 : 1;
}

static void Ortho2D(void)
{
    rd_MatrixMode(RD_PROJECTION);
    rd_LoadIdentity();
    rd_Ortho(0, W, H, 0, -1, 1);           // y down, as the engine's 2D
    rd_MatrixMode(RD_MODELVIEW);
    rd_LoadIdentity();
}

// GL's perspective with eye-space depth w, as mr_LoadCamera makes it.
static void Perspective(float fNear, float fFar)
{
    float f = 1.0f / tanf(0.5f);
    float m[16] = { 0 };
    m[0] = f * H / W;
    m[5] = f;
    m[10] = (fFar + fNear) / (fNear - fFar);
    m[11] = -1.0f;
    m[14] = 2.0f * fFar * fNear / (fNear - fFar);
    rd_MatrixMode(RD_PROJECTION);
    rd_LoadMatrix(m);
    rd_MatrixMode(RD_MODELVIEW);
    rd_LoadIdentity();
}

static void Rect(float x0, float y0, float x1, float y1)
{
    rd_Begin(RD_QUADS);
    rd_TexCoord2f(0, 0); rd_MultiTexCoord2f(1, 0, 0); rd_Vertex2f(x0, y0);
    rd_TexCoord2f(1, 0); rd_MultiTexCoord2f(1, 1, 0); rd_Vertex2f(x1, y0);
    rd_TexCoord2f(1, 1); rd_MultiTexCoord2f(1, 1, 1); rd_Vertex2f(x1, y1);
    rd_TexCoord2f(0, 1); rd_MultiTexCoord2f(1, 0, 1); rd_Vertex2f(x0, y1);
    rd_End();
}

// A square facing the eye at eye depth d (in front of the eye: z = -d).
static void DepthQuad(float d, float s)
{
    rd_Begin(RD_TRIANGLE_FAN);
    rd_Vertex3f(-s, -s, -d);
    rd_Vertex3f(s, -s, -d);
    rd_Vertex3f(s, s, -d);
    rd_Vertex3f(-s, s, -d);
    rd_End();
}

static uint32_t Texture(RDFormat eFormat, RDPixels ePixels, const void* pData, int w, int h)
{
    uint32_t n = rd_GenTexture();
    rd_BindTexture(n);
    rd_TexFilter(RD_NEAREST, RD_NEAREST);
    rd_TexWrap(false, false);
    rd_TexMaxLevel(0);
    rd_TexImage(0, eFormat, w, h, ePixels, pData);
    return n;
}

static void Run(int nSamples, Window win)
{
    printf("---- %dx samples ----\n", nSamples);
    if (!rd_CreateDevice((void*)win, W, H, true, 0, nSamples))
    {
        printf("FAIL  device\n");
        s_nFail++;
        return;
    }
    rd_PrintInfo(mr_Print);

    // Clear, and the scissor limiting it.
    rd_ClearColor(1, 0, 0, 1);
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    Check("clear", Pixel(10, 10), 0xffff0000);
    rd_Scissor(0, 0, 100, 100);
    rd_Enable(RD_SCISSOR_TEST);
    rd_ClearColor(0, 0, 1, 1);
    rd_Clear(RD_CLEAR_COLOR);
    rd_Disable(RD_SCISSOR_TEST);
    Check("scissored clear, inside (top left)", Pixel(10, 10), 0xff0000ff);
    Check("scissored clear, outside", Pixel(200, 200), 0xffff0000);

    // Untextured 2D with vertex colour; top-left origin of the viewport.
    rd_Disable(RD_DEPTH_TEST);
    Ortho2D();
    rd_Color4ub(0, 255, 0, 255);
    Rect(0, 0, 40, 30);
    Check("vertex colour quad", Pixel(20, 15), 0xff00ff00);
    Check("outside quad untouched", Pixel(60, 15), 0xff0000ff);

    rd_Viewport(W / 2, H / 2, W / 2, H / 2);
    Ortho2D();
    rd_Color4ub(255, 255, 0, 255);
    Rect(0, 0, W, H);
    rd_Viewport(0, 0, W, H);
    Check("viewport bottom right drawn", Pixel(W - 5, H - 5), 0xffffff00);
    Check("viewport leaves top left", Pixel(20, 15), 0xff00ff00);

    // Textures: BGRA words, modulate by colour.
    uint32_t blue[4] = { 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff };
    uint32_t nBlue = Texture(RD_FMT_RGBA8, RD_PIX_BGRA, blue, 2, 2);
    rd_Enable(RD_TEXTURE_2D);
    rd_TexEnv(RD_ENV_MODULATE);
    rd_Color4ub(255, 255, 255, 255);
    Rect(100, 100, 140, 140);
    Check("BGRA texture, modulate white", Pixel(120, 120), 0xff0000ff);
    rd_Color4ub(128, 128, 128, 255);
    Rect(100, 100, 140, 140);
    Check("BGRA texture, modulate grey", Pixel(120, 120), 0xff000080);

    // 5:6:5 RGB texture, REPLACE keeps the incoming alpha (an RGB texture
    // has none): blended, it shows.
    uint16_t white565[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
    uint32_t nWhite = Texture(RD_FMT_RGB, RD_PIX_565, white565, 2, 2);
    rd_TexEnv(RD_ENV_REPLACE);
    rd_ClearColor(0, 0, 0, 1);
    rd_Clear(RD_CLEAR_COLOR);
    rd_Enable(RD_BLEND);
    rd_BlendFunc(RD_SRC_ALPHA, RD_ONE_MINUS_SRC_ALPHA);
    rd_Color4ub(0, 0, 0, 128);
    Rect(0, 0, 40, 40);
    rd_Disable(RD_BLEND);
    // The blend takes the destination alpha too: 0.5 * 0.5 + 1 * 0.5.
    Check("RGB565 texture, replace keeps colour alpha", Pixel(20, 20), 0xbf808080);

    // Alpha test: alpha 0.3 against 0.5 is not drawn.
    rd_TexEnv(RD_ENV_MODULATE);
    rd_Enable(RD_ALPHA_TEST);
    rd_AlphaGreater(0.5f);
    rd_Color4f(1, 1, 1, 0.3f);
    Rect(50, 0, 90, 40);
    Check("alpha test rejects", Pixel(70, 20), 0xff000000);
    rd_Color4f(1, 1, 1, 0.8f);
    Rect(50, 0, 90, 40);
    Check("alpha test passes", Pixel(70, 20), 0xccffffff);
    rd_Disable(RD_ALPHA_TEST);

    // Two units: the lightmap pass, previous x texture x 2 (combine).
    uint32_t grey[4] = { 0xff606060, 0xff606060, 0xff606060, 0xff606060 };
    rd_ActiveTexture(0);
    rd_BindTexture(nWhite);
    rd_TexEnv(RD_ENV_MODULATE);
    rd_ActiveTexture(1);
    uint32_t nGrey = Texture(RD_FMT_RGBA8, RD_PIX_BGRA, grey, 2, 2);
    rd_Enable(RD_TEXTURE_2D);
    RDCombine c = { RD_OP_MODULATE, RD_ARG_PREVIOUS, RD_ARG_TEXTURE, 2.0f, RD_OP_REPLACE, RD_ARG_PREVIOUS,
                    RD_ARG_PREVIOUS };
    rd_TexCombine(c);
    rd_Color4ub(128, 128, 128, 255);
    Rect(100, 0, 140, 40);
    Check("combine: colour x lightmap x 2", Pixel(120, 20), 0xff606060, 4);
    RDCombine s = { RD_OP_ADD_SIGNED, RD_ARG_PREVIOUS, RD_ARG_TEXTURE, 1.0f, RD_OP_REPLACE, RD_ARG_PREVIOUS,
                    RD_ARG_PREVIOUS };
    rd_TexCombine(s);
    rd_Color4ub(200, 200, 200, 255);
    Rect(100, 0, 140, 40);
    // 200/255 + 0x60/255 - 0.5 = 0.66
    Check("combine: add signed", Pixel(120, 20), 0xffa8a8a8, 4);
    rd_Disable(RD_TEXTURE_2D);
    rd_ActiveTexture(0);
    rd_Disable(RD_TEXTURE_2D);

    // Push/pop: blending and the bound texture come back.
    rd_PushAttrib(RD_ATTR_COLOR | RD_ATTR_TEXTURE);
    rd_Enable(RD_BLEND);
    rd_BindTexture(nBlue);
    rd_PopAttrib();
    rd_Color4ub(10, 20, 30, 40);
    Rect(150, 0, 190, 40);
    Check("pop restores blend off and texture", Pixel(170, 20), 0x280a141e);

    // Fog: linear on eye depth, half way at 50 of 0..100.
    rd_Enable(RD_DEPTH_TEST);
    rd_ClearColor(0, 0, 0, 1);
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    Perspective(1.0f, 1000.0f);
    rd_Enable(RD_FOG);
    rd_FogColor(1, 1, 1);
    rd_FogRange(0, 100);
    rd_Color4ub(0, 0, 0, 255);
    DepthQuad(50, 100);
    rd_Disable(RD_FOG);
    Check("fog half way", Pixel(W / 2, H / 2), 0xff808080, 4);

    // Depth: the nearer of two quads wins.
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    rd_Color4ub(255, 0, 0, 255);
    DepthQuad(10, 20);
    rd_Color4ub(0, 255, 0, 255);
    DepthQuad(20, 40);
    Check("depth test keeps nearer", Pixel(W / 2, H / 2), 0xffff0000);

    // Nearer than the near plane: clipped without clamping, drawn with it.
    Perspective(4.0f, 1000.0f);
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    rd_Color4ub(0, 0, 255, 255);
    DepthQuad(2, 4);
    Check("nearer than near plane is clipped", Pixel(W / 2, H / 2), 0xff000000);
    rd_Enable(RD_DEPTH_CLAMP);
    DepthQuad(2, 4);
    rd_Disable(RD_DEPTH_CLAMP);
    Check("depth clamp draws it", Pixel(W / 2, H / 2), 0xff0000ff);

    // rd_NearDepth: two quads nearer than the near plane keep their order
    // in front of the depth buffer, and stay in front of the scene.
    rd_Clear(RD_CLEAR_COLOR | RD_CLEAR_DEPTH);
    rd_Color4ub(0, 255, 0, 255);
    DepthQuad(10, 20);                   // the scene
    rd_DepthRange(0.1f, 1.0f);           // the scene's depths, as the core keeps 0..0.1 for these
    rd_Enable(RD_DEPTH_CLAMP);
    rd_NearDepth(0.5f, 4.0f, 0.0f, 0.1f);
    rd_Color4ub(255, 0, 0, 255);
    DepthQuad(2, 0.5f);                  // nearer
    rd_Color4ub(0, 0, 255, 255);
    DepthQuad(3, 3.0f);                  // farther, larger: shows around the red
    rd_NearDepth(0, 0, 0, 0);
    rd_Disable(RD_DEPTH_CLAMP);
    rd_DepthRange(0.0f, 1.0f);
    Check("near depth: nearest of near objects", Pixel(W / 2, H / 2), 0xffff0000);
    Check("near depth: farther near object beside it", Pixel(W / 2 + 70, H / 2), 0xff0000ff);
    rd_Color4ub(255, 255, 255, 255);
    DepthQuad(9, 100);                   // nearer than the green scene, farther than the near pass
    Check("near depth: in front of the scene", Pixel(W / 2, H / 2), 0xffff0000);

    // Present, then the front buffer and 5:6:5 reads.
    rd_Present();
    Check("front buffer after present", Pixel(W / 2, H / 2, true), 0xffff0000);
    uint16_t v565 = 0;
    rd_ReadPixels(W / 2, H / 2, 1, 1, RD_PIX_565, &v565);
    printf("%s  %-52s got %04x want f800\n", v565 == 0xf800 ? "PASS" : "FAIL", "565 read", v565);
    s_nFail += v565 == 0xf800 ? 0 : 1;

    rd_DeleteTexture(nBlue);
    rd_DeleteTexture(nWhite);
    rd_DeleteTexture(nGrey);
    rd_DestroyDevice();
}

int main(void)
{
    Display* pDisplay = XOpenDisplay(NULL);
    if (!pDisplay)
    {
        printf("no X display\n");
        return 100;
    }
    Window win = XCreateSimpleWindow(pDisplay, DefaultRootWindow(pDisplay), 0, 0, 640, 360, 0, 0, 0);
    XMapWindow(pDisplay, win);
    XSync(pDisplay, False);
    Run(1, win);
    Run(4, win);
    printf("%d failed\n", s_nFail);
    XDestroyWindow(pDisplay, win);
    XCloseDisplay(pDisplay);
    return s_nFail;
}

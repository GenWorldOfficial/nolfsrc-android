// gl_2d.cpp -- the engine's 2D surfaces on OpenGL: menus, the HUD, fonts,
// fades and the console all arrive here.
//
// As in the Mac renderer (r_CreateSurface, r_BlitToScreen, r_DrawImage,
// r_UpdateTexImage): a surface is CPU memory in the screen pixel format,
// which the engine draws into itself; drawing one to the screen uploads it
// to a texture when it has changed and draws a textured quad. The colour key
// becomes alpha 0 with black colour, so it vanishes under every blend mode
// with the alpha test on. Unlike the Mac renderer, the screen lock
// (FillRect and the engine's software paths onto the screen), BlitFromScreen
// and WarpToScreen work.

#include "glr.h"

struct GLRSurface {
    uint32_t m_nWidth, m_nHeight;
    uint32_t m_nPitch;              // bytes
    uint8_t* m_pPixels;
    GLuint   m_Tex;
    uint32_t m_nTexWidth, m_nTexHeight;   // power-of-two texture holding it
    uint32_t m_nTexGen;             // context the texture belongs to
    bool     m_bDirty;              // pixels changed since the upload
    bool     m_bTexKeyed;           // the upload's keying
    uint32_t m_nTexKey;
};

static uint32_t glr_Pow2(uint32_t n)
{
    uint32_t p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

// ---- state ------------------------------------------------------------------

// Blend modes as both d3d.ren and the Mac renderer map them.
void glr_ApplyBlend(LTSurfaceBlend eBlend)
{
    static const GLenum blend[7][2] = {
        { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA },   // ALPHA
        { GL_ONE,       GL_ZERO },                  // SOLID
        { GL_ONE,       GL_ONE },                   // ADD
        { GL_DST_COLOR, GL_ZERO },                  // MULTIPLY
        { GL_DST_COLOR, GL_SRC_COLOR },             // MULTIPLY2
        { GL_ZERO,      GL_ONE_MINUS_SRC_COLOR },   // MASK
        { GL_ONE,       GL_ONE_MINUS_SRC_COLOR },   // MASKADD
    };
    uint32_t i = (uint32_t)eBlend <= LTSURFACEBLEND_MASKADD ? (uint32_t)eBlend : 0;
    glEnable(GL_BLEND);
    glBlendFunc(blend[i][0], blend[i][1]);
}

// Screen pixels, top-left origin, nothing from the 3D state.
void glr_Begin2D(void)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, g.nWidth, g.nHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glViewport(0, 0, g.nWidth, g.nHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
}

void glr_End2D(void)
{
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

LTBOOL glr_StartOptimized2D(void)
{
    if (!g.bContext || !g.bIn3D)
        return 0;
    g.bInOptimized2D = true;
    return 1;
}

void glr_EndOptimized2D(void)
{
    g.bInOptimized2D = false;
}

// ---- surfaces -------------------------------------------------------------------

void* glr_CreateSurface(uint32_t nWidth, uint32_t nHeight)
{
    if (!nWidth || !nHeight)
        return NULL;
    GLRSurface* s = (GLRSurface*)calloc(1, sizeof(GLRSurface));
    if (!s)
        return NULL;
    s->m_nWidth = nWidth;
    s->m_nHeight = nHeight;
    s->m_nPitch = (nWidth * glr_BytesPerPixel() + 3) & ~3u;
    s->m_pPixels = (uint8_t*)calloc(1, s->m_nPitch * nHeight);
    if (!s->m_pPixels)
    {
        free(s);
        return NULL;
    }
    s->m_bDirty = true;
    return s;
}

static void glr_FreeSurfaceTexture(GLRSurface* s)
{
    // A texture from an earlier context is gone with that context.
    if (s->m_Tex && s->m_nTexGen == g.nContextGen && g.bContext)
        glDeleteTextures(1, &s->m_Tex);
    s->m_Tex = 0;
}

void glr_DeleteSurface(void* pSurface)
{
    GLRSurface* s = (GLRSurface*)pSurface;
    if (!s)
        return;
    glr_FreeSurfaceTexture(s);
    free(s->m_pPixels);
    free(s);
}

void glr_GetSurfaceInfo(void* pSurface, uint32_t* pWidth, uint32_t* pHeight, uint32_t* pPitch)
{
    GLRSurface* s = (GLRSurface*)pSurface;
    if (pWidth)  *pWidth  = s ? s->m_nWidth : 0;
    if (pHeight) *pHeight = s ? s->m_nHeight : 0;
    if (pPitch)  *pPitch  = s ? s->m_nPitch : 0;
}

// Any lock may write, so any lock makes the texture stale (the Mac sets its
// dirty byte here too).
void* glr_LockSurface(void* pSurface)
{
    GLRSurface* s = (GLRSurface*)pSurface;
    if (!s)
        return NULL;
    s->m_bDirty = true;
    return s->m_pPixels;
}

void glr_UnlockSurface(void* pSurface)
{
    (void)pSurface;
}

// Keying is done per blit from the request, so there is nothing to prepare;
// answering TRUE is what lets the engine's ILTClient::OptimizeSurface
// succeed.
LTBOOL glr_OptimizeSurface(void* pSurface, HLTCOLOR hTransColor)
{
    (void)hTransColor;
    return pSurface != NULL;
}

void glr_UnoptimizeSurface(void* pSurface)
{
    (void)pSurface;
}

// Bring the surface's texture up to date for a blit keyed (or not) on nKey.
static bool glr_UploadSurface(GLRSurface* s, bool bKeyed, uint32_t nKey)
{
    if (s->m_Tex && s->m_nTexGen != g.nContextGen)
        s->m_Tex = 0;                           // made by an earlier context
    if (s->m_Tex && !s->m_bDirty && s->m_bTexKeyed == bKeyed && (!bKeyed || s->m_nTexKey == nKey))
        return true;

    uint32_t tw = glr_Pow2(s->m_nWidth), th = glr_Pow2(s->m_nHeight);
    if ((GLint)tw > g.nMaxTextureSize || (GLint)th > g.nMaxTextureSize)
        return false;
    uint32_t* pRGBA = (uint32_t*)calloc(tw * th, 4);    // padding: transparent black
    if (!pRGBA)
        return false;

    if (g.nBitDepth == 16)
        nKey &= 0xffff;
    else
        nKey &= 0x00ffffff;
    for (uint32_t y = 0; y < s->m_nHeight; y++)
    {
        const uint8_t* pRow = s->m_pPixels + y * s->m_nPitch;
        uint32_t* pOut = pRGBA + y * tw;
        if (g.nBitDepth == 16)
        {
            const uint16_t* p = (const uint16_t*)pRow;
            for (uint32_t x = 0; x < s->m_nWidth; x++)
                pOut[x] = (bKeyed && p[x] == nKey) ? 0 : glr_PixelToARGB(p[x]);
        }
        else
        {
            const uint32_t* p = (const uint32_t*)pRow;
            for (uint32_t x = 0; x < s->m_nWidth; x++)
            {
                uint32_t px = p[x] & 0x00ffffff;
                pOut[x] = (bKeyed && px == nKey) ? 0 : (px | 0xff000000);
            }
        }
    }

    if (!s->m_Tex || s->m_nTexWidth != tw || s->m_nTexHeight != th)
    {
        if (s->m_Tex)
            glDeleteTextures(1, &s->m_Tex);
        glGenTextures(1, &s->m_Tex);
        glBindTexture(GL_TEXTURE_2D, s->m_Tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_BGRA, GL_UNSIGNED_BYTE, pRGBA);
        s->m_nTexWidth = tw;
        s->m_nTexHeight = th;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, s->m_Tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw, th, GL_BGRA, GL_UNSIGNED_BYTE, pRGBA);
    }
    free(pRGBA);
    s->m_nTexGen = g.nContextGen;
    s->m_bDirty = false;
    s->m_bTexKeyed = bKeyed;
    s->m_nTexKey = nKey;
    return true;
}

// Draw [src] of a surface's texture onto four screen corners (x,y pairs, in
// the order top-left, top-right, bottom-right, bottom-left), under the
// Optimized-2D colour and blend when that bracket is open.
static void glr_DrawSurfaceQuad(GLRSurface* s, const float* pSrc, const float* pCorners,
                                float fAlpha, bool bFilter)
{
    glr_Begin2D();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s->m_Tex);
    GLint filter = bFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);

    float r = 1.0f, gg = 1.0f, b = 1.0f;
    if (g.bInOptimized2D)
    {
        r = ((g.hOptColor >> 16) & 0xff) / 255.0f;
        gg = ((g.hOptColor >> 8) & 0xff) / 255.0f;
        b = (g.hOptColor & 0xff) / 255.0f;
        glr_ApplyBlend(g.eBlend);
    }
    else
        glr_ApplyBlend(LTSURFACEBLEND_ALPHA);
    if (fAlpha < 0.0f) fAlpha = 0.0f;
    if (fAlpha > 1.0f) fAlpha = 1.0f;
    glColor4f(r, gg, b, fAlpha);

    float u0 = pSrc[0] / s->m_nTexWidth, v0 = pSrc[1] / s->m_nTexHeight;
    float u1 = pSrc[2] / s->m_nTexWidth, v1 = pSrc[3] / s->m_nTexHeight;
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(pCorners[0], pCorners[1]);
    glTexCoord2f(u1, v0); glVertex2f(pCorners[2], pCorners[3]);
    glTexCoord2f(u1, v1); glVertex2f(pCorners[4], pCorners[5]);
    glTexCoord2f(u0, v1); glVertex2f(pCorners[6], pCorners[7]);
    glEnd();
    glr_End2D();
}

// DrawSurfaceToSurface[Transparent] and ScaleSurfaceToSurface[Transparent]
// onto the screen. Rects come clipped; a scaled blit stretches.
void glr_BlitToScreen(RenderBlitRequest* pRequest)
{
    if (!g.bContext || !pRequest || !pRequest->m_pSurfaceData || g.pScreenLock)
        return;
    GLRSurface* s = (GLRSurface*)pRequest->m_pSurfaceData;
    const LTRect* src = pRequest->m_pSrcRect;
    const LTRect* dst = pRequest->m_pDestRect;
    if (!src || !dst || src->right <= src->left || src->bottom <= src->top)
        return;
    glr_Debug(4, "blit %ux%u (%d,%d,%d,%d)->(%d,%d,%d,%d) key %u:%06x alpha %.2f 3d %d opt %d blend %d col %08x",
              s->m_nWidth, s->m_nHeight, src->left, src->top, src->right, src->bottom,
              dst->left, dst->top, dst->right, dst->bottom, pRequest->m_bTransparent,
              pRequest->m_TransColor, pRequest->m_fAlpha, g.bIn3D, g.bInOptimized2D, g.eBlend, g.hOptColor);
    if (!glr_UploadSurface(s, (pRequest->m_bTransparent & 1) != 0, pRequest->m_TransColor))
        return;

    float fSrc[4] = { (float)src->left, (float)src->top, (float)src->right, (float)src->bottom };
    float x0 = (float)dst->left, y0 = (float)dst->top, x1 = (float)dst->right, y1 = (float)dst->bottom;
    float fCorners[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    bool bScaled = (dst->right - dst->left) != (src->right - src->left) ||
                   (dst->bottom - dst->top) != (src->bottom - src->top);
    glr_DrawSurfaceQuad(s, fSrc, fCorners, pRequest->m_fAlpha,
                        bScaled && cvi(CV_FilterOptimized));
}

// TransformSurfaceToSurface onto the screen: four warp points, the source
// corners onto the destination corners. Points 0..3 run around the quad.
LTBOOL glr_WarpToScreen(RenderBlitRequest* pRequest)
{
    if (!g.bContext || !pRequest || !pRequest->m_pSurfaceData || !pRequest->m_pPoints ||
        pRequest->m_nPoints != 4 || g.pScreenLock)
        return 0;
    GLRSurface* s = (GLRSurface*)pRequest->m_pSurfaceData;
    if (!glr_UploadSurface(s, (pRequest->m_bTransparent & 1) != 0, pRequest->m_TransColor))
        return 0;
    const LTWarpPt* p = pRequest->m_pPoints;
    glr_Begin2D();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s->m_Tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    if (g.bInOptimized2D)
    {
        glr_ApplyBlend(g.eBlend);
        glColor4f(((g.hOptColor >> 16) & 0xff) / 255.0f, ((g.hOptColor >> 8) & 0xff) / 255.0f,
                  (g.hOptColor & 0xff) / 255.0f, pRequest->m_fAlpha);
    }
    else
    {
        glr_ApplyBlend(LTSURFACEBLEND_ALPHA);
        glColor4f(1, 1, 1, pRequest->m_fAlpha);
    }
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++)
    {
        glTexCoord2f((p[i].source_x + 0.5f) / s->m_nTexWidth, (p[i].source_y + 0.5f) / s->m_nTexHeight);
        glVertex2f(p[i].dest_x, p[i].dest_y);
    }
    glEnd();
    glr_End2D();
    return 1;
}

// ---- the screen as pixels ------------------------------------------------------------

// The engine's software paths onto the screen (FillRect, solid-colour and
// masked draws, text drawn straight to the screen) lock it, always whole.
// The back buffer is read into a top-down buffer in the screen format and
// written back on unlock.
LTBOOL glr_LockScreen(int left, int top, int right, int bottom, void** ppPixels, uint32_t* pPitch)
{
    glr_Debug(4, "lockscreen (%d,%d,%d,%d)", left, top, right, bottom);
    if (!g.bContext || g.pScreenLock || !ppPixels || !pPitch)
        return 0;
    uint32_t bpp = glr_BytesPerPixel();
    uint32_t nPitch = g.nWidth * bpp;
    uint8_t* pBuf = (uint8_t*)malloc(nPitch * g.nHeight);
    uint8_t* pRead = (uint8_t*)malloc(nPitch * g.nHeight);
    if (!pBuf || !pRead)
    {
        free(pBuf);
        free(pRead);
        return 0;
    }
    glReadBuffer(GL_BACK);
    if (bpp == 2)
        glReadPixels(0, 0, g.nWidth, g.nHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pRead);
    else
        glReadPixels(0, 0, g.nWidth, g.nHeight, GL_BGRA, GL_UNSIGNED_BYTE, pRead);
    for (uint32_t y = 0; y < g.nHeight; y++)
        memcpy(pBuf + y * nPitch, pRead + (g.nHeight - 1 - y) * nPitch, nPitch);
    free(pRead);
    if (bpp == 4)
    {
        uint32_t* p = (uint32_t*)pBuf;
        for (uint32_t i = 0; i < g.nWidth * g.nHeight; i++)
            p[i] &= 0x00ffffff;                 // X8R8G8B8: no alpha in the format
    }
    g.pScreenLock = pBuf;
    *ppPixels = pBuf;
    *pPitch = nPitch;
    return 1;
}

void glr_UnlockScreen(void)
{
    if (!g.pScreenLock)
        return;
    uint8_t* pBuf = g.pScreenLock;
    g.pScreenLock = NULL;
    if (!g.bContext)
    {
        free(pBuf);
        return;
    }
    uint32_t tw = glr_Pow2(g.nWidth), th = glr_Pow2(g.nHeight);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    if (g.nBitDepth == 16)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tw, th, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g.nWidth, g.nHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pBuf);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, tw, th, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g.nWidth, g.nHeight, GL_BGRA, GL_UNSIGNED_BYTE, pBuf);
    }
    free(pBuf);

    glr_Begin2D();
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    float u = (float)g.nWidth / tw, v = (float)g.nHeight / th;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(u, 0); glVertex2f((float)g.nWidth, 0);
    glTexCoord2f(u, v); glVertex2f((float)g.nWidth, (float)g.nHeight);
    glTexCoord2f(0, v); glVertex2f(0, (float)g.nHeight);
    glEnd();
    glr_End2D();
    glDeleteTextures(1, &tex);
}

// The modern engine's FillRect on the screen (MODERN.md M-P0-03): a
// scissored clear instead of a whole-screen lock. nPixel is in the screen
// format.
LTBOOL glr_FillScreenRect(LTRect* pRect, uint32_t nPixel)
{
    if (pRect)
        glr_Debug(4, "fill (%d,%d,%d,%d) %08x", pRect->left, pRect->top, pRect->right, pRect->bottom, nPixel);
    if (!g.bContext || !pRect || g.pScreenLock)
        return 0;
    int l = pRect->left < 0 ? 0 : pRect->left, t = pRect->top < 0 ? 0 : pRect->top;
    int r = pRect->right > (int)g.nWidth ? (int)g.nWidth : pRect->right;
    int b = pRect->bottom > (int)g.nHeight ? (int)g.nHeight : pRect->bottom;
    if (r <= l || b <= t)
        return 1;
    uint32_t argb = glr_PixelToARGB(nPixel);
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(l, g.nHeight - b, r - l, b - t);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(((argb >> 16) & 0xff) / 255.0f, ((argb >> 8) & 0xff) / 255.0f,
                 (argb & 0xff) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glPopAttrib();
    return 1;
}

// DrawSurfaceToSurface[Transparent] from the screen into a surface: the rect
// read back and converted, keyed pixels (on the screen's colour) left as
// they were.
void glr_BlitFromScreen(RenderBlitRequest* pRequest)
{
    if (!g.bContext || !pRequest || !pRequest->m_pSurfaceData || !pRequest->m_pSrcRect ||
        !pRequest->m_pDestRect || g.pScreenLock)
        return;
    GLRSurface* s = (GLRSurface*)pRequest->m_pSurfaceData;
    const LTRect* src = pRequest->m_pSrcRect;
    const LTRect* dst = pRequest->m_pDestRect;
    int w = src->right - src->left, h = src->bottom - src->top;
    if (w <= 0 || h <= 0)
        return;
    uint32_t bpp = glr_BytesPerPixel();
    uint8_t* pRead = (uint8_t*)malloc(w * h * bpp);
    if (!pRead)
        return;
    glReadBuffer(GL_BACK);
    if (bpp == 2)
        glReadPixels(src->left, g.nHeight - src->bottom, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pRead);
    else
        glReadPixels(src->left, g.nHeight - src->bottom, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pRead);
    bool bKeyed = (pRequest->m_bTransparent & 1) != 0;
    uint32_t nKey = pRequest->m_TransColor & (bpp == 2 ? 0xffff : 0x00ffffff);
    for (int y = 0; y < h; y++)
    {
        int dy = dst->top + y;
        if (dy < 0 || dy >= (int)s->m_nHeight)
            continue;
        const uint8_t* pIn = pRead + (h - 1 - y) * w * bpp;
        uint8_t* pOut = s->m_pPixels + dy * s->m_nPitch;
        for (int x = 0; x < w; x++)
        {
            int dx = dst->left + x;
            if (dx < 0 || dx >= (int)s->m_nWidth)
                continue;
            uint32_t px = bpp == 2 ? ((const uint16_t*)pIn)[x] : (((const uint32_t*)pIn)[x] & 0x00ffffff);
            if (bKeyed && px == nKey)
                continue;
            if (bpp == 2)
                ((uint16_t*)pOut)[dx] = (uint16_t)px;
            else
                ((uint32_t*)pOut)[dx] = px;
        }
    }
    free(pRead);
    s->m_bDirty = true;
}

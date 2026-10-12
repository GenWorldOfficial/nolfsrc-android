// gl_main.cpp -- opengl.ren's exports, device and frame: Init/Term (a GL
// context on the engine's window), the mode list, clearing and flipping,
// console variables and the small slots.
//
// The shape follows the Mac release's opengl_init.cpp (r_Init, r_Term,
// r_Clear, r_SwapBuffers, glRenderDLLSetup, glGetSupportedModes), with AGL
// and DrawSprocket replaced by glplatform.h (gl_wgl.cpp on Windows), and
// the behaviour the PC engine expects from d3d.ren where the two differ.

#include "glr.h"

GLRGlobals g;

// ---- console ---------------------------------------------------------------

void glr_Print(const char* pFormat, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(buf, sizeof(buf), pFormat, ap);
    va_end(ap);
    if (g.pStruct && g.pStruct->ConsolePrint)
        g.pStruct->ConsolePrint("%s", buf);
}

// RenderDebug 1+ goes to the console; RenderDebug 3+ also to opengl.log
// beside the game, which survives a crash.
void glr_Debug(int nLevel, const char* pFormat, ...)
{
    if (cvi(CV_RenderDebug) < nLevel)
        return;
    char buf[512];
    va_list ap;
    va_start(ap, pFormat);
    vsnprintf(buf, sizeof(buf), pFormat, ap);
    va_end(ap);
    glr_Print("opengl.ren: %s", buf);
    if (cvi(CV_RenderDebug) >= 3)
    {
        static FILE* f;
        if (!f)
            f = fopen("opengl.log", "w");
        if (f)
        {
            fprintf(f, "%s\n", buf);
            fflush(f);
        }
    }
}

// The names and defaults d3d.ren and the Mac renderer register, where they
// share one; VSync is ours.
GLRVar g_Vars[CV_Count] = {
    { "RenderDebug", 0 }, { "Bilinear", 1 }, { "Trilinear", 0 }, { "Anisotropic", 0 },
    { "MipmapOffset", 0 }, { "FilterOptimized", 0 }, { "MaxTextureSize", 16384 },
    { "S3TCEnable", 1 },
    { "GroupOffset0", 0 }, { "GroupOffset1", 0 }, { "GroupOffset2", 0 }, { "GroupOffset3", 0 },
    { "GroupOffset4", 0 }, { "GroupOffset5", 0 }, { "GroupOffset6", 0 }, { "GroupOffset7", 0 },
    { "GroupOffset8", 0 }, { "GroupOffset9", 0 },
    { "VSync", 0 }, { "LightMap", 1 },
    { "DrawWorld", 1 }, { "DrawSky", 1 }, { "DrawWorldModels", 1 }, { "DrawAll", 0 },
    { "SkyScale", 1 }, { "LightAddPoly", 1 },
    { "FogEnable", 0 }, { "FogR", 255 }, { "FogG", 255 }, { "FogB", 255 },
    { "FogNearZ", 0 }, { "FogFarZ", 2000 },
    { "SkyFogEnable", 1 }, { "SkyFogNearZ", 0 }, { "SkyFogFarZ", 2000 },
    { "FixTJunc", 0 }, { "DrawSorted", 1 },
    { "Saturate", 0 }, { "DetailTextures", 1 }, { "DetailTextureAdd", 1 }, { "DetailTextureScale", 0.2f },
    { "DrawModels", 1 }, { "DrawGuns", 1 }, { "TextureModels", 1 }, { "ModelSaturation", 2 },
    { "MaxModelLights", 4 }, { "ModelLODOffset", 0 }, { "DrawSprites", 1 },
    { "32BitTextures", 0 },
    { "DrawParticles", 1 }, { "DrawPolyGrids", 1 }, { "DrawLineSystems", 1 }, { "DrawCanvases", 1 },
    { "DynamicLight", 1 }, { "LightSaturate", 1 }, { "FastLight", 0 },
    { "ModelApplySun", 1 }, { "ModelSunVariance", 0.05f },
    { "EnvMapWorld", 1 }, { "EnvMapEnable", 0 }, { "EnvMapAll", 0 }, { "EnvScale", 1 },
    { "EnvPanSpeed", 0.0005f }, { "EnvMapPolyGrids", 1 },
};

// Find a variable, creating it with its default if the engine has none
// (r_MaybeCreateCVar).
static HCONSOLEVAR glr_FindVar(GLRVar* pVar)
{
    HCONSOLEVAR h = g.pStruct->GetParameter(pVar->m_pName);
    if (!h)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %f", pVar->m_pName, pVar->m_fDefault);
        g.pStruct->RunConsoleString(buf);
        h = g.pStruct->GetParameter(pVar->m_pName);
    }
    return h;
}

void glr_RegisterVars(void)
{
    for (int i = 0; i < CV_Count; i++)
        g_Vars[i].m_fValue = g_Vars[i].m_fDefault;
    if (!g.pStruct || !g.pStruct->GetParameter)
        return;
    for (int i = 0; i < CV_Count; i++)
        glr_FindVar(&g_Vars[i]);
}

void glr_ReadConsoleVariables(void)
{
    if (!g.pStruct || !g.pStruct->GetParameter)
        return;
    for (int i = 0; i < CV_Count; i++)
    {
        HCONSOLEVAR h = g.pStruct->GetParameter(g_Vars[i].m_pName);
        g_Vars[i].m_fValue = h ? g.pStruct->GetParameterValueFloat(h) : g_Vars[i].m_fDefault;
    }
}

const char* glr_GetVarString(const char* pName)
{
    HCONSOLEVAR h = g.pStruct->GetParameter(pName);
    return h ? g.pStruct->GetParameterValueString(h) : NULL;
}

// ---- the mode list -----------------------------------------------------------

#define GLR_CARD "OpenGL"            // RMode::m_InternalName: no spaces
#define GLR_DESC "OpenGL"            // RMode::m_Description

static void glr_FillMode(RMode* pMode, uint32_t w, uint32_t h, uint32_t bpp)
{
    memset(pMode, 0, sizeof(*pMode));
    pMode->m_bHardware = 1;
    strcpy(pMode->m_RenderDLL, "opengl.ren");
    strcpy(pMode->m_InternalName, GLR_CARD);
    strcpy(pMode->m_Description, GLR_DESC);
    pMode->m_Width = w;
    pMode->m_Height = h;
    pMode->m_BitDepth = bpp;
}

struct GLRModeList { RMode* m_pHead; RMode* m_pTail; };

static void glr_AddMode(uint32_t w, uint32_t h, uint32_t bpp, void* pUser)
{
    GLRModeList* pList = (GLRModeList*)pUser;
    if ((bpp != 16 && bpp != 32) || w < 640 || h < 480)
        return;
    for (RMode* p = pList->m_pHead; p; p = p->m_pNext)
        if (p->m_Width == w && p->m_Height == h && p->m_BitDepth == bpp)
            return;
    RMode* pMode = (RMode*)malloc(sizeof(RMode));
    if (!pMode)
        return;
    glr_FillMode(pMode, w, h, bpp);
    if (pList->m_pTail)
        pList->m_pTail->m_pNext = pMode;
    else
        pList->m_pHead = pMode;
    pList->m_pTail = pMode;
}

// Every display mode the system offers at 16 and 32 bits, 640x480 and up
// -- what d3d.ren's DirectDraw enumeration yields. Called with no device
// up: nothing here touches GL.
extern "C" RMode* GetSupportedModes(void)
{
    GLRModeList list = { NULL, NULL };
    plat_EnumModes(glr_AddMode, &list);
    if (!list.m_pHead)
    {
        // No list at all: offer the one mode every setup has.
        list.m_pHead = (RMode*)malloc(sizeof(RMode));
        if (list.m_pHead)
            glr_FillMode(list.m_pHead, 640, 480, 32);
    }
    return list.m_pHead;
}

extern "C" void FreeModeList(RMode* pHead)
{
    while (pHead)
    {
        RMode* pNext = pHead->m_pNext;
        free(pHead);
        pHead = pNext;
    }
}

// ---- the context ------------------------------------------------------------------
//
// The window and the context are glplatform.h's; what the renderer needs
// to know of the context is read here.

static bool glr_CreateGLContext(void)
{
    if (!plat_CreateContext())
        return false;
    g.bContext = true;
    const char* pExt = (const char*)glGetString(GL_EXTENSIONS);
    if (!pExt)
        pExt = "";
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g.nMaxTextureSize);
    g.glCompressedTexImage2D = (PFN_glCompressedTexImage2D)plat_GetProcAddress("glCompressedTexImage2DARB");
    g.bS3TC = strstr(pExt, "GL_EXT_texture_compression_s3tc") && g.glCompressedTexImage2D;
    g.bAniso = strstr(pExt, "GL_EXT_texture_filter_anisotropic") != NULL;
    g.fMaxAniso = 1.0f;
    if (g.bAniso)
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g.fMaxAniso);
    plat_SetSwapInterval(cvi(CV_VSync) ? 1 : 0);
    return true;
}

static void glr_DestroyGLContext(void)
{
    plat_DestroyContext();
    g.bContext = false;
}

// The state every draw starts from (r_SetStartingRenderStates).
static void glr_SetStartingStates(void)
{
    glViewport(0, 0, g.nWidth, g.nHeight);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// ---- the slots ---------------------------------------------------------------

static void r_Term(void);

static int r_Init(RenderStructInit* pInit)
{
    pInit->m_nVersion = RENDERSTRUCT_VERSION;

    glr_RegisterVars();
    glr_ReadConsoleVariables();

    g.hWnd = pInit->m_hWnd;
    g.nWidth = pInit->m_Mode.m_Width;
    g.nHeight = pInit->m_Mode.m_Height;
    g.nBitDepth = pInit->m_Mode.m_BitDepth == 16 ? 16 : 32;
    const char* pWindowed = glr_GetVarString("windowed");
    g.bWindowed = pWindowed && atoi(pWindowed) == 1;
    g.bIn3D = g.bInOptimized2D = false;
    g.eBlend = LTSURFACEBLEND_ALPHA;
    g.hOptColor = 0xffffffff;
    g.nContextGen++;

    if (!g.hWnd || !g.nWidth || !g.nHeight)
        return 1;

    plat_OpenWindow(g.hWnd, g.nWidth, g.nHeight, &g.bWindowed);
    if (!glr_CreateGLContext())
    {
        r_Term();
        return 1;
    }
    glr_SetStartingStates();

    // The names GetSupportedModes reports, so the display menu can find the
    // current mode in its list again.
    strcpy(pInit->m_Mode.m_InternalName, GLR_CARD);
    strcpy(pInit->m_Mode.m_Description, GLR_DESC);
    pInit->m_Mode.m_BitDepth = g.nBitDepth;
    g.pStruct->m_Width = g.nWidth;
    g.pStruct->m_Height = g.nHeight;

    glr_Print("opengl.ren: %ux%ux%u %s, %s (%s)", g.nWidth, g.nHeight, g.nBitDepth,
              g.bWindowed ? "windowed" : "full screen",
              (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
    return 0;
}

static void r_Term(void)
{
    if (g.pScreenLock)
    {
        free(g.pScreenLock);
        g.pScreenLock = NULL;
    }
    if (g.bContext)
    {
        glFlush();
        glFinish();
    }
    glr_DestroyGLContext();
    plat_CloseWindow();
    g.bIn3D = g.bInOptimized2D = false;
}

// The engine always passes a rect, clamped to the screen.
static void r_Clear(LTRect* pRect, uint32_t nFlags)
{
    if (!g.bContext)
        return;
    GLbitfield bits = 0;
    if (nFlags & CLEARSCREEN_SCREEN)
        bits |= GL_COLOR_BUFFER_BIT;
    if (nFlags & CLEARSCREEN_RENDER)
        bits |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (!bits)
        return;
    bool bFull = !pRect || (pRect->left <= 0 && pRect->top <= 0 &&
                            pRect->right >= (int)g.nWidth && pRect->bottom >= (int)g.nHeight);
    if (!bFull)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(pRect->left, g.nHeight - pRect->bottom,
                  pRect->right - pRect->left, pRect->bottom - pRect->top);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(bits);
    if (!bFull)
        glDisable(GL_SCISSOR_TEST);
}

static LTBOOL r_Start3D(void)
{
    g.bIn3D = true;
    return 1;
}

static LTBOOL r_End3D(void)
{
    if (g.bInOptimized2D)
        glr_EndOptimized2D();
    g.bIn3D = false;
    return 1;
}

static LTBOOL r_IsIn3D(void) { return g.bIn3D; }
static LTBOOL r_IsInOptimized2D(void) { return g.bInOptimized2D; }

static LTBOOL r_SetOptimized2DBlend(LTSurfaceBlend eBlend)
{
    g.eBlend = eBlend;
    if ((uint32_t)eBlend > LTSURFACEBLEND_MASKADD)
        return 0;
    if (g.bInOptimized2D)
        glr_ApplyBlend(eBlend);
    return 1;
}

static LTBOOL r_GetOptimized2DBlend(LTSurfaceBlend* pBlend)
{
    if (pBlend)
        *pBlend = g.eBlend;
    return 1;
}

static LTBOOL r_SetOptimized2DColor(HLTCOLOR hColor)
{
    g.hOptColor = (hColor & 0x00ffffff) | 0xff000000;
    return 1;
}

static LTBOOL r_GetOptimized2DColor(HLTCOLOR* pColor)
{
    if (pColor)
        *pColor = g.hOptColor;
    return 1;
}

static void r_RenderCommand(int argc, char** argv)
{
    if (argc < 1 || !g.bContext)
        return;
    if (!_stricmp(argv[0], "info") || !_stricmp(argv[0], "gl_strings"))
    {
        glr_Print("GL_VENDOR: %s", (const char*)glGetString(GL_VENDOR));
        glr_Print("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));
        glr_Print("GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
        glr_Print("max texture %d, S3TC %s, anisotropy %s (%.0f)", g.nMaxTextureSize,
                  g.bS3TC ? "yes" : "no", g.bAniso ? "yes" : "no", g.fMaxAniso);
    }
}

// Named objects for the engine. The engine's own askers want DirectDraw
// ("LPDIRECTDRAW", "BACKBUFFER": the screen-mode movie players), which an
// OpenGL renderer has none of, so they get NULL and the movie is skipped.
// The modern engine asks for "FillScreenRect" (MODERN.md M-P0-03).
static void* r_GetHook(const char* pName)
{
    if (pName && !_stricmp(pName, "FillScreenRect"))
        return (void*)&glr_FillScreenRect;
    return NULL;
}

static void r_SwapBuffers(uint32_t nFlags)
{
    (void)nFlags;
    if (g.bContext)
        plat_SwapBuffers();
}

static uint32_t r_GetInfoFlags(void) { return 0; }

// X8R8G8B8 at 32 bits and 565 at 16: the format every engine surface and
// the screen lock are in. The masks, bit counts and shifts are what the
// engine's converters read, so all three are filled.
static void r_GetScreenFormat(PFormat* pFormat)
{
    if (g.nBitDepth == 16)
    {
        static const uint32_t m[4] = { 0, 0xf800, 0x07e0, 0x001f };
        static const uint32_t b[4] = { 0, 5, 6, 5 }, s[4] = { 32, 11, 5, 0 };
        pFormat->m_nType = BPP_16;
        memcpy(pFormat->m_Masks, m, sizeof(m));
        memcpy(pFormat->m_nBits, b, sizeof(b));
        memcpy(pFormat->m_nShifts, s, sizeof(s));
    }
    else
    {
        static const uint32_t m[4] = { 0, 0x00ff0000, 0x0000ff00, 0x000000ff };
        static const uint32_t b[4] = { 0, 8, 8, 8 }, s[4] = { 32, 16, 8, 0 };
        pFormat->m_nType = BPP_32;
        memcpy(pFormat->m_Masks, m, sizeof(m));
        memcpy(pFormat->m_nBits, b, sizeof(b));
        memcpy(pFormat->m_nShifts, s, sizeof(s));
    }
}

// F8: the front buffer as a 24-bit .bmp. Bottom-up rows are what both
// glReadPixels and BMP use.
static void r_MakeScreenShot(const char* pFilename)
{
    if (!g.bContext || !pFilename)
        return;
    uint32_t nPitch = (g.nWidth * 3 + 3) & ~3u;
    uint8_t* pBits = (uint8_t*)malloc(nPitch * g.nHeight);
    if (!pBits)
        return;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, g.nWidth, g.nHeight, 0x80E0 /* GL_BGR */, GL_UNSIGNED_BYTE, pBits);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    FILE* f = fopen(pFilename, "wb");
    if (f)
    {
        BITMAPFILEHEADER fh;
        BITMAPINFOHEADER ih;
        memset(&fh, 0, sizeof(fh));
        memset(&ih, 0, sizeof(ih));
        fh.bfType = 0x4d42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + nPitch * g.nHeight;
        ih.biSize = sizeof(ih);
        ih.biWidth = g.nWidth;
        ih.biHeight = g.nHeight;
        ih.biPlanes = 1;
        ih.biBitCount = 24;
        ih.biCompression = BI_RGB;
        ih.biSizeImage = nPitch * g.nHeight;
        fwrite(&fh, sizeof(fh), 1, f);
        fwrite(&ih, sizeof(ih), 1, f);
        fwrite(pBits, nPitch, g.nHeight, f);
        fclose(f);
        glr_Print("Wrote screenshot %s", pFilename);
    }
    free(pBits);
}

static void r_ReadConsoleVariables(void)
{
    glr_ReadConsoleVariables();
}

// ---- the export --------------------------------------------------------------

extern "C" void RenderDLLSetup(RenderStruct* pStruct)
{
    g.pStruct = pStruct;

    pStruct->Init                 = r_Init;
    pStruct->Term                 = r_Term;
    pStruct->BindTexture          = glr_BindTexture;
    pStruct->UnbindTexture        = glr_UnbindTexture;
    pStruct->CreateContext        = glr_CreateContext;
    pStruct->DeleteContext        = glr_DeleteContext;
    pStruct->Clear                = r_Clear;
    pStruct->Start3D              = r_Start3D;
    pStruct->End3D                = r_End3D;
    pStruct->IsIn3D               = r_IsIn3D;
    pStruct->StartOptimized2D     = glr_StartOptimized2D;
    pStruct->EndOptimized2D       = glr_EndOptimized2D;
    pStruct->IsInOptimized2D      = r_IsInOptimized2D;
    pStruct->SetOptimized2DBlend  = r_SetOptimized2DBlend;
    pStruct->GetOptimized2DBlend  = r_GetOptimized2DBlend;
    pStruct->SetOptimized2DColor  = r_SetOptimized2DColor;
    pStruct->GetOptimized2DColor  = r_GetOptimized2DColor;
    pStruct->RenderScene          = glr_RenderScene;
    pStruct->RenderCommand        = r_RenderCommand;
    pStruct->GetHook              = r_GetHook;
    pStruct->SwapBuffers          = r_SwapBuffers;
    pStruct->GetInfoFlags         = r_GetInfoFlags;
    pStruct->GetScreenFormat      = r_GetScreenFormat;
    pStruct->CreateSurface        = glr_CreateSurface;
    pStruct->DeleteSurface        = glr_DeleteSurface;
    pStruct->GetSurfaceInfo       = glr_GetSurfaceInfo;
    pStruct->LockSurface          = glr_LockSurface;
    pStruct->UnlockSurface        = glr_UnlockSurface;
    pStruct->OptimizeSurface      = glr_OptimizeSurface;
    pStruct->UnoptimizeSurface    = glr_UnoptimizeSurface;
    pStruct->LockScreen           = glr_LockScreen;
    pStruct->UnlockScreen         = glr_UnlockScreen;
    pStruct->BlitToScreen         = glr_BlitToScreen;
    pStruct->WarpToScreen         = glr_WarpToScreen;
    pStruct->MakeScreenShot       = r_MakeScreenShot;
    pStruct->ReadConsoleVariables = r_ReadConsoleVariables;
    pStruct->BlitFromScreen       = glr_BlitFromScreen;
}

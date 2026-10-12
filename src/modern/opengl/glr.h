// glr.h -- opengl.ren's own declarations, shared by its files.
//
// The renderer is fixed-function OpenGL 1.x with a few extensions, which is
// what the Mac release's renderer it follows uses, and what every OpenGL a
// Wine prefix can reach provides. See README.md for the plan.

#ifndef GLR_H
#define GLR_H

#include <windows.h>
#include <GL/gl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ltrender.h"

// GL 1.2+ and extension tokens the mingw-w64 GL 1.1 header lacks.
#ifndef GL_BGRA
#define GL_BGRA                         0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                0x812F
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5         0x8363
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL            0x813D
#endif
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_TEXTURE_MAX_ANISOTROPY_EXT   0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

typedef void (APIENTRY* PFN_glCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei,
                                                     GLsizei, GLint, GLsizei, const void*);
typedef BOOL (APIENTRY* PFN_wglSwapInterval)(int);

// ---- the device ------------------------------------------------------------

struct GLRGlobals {
    RenderStruct* pStruct;          // the engine's table (RenderDLLSetup)
    HWND     hWnd;
    HDC      hDC;
    HGLRC    hRC;
    uint32_t nWidth, nHeight;       // the mode
    uint32_t nBitDepth;             // 16 or 32: the format engine surfaces are in
    bool     bWindowed;
    bool     bChangedDisplay;       // ChangeDisplaySettings to undo in Term
    LONG     nOldStyle, nOldExStyle;
    RECT     rcOldWindow;

    bool     bIn3D;
    bool     bInOptimized2D;
    LTSurfaceBlend eBlend;          // Optimized-2D blend, as last set
    HLTCOLOR hOptColor;             // Optimized-2D colour, 0xffRRGGBB

    GLint    nMaxTextureSize;
    bool     bS3TC;                 // GL_EXT_texture_compression_s3tc
    bool     bAniso;
    float    fMaxAniso;
    PFN_glCompressedTexImage2D glCompressedTexImage2D;

    uint32_t nContextGen;           // bumped by every Init: textures made before are gone

    uint8_t* pScreenLock;           // LockScreen's buffer, NULL when not locked
};
extern GLRGlobals g;

// Console output through the engine; RenderDebug-gated chatter.
void glr_Print(const char* pFormat, ...);
void glr_Debug(int nLevel, const char* pFormat, ...);

// ---- console variables -----------------------------------------------------

struct GLRVar {
    const char* m_pName;
    float       m_fDefault;
    float       m_fValue;
};
// The renderer's variables, registered with the engine (created if missing)
// at Init and re-read by ReadConsoleVariables.
enum {
    CV_RenderDebug, CV_Bilinear, CV_Trilinear, CV_Anisotropic, CV_MipmapOffset,
    CV_FilterOptimized, CV_MaxTextureSize, CV_S3TCEnable,
    CV_GroupOffset0, CV_GroupOffset9 = CV_GroupOffset0 + 9,
    CV_VSync, CV_LightMap,
    CV_DrawWorld, CV_DrawSky, CV_DrawWorldModels, CV_DrawAll, CV_SkyScale, CV_LightAddPoly,
    CV_FogEnable, CV_FogR, CV_FogG, CV_FogB, CV_FogNearZ, CV_FogFarZ,
    CV_SkyFogEnable, CV_SkyFogNearZ, CV_SkyFogFarZ, CV_FixTJunc, CV_DrawSorted,
    CV_Saturate, CV_DetailTextures, CV_DetailTextureAdd, CV_DetailTextureScale,
    CV_DrawModels, CV_DrawGuns, CV_TextureModels, CV_ModelSaturation, CV_MaxModelLights,
    CV_ModelLODOffset, CV_DrawSprites, CV_32BitTextures,
    CV_DrawParticles, CV_DrawPolyGrids, CV_DrawLineSystems, CV_DrawCanvases, CV_DynamicLight,
    CV_Count
};
extern GLRVar g_Vars[CV_Count];
inline float cvf(int i) { return g_Vars[i].m_fValue; }
inline int   cvi(int i) { return (int)g_Vars[i].m_fValue; }
void glr_RegisterVars(void);
void glr_ReadConsoleVariables(void);
const char* glr_GetVarString(const char* pName);

// ---- pixels in the screen format ---------------------------------------------

inline uint32_t glr_BytesPerPixel(void) { return g.nBitDepth == 16 ? 2 : 4; }
// One screen-format pixel to 0xAARRGGBB with alpha 0xff.
inline uint32_t glr_PixelToARGB(uint32_t p)
{
    if (g.nBitDepth == 16)
    {
        uint32_t r = (p >> 11) & 0x1f, gg = (p >> 5) & 0x3f, b = p & 0x1f;
        return 0xff000000 | ((r << 3 | r >> 2) << 16) | ((gg << 2 | gg >> 4) << 8) | (b << 3 | b >> 2);
    }
    return 0xff000000 | (p & 0x00ffffff);
}

// ---- 2D (gl_2d.cpp) ------------------------------------------------------------

void  glr_Begin2D(void);            // ortho over the screen, depth/fog off
void  glr_End2D(void);
void  glr_ApplyBlend(LTSurfaceBlend eBlend);
void* glr_CreateSurface(uint32_t nWidth, uint32_t nHeight);
void  glr_DeleteSurface(void* pSurface);
void  glr_GetSurfaceInfo(void* pSurface, uint32_t* pWidth, uint32_t* pHeight, uint32_t* pPitch);
void* glr_LockSurface(void* pSurface);
void  glr_UnlockSurface(void* pSurface);
LTBOOL glr_OptimizeSurface(void* pSurface, HLTCOLOR hTransColor);
void  glr_UnoptimizeSurface(void* pSurface);
LTBOOL glr_LockScreen(int left, int top, int right, int bottom, void** ppPixels, uint32_t* pPitch);
void  glr_UnlockScreen(void);
void  glr_BlitToScreen(RenderBlitRequest* pRequest);
LTBOOL glr_WarpToScreen(RenderBlitRequest* pRequest);
void  glr_BlitFromScreen(RenderBlitRequest* pRequest);
LTBOOL glr_StartOptimized2D(void);
void  glr_EndOptimized2D(void);
LTBOOL glr_FillScreenRect(LTRect* pRect, uint32_t nPixel);

// ---- textures (gl_texture.cpp) -------------------------------------------------

struct GLRTexture {
    GLuint   m_Id;
    uint32_t m_nWidth, m_nHeight;   // of the first level uploaded
    uint32_t m_nBaseWidth, m_nBaseHeight;   // of the full-size image (TextureData mip 0)
    uint32_t m_DtxFlags;            // DTX flags (DTX_FULLBRITE, ...)
    uint32_t m_nBytes;
    bool     m_bAlpha;              // the data carries an alpha channel worth testing
    // World texture coordinates are in texels of the full-size image, scaled
    // by the DTX's coordinate shift (r_SetupTextureMip): u * m_fUMul.
    float    m_fUMul, m_fVMul;
    // How this texture's detail texture lies on it (RTexture+0x34..+0x3c).
    float    m_fDetailScale, m_fDetailCos, m_fDetailSin;
};
void glr_BindTexture(SharedTexture* pTexture, LTBOOL bChanged);
void glr_UnbindTexture(SharedTexture* pTexture);
// The GL texture of a SharedTexture, binding it on first use; 0 if it has none.
GLuint glr_TextureId(SharedTexture* pTexture);

// ---- the scene (gl_scene.cpp) ---------------------------------------------------

void*  glr_CreateContext(RenderContextInit* pInit);
void   glr_DeleteContext(void* pContext);
LTBOOL glr_RenderScene(SceneDesc* pScene);

#endif

// ltrender.h -- the engine <-> render DLL contract, as opengl.ren sees it.
//
// lithtech.exe loads a render DLL by name ("RenderDll" console variable),
// calls its RenderDLLSetup export with the engine's RenderStruct, and from
// then on calls the 37 slots the DLL filled in (+0x6c..+0xfc) and offers it
// 15 callbacks of its own (+0x00..+0x38). The layouts here are the engine's,
// read off the reconstruction (src/engine/lithtech/clientrender.cpp,
// clientblit.cpp, cissurf.cpp, clientdsi.cpp) and confirmed against both
// renderers that implement the other side: the retail d3d.ren
// (src/engine/d3d/) and the Mac release's OpenGL renderer, whose functions
// keep their names (tools/pef.py). Every size is checked at compile time.
//
// Everything crossing the boundary is cdecl, 32-bit, little-endian.

#ifndef LTRENDER_H
#define LTRENDER_H

#include <stdint.h>

typedef uint32_t LTBOOL;
typedef uint32_t HLTCOLOR;          // 0x00RRGGBB; bit 31 = "transparent" flag
typedef uint32_t LTRESULT;          // LT_OK 0, LT_ERROR 1, ...

#define RENDERSTRUCT_VERSION 0xd5d  // Init writes it into RenderStructInit

struct LTRect { int32_t left, top, right, bottom; };     // right/bottom exclusive
struct LTVector { float x, y, z; };
struct LTWarpPt { float source_x, source_y, dest_x, dest_y; };

enum LTSurfaceBlend {
    LTSURFACEBLEND_ALPHA, LTSURFACEBLEND_SOLID, LTSURFACEBLEND_ADD,
    LTSURFACEBLEND_MULTIPLY, LTSURFACEBLEND_MULTIPLY2, LTSURFACEBLEND_MASK,
    LTSURFACEBLEND_MASKADD
};

// ClearScreen flags (ltcodes.h)
#define CLEARSCREEN_SCREEN 1
#define CLEARSCREEN_RENDER 2

// A display mode (ltbasedefs.h).
struct RMode {
    LTBOOL   m_bHardware;           // 0x000
    char     m_RenderDLL[256];      // 0x004  the engine overwrites it with the file name
    char     m_InternalName[128];   // 0x104  identifies the device; no spaces
    char     m_Description[128];    // 0x184
    uint32_t m_Width;               // 0x204
    uint32_t m_Height;              // 0x208
    uint32_t m_BitDepth;            // 0x20c  16 or 32
    RMode*   m_pNext;               // 0x210
};
static_assert(sizeof(RMode) == 0x214, "RMode");

// What the engine hands Init (built on r_InitRender's frame).
struct RenderStructInit {
    uint32_t m_nVersion;            // 0x000  OUT: RENDERSTRUCT_VERSION
    RMode    m_Mode;                // 0x004  IN/OUT: copied to the current mode afterwards
    void*    m_hWnd;                // 0x218  the engine's main window
};
static_assert(sizeof(RenderStructInit) == 0x21c, "RenderStructInit");

// The engine's pixel format object (fmconvert). The screen format the
// renderer reports is the format every engine surface is in.
struct PFormat {
    void*    m_pVtbl;               // 0x00  the engine's; left alone
    uint32_t m_nType;               // 0x04  2 = 16-bit, 3 = 32-bit (the only two the engine handles)
    uint32_t m_Masks[4];            // 0x08  A, R, G, B
    uint32_t m_nBits[4];            // 0x18
    uint32_t m_nShifts[4];          // 0x28  lowest set bit; 32 for an empty mask
};
static_assert(sizeof(PFormat) == 0x38, "PFormat");

// The request BlitToScreen / WarpToScreen / BlitFromScreen get.
struct RenderBlitRequest {
    void*     m_pSurfaceData;       // 0x00  the non-screen surface's render data
    uint32_t  m_bTransparent;       // 0x04  bit 0: honour m_TransColor
    uint32_t  m_TransColor;         // 0x08  key, already in the SCREEN pixel format
    LTRect*   m_pSrcRect;           // 0x0c
    LTRect*   m_pDestRect;          // 0x10
    float     m_fAlpha;             // 0x14  0..1
    LTWarpPt* m_pPoints;            // 0x18  WarpToScreen only
    uint32_t  m_nPoints;            // 0x1c
};
static_assert(sizeof(RenderBlitRequest) == 0x20, "RenderBlitRequest");

struct TextureData;

// The engine's texture handle. m_pRenderData is ours.
struct SharedTexture {
    void*          m_BankLink[2];   // 0x00
    TextureData*   m_pEngineData;   // 0x08  engine copy; get it through GetTexture
    void*          m_pRenderData;   // 0x0c  RENDERER-OWNED (NULL = not bound)
    void*          m_Link[3];       // 0x10
    uint32_t       m_Unknown1c[2];  // 0x1c
    void*          m_pFile;         // 0x24
    SharedTexture* m_pLinkedTexture;// 0x28  detail / environment map texture
    uint32_t       m_nLinkType;     // 0x2c  0 detail, 1 env map, 2 env map in alpha
    uint16_t       m_nFrameCode;    // 0x30
    uint16_t       m_nRefCount;     // 0x32
};
static_assert(sizeof(SharedTexture) == 0x34, "SharedTexture");

// DTX data as the engine keeps it (clientdsi.cpp).
struct DtxHeader {
    uint32_t m_ResType;             // +0x08 in TextureData
    int32_t  m_Version;             // +0x0c  -5
    uint16_t m_BaseWidth;           // +0x10
    uint16_t m_BaseHeight;          // +0x12
    uint16_t m_nMipmaps;            // +0x14
    uint16_t m_nSections;           // +0x16
    uint32_t m_IFlags;              // +0x18  DTX flags: 1 fullbright, 2 prefer 16-bit,
                                    //        0x40 the engine keeps the data (texmod),
                                    //        0x80 / 0x100 prefer 4444 / 5551 (Mac r_SetupTextureSurface)
    uint32_t m_UserFlags;           // +0x1c
    uint8_t  m_Extra[12];           // +0x20  [0] group, [1] mips to use, [2] BPP type, [3] mip offset
    char     m_CommandString[128];  // +0x2c
};
static_assert(sizeof(DtxHeader) == 0xa4, "DtxHeader");

#define DTX_FULLBRITE 1

// BPP types (fmconvert BPPIdent)
enum { BPP_8P, BPP_8, BPP_16, BPP_32, BPP_S3TC_DXT1, BPP_S3TC_DXT3, BPP_S3TC_DXT5, BPP_32P };

struct TextureMip {
    uint32_t m_Width, m_Height;     // 0x00
    uint8_t* m_pData;               // 0x08
    uint32_t m_Pitch;               // 0x0c  bytes (0 for compressed levels)
    uint8_t* m_pBump;               // 0x10
    uint32_t m_BumpPitch;           // 0x14
};
static_assert(sizeof(TextureMip) == 0x18, "TextureMip");

#define MAX_DTX_MIPS 8

struct TextureData {
    void*          m_pVtbl;         // 0x00
    uint32_t       m_Unknown04;     // 0x04
    DtxHeader      m_Header;        // 0x08
    void*          m_Link[3];       // 0xac
    uint32_t       m_AllocSize;     // 0xb8
    void*          m_pSections;     // 0xbc
    SharedTexture* m_pSharedTexture;// 0xc0
    uint32_t       m_Unknownc4;     // 0xc4
    void*          m_pDataBuffer;   // 0xc8
    TextureMip     m_Mips[MAX_DTX_MIPS]; // 0xcc
};
static_assert(sizeof(TextureData) == 0x18c, "TextureData");

struct RenderContextInit { void* m_pWorld; };
struct SceneDesc;
struct LTObject;
struct Attachment;
typedef void* HCONSOLEVAR;

// The table itself (0x150 bytes, one global in lithtech.exe).
struct RenderStruct {
    // ---- the engine's callbacks (Mac names) -------------------------------
    LTObject*      (*ProcessAttachment)(LTObject* pObj, Attachment* pAttach); // 0x00
    SharedTexture* (*GetSharedTexture)(const char* pFilename);                 // 0x04
    TextureData*   (*GetTexture)(SharedTexture* pTexture);                     // 0x08
    void           (*FreeTexture)(SharedTexture* pTexture);                    // 0x0c
    void           (*RunConsoleString)(const char* pStr);                      // 0x10
    void           (*ConsolePrint)(const char* pFormat, ...);                  // 0x14
    HCONSOLEVAR    (*GetParameter)(const char* pName);                         // 0x18
    float          (*GetParameterValueFloat)(HCONSOLEVAR hVar);                // 0x1c
    const char*    (*GetParameterValueString)(HCONSOLEVAR hVar);               // 0x20
    void           (*RendererPing)(void);                                      // 0x24
    uint32_t       (*IncObjectFrameCode)(void);                                // 0x28 returns the new code
    uint32_t       (*GetObjectFrameCode)(void);                                // 0x2c
    uint16_t       (*IncCurTextureFrameCode)(void);                            // 0x30
    void*          (*Alloc)(uint32_t nBytes);                                  // 0x34
    void           (*Free)(void* p);                                           // 0x38

    // ---- engine data ------------------------------------------------------
    uint32_t m_Width;                                                          // 0x3c
    uint32_t m_Height;                                                         // 0x40
    uint32_t m_bInited;                                                        // 0x44
    uint32_t m_nBytes[4];                                                      // 0x48  renderer statistics
    uint32_t m_nTagVisibleLeaves, m_nFlushObjectQueues, m_nModels,
             m_nWorldModels, m_nTranslucent;                                   // 0x58

    // ---- ours -------------------------------------------------------------
    int      (*Init)(RenderStructInit* pInit);                                 // 0x6c
    void     (*Term)(void);                                                    // 0x70
    void     (*BindTexture)(SharedTexture* pTexture, LTBOOL bChanged);         // 0x74
    void     (*UnbindTexture)(SharedTexture* pTexture);                        // 0x78
    void*    (*CreateContext)(RenderContextInit* pInit);                       // 0x7c
    void     (*DeleteContext)(void* pContext);                                 // 0x80
    void     (*Clear)(LTRect* pRect, uint32_t nFlags);                         // 0x84
    LTBOOL   (*Start3D)(void);                                                 // 0x88
    LTBOOL   (*End3D)(void);                                                   // 0x8c
    LTBOOL   (*IsIn3D)(void);                                                  // 0x90
    LTBOOL   (*StartOptimized2D)(void);                                        // 0x94
    void     (*EndOptimized2D)(void);                                          // 0x98
    LTBOOL   (*IsInOptimized2D)(void);                                         // 0x9c
    LTBOOL   (*SetOptimized2DBlend)(LTSurfaceBlend eBlend);                    // 0xa0
    LTBOOL   (*GetOptimized2DBlend)(LTSurfaceBlend* pBlend);                   // 0xa4
    LTBOOL   (*SetOptimized2DColor)(HLTCOLOR hColor);                          // 0xa8
    LTBOOL   (*GetOptimized2DColor)(HLTCOLOR* pColor);                         // 0xac
    LTBOOL   (*RenderScene)(SceneDesc* pScene);                                // 0xb0
    void     (*RenderCommand)(int argc, char** argv);                          // 0xb4
    void*    (*GetHook)(const char* pName);                                    // 0xb8
    void     (*SwapBuffers)(uint32_t nFlags);                                  // 0xbc
    uint32_t (*GetInfoFlags)(void);                                            // 0xc0
    void     (*GetScreenFormat)(PFormat* pFormat);                             // 0xc4
    void*    (*CreateSurface)(uint32_t nWidth, uint32_t nHeight);              // 0xc8
    void     (*DeleteSurface)(void* pSurface);                                 // 0xcc
    void     (*GetSurfaceInfo)(void* pSurface, uint32_t* pWidth,
                               uint32_t* pHeight, uint32_t* pPitch);           // 0xd0
    void*    (*LockSurface)(void* pSurface);                                   // 0xd4
    void     (*UnlockSurface)(void* pSurface);                                 // 0xd8
    LTBOOL   (*OptimizeSurface)(void* pSurface, HLTCOLOR hTransColor);         // 0xdc
    void     (*UnoptimizeSurface)(void* pSurface);                             // 0xe0
    LTBOOL   (*LockScreen)(int left, int top, int right, int bottom,
                           void** ppPixels, uint32_t* pPitch);                 // 0xe4
    void     (*UnlockScreen)(void);                                            // 0xe8
    void     (*BlitToScreen)(RenderBlitRequest* pRequest);                     // 0xec
    LTBOOL   (*WarpToScreen)(RenderBlitRequest* pRequest);                     // 0xf0
    void     (*MakeScreenShot)(const char* pFilename);                         // 0xf4
    void     (*ReadConsoleVariables)(void);                                    // 0xf8
    void     (*BlitFromScreen)(RenderBlitRequest* pRequest);                   // 0xfc

    // ---- engine data ------------------------------------------------------
    uint32_t       m_Unknown100;                                               // 0x100
    SharedTexture* m_pEnvMap;                                                  // 0x104
    uint8_t        m_GlobalPanInfo[0x28];                                      // 0x108 PanInfo[2], below
    LTVector       m_GlobalLightDir;                                           // 0x130
    LTVector       m_GlobalLightColor;                                         // 0x13c
    uint32_t       m_Unknown148, m_Unknown14c;                                 // 0x148
};
static_assert(sizeof(RenderStruct) == 0x150, "RenderStruct");
static_assert(__builtin_offsetof(RenderStruct, Init) == 0x6c, "RenderStruct part 3");
static_assert(__builtin_offsetof(RenderStruct, BlitFromScreen) == 0xfc, "RenderStruct part 3 end");

#endif

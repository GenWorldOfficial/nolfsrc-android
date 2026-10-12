// glscene.h -- the 3D half's shared declarations: the per-world context,
// the lightmap atlases, the multitexture entry points.

#ifndef GLSCENE_H
#define GLSCENE_H

#include "glr.h"
#include "ltworld.h"

// ---- lightmap data, as the engine keeps it (docs/world.md C.1) -----------------

struct LMPolyRef { uint16_t m_iWorldModel, m_iPoly; };
struct LMFrameEntry { uint8_t* m_pData; uint32_t m_nSize; };
struct PolyAnimRef { uint16_t m_iAnim, m_iPoly; };

struct LMAnim {                         // 0x60
    char           m_Name[32];          // 0x00
    uint32_t       m_bShadowMap;        // 0x20
    LMFrameEntry** m_Frames;            // 0x24 [m_nFrames] -> LMFrameEntry[m_nPolies]
    uint32_t       m_nFrames;           // 0x28
    LMPolyRef*     m_Polies;            // 0x2c
    uint32_t       m_nPolies;           // 0x30
    uint32_t       m_iFrames[2];        // 0x34
    float          m_fPercentBetween;   // 0x3c
    float          m_fBlendPercent;     // 0x40
    LTVector       m_vLightPos;         // 0x44
    LTVector       m_vLightColor;       // 0x50
    float          m_fLightRadius;      // 0x5c
};
static_assert(sizeof(LMAnim) == 0x60, "LMAnim");

#define LM_MAXTEXELS 1024               // 32x32: the decompressor's cap
#define POLYFLAG_LMCHANGED 0x8000       // WorldPoly::m_Flags: the engine marks a changed animation

// ---- the renderer's per-world state ---------------------------------------------

struct GLRLMAtlas {
    GLuint      m_Id;
    uint32_t    m_nSize;                // square
    uint32_t    m_ShelfX, m_ShelfY, m_ShelfH;
    uint8_t*    m_pRGB;                 // m_nSize^2 * 3, kept for animated updates
    bool        m_bDirty;
    GLRLMAtlas* m_pNext;
};

// Where a lightmapped poly's texels are; hung on WorldPoly::m_pLMPage, which
// is the renderer's field.
struct GLRPolyLM {
    GLRLMAtlas* m_pAtlas;
    uint16_t    m_X, m_Y;               // the lightmap's first texel (inside its border)
};

struct GLRContext {
    MainWorld*  m_pWorld;
    uint16_t    m_FrameCode;
    GLRLMAtlas* m_pAtlases;
    GLRPolyLM*  m_pPolyLMs;             // one block for every lightmapped poly
    uint32_t    m_nPolyLMs;
    uint32_t    m_nContextGen;          // GL context the atlases were made in
};

bool glr_BuildLightmaps(GLRContext* pContext);
void glr_FreeLightmaps(GLRContext* pContext);
void glr_UpdateLightmaps(GLRContext* pContext);   // animated lightmaps, before drawing
LMAnim* glr_FindLMAnim(MainWorld* pWorld, const char* pName);
void glr_LMVectors(const Surface* pSurf, LTVector* pP, LTVector* pQ);
WorldPoly* glr_GetLMPoly(MainWorld* pWorld, const LMPolyRef& ref);

// ---- multitexture (GL 1.3 / ARB_multitexture) -----------------------------------

#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
typedef void (APIENTRY* PFN_glActiveTexture)(GLenum);
typedef void (APIENTRY* PFN_glMultiTexCoord2f)(GLenum, GLfloat, GLfloat);
struct GLRMulti {
    PFN_glActiveTexture   glActiveTexture;
    PFN_glMultiTexCoord2f glMultiTexCoord2f;
    bool                  m_bOK;
};
extern GLRMulti g_Multi;
void glr_InitMultitexture(void);

#endif

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
    uint32_t    m_DirtyY0, m_DirtyY1;   // the rows changed since the last upload
    GLRLMAtlas* m_pNext;
};

// Where a lightmapped poly's texels are; hung on WorldPoly::m_pLMPage, which
// is the renderer's field.
struct GLRPolyLM {
    GLRLMAtlas* m_pAtlas;
    uint16_t    m_X, m_Y;               // the lightmap's first texel (inside its border)
    bool        m_bLit;                 // holds dynamic light this frame
};

// A dynamic light as a BSP's polys see it: in that BSP's own space.
struct GLRWorldLight {
    LTVector m_Pos;
    float    m_fRadius;
    float    m_Add[3];                  // 2c - 255: a colour below 128 darkens
    uint8_t  m_Color[3];
    bool     m_bSolid;                  // FLAG_SOLIDLIGHT or FastLight
    bool     m_bFrontOnly;              // FLAG_DONTLIGHTBACKFACING
};
#define GLR_MAXWORLDLIGHTS 40           // r_DrawLight's cap

struct GLRContext {
    MainWorld*  m_pWorld;
    uint16_t    m_FrameCode;
    GLRLMAtlas* m_pAtlases;
    GLRPolyLM*  m_pPolyLMs;             // one block for every lightmapped poly
    uint32_t    m_nPolyLMs;
    uint32_t    m_nContextGen;          // GL context the atlases were made in
    WorldPoly** m_pLitPolys;            // polys whose atlas texels hold dynamic light
    uint32_t    m_nLitPolys, m_nLitAlloc;
};

bool glr_BuildLightmaps(GLRContext* pContext);
void glr_FreeLightmaps(GLRContext* pContext);
void glr_UpdateLightmaps(GLRContext* pContext);   // animated lightmaps, before drawing
void glr_UploadLightmaps(GLRContext* pContext);   // the atlas rows changed since the last call
void glr_LightPolyLM(GLRContext* pContext, WorldPoly* pPoly, const GLRWorldLight* pLights,
                     uint32_t nLights);
LMAnim* glr_FindLMAnim(MainWorld* pWorld, const char* pName);
void glr_LMVectors(const Surface* pSurf, LTVector* pP, LTVector* pQ);
WorldPoly* glr_GetLMPoly(MainWorld* pWorld, const LMPolyRef& ref);

// ---- the frame ------------------------------------------------------------------------

struct GLRCamera {
    LTVector m_Pos;
    float    m_R[3][3];                 // camera -> world: columns right, up, forward
    float    m_fTanX, m_fTanY;          // tan of the half field of view
    float    m_fNear, m_fFar;
    float    m_fScale;                  // view scale (the sky's SkyScale)
    int      m_nViewX, m_nViewY, m_nViewW, m_nViewH;   // GL viewport
    LTPlane  m_Planes[6];               // world-space frustum, normals in
};

struct GLRFrame {
    SceneDesc*  m_pScene;
    GLRContext* m_pContext;
    GLRCamera   m_Cam;
    GLRCamera   m_SkyCam;               // while sky objects draw
    uint16_t    m_WorldFrameCode;
    uint32_t    m_ObjectFrameCode;
    uint32_t    m_nSkyPolys;            // visible sky polygons: the sky pass runs if any
    float       m_fLMGrid;
    bool        m_bLightmaps;
    bool        m_bFog;
};
extern GLRFrame g_Frame;

void glr_LoadCamera(const GLRCamera* pCam);
void glr_MultMatrix(const LTMatrix& M);
void glr_RotationMatrix(const LTRotation& q, float m[3][3]);
bool glr_SphereVisible(const LTVector& c, float r);
bool glr_ShouldDraw(LTObject* pObj);
void glr_ObjectFog(LTObject* pObj, bool bBegin);
void glr_SetBaseTexture(GLRTexture* pTex);
void glr_ForgetBindings(void);
GLRTexture* glr_SurfaceTexture(Surface* pSurf);
void glr_ProcessWorldModel(LTObject* pObj);
void glr_DrawSolidWorldModel(LTObject* pObj);
void glr_DrawTranslucentWorldModel(LTObject* pObj);

// ---- multitexture (GL 1.3 / ARB_multitexture) -----------------------------------

#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#define GL_COMBINE_ARB        0x8570
#define GL_COMBINE_RGB_ARB    0x8571
#define GL_COMBINE_ALPHA_ARB  0x8572
#define GL_RGB_SCALE_ARB      0x8573
#define GL_ADD_SIGNED_ARB     0x8574
#define GL_PRIMARY_COLOR_ARB  0x8577
#define GL_PREVIOUS_ARB       0x8578
#define GL_SOURCE0_RGB_ARB    0x8580
#define GL_SOURCE1_RGB_ARB    0x8581
#define GL_SOURCE0_ALPHA_ARB  0x8588
#define GL_OPERAND0_RGB_ARB   0x8590
#define GL_OPERAND1_RGB_ARB   0x8591
#define GL_OPERAND0_ALPHA_ARB 0x8598
typedef void (APIENTRY* PFN_glActiveTexture)(GLenum);
typedef void (APIENTRY* PFN_glMultiTexCoord2f)(GLenum, GLfloat, GLfloat);
struct GLRMulti {
    PFN_glActiveTexture   glActiveTexture;
    PFN_glMultiTexCoord2f glMultiTexCoord2f;
    bool                  m_bOK;            // two units: lightmaps
    bool                  m_bCombine;       // ARB_texture_env_combine
    int                   m_nUnits;
};
extern GLRMulti g_Multi;
void glr_InitMultitexture(void);

#endif

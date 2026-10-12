// gl_effects.cpp -- particle systems, poly grids, line systems and
// canvases (docs/objects.md part III.2-5).
//
// The engine moves the particles and advances the textures' animations;
// the renderer only reads. All four are relative to their object's
// position, rotation and scale; particles and lines are always
// translucent, poly grids and canvases only when the object says so.

#include <math.h>

#include "globjects.h"

#define FLAG_UNSIGNED            0x2     // poly grid: heights are unsigned
#define FLAG_ENVIRONMENTMAPONLY  0x20
#define CF_SOLIDCANVAS           0x10    // client flags (LTObject+0x180)

template <typename T> static inline T rd(const void* p, uint32_t off) { T v; memcpy(&v, (const uint8_t*)p + off, sizeof(T)); return v; }

// The object's matrix, T * R * S, as LTObject::SetupTransform builds it.
static void glr_ObjectMatrix(LTObject* pObj, LTMatrix* pM)
{
    float R[3][3];
    glr_RotationMatrix(pObj->m_Rotation, R);
    for (int r = 0; r < 3; r++)
    {
        pM->m[r][0] = R[r][0] * pObj->m_Scale.x;
        pM->m[r][1] = R[r][1] * pObj->m_Scale.y;
        pM->m[r][2] = R[r][2] * pObj->m_Scale.z;
    }
    pM->m[0][3] = pObj->m_Pos.x;
    pM->m[1][3] = pObj->m_Pos.y;
    pM->m[2][3] = pObj->m_Pos.z;
    pM->m[3][0] = pM->m[3][1] = pM->m[3][2] = 0.0f;
    pM->m[3][3] = 1.0f;
}

// Bind a SharedTexture on unit 0 (or turn texturing off); its record.
static GLRTexture* glr_BindShared(SharedTexture* pShared)
{
    GLRTexture* pTex = NULL;
    if (pShared)
    {
        glr_TextureId(pShared);
        pTex = (GLRTexture*)pShared->m_pRenderData;
    }
    glr_ForgetBindings();
    glr_SetBaseTexture(pTex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    return pTex;
}

// Two texels in from each edge, as sprites.
static void glr_InsetUV(GLRTexture* pTex, float* pUV)
{
    float W = pTex ? (float)pTex->m_nBaseWidth : 0.0f, H = pTex ? (float)pTex->m_nBaseHeight : 0.0f;
    pUV[0] = W > 4 ? 2.0f / W : 0.0f;
    pUV[1] = H > 4 ? 2.0f / H : 0.0f;
    pUV[2] = W > 4 ? (W - 2.0f) / W : 1.0f;
    pUV[3] = H > 4 ? (H - 2.0f) / H : 1.0f;
}

// The object's own blend, fog and (for really-close objects) camera, for
// the length of one draw.
struct GLREffectState {
    LTObject* m_pObj;
    bool      m_bClose;
    GLREffectState(LTObject* pObj, bool bBlend) : m_pObj(pObj)
    {
        glPushAttrib(GL_COLOR_BUFFER_BIT | GL_FOG_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
        if (bBlend)
        {
            glEnable(GL_BLEND);
            glr_ObjectBlend(pObj);
        }
        glr_ObjectFog(pObj, true);
        m_bClose = (pObj->m_Flags & FLAG_REALLYCLOSE) != 0;
        if (m_bClose)
            glr_BeginReallyClose(pObj);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        if (m_bClose)
            glLoadIdentity();
    }
    ~GLREffectState()
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        if (m_bClose)
            glr_EndReallyClose();
        glr_ObjectFog(m_pObj, false);
        glPopAttrib();
        glr_ForgetBindings();
    }
};

// ---- particle systems ------------------------------------------------------------

// r_DrawParticleSystem / r_DrawParticleBatch: each particle a screen-
// aligned square around its place in the system, half a side of 2 x its
// size in world units (d3d.ren 0x1000daa0: size x xScale x view width x
// rhw pixels), coloured by the particle and the object.
void glr_DrawParticleSystem(LTObject* pObj)
{
    if (!cvi(CV_DrawParticles))
        return;
    const uint8_t* p = (const uint8_t*)pObj;
    const uint8_t* pHead = p + 0x1a8;
    const uint8_t* pFirst = rd<const uint8_t*>(p, 0x1a8 + 0x20);
    if (!pFirst || pFirst == pHead)
        return;

    GLREffectState state(pObj, true);
    GLRTexture* pTex = glr_BindShared(rd<SharedTexture*>(p, 0x1e8));
    float uv[4];
    glr_InsetUV(pTex, uv);

    LTMatrix M;
    glr_ObjectMatrix(pObj, &M);
    const GLRCamera& cam = g_Frame.m_Cam;
    float R[3][3];
    memcpy(R, cam.m_R, sizeof(R));
    // Camera space: the view (or, really close, the object is there already).
    float fW = (float)cam.m_nViewW, fH = (float)cam.m_nViewH;
    float fAspect = (fW / fH) * (cam.m_fTanY / cam.m_fTanX);   // a square on screen
    float fR = pObj->m_ColorR / 255.0f, fG = pObj->m_ColorG / 255.0f, fB = pObj->m_ColorB / 255.0f;
    float fA = pObj->m_ColorA;
    bool bClose = state.m_bClose;

    glLoadIdentity();
    glBegin(GL_QUADS);
    int nGuard = 0;
    for (const uint8_t* q = pFirst; q && q != pHead && nGuard < 100000; q = rd<const uint8_t*>(q, 0x20), nGuard++)
    {
        LTVector lp = rd<LTVector>(q, 0x24);
        LTVector w = { M.m[0][0] * lp.x + M.m[0][1] * lp.y + M.m[0][2] * lp.z + M.m[0][3],
                       M.m[1][0] * lp.x + M.m[1][1] * lp.y + M.m[1][2] * lp.z + M.m[1][3],
                       M.m[2][0] * lp.x + M.m[2][1] * lp.y + M.m[2][2] * lp.z + M.m[2][3] };
        LTVector c;
        if (bClose)
            c = w;
        else
        {
            LTVector d = { w.x - cam.m_Pos.x, w.y - cam.m_Pos.y, w.z - cam.m_Pos.z };
            c.x = R[0][0] * d.x + R[1][0] * d.y + R[2][0] * d.z;
            c.y = R[0][1] * d.x + R[1][1] * d.y + R[2][1] * d.z;
            c.z = R[0][2] * d.x + R[1][2] * d.y + R[2][2] * d.z;
        }
        if (c.z <= (bClose ? 0.01f : cam.m_fNear))
            continue;
        float fSize = rd<float>(q, 0x1c) * 2.0f;
        float hx = fSize, hy = fSize * fAspect;
        LTVector col = rd<LTVector>(q, 0x0c);
        float a = rd<float>(q, 0x18) * fA;
        glColor4ub((uint8_t)(col.x * fR > 255 ? 255 : col.x * fR < 0 ? 0 : col.x * fR),
                   (uint8_t)(col.y * fG > 255 ? 255 : col.y * fG < 0 ? 0 : col.y * fG),
                   (uint8_t)(col.z * fB > 255 ? 255 : col.z * fB < 0 ? 0 : col.z * fB),
                   (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
        glTexCoord2f(uv[0], uv[1]); glVertex3f(c.x - hx, c.y + hy, c.z);
        glTexCoord2f(uv[2], uv[1]); glVertex3f(c.x + hx, c.y + hy, c.z);
        glTexCoord2f(uv[2], uv[3]); glVertex3f(c.x + hx, c.y - hy, c.z);
        glTexCoord2f(uv[0], uv[3]); glVertex3f(c.x - hx, c.y - hy, c.z);
    }
    glEnd();
}

// ---- poly grids ---------------------------------------------------------------------

// r_DrawPolyGrid: a w x h height field around the object, one colour per
// height from the object's colour table, the texture the frame of its
// sprite, panned and scaled.
void glr_DrawPolyGrid(LTObject* pObj)
{
    if (!cvi(CV_DrawPolyGrids))
        return;
    const uint8_t* p = (const uint8_t*)pObj;
    const int8_t* pData = rd<const int8_t*>(p, 0x1a8);
    const uint16_t* pIndices = rd<const uint16_t*>(p, 0x1ac);
    uint32_t nIndices = rd<uint32_t>(p, 0x1e0);
    uint32_t w = rd<uint32_t>(p, 0x1f0), h = rd<uint32_t>(p, 0x1f4);
    if (!pData || !pIndices || w < 2 || h < 2 || w * h > 65000)
        return;
    if (pObj->m_Flags & FLAG_ENVIRONMENTMAPONLY)
        return;                         // the env-map pass is not drawn yet

    bool bTranslucent = pObj->m_ColorA != 255;
    GLREffectState state(pObj, bTranslucent);
    SharedTexture** ppFrame = rd<SharedTexture**>(p, 0x1b4 + 8);
    GLRTexture* pTex = glr_BindShared(ppFrame ? *ppFrame : NULL);
    float tu = pTex && pTex->m_nBaseWidth ? 1.0f / pTex->m_nBaseWidth : 0.0f;
    float tv = pTex && pTex->m_nBaseHeight ? 1.0f / pTex->m_nBaseHeight : 0.0f;
    float xPan = rd<float>(p, 0x1cc), yPan = rd<float>(p, 0x1d0);
    float xScale = rd<float>(p, 0x1d4), yScale = rd<float>(p, 0x1d8);
    float uScale = (xScale != 0.0f ? 1.0f / xScale : 1.0f) * tu * pObj->m_Scale.x * w / (w - 1.0f);
    float vScale = (yScale != 0.0f ? 1.0f / yScale : 1.0f) * tv * pObj->m_Scale.z * h / (h - 1.0f);
    float uPan = xPan * tu, vPan = yPan * tv;
    const float* pColors = (const float*)(p + 0x1f8);
    bool bUnsigned = (pObj->m_Flags & FLAG_UNSIGNED) != 0;
    float fA = pObj->m_ColorA / 255.0f;

    LTMatrix M;
    glr_ObjectMatrix(pObj, &M);
    glr_MultMatrix(M);
    float fHalfW = (w - 1) * 0.5f, fHalfH = (h - 1) * 0.5f;
    glBegin(GL_TRIANGLES);
    for (uint32_t i = 0; i < nIndices; i++)
    {
        uint32_t idx = pIndices[i];
        if (idx >= w * h)
            continue;
        uint32_t row = idx / w, col = idx % w;
        int nValue = bUnsigned ? (int)(uint8_t)pData[idx] - 128 : (int)pData[idx];
        int nColor = bUnsigned ? (int)(uint8_t)pData[idx] : (int)pData[idx] + 128;
        const float* c = &pColors[nColor * 4];
        float a = c[3] * fA;
        glColor4ub((uint8_t)(c[0] > 255 ? 255 : c[0] < 0 ? 0 : c[0]), (uint8_t)(c[1] > 255 ? 255 : c[1] < 0 ? 0 : c[1]),
                   (uint8_t)(c[2] > 255 ? 255 : c[2] < 0 ? 0 : c[2]), (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
        glTexCoord2f((col + uPan) * uScale, (row + vPan) * vScale);
        glVertex3f(col - fHalfW, (float)nValue, row - fHalfH);
    }
    glEnd();
}

// ---- line systems ---------------------------------------------------------------------

// r_DrawLineSystem: each line between its two points, colours 0..1 (the
// SDK's, as d3d.ren reads them), alpha times the object's.
void glr_DrawLineSystem(LTObject* pObj)
{
    if (!cvi(CV_DrawLineSystems))
        return;
    const uint8_t* p = (const uint8_t*)pObj;
    const uint8_t* pHead = p + 0x1b0;
    const uint8_t* pFirst = rd<const uint8_t*>(p, 0x1b0 + 0x40);
    if (!pFirst || pFirst == pHead)
        return;
    GLREffectState state(pObj, true);
    glr_ForgetBindings();
    glr_SetBaseTexture(NULL);
    LTMatrix M;
    glr_ObjectMatrix(pObj, &M);
    glr_MultMatrix(M);
    float fA = pObj->m_ColorA;
    glBegin(GL_LINES);
    int nGuard = 0;
    for (const uint8_t* q = pFirst; q && q != pHead && nGuard < 100000; q = rd<const uint8_t*>(q, 0x40), nGuard++)
    {
        for (int k = 0; k < 2; k++)
        {
            const float* pt = (const float*)(q + k * 0x1c);
            float a = pt[6] * fA;
            glColor4ub((uint8_t)(pt[3] * 255 > 255 ? 255 : pt[3] < 0 ? 0 : pt[3] * 255),
                       (uint8_t)(pt[4] * 255 > 255 ? 255 : pt[4] < 0 ? 0 : pt[4] * 255),
                       (uint8_t)(pt[5] * 255 > 255 ? 255 : pt[5] < 0 ? 0 : pt[5] * 255),
                       (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
            glVertex3f(pt[0], pt[1], pt[2]);
        }
    }
    glEnd();
}

// ---- canvases --------------------------------------------------------------------------
//
// A canvas is the game's own drawing: its function gets an ILTCustomDraw
// (iltcustomdraw.h: five virtuals, thiscall) and draws world-space
// polygons through it (CanvasDrawMgr).

enum { LTRS_ALPHABLEND, LTRS_ZREAD, LTRS_ZWRITE, LTRS_SRCBLEND, LTRS_DESTBLEND, LTRS_TEXADDR,
       LTRS_COLOROP, LTRS_ALPHAOP, LTRS_COUNT };

struct GLRCustomDraw;
struct GLRCustomDrawVtbl {
    LTRESULT (__thiscall* DrawPrimitive)(GLRCustomDraw* pThis, const void* pVerts, uint32_t nVerts, uint32_t nFlags);
    LTRESULT (__thiscall* SetState)(GLRCustomDraw* pThis, uint32_t nState, uint32_t nVal);
    LTRESULT (__thiscall* GetState)(GLRCustomDraw* pThis, uint32_t nState, uint32_t* pVal);
    LTRESULT (__thiscall* SetTexture)(GLRCustomDraw* pThis, const char* pName);
    LTRESULT (__thiscall* GetTexelSize)(GLRCustomDraw* pThis, float* pU, float* pV);
};
struct GLRCustomDraw {
    const GLRCustomDrawVtbl* m_pVtbl;
    uint32_t    m_States[LTRS_COUNT];
    GLRTexture* m_pTex;
};

static GLenum glr_LTBlend(uint32_t n)
{
    static const GLenum blend[6] = { GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                                     GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
    return n >= 1 && n <= 6 ? blend[n - 1] : GL_ONE;
}

static void glr_ApplyCanvasOp(uint32_t nColorOp, uint32_t nAlphaOp)
{
    // LTOP_SELECTTEXTURE 1, SELECTDIFFUSE 2, MODULATE 3, ADD 4, ADDSIGNED 5
    static const GLenum op[5] = { GL_REPLACE, GL_REPLACE, GL_MODULATE, GL_ADD, GL_ADD_SIGNED_ARB };
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    uint32_t c = nColorOp >= 1 && nColorOp <= 5 ? nColorOp : 3;
    uint32_t a = nAlphaOp >= 1 && nAlphaOp <= 5 ? nAlphaOp : 2;
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, op[c - 1]);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, c == 2 ? GL_PRIMARY_COLOR_ARB : GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, op[a - 1]);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, a == 2 ? GL_PRIMARY_COLOR_ARB : GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, 0x8589 /* GL_SOURCE1_ALPHA_ARB */, GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV, 0x8599 /* GL_OPERAND1_ALPHA_ARB */, GL_SRC_ALPHA);
}

static void glr_ApplyCanvasState(GLRCustomDraw* pThis, uint32_t nState)
{
    uint32_t v = pThis->m_States[nState];
    switch (nState)
    {
    case LTRS_ALPHABLEND: if (v) glEnable(GL_BLEND); else glDisable(GL_BLEND); break;
    case LTRS_ZREAD:      if (v) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); break;
    case LTRS_ZWRITE:     glDepthMask(v ? GL_TRUE : GL_FALSE); break;
    case LTRS_SRCBLEND:
    case LTRS_DESTBLEND:
        glBlendFunc(glr_LTBlend(pThis->m_States[LTRS_SRCBLEND]), glr_LTBlend(pThis->m_States[LTRS_DESTBLEND]));
        break;
    case LTRS_TEXADDR:
        if (pThis->m_pTex)
        {
            GLint wrap = v == 2 ? GL_CLAMP_TO_EDGE : GL_REPEAT;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        }
        break;
    case LTRS_COLOROP:
    case LTRS_ALPHAOP:
        if (g_Multi.m_bCombine)
            glr_ApplyCanvasOp(pThis->m_States[LTRS_COLOROP], pThis->m_States[LTRS_ALPHAOP]);
        break;
    }
}

static LTRESULT __thiscall cd_DrawPrimitive(GLRCustomDraw* pThis, const void* pVerts, uint32_t nVerts, uint32_t nFlags)
{
    (void)pThis; (void)nFlags;
    if (!pVerts || nVerts < 3 || nVerts > 100)
        return 1;                       // LT_ERROR
    // LTVertex: m_Vec, m_RHW, colour (b,g,r,a), specular, tu, tv -- 0x20 bytes.
    const uint8_t* v = (const uint8_t*)pVerts;
    glBegin(GL_TRIANGLE_FAN);
    for (uint32_t i = 0; i < nVerts; i++, v += 0x20)
    {
        glColor4ub(v[0x12], v[0x11], v[0x10], v[0x13]);
        glTexCoord2f(rd<float>(v, 0x18), rd<float>(v, 0x1c));
        glVertex3f(rd<float>(v, 0), rd<float>(v, 4), rd<float>(v, 8));
    }
    glEnd();
    return 0;                           // LT_OK
}

static LTRESULT __thiscall cd_SetState(GLRCustomDraw* pThis, uint32_t nState, uint32_t nVal)
{
    if (nState >= LTRS_COUNT)
        return 1;
    pThis->m_States[nState] = nVal;
    glr_ApplyCanvasState(pThis, nState);
    return 0;
}

static LTRESULT __thiscall cd_GetState(GLRCustomDraw* pThis, uint32_t nState, uint32_t* pVal)
{
    if (nState >= LTRS_COUNT || !pVal)
        return 1;
    *pVal = pThis->m_States[nState];
    return 0;
}

static LTRESULT __thiscall cd_SetTexture(GLRCustomDraw* pThis, const char* pName)
{
    SharedTexture* pShared = pName ? g.pStruct->GetSharedTexture(pName) : NULL;
    pThis->m_pTex = glr_BindShared(pShared);
    glr_ApplyCanvasState(pThis, LTRS_TEXADDR);
    glr_ApplyCanvasState(pThis, LTRS_COLOROP);
    return pName && !pThis->m_pTex ? 1 : 0;
}

static LTRESULT __thiscall cd_GetTexelSize(GLRCustomDraw* pThis, float* pU, float* pV)
{
    GLRTexture* pTex = pThis->m_pTex;
    if (pU) *pU = pTex && pTex->m_nBaseWidth ? 1.0f / pTex->m_nBaseWidth : 0.0f;
    if (pV) *pV = pTex && pTex->m_nBaseHeight ? 1.0f / pTex->m_nBaseHeight : 0.0f;
    return 0;
}

static const GLRCustomDrawVtbl g_CustomDrawVtbl = {
    cd_DrawPrimitive, cd_SetState, cd_GetState, cd_SetTexture, cd_GetTexelSize,
};

typedef void (*CanvasDrawFn)(GLRCustomDraw* pDraw, LTObject* hObj, void* pUser);

// CanvasDrawMgr::DrawCanvas: the default states (InitStates), the game's
// function, then everything put back.
void glr_DrawCanvas(LTObject* pObj)
{
    if (!cvi(CV_DrawCanvases))
        return;
    CanvasDrawFn fn = rd<CanvasDrawFn>(pObj, 0x1a8);
    if (!fn)
        return;
    GLREffectState state(pObj, false);
    GLRCustomDraw draw;
    draw.m_pVtbl = &g_CustomDrawVtbl;
    draw.m_pTex = NULL;
    static const uint32_t def[LTRS_COUNT] = { 0, 1, 1, 5, 6, 1, 3, 2 };
    memcpy(draw.m_States, def, sizeof(def));
    glr_ForgetBindings();
    glr_SetBaseTexture(NULL);
    for (uint32_t i = 0; i < LTRS_COUNT; i++)
        glr_ApplyCanvasState(&draw, i);
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    fn(&draw, pObj, rd<void*>(pObj, 0x1ac));
    glPopAttrib();
}

bool glr_CanvasIsSolid(LTObject* pObj)
{
    return (rd<uint16_t>(pObj, 0x180) & CF_SOLIDCANVAS) != 0;
}

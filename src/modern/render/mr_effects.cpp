// mr_effects.cpp -- particle systems, poly grids, line systems and
// canvases (docs/objects.md part III.2-5).
//
// The engine moves the particles and advances the textures' animations;
// the renderer only reads. All four are relative to their object's
// position, rotation and scale; particles and lines are always
// translucent, poly grids and canvases only when the object says so.

#include <math.h>

#include "mrobjects.h"

#define FLAG_UNSIGNED            0x2     // poly grid: heights are unsigned
#define FLAG_ENVIRONMENTMAPONLY  0x20
#define CF_SOLIDCANVAS           0x10    // client flags (LTObject+0x180)

template <typename T> static inline T rd(const void* p, uint32_t off) { T v; memcpy(&v, (const uint8_t*)p + off, sizeof(T)); return v; }

// The object's matrix, T * R * S, as LTObject::SetupTransform builds it.
static void mr_ObjectMatrix(LTObject* pObj, LTMatrix* pM)
{
    float R[3][3];
    mr_RotationMatrix(pObj->m_Rotation, R);
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
static MRTexture* mr_BindShared(SharedTexture* pShared)
{
    MRTexture* pTex = NULL;
    if (pShared)
    {
        mr_TextureId(pShared);
        pTex = (MRTexture*)pShared->m_pRenderData;
    }
    mr_ForgetBindings();
    mr_SetBaseTexture(pTex);
    rd_TexEnv(RD_ENV_MODULATE);
    return pTex;
}

// Two texels in from each edge, as sprites.
static void mr_InsetUV(MRTexture* pTex, float* pUV)
{
    float W = pTex ? (float)pTex->m_nBaseWidth : 0.0f, H = pTex ? (float)pTex->m_nBaseHeight : 0.0f;
    pUV[0] = W > 4 ? 2.0f / W : 0.0f;
    pUV[1] = H > 4 ? 2.0f / H : 0.0f;
    pUV[2] = W > 4 ? (W - 2.0f) / W : 1.0f;
    pUV[3] = H > 4 ? (H - 2.0f) / H : 1.0f;
}

// The object's own blend, fog and (for really-close objects) camera, for
// the length of one draw.
struct MREffectState {
    LTObject* m_pObj;
    bool      m_bClose;
    MREffectState(LTObject* pObj, bool bBlend) : m_pObj(pObj)
    {
        rd_PushAttrib(RD_ATTR_COLOR | RD_ATTR_FOG | RD_ATTR_ENABLE | RD_ATTR_TEXTURE);
        if (bBlend)
        {
            rd_Enable(RD_BLEND);
            mr_ObjectBlend(pObj);
        }
        mr_ObjectFog(pObj, true);
        m_bClose = (pObj->m_Flags & FLAG_REALLYCLOSE) != 0;
        if (m_bClose)
            mr_BeginReallyClose(pObj);
        rd_MatrixMode(RD_MODELVIEW);
        rd_PushMatrix();
        if (m_bClose)
            rd_LoadIdentity();
    }
    ~MREffectState()
    {
        rd_MatrixMode(RD_MODELVIEW);
        rd_PopMatrix();
        if (m_bClose)
            mr_EndReallyClose();
        mr_ObjectFog(m_pObj, false);
        rd_PopAttrib();
        mr_ForgetBindings();
    }
};

// ---- particle systems ------------------------------------------------------------

// r_DrawParticleSystem / r_DrawParticleBatch: each particle a screen-
// aligned square around its place in the system, half a side of 2 x its
// size in world units (d3d.ren 0x1000daa0: size x xScale x view width x
// rhw pixels), coloured by the particle and the object.
void mr_DrawParticleSystem(LTObject* pObj)
{
    if (!cvi(CV_DrawParticles))
        return;
    const uint8_t* p = (const uint8_t*)pObj;
    const uint8_t* pHead = p + 0x1a8;
    const uint8_t* pFirst = rd<const uint8_t*>(p, 0x1a8 + 0x20);
    if (!pFirst || pFirst == pHead)
        return;

    MREffectState state(pObj, true);
    MRTexture* pTex = mr_BindShared(rd<SharedTexture*>(p, 0x1e8));
    float uv[4];
    mr_InsetUV(pTex, uv);

    LTMatrix M;
    mr_ObjectMatrix(pObj, &M);
    const MRCamera& cam = g_Frame.m_Cam;
    float R[3][3];
    memcpy(R, cam.m_R, sizeof(R));
    // Camera space: the view (or, really close, the object is there already).
    float fW = (float)cam.m_nViewW, fH = (float)cam.m_nViewH;
    float fAspect = (fW / fH) * (cam.m_fTanY / cam.m_fTanX);   // a square on screen
    float fR = pObj->m_ColorR / 255.0f, fG = pObj->m_ColorG / 255.0f, fB = pObj->m_ColorB / 255.0f;
    float fA = pObj->m_ColorA;
    bool bClose = state.m_bClose;

    rd_LoadIdentity();
    rd_Begin(RD_QUADS);
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
        rd_Color4ub((uint8_t)(col.x * fR > 255 ? 255 : col.x * fR < 0 ? 0 : col.x * fR),
                   (uint8_t)(col.y * fG > 255 ? 255 : col.y * fG < 0 ? 0 : col.y * fG),
                   (uint8_t)(col.z * fB > 255 ? 255 : col.z * fB < 0 ? 0 : col.z * fB),
                   (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
        rd_TexCoord2f(uv[0], uv[1]); rd_Vertex3f(c.x - hx, c.y + hy, c.z);
        rd_TexCoord2f(uv[2], uv[1]); rd_Vertex3f(c.x + hx, c.y + hy, c.z);
        rd_TexCoord2f(uv[2], uv[3]); rd_Vertex3f(c.x + hx, c.y - hy, c.z);
        rd_TexCoord2f(uv[0], uv[3]); rd_Vertex3f(c.x - hx, c.y - hy, c.z);
    }
    rd_End();
}

// ---- poly grids ---------------------------------------------------------------------

// r_DrawPolyGrid: a w x h height field around the object, one colour per
// height from the object's colour table, the texture the frame of its
// sprite, panned and scaled.
void mr_DrawPolyGrid(LTObject* pObj)
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

    bool bTranslucent = pObj->m_ColorA != 255;
    MREffectState state(pObj, bTranslucent);
    SharedTexture** ppFrame = rd<SharedTexture**>(p, 0x1b4 + 8);
    SharedTexture* pShared = ppFrame ? *ppFrame : NULL;

    // The texture's environment map (EnvMapPolyGrids): the map on unit 0,
    // the texture over it on unit 1 (mr_BeginEnvUnit);
    // FLAG_ENVIRONMENTMAPONLY draws the map alone.
    MRTexture* pEnv = cvi(CV_EnvMapPolyGrids) ? mr_EnvMapOf(pShared) : NULL;
    bool bEnvOnly = (pObj->m_Flags & FLAG_ENVIRONMENTMAPONLY) != 0;
    if (bEnvOnly && !pEnv)
        return;
    MRTexture* pTex = NULL;
    if (pShared)
    {
        mr_TextureId(pShared);
        pTex = (MRTexture*)pShared->m_pRenderData;
    }
    bool bEnvUnit = pEnv && !bEnvOnly && mr_BeginEnvUnit(pTex);
    mr_BindShared(pEnv && (bEnvOnly || bEnvUnit) ? pShared->m_pLinkedTexture : pShared);
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
    // The colours go through d3d.ren's colour ramps, which Saturate doubles.
    float fRamp = cvi(CV_Saturate) ? 2.0f : 1.0f;

    LTMatrix M;
    mr_ObjectMatrix(pObj, &M);
    mr_MultMatrix(M);
    float fHalfW = (w - 1) * 0.5f, fHalfH = (h - 1) * 0.5f;

    // Env coordinates (r_DrawPolyGrid's 0x18eb80): each vertex's normal
    // from the heights around it along the grid's scaled axes, the camera's
    // reflection off it.
    LTVector xa = { M.m[0][0], M.m[1][0], M.m[2][0] }, up = { M.m[0][1], M.m[1][1], M.m[2][1] },
             za = { M.m[0][2], M.m[1][2], M.m[2][2] };
    float fUp2 = (up.x * up.x + up.y * up.y + up.z * up.z) / 128.0f;
    float fXa = sqrtf(xa.x * xa.x + xa.y * xa.y + xa.z * xa.z), fZa = sqrtf(za.x * za.x + za.y * za.y + za.z * za.z);
    float kx = fXa > 0.0f ? 0.5f * fUp2 / fXa : 0.0f, kz = fZa > 0.0f ? 0.5f * fUp2 / fZa : 0.0f;
    bool bEnvCoords = bEnvUnit || bEnvOnly;
    const LTVector& vCam = g_Frame.m_Cam.m_Pos;

    rd_Begin(RD_TRIANGLES);
    for (uint32_t i = 0; i < nIndices; i++)
    {
        uint32_t idx = pIndices[i];
        if (idx >= w * h)
            continue;
        uint32_t row = idx / w, col = idx % w;
        int nValue = bUnsigned ? (int)(uint8_t)pData[idx] - 128 : (int)pData[idx];
        int nColor = bUnsigned ? (int)(uint8_t)pData[idx] : (int)pData[idx] + 128;
        const float* c = &pColors[nColor * 4];
        float a = c[3] * fA, r = c[0] * fRamp, gg = c[1] * fRamp, b = c[2] * fRamp;
        rd_Color4ub((uint8_t)(r > 255 ? 255 : r < 0 ? 0 : r), (uint8_t)(gg > 255 ? 255 : gg < 0 ? 0 : gg),
                   (uint8_t)(b > 255 ? 255 : b < 0 ? 0 : b), (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
        float s = 0.0f, t = 0.0f;
        if (bEnvCoords)
        {
            int hl = pData[col > 0 ? idx - 1 : idx], hr = pData[col + 1 < w ? idx + 1 : idx];
            int hu = pData[row > 0 ? idx - w : idx], hd = pData[row + 1 < h ? idx + w : idx];
            float gx = (hl - hr) * kx, gz = (hu - hd) * kz;
            LTVector N = { up.x + xa.x * gx + za.x * gz, up.y + xa.y * gx + za.y * gz, up.z + xa.z * gx + za.z * gz };
            float len = sqrtf(N.x * N.x + N.y * N.y + N.z * N.z);
            if (len > 0.0f)
            {
                N.x /= len; N.y /= len; N.z /= len;
            }
            LTVector local = { col - fHalfW, (float)nValue, row - fHalfH };
            LTVector pos = { M.m[0][0] * local.x + M.m[0][1] * local.y + M.m[0][2] * local.z + M.m[0][3],
                             M.m[1][0] * local.x + M.m[1][1] * local.y + M.m[1][2] * local.z + M.m[1][3],
                             M.m[2][0] * local.x + M.m[2][1] * local.y + M.m[2][2] * local.z + M.m[2][3] };
            mr_ReflectCoords(vCam, pos, N, &s, &t);
        }
        if (bEnvOnly)
            rd_TexCoord2f(s, t);
        else if (bEnvUnit)
        {
            rd_TexCoord2f(s, t);
            rd_MultiTexCoord2f(1, (col + uPan) * uScale, (row + vPan) * vScale);
        }
        else
            rd_TexCoord2f((col + uPan) * uScale, (row + vPan) * vScale);
        rd_Vertex3f(col - fHalfW, (float)nValue, row - fHalfH);
    }
    rd_End();
    if (bEnvUnit)
        mr_EndEnvUnit();
}

// ---- line systems ---------------------------------------------------------------------

// r_DrawLineSystem: each line between its two points, colours 0..1 (the
// SDK's, as d3d.ren reads them), alpha times the object's.
void mr_DrawLineSystem(LTObject* pObj)
{
    if (!cvi(CV_DrawLineSystems))
        return;
    const uint8_t* p = (const uint8_t*)pObj;
    const uint8_t* pHead = p + 0x1b0;
    const uint8_t* pFirst = rd<const uint8_t*>(p, 0x1b0 + 0x40);
    if (!pFirst || pFirst == pHead)
        return;
    MREffectState state(pObj, true);
    mr_ForgetBindings();
    mr_SetBaseTexture(NULL);
    LTMatrix M;
    mr_ObjectMatrix(pObj, &M);
    mr_MultMatrix(M);
    float fA = pObj->m_ColorA;
    rd_Begin(RD_LINES);
    int nGuard = 0;
    for (const uint8_t* q = pFirst; q && q != pHead && nGuard < 100000; q = rd<const uint8_t*>(q, 0x40), nGuard++)
    {
        for (int k = 0; k < 2; k++)
        {
            const float* pt = (const float*)(q + k * 0x1c);
            float a = pt[6] * fA;
            rd_Color4ub((uint8_t)(pt[3] * 255 > 255 ? 255 : pt[3] < 0 ? 0 : pt[3] * 255),
                       (uint8_t)(pt[4] * 255 > 255 ? 255 : pt[4] < 0 ? 0 : pt[4] * 255),
                       (uint8_t)(pt[5] * 255 > 255 ? 255 : pt[5] < 0 ? 0 : pt[5] * 255),
                       (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a));
            rd_Vertex3f(pt[0], pt[1], pt[2]);
        }
    }
    rd_End();
}

// ---- canvases --------------------------------------------------------------------------
//
// A canvas is the game's own drawing: its function gets an ILTCustomDraw
// (iltcustomdraw.h: five virtuals, thiscall) and draws world-space
// polygons through it (CanvasDrawMgr).

enum { LTRS_ALPHABLEND, LTRS_ZREAD, LTRS_ZWRITE, LTRS_SRCBLEND, LTRS_DESTBLEND, LTRS_TEXADDR,
       LTRS_COLOROP, LTRS_ALPHAOP, LTRS_COUNT };

struct MRCustomDraw;
struct MRCustomDrawVtbl {
    LTRESULT (__thiscall* DrawPrimitive)(MRCustomDraw* pThis, const void* pVerts, uint32_t nVerts, uint32_t nFlags);
    LTRESULT (__thiscall* SetState)(MRCustomDraw* pThis, uint32_t nState, uint32_t nVal);
    LTRESULT (__thiscall* GetState)(MRCustomDraw* pThis, uint32_t nState, uint32_t* pVal);
    LTRESULT (__thiscall* SetTexture)(MRCustomDraw* pThis, const char* pName);
    LTRESULT (__thiscall* GetTexelSize)(MRCustomDraw* pThis, float* pU, float* pV);
};
struct MRCustomDraw {
    const MRCustomDrawVtbl* m_pVtbl;
    uint32_t    m_States[LTRS_COUNT];
    MRTexture* m_pTex;
};

static RDBlend mr_LTBlend(uint32_t n)
{
    static const RDBlend blend[6] = { RD_ZERO, RD_ONE, RD_SRC_COLOR, RD_ONE_MINUS_SRC_COLOR,
                                      RD_SRC_ALPHA, RD_ONE_MINUS_SRC_ALPHA };
    return n >= 1 && n <= 6 ? blend[n - 1] : RD_ONE;
}

static void mr_ApplyCanvasOp(uint32_t nColorOp, uint32_t nAlphaOp)
{
    // LTOP_SELECTTEXTURE 1, SELECTDIFFUSE 2, MODULATE 3, ADD 4, ADDSIGNED 5
    static const RDCombineOp op[5] = { RD_OP_REPLACE, RD_OP_REPLACE, RD_OP_MODULATE, RD_OP_ADD, RD_OP_ADD_SIGNED };
    uint32_t c = nColorOp >= 1 && nColorOp <= 5 ? nColorOp : 3;
    uint32_t a = nAlphaOp >= 1 && nAlphaOp <= 5 ? nAlphaOp : 2;
    RDCombine cb = {
        op[c - 1], c == 2 ? RD_ARG_PRIMARY : RD_ARG_TEXTURE, RD_ARG_PRIMARY, 1.0f,
        op[a - 1], a == 2 ? RD_ARG_PRIMARY : RD_ARG_TEXTURE, RD_ARG_PRIMARY,
    };
    rd_TexCombine(cb);
}

static void mr_ApplyCanvasState(MRCustomDraw* pThis, uint32_t nState)
{
    uint32_t v = pThis->m_States[nState];
    switch (nState)
    {
    case LTRS_ALPHABLEND: if (v) rd_Enable(RD_BLEND); else rd_Disable(RD_BLEND); break;
    case LTRS_ZREAD:      if (v) rd_Enable(RD_DEPTH_TEST); else rd_Disable(RD_DEPTH_TEST); break;
    case LTRS_ZWRITE:     rd_DepthMask(v != 0); break;
    case LTRS_SRCBLEND:
    case LTRS_DESTBLEND:
        rd_BlendFunc(mr_LTBlend(pThis->m_States[LTRS_SRCBLEND]), mr_LTBlend(pThis->m_States[LTRS_DESTBLEND]));
        break;
    case LTRS_TEXADDR:
        if (pThis->m_pTex)
        {
            rd_TexWrap(v != 2, v != 2);
        }
        break;
    case LTRS_COLOROP:
    case LTRS_ALPHAOP:
        if (g_RDCaps.bCombine)
            mr_ApplyCanvasOp(pThis->m_States[LTRS_COLOROP], pThis->m_States[LTRS_ALPHAOP]);
        break;
    }
}

static LTRESULT __thiscall cd_DrawPrimitive(MRCustomDraw* pThis, const void* pVerts, uint32_t nVerts, uint32_t nFlags)
{
    (void)pThis; (void)nFlags;
    if (!pVerts || nVerts < 3 || nVerts > 100)
        return 1;                       // LT_ERROR
    // LTVertex: m_Vec, m_RHW, colour (b,g,r,a), specular, tu, tv -- 0x20 bytes.
    const uint8_t* v = (const uint8_t*)pVerts;
    rd_Begin(RD_TRIANGLE_FAN);
    for (uint32_t i = 0; i < nVerts; i++, v += 0x20)
    {
        rd_Color4ub(v[0x12], v[0x11], v[0x10], v[0x13]);
        rd_TexCoord2f(rd<float>(v, 0x18), rd<float>(v, 0x1c));
        rd_Vertex3f(rd<float>(v, 0), rd<float>(v, 4), rd<float>(v, 8));
    }
    rd_End();
    return 0;                           // LT_OK
}

static LTRESULT __thiscall cd_SetState(MRCustomDraw* pThis, uint32_t nState, uint32_t nVal)
{
    if (nState >= LTRS_COUNT)
        return 1;
    pThis->m_States[nState] = nVal;
    mr_ApplyCanvasState(pThis, nState);
    return 0;
}

static LTRESULT __thiscall cd_GetState(MRCustomDraw* pThis, uint32_t nState, uint32_t* pVal)
{
    if (nState >= LTRS_COUNT || !pVal)
        return 1;
    *pVal = pThis->m_States[nState];
    return 0;
}

static LTRESULT __thiscall cd_SetTexture(MRCustomDraw* pThis, const char* pName)
{
    SharedTexture* pShared = pName ? g.pStruct->GetSharedTexture(pName) : NULL;
    pThis->m_pTex = mr_BindShared(pShared);
    mr_ApplyCanvasState(pThis, LTRS_TEXADDR);
    mr_ApplyCanvasState(pThis, LTRS_COLOROP);
    return pName && !pThis->m_pTex ? 1 : 0;
}

static LTRESULT __thiscall cd_GetTexelSize(MRCustomDraw* pThis, float* pU, float* pV)
{
    MRTexture* pTex = pThis->m_pTex;
    if (pU) *pU = pTex && pTex->m_nBaseWidth ? 1.0f / pTex->m_nBaseWidth : 0.0f;
    if (pV) *pV = pTex && pTex->m_nBaseHeight ? 1.0f / pTex->m_nBaseHeight : 0.0f;
    return 0;
}

static const MRCustomDrawVtbl g_CustomDrawVtbl = {
    cd_DrawPrimitive, cd_SetState, cd_GetState, cd_SetTexture, cd_GetTexelSize,
};

typedef void (*CanvasDrawFn)(MRCustomDraw* pDraw, LTObject* hObj, void* pUser);

// CanvasDrawMgr::DrawCanvas: the default states (InitStates), the game's
// function, then everything put back.
void mr_DrawCanvas(LTObject* pObj)
{
    if (!cvi(CV_DrawCanvases))
        return;
    CanvasDrawFn fn = rd<CanvasDrawFn>(pObj, 0x1a8);
    if (!fn)
        return;
    MREffectState state(pObj, false);
    MRCustomDraw draw;
    draw.m_pVtbl = &g_CustomDrawVtbl;
    draw.m_pTex = NULL;
    static const uint32_t def[LTRS_COUNT] = { 0, 1, 1, 5, 6, 1, 3, 2 };
    memcpy(draw.m_States, def, sizeof(def));
    mr_ForgetBindings();
    mr_SetBaseTexture(NULL);
    for (uint32_t i = 0; i < LTRS_COUNT; i++)
        mr_ApplyCanvasState(&draw, i);
    rd_PushAttrib(RD_ATTR_DEPTH);
    fn(&draw, pObj, rd<void*>(pObj, 0x1ac));
    rd_PopAttrib();
}

bool mr_CanvasIsSolid(LTObject* pObj)
{
    return (rd<uint16_t>(pObj, 0x180) & CF_SOLIDCANVAS) != 0;
}

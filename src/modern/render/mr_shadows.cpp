// mr_shadows.cpp -- model shadows: d3d.ren's projected-shadow pass
// (0x10048180, 0x10049bb4, the collector 0x10073700).
//
// A model with FLAG_SHADOW (after the model hook) and ShadowEnable in its
// command string casts a soft blob straight down. A point light stands
// ShadowLightDist above the model; the world polys that segment from it,
// ShadowLightDist + ShadowProjectLength long, passes near (ShadowSizeX/Y)
// are gathered through the world tree, and each is drawn again, lifted
// off its plane by ModelShadowOffset and clipped to the shadow's volume,
// black under the blob texture at ModelShadowAlpha. The blob is projected
// from the light through the model's centre (ShadowCenterOffset).
//
// d3d.ren reads a world model's BSP as if it were in world space, right
// only for one that has not moved; here a moved one has the light brought
// into its space and its polys back out.

#include <math.h>

#include "mrobjects.h"

template <typename T> static inline T rd(const void* p, uint32_t off) { T v; memcpy(&v, (const uint8_t*)p + off, sizeof(T)); return v; }

// ---- the blob (d3d.ren's 0x10039210 at device start, d3dqip2.cpp) -----------------

static uint32_t g_BlobId;
static uint32_t g_nBlobGen;

static uint32_t mr_BlobTexture(void)
{
    if (g_BlobId && g_nBlobGen == g.nContextGen)
        return g_BlobId;
    // 16x16 white, alpha falling off as 1 - (d/7.5)^3 from the centre.
    uint8_t px[16 * 16 * 4];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            float fx = x - 7.5f, fy = y - 7.5f;
            float d = sqrtf(fx * fx + fy * fy);
            if (d > 7.5f)
                d = 7.5f;
            float t = d / 7.5f;
            int a = (int)((1.0f - t * t * t) * 255.9f);
            uint8_t* p = &px[(y * 16 + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a);
        }
    g_BlobId = rd_GenTexture();
    rd_BindTexture(g_BlobId);
    rd_TexFilter(RD_LINEAR, RD_LINEAR);
    rd_TexWrap(false, false);
    rd_TexMaxLevel(0);
    rd_TexImage(0, RD_FMT_RGBA8, 16, 16, RD_PIX_RGBA, px);
    g_nBlobGen = g.nContextGen;
    return g_BlobId;
}

// ---- small vector maths ------------------------------------------------------------

static inline LTVector v3(float x, float y, float z) { LTVector v = { x, y, z }; return v; }
static inline LTVector vadd(const LTVector& a, const LTVector& b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline LTVector vsub(const LTVector& a, const LTVector& b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline LTVector vmul(const LTVector& a, float f) { return v3(a.x * f, a.y * f, a.z * f); }
static inline float vdot(const LTVector& a, const LTVector& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline LTVector vcross(const LTVector& a, const LTVector& b)
{
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline LTVector vnorm(const LTVector& a)
{
    float m = sqrtf(vdot(a, a));
    return m != 0.0f ? vmul(a, 1.0f / m) : a;
}
static inline LTVector vxform(const LTMatrix& M, const LTVector& v)
{
    return v3(M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z + M.m[0][3],
              M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z + M.m[1][3],
              M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z + M.m[2][3]);
}
static inline LTVector vrot(const LTMatrix& M, const LTVector& v)
{
    return v3(M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z,
              M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z,
              M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z);
}

// ---- gathering the receivers (d3d.ren's 0x10073700) --------------------------------

#define MR_MAXSHADOWPOLYS 256

struct ShadowItem {
    WorldPoly*                m_pPoly;
    const WorldModelInstance* m_pWM;    // whose space the poly is in; NULL: the world's
};

struct ShadowQuery {
    ShadowItem* m_pItems;
    uint32_t    m_nItems;
    LTVector    m_vStart, m_vEnd;
    float       m_fRadius;
};

// The world tree's slot 3 (+0x0c): the objects filed near a point, to a
// callback (d3d.ren calls it with the model's position and this collector).
typedef void (__thiscall* NearObjectsFn)(void* pTree, LTVector* pPos, void* pCB, void* pUser, uint32_t nFlags);

static bool mr_IsIdentity(const LTMatrix& M)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (fabsf(M.m[r][c] - (r == c ? 1.0f : 0.0f)) > 1.0e-4f)
                return false;
    return true;
}

// Walk an object's BSP along the light's segment (its space), collecting the
// polys drawn this frame that face the camera and face up the segment
// enough to take a shadow, near enough to it.
static void mr_ShadowCollect(LTObject* pObj, void* pUser)
{
    ShadowQuery* q = (ShadowQuery*)pUser;
    if (rd<uint32_t>(pObj, 0x54) || pObj->m_ObjectType != OT_WORLDMODEL)
        return;
    const WorldModelInstance* pWM = (const WorldModelInstance*)pObj;
    // The BSP's root through its interface (vtable +0x04, GetRootNode), as
    // the collector reads it.
    WorldBsp* pBsp = pWM->m_pWorldBsp;
    if (!pBsp)
        return;
    typedef Node* (__thiscall* GetRootNodeFn)(void* pThis);
    Node* pRoot = ((*(GetRootNodeFn**)pBsp)[1])(pBsp);
    if (!pRoot)
        return;
    bool bMoved = !mr_IsIdentity(pWM->m_Transform);
    LTVector vStart = q->m_vStart, vEnd = q->m_vEnd, vCam = g_Frame.m_Cam.m_Pos;
    if (bMoved)
    {
        vStart = vxform(pWM->m_BackTransform, vStart);
        vEnd = vxform(pWM->m_BackTransform, vEnd);
        vCam = vxform(pWM->m_BackTransform, vCam);
    }
    LTVector vDir = vnorm(vsub(vEnd, vStart));

    struct Seg { Node* m_pNode; LTVector m_vStart, m_vEnd; };
    Seg stack[512];
    int nTop = 0;
    Node* pNode = pRoot;
    for (;;)
    {
        if (!pNode || (pNode->m_Flags & 3))
        {
            if (!nTop)
                return;
            nTop--;
            pNode = stack[nTop].m_pNode;
            vStart = stack[nTop].m_vStart;
            vEnd = stack[nTop].m_vEnd;
            continue;
        }
        WorldPoly* pPoly = pNode->m_pPoly;
        if (!pPoly || !pPoly->m_pSurface || !pPoly->m_pSurface->m_pPlane)
            return;
        const LTPlane& P = *pPoly->m_pSurface->m_pPlane;
        float d1 = vdot(P.m_Normal, vStart) - P.m_Dist, d2 = vdot(P.m_Normal, vEnd) - P.m_Dist;
        float r = q->m_fRadius;
        if (d1 > r && d2 > r)
        {
            pNode = pNode->m_Sides[1];
            continue;
        }
        if (d1 < -r && d2 < -r)
        {
            pNode = pNode->m_Sides[0];
            continue;
        }
        int side = d1 > 0.0f ? 1 : 0;
        float t = fabsf(d1 - d2) > 0.00001f ? d1 / (d1 - d2) : 0.0f;
        LTVector vMid = vadd(vStart, vmul(vsub(vEnd, vStart), t));
        if (pPoly->m_FrameCode == g_Frame.m_WorldFrameCode && q->m_nItems < MR_MAXSHADOWPOLYS &&
            vdot(P.m_Normal, vCam) - P.m_Dist > 1.0f && vdot(P.m_Normal, vDir) < 0.7f &&
            !(pPoly->m_pSurface->m_Flags & (SURF_INVISIBLE | SURF_SKY)))
        {
            LTVector vOff = vsub(vMid, pPoly->m_Center);
            if (sqrtf(vdot(vOff, vOff)) < pPoly->m_Radius + q->m_fRadius)
            {
                uint32_t i;
                for (i = 0; i < q->m_nItems && q->m_pItems[i].m_pPoly != pPoly; i++)
                    ;
                if (i == q->m_nItems)
                {
                    q->m_pItems[i].m_pPoly = pPoly;
                    q->m_pItems[i].m_pWM = bMoved ? pWM : NULL;
                    q->m_nItems++;
                }
            }
        }
        if (nTop >= 512)
        {
            pNode = NULL;               // full: unwind
            continue;
        }
        if (t > 0.0f && t < 1.0f)
        {
            stack[nTop].m_vStart = vMid;
            stack[nTop].m_vEnd = vEnd;
            vEnd = vMid;
        }
        else
        {
            stack[nTop].m_vStart = vStart;
            stack[nTop].m_vEnd = vEnd;
        }
        stack[nTop].m_pNode = pNode->m_Sides[side == 0];
        nTop++;
        pNode = pNode->m_Sides[side];
    }
}

// ---- drawing ---------------------------------------------------------------------------

static void mat_Mul4(LTMatrix* pOut, const LTMatrix& a, const LTMatrix& b)
{
    LTMatrix r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
    *pOut = r;
}

// Clip a polygon to keep where dot(n, p) - d > 0 (d3d.ren's 0x10047d80).
static int mr_ClipToPlane(const LTVector* pIn, int nIn, const LTPlane& P, LTVector* pOut)
{
    int nOut = 0;
    for (int i = 0; i < nIn; i++)
    {
        const LTVector& a = pIn[i];
        const LTVector& b = pIn[(i + 1) % nIn];
        float da = vdot(P.m_Normal, a) - P.m_Dist, db = vdot(P.m_Normal, b) - P.m_Dist;
        if (da > 0.0f)
            pOut[nOut++] = a;
        if ((da > 0.0f) != (db > 0.0f))
        {
            float t = da / (da - db);
            pOut[nOut++] = vadd(a, vmul(vsub(b, a), t));
        }
    }
    return nOut;
}

void mr_DrawModelShadow(LTObject* pInst, const void* pModel)
{
    int nDirs = cvi(CV_MaxModelShadows);
    MainWorld* pWorld = g_Frame.m_pContext ? g_Frame.m_pContext->m_pWorld : NULL;
    if (nDirs <= 0 || !pWorld || !pModel || !rd<uint32_t>(pModel, 0xd4))
        return;
    float fProject = rd<float>(pModel, 0xd8);       // ShadowProjectLength
    float fLightDist = rd<float>(pModel, 0xdc);     // ShadowLightDist
    float fHalfH = rd<float>(pModel, 0xe0);         // ShadowSizeX
    float fHalfW = rd<float>(pModel, 0xe4);         // ShadowSizeY
    LTVector vOffset = rd<LTVector>(pModel, 0xe8);  // ShadowCenterOffset
    if (fHalfH <= 0.0f || fHalfW <= 0.0f)
        return;
    const LTVector& vPos = pInst->m_Pos;
    float fLength = fLightDist + fProject;

    // The one direction d3d.ren fills in: straight down.
    LTVector vDown = v3(0.0f, -1.0f, 0.0f);
    LTVector vStart = vsub(vPos, vmul(vDown, fLightDist));
    LTVector vDir = vnorm(vsub(vadd(vPos, vOffset), vStart));
    LTVector vEnd = vadd(vStart, vmul(vDir, fLength));

    static ShadowItem s_Items[MR_MAXSHADOWPOLYS];
    ShadowQuery q;
    q.m_pItems = s_Items;
    q.m_nItems = 0;
    q.m_vStart = vStart;
    q.m_vEnd = vEnd;
    q.m_fRadius = fHalfH > fHalfW ? fHalfH : fHalfW;
    void* pTree = pWorld->m_WorldTree;
    LTVector vQueryPos = vPos;
    ((*(NearObjectsFn**)pTree)[3])(pTree, &vQueryPos, (void*)mr_ShadowCollect, &q, 0);
    if (!q.m_nItems)
        return;

    // The volume: below the model, above the far cap, and four sides through
    // the light and the blob's corners.
    LTVector vNormal = vDir;
    LTVector vHint = fabsf(vNormal.y) > 0.9f ? v3(1.0f, 0.0f, 0.0f) : v3(0.0f, 1.0f, 0.0f);
    LTVector vRight = vnorm(vcross(vHint, vNormal));
    LTVector vUp = vcross(vRight, vNormal);         // normal.Cross(right), LithTech's
    float fDist = vdot(vDir, vEnd);
    LTVector c0 = vsub(vEnd, vmul(vUp, fHalfH)), c1 = vadd(vEnd, vmul(vUp, fHalfH));
    LTVector c2 = vadd(vEnd, vmul(vRight, fHalfW)), c3 = vsub(vEnd, vmul(vRight, fHalfW));
    LTVector vCorner = vsub(vsub(vEnd, vmul(vUp, fHalfH)), vmul(vRight, fHalfW));
    LTPlane planes[6];
    planes[0].m_Normal = vNormal;
    planes[0].m_Dist = vdot(vNormal, vPos);
    planes[1].m_Normal = vmul(vNormal, -1.0f);
    planes[1].m_Dist = vdot(planes[1].m_Normal, vadd(vPos, vmul(vNormal, fProject)));
    planes[2].m_Normal = vnorm(vcross(vRight, vsub(c0, vStart)));
    planes[4].m_Normal = vnorm(vcross(vsub(c1, vStart), vRight));
    planes[3].m_Normal = vnorm(vcross(vUp, vsub(c2, vStart)));
    planes[5].m_Normal = vnorm(vcross(vsub(c3, vStart), vUp));
    for (int i = 2; i < 6; i++)
        planes[i].m_Dist = vdot(planes[i].m_Normal, vStart);

    // Texture coordinates: the point light's projection onto the far cap's
    // plane, then the blob's basis there, scaled to 0..1.
    LTMatrix mShadow, mBasis, mScale, mTemp, mTex;
    float k = vdot(vDir, vStart) - fDist;
    const LTVector& b = vStart;
    const LTVector& n = vDir;
    float bb[3] = { b.x, b.y, b.z }, nn[3] = { n.x, n.y, n.z };
    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
            mShadow.m[r][c] = (r == c ? k : 0.0f) - bb[r] * nn[c];
        mShadow.m[r][3] = -bb[r] * -fDist;
    }
    mShadow.m[3][0] = -n.x; mShadow.m[3][1] = -n.y; mShadow.m[3][2] = -n.z;
    mShadow.m[3][3] = k + fDist;
    memset(&mBasis, 0, sizeof(mBasis));
    mBasis.m[0][0] = vUp.x; mBasis.m[0][1] = vUp.y; mBasis.m[0][2] = vUp.z;
    mBasis.m[0][3] = -vdot(vUp, vCorner);
    mBasis.m[1][0] = vRight.x; mBasis.m[1][1] = vRight.y; mBasis.m[1][2] = vRight.z;
    mBasis.m[1][3] = -vdot(vRight, vCorner);
    mBasis.m[2][2] = mBasis.m[3][3] = 1.0f;
    memset(&mScale, 0, sizeof(mScale));
    mScale.m[0][0] = 1.0f / (2.0f * fHalfH);
    mScale.m[1][1] = 1.0f / (2.0f * fHalfW);
    mScale.m[2][2] = mScale.m[3][3] = 1.0f;
    mat_Mul4(&mTemp, mScale, mBasis);
    mat_Mul4(&mTex, mTemp, mShadow);

    rd_PushAttrib(RD_ATTR_COLOR | RD_ATTR_DEPTH | RD_ATTR_ENABLE | RD_ATTR_TEXTURE);
    mr_ForgetBindings();
    rd_Enable(RD_TEXTURE_2D);
    rd_BindTexture(mr_BlobTexture());
    rd_TexEnv(RD_ENV_MODULATE);
    rd_Enable(RD_BLEND);
    rd_BlendFunc(RD_SRC_ALPHA, RD_ONE_MINUS_SRC_ALPHA);
    rd_Disable(RD_ALPHA_TEST);
    rd_Enable(RD_DEPTH_TEST);
    rd_DepthFunc(RD_LEQUAL);
    rd_DepthMask(false);
    float fAlpha = cvf(CV_ModelShadowAlpha);
    rd_Color4ub(0, 0, 0, (uint8_t)(fAlpha < 0.0f ? 0.0f : fAlpha > 255.0f ? 255.0f : fAlpha));
    float fLift = cvf(CV_ModelShadowOffset);

    LTVector bufA[64], bufB[64];
    for (uint32_t i = 0; i < q.m_nItems; i++)
    {
        WorldPoly* pPoly = q.m_pItems[i].m_pPoly;
        const WorldModelInstance* pWM = q.m_pItems[i].m_pWM;
        uint32_t nVerts = pPoly->m_nVerts;
        if (nVerts < 3 || nVerts > 32)
            continue;
        LTVector vLift = vmul(pPoly->m_pSurface->m_pPlane->m_Normal, fLift);
        if (pWM)
            vLift = vrot(pWM->m_Transform, vLift);
        int n = 0;
        for (uint32_t v = 0; v < nVerts; v++)
        {
            LTVector p = *pPoly->m_Verts[v].m_pPos;
            if (pWM)
                p = vxform(pWM->m_Transform, p);
            bufA[n++] = vadd(p, vLift);
        }
        LTVector* pIn = bufA;
        LTVector* pOut = bufB;
        for (int pl = 0; pl < 6 && n >= 3; pl++)
        {
            n = mr_ClipToPlane(pIn, n, planes[pl], pOut);
            LTVector* t = pIn; pIn = pOut; pOut = t;
        }
        if (n < 3)
            continue;
        rd_Begin(RD_TRIANGLE_FAN);
        for (int v = 0; v < n; v++)
        {
            const LTVector& p = pIn[v];
            float u = mTex.m[0][0] * p.x + mTex.m[0][1] * p.y + mTex.m[0][2] * p.z + mTex.m[0][3];
            float w = mTex.m[1][0] * p.x + mTex.m[1][1] * p.y + mTex.m[1][2] * p.z + mTex.m[1][3];
            float qq = mTex.m[3][0] * p.x + mTex.m[3][1] * p.y + mTex.m[3][2] * p.z + mTex.m[3][3];
            rd_TexCoord4f(u, w, 0.0f, qq);
            rd_Vertex3f(p.x, p.y, p.z);
        }
        rd_End();
    }
    rd_PopAttrib();
    mr_ForgetBindings();
}

// gl_scene.cpp -- the 3D half: the world context, RenderScene, the world.
//
// RenderScene follows the Mac renderer's r_RenderScene / r_FullDrawScene
// (docs/world.md part A, docs/objects.md part I.2):
//
//   * the camera (r_InitFrustrum2's projection, done by GL here: the Mac
//     projects every vertex itself);
//   * the visible set: in the normal draw mode the engine's own visibility
//     query walks the PVS and calls back for every visible leaf (whose
//     polygons are queued) and object; the object-list draw mode
//     (ILTClient::RenderObjects) queues just the list;
//   * the sky, from its squashed camera, wherever sky polygons are visible;
//   * the world's polygons: base texture times lightmap or vertex colours;
//   * the objects, in r_FlushObjectQueues' order: solid world models, solid
//     models, chromakey, then the translucent ones far to near;
//   * the global light scale and the camera's light add as screen passes.
//
// The object types other than world models are drawn by gl_objects.cpp.

#include <math.h>

#include "glscene.h"
#include "globjects.h"

GLRMulti g_Multi;
static uint32_t g_nMultiGen;

void glr_InitMultitexture(void)
{
    if (g_nMultiGen == g.nContextGen)
        return;
    g_nMultiGen = g.nContextGen;
    g_Multi.glActiveTexture = (PFN_glActiveTexture)wglGetProcAddress("glActiveTextureARB");
    g_Multi.glMultiTexCoord2f = (PFN_glMultiTexCoord2f)wglGetProcAddress("glMultiTexCoord2fARB");
    GLint nUnits = 1;
    glGetIntegerv(0x84E2 /* GL_MAX_TEXTURE_UNITS_ARB */, &nUnits);
    g_Multi.m_nUnits = nUnits;
    g_Multi.m_bOK = g_Multi.glActiveTexture && g_Multi.glMultiTexCoord2f && nUnits >= 2;
    const char* pExt = (const char*)glGetString(GL_EXTENSIONS);
    g_Multi.m_bCombine = pExt && (strstr(pExt, "GL_ARB_texture_env_combine") || strstr(pExt, "GL_EXT_texture_env_combine"));
}

// r_CreateContext: the engine binds a loaded world to the renderer and keeps
// what this returns at MainWorld+0x1c8, handing it back in every SceneDesc
// (NULL fails the whole renderer init). The lightmaps are built here.
void* glr_CreateContext(RenderContextInit* pInit)
{
    GLRContext* pContext = (GLRContext*)calloc(1, sizeof(GLRContext));
    if (!pContext)
        return NULL;
    pContext->m_FrameCode = 0xffff;
    pContext->m_pWorld = pInit ? (MainWorld*)pInit->m_pWorld : NULL;
    if (pContext->m_pWorld)
        glr_BuildLightmaps(pContext);
    return pContext;
}

void glr_DeleteContext(void* pContext)
{
    if (!pContext)
        return;
    glr_FreeLightmaps((GLRContext*)pContext);
    free(pContext);
}

// ---- the frame ----------------------------------------------------------------------

GLRFrame g_Frame;

static void glr_QuatToMatrix(const LTRotation& q, float m[3][3])
{
    float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    float s = n > 0.0f ? 2.0f / n : 0.0f;
    float xx = q.x * q.x * s, yy = q.y * q.y * s, zz = q.z * q.z * s;
    float xy = q.x * q.y * s, xz = q.x * q.z * s, yz = q.y * q.z * s;
    float wx = q.w * q.x * s, wy = q.w * q.y * s, wz = q.w * q.z * s;
    m[0][0] = 1 - (yy + zz); m[0][1] = xy - wz;       m[0][2] = xz + wy;
    m[1][0] = xy + wz;       m[1][1] = 1 - (xx + zz); m[1][2] = yz - wx;
    m[2][0] = xz - wy;       m[2][1] = yz + wx;       m[2][2] = 1 - (xx + yy);
}

void glr_RotationMatrix(const LTRotation& q, float m[3][3])
{
    glr_QuatToMatrix(q, m);
}

static void glr_SetupCamera(SceneDesc* pScene, GLRCamera* pCam)
{
    const LTRect& r = pScene->m_Rect;
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    pCam->m_nViewX = r.left;
    pCam->m_nViewY = (int)g.nHeight - r.bottom;
    pCam->m_nViewW = w;
    pCam->m_nViewH = h;

    pCam->m_Pos = pScene->m_Pos;
    glr_QuatToMatrix(pScene->m_Rotation, pCam->m_R);

    // Hor+ (MODERN.md M-P0-01, as the modern d3d.ren does): the game's
    // horizontal field of view is for a 4:3 view; a wider rect sees more.
    pCam->m_fTanX = tanf(pScene->m_xFov * 0.5f) * ((float)w / (float)h) / (4.0f / 3.0f);
    pCam->m_fTanY = tanf(pScene->m_yFov * 0.5f);

    pCam->m_fNear = 7.0f;
    float fFar = pScene->m_FarZ;
    pCam->m_fFar = fFar < 3.0f ? 3.0f : fFar > 500000.0f ? 500000.0f : fFar;
    pCam->m_fScale = 1.0f;
}

// The six world-space frustum planes, normals pointing in: a point p is
// inside a plane when N.p - D >= 0.
static void glr_SetupFrustum(GLRCamera* pCam)
{
    float tx = pCam->m_fTanX, ty = pCam->m_fTanY;
    float cam[6][4] = {
        { 0, 0, 1, pCam->m_fNear },          // near: z >= near
        { 0, 0, -1, -pCam->m_fFar },         // far: -z >= -far
        { 1, 0, tx, 0 }, { -1, 0, tx, 0 },   // left, right: x/z >= -tan, <= tan
        { 0, 1, ty, 0 }, { 0, -1, ty, 0 },   // bottom, top
    };
    const float (*R)[3] = pCam->m_R;
    for (int i = 0; i < 6; i++)
    {
        float l = sqrtf(cam[i][0] * cam[i][0] + cam[i][1] * cam[i][1] + cam[i][2] * cam[i][2]);
        float nx = cam[i][0] / l, ny = cam[i][1] / l, nz = cam[i][2] / l, d = cam[i][3] / l;
        LTPlane& p = pCam->m_Planes[i];
        p.m_Normal.x = R[0][0] * nx + R[0][1] * ny + R[0][2] * nz;
        p.m_Normal.y = R[1][0] * nx + R[1][1] * ny + R[1][2] * nz;
        p.m_Normal.z = R[2][0] * nx + R[2][1] * ny + R[2][2] * nz;
        p.m_Dist = p.m_Normal.x * pCam->m_Pos.x + p.m_Normal.y * pCam->m_Pos.y +
                   p.m_Normal.z * pCam->m_Pos.z + d;
    }
}

bool glr_SphereVisible(const LTVector& c, float r)
{
    for (int i = 0; i < 6; i++)
    {
        const LTPlane& p = g_Frame.m_Cam.m_Planes[i];
        if (p.m_Normal.x * c.x + p.m_Normal.y * c.y + p.m_Normal.z * c.z - p.m_Dist < -r)
            return false;
    }
    return true;
}

// Projection and view for GL (column-major), and the viewport. The view
// carries the camera's scale (the sky's SkyScale) as the Mac's
// m_MatWorld = Scale * R^T * T(-pos) does.
void glr_LoadCamera(const GLRCamera* pCam)
{
    glViewport(pCam->m_nViewX, pCam->m_nViewY, pCam->m_nViewW, pCam->m_nViewH);

    float n = pCam->m_fNear, f = pCam->m_fFar;
    float P[16] = {
        1.0f / pCam->m_fTanX, 0, 0, 0,
        0, 1.0f / pCam->m_fTanY, 0, 0,
        0, 0, (f + n) / (f - n), 1,
        0, 0, -2.0f * f * n / (f - n), 0,
    };
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(P);

    const float (*R)[3] = pCam->m_R;
    const LTVector& p = pCam->m_Pos;
    float s = pCam->m_fScale;
    float V[16];
    for (int row = 0; row < 3; row++)
    {
        V[0 * 4 + row] = R[0][row] * s;
        V[1 * 4 + row] = R[1][row] * s;
        V[2 * 4 + row] = R[2][row] * s;
        V[3 * 4 + row] = -(R[0][row] * p.x + R[1][row] * p.y + R[2][row] * p.z) * s;
    }
    V[3] = V[7] = V[11] = 0.0f;
    V[15] = 1.0f;
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(V);
}

void glr_MultMatrix(const LTMatrix& M)
{
    float m[16];
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            m[col * 4 + row] = M.m[row][col];
    glMultMatrixf(m);
}

// ---- fog ---------------------------------------------------------------------------

static void glr_SetFog(bool bSky)
{
    bool bOn = bSky ? cvi(CV_FogEnable) && cvi(CV_SkyFogEnable) : cvi(CV_FogEnable) != 0;
    if (!bOn)
    {
        glDisable(GL_FOG);
        g_Frame.m_bFog = false;
        return;
    }
    float col[4] = { cvf(CV_FogR) / 255.0f, cvf(CV_FogG) / 255.0f, cvf(CV_FogB) / 255.0f, 1.0f };
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, col);
    glFogf(GL_FOG_START, cvf(bSky ? CV_SkyFogNearZ : CV_FogNearZ));
    glFogf(GL_FOG_END, cvf(bSky ? CV_SkyFogFarZ : CV_FogFarZ));
    glHint(GL_FOG_HINT, GL_NICEST);
    glEnable(GL_FOG);
    g_Frame.m_bFog = true;
}

// An object that asks for no fog (FLAG_FOGDISABLE) turns it off while it
// draws.
void glr_ObjectFog(LTObject* pObj, bool bBegin)
{
    if (!g_Frame.m_bFog || !(pObj->m_Flags & FLAG_FOGDISABLE))
        return;
    if (bBegin)
        glDisable(GL_FOG);
    else
        glEnable(GL_FOG);
}

// ---- polygons ----------------------------------------------------------------------

struct GLRDrawPoly {
    GLRTexture* m_pTex;
    WorldPoly*  m_pPoly;
};

static GLRDrawPoly* g_pDrawList;
static uint32_t     g_nDrawList, g_nDrawAlloc;

static void glr_QueuePoly(GLRTexture* pTex, WorldPoly* pPoly)
{
    if (g_nDrawList == g_nDrawAlloc)
    {
        uint32_t n = g_nDrawAlloc ? g_nDrawAlloc * 2 : 4096;
        GLRDrawPoly* p = (GLRDrawPoly*)realloc(g_pDrawList, n * sizeof(GLRDrawPoly));
        if (!p)
            return;
        g_pDrawList = p;
        g_nDrawAlloc = n;
    }
    g_pDrawList[g_nDrawList].m_pTex = pTex;
    g_pDrawList[g_nDrawList].m_pPoly = pPoly;
    g_nDrawList++;
}

static int glr_CompareDrawPoly(const void* a, const void* b)
{
    const GLRDrawPoly* x = (const GLRDrawPoly*)a;
    const GLRDrawPoly* y = (const GLRDrawPoly*)b;
    if (x->m_pTex != y->m_pTex)
        return (uintptr_t)x->m_pTex < (uintptr_t)y->m_pTex ? -1 : 1;
    GLRPolyLM* lx = (GLRPolyLM*)x->m_pPoly->m_pLMPage;
    GLRPolyLM* ly = (GLRPolyLM*)y->m_pPoly->m_pLMPage;
    void* ax = lx ? lx->m_pAtlas : NULL;
    void* ay = ly ? ly->m_pAtlas : NULL;
    return (uintptr_t)ax < (uintptr_t)ay ? -1 : (uintptr_t)ax > (uintptr_t)ay ? 1 : 0;
}

GLRTexture* glr_SurfaceTexture(Surface* pSurf)
{
    if (!pSurf->m_pTexture)
        return NULL;
    glr_TextureId(pSurf->m_pTexture);
    return (GLRTexture*)pSurf->m_pTexture->m_pRenderData;
}

// Queue a poly unless it faces away from vCam (in the poly's own space).
static void glr_QueueFacingPoly(WorldPoly* pPoly, const LTVector& vCam)
{
    Surface* pSurf = pPoly->m_pSurface;
    const LTPlane* pPlane = pSurf->m_pPlane;
    if (pPlane && pPlane->m_Normal.x * vCam.x + pPlane->m_Normal.y * vCam.y +
                  pPlane->m_Normal.z * vCam.z - pPlane->m_Dist <= 0.01f)
        return;
    glr_QueuePoly(glr_SurfaceTexture(pSurf), pPoly);
}

// ---- texture units ---------------------------------------------------------------------
//
// A lightmapped poly is the retail PC's two passes in one (docs/world.md
// E.4): base texture, plus its detail texture added signed (DetailTextures,
// DetailTextureAdd), times the lightmap, doubled when Saturate is set
// (blend DST_COLOR, SRC_COLOR). Unit 0 is the base, then the detail, then
// the lightmap; without three units or texture combiners the detail goes.

static int g_nDetailUnit = -1, g_nLMUnit = -1;
static GLRLMAtlas* g_pBoundAtlas;
static GLRTexture* g_pBoundDetail;
static GLRTexture* g_pBoundTex;

static void glr_ActiveUnit(int n)
{
    g_Multi.glActiveTexture(GL_TEXTURE0_ARB + n);
}

// combine RGB = op(TEXTURE, PREVIOUS) * scale, alpha = PREVIOUS
static void glr_CombineUnit(GLenum op, float fScale)
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, op);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, fScale);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
}

static void glr_BeginUnits(bool bLightmaps)
{
    g_nDetailUnit = g_nLMUnit = -1;
    if (!bLightmaps)
        return;
    g_nLMUnit = 1;
    if (g_Multi.m_bCombine && g_Multi.m_nUnits >= 3 && cvi(CV_DetailTextures))
    {
        g_nDetailUnit = 1;
        g_nLMUnit = 2;
        glr_ActiveUnit(g_nDetailUnit);
        glr_CombineUnit(cvi(CV_DetailTextureAdd) ? GL_ADD_SIGNED_ARB : GL_MODULATE, 1.0f);
        glDisable(GL_TEXTURE_2D);
    }
    glr_ActiveUnit(g_nLMUnit);
    if (g_Multi.m_bCombine)
        glr_CombineUnit(GL_MODULATE, cvi(CV_Saturate) ? 2.0f : 1.0f);
    else
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glr_ActiveUnit(0);
}

static void glr_EndUnits(void)
{
    for (int n = 1; n <= 2; n++)
    {
        if (n != g_nDetailUnit && n != g_nLMUnit)
            continue;
        glr_ActiveUnit(n);
        glDisable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    if (g_nLMUnit >= 0)
        glr_ActiveUnit(0);
    g_nDetailUnit = g_nLMUnit = -1;
}

static void glr_SetUnitTexture(int nUnit, GLuint id)
{
    glr_ActiveUnit(nUnit);
    if (id)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, id);
    }
    else
        glDisable(GL_TEXTURE_2D);
    glr_ActiveUnit(0);
}

static void glr_SetLightmapUnit(GLRLMAtlas* pAtlas)
{
    if (pAtlas == g_pBoundAtlas || g_nLMUnit < 0)
        return;
    glr_SetUnitTexture(g_nLMUnit, pAtlas ? pAtlas->m_Id : 0);
    g_pBoundAtlas = pAtlas;
}

static void glr_SetDetailUnit(GLRTexture* pDetail)
{
    if (pDetail == g_pBoundDetail || g_nDetailUnit < 0)
        return;
    glr_SetUnitTexture(g_nDetailUnit, pDetail ? pDetail->m_Id : 0);
    g_pBoundDetail = pDetail;
}

void glr_SetBaseTexture(GLRTexture* pTex)
{
    if (pTex == g_pBoundTex)
        return;
    if (pTex)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, pTex->m_Id);
    }
    else
        glDisable(GL_TEXTURE_2D);
    g_pBoundTex = pTex;
}

void glr_ForgetBindings(void)
{
    g_pBoundTex = (GLRTexture*)-1;
    g_pBoundAtlas = (GLRLMAtlas*)-1;
    g_pBoundDetail = (GLRTexture*)-1;
}

// The detail texture a base texture names (DTX "DetailTex", link type 0).
static GLRTexture* glr_DetailOf(Surface* pSurf)
{
    SharedTexture* pBase = pSurf->m_pTexture;
    if (!pBase || !pBase->m_pLinkedTexture || pBase->m_nLinkType != 0)
        return NULL;
    glr_TextureId(pBase->m_pLinkedTexture);
    return (GLRTexture*)pBase->m_pLinkedTexture->m_pRenderData;
}

static inline uint8_t glr_Saturate(uint8_t c, float fTint, bool bDouble)
{
    float v = c * fTint * (bDouble ? 2.0f : 1.0f);
    return (uint8_t)(v > 255.0f ? 255.0f : v);
}

// One poly as a fan. Lightmapped: white vertices, the units above. Vertex-
// lit: base times the vertex colours (doubled under Saturate, the colour
// map r_FillColorMap builds). Either way tinted by the drawing object's
// colour (r_SetupWMColorScale) with its alpha.
// ---- dynamic lights on the world (r_DrawLight, r_ProcessBspLight) -------------------

static GLRWorldLight g_WLights[GLR_MAXWORLDLIGHTS];
static uint32_t      g_nWLights;

// The frame's lights that light the world, in the space of the BSP about to
// draw (pBack: world -> model; NULL for the main world).
static void glr_SetupWorldLights(const LTMatrix* pBack)
{
    g_nWLights = 0;
    if (!cvi(CV_DynamicLight) || !g_Frame.m_pContext)
        return;
    bool bFast = cvi(CV_FastLight) != 0;
    GLRObjList* pList = glr_ObjList(VS_LIGHTS);
    for (uint32_t i = 0; i < pList->m_nObjects && g_nWLights < GLR_MAXWORLDLIGHTS; i++)
    {
        LTObject* L = pList->m_pObjects[i];
        float r;
        memcpy(&r, (const uint8_t*)L + 0x1a8, 4);
        if ((L->m_Flags & FLAG_ONLYLIGHTOBJECTS) || r <= 0.0f)
            continue;
        GLRWorldLight& W = g_WLights[g_nWLights++];
        const LTVector& v = L->m_Pos;
        if (pBack)
        {
            const LTMatrix& M = *pBack;
            W.m_Pos.x = M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z + M.m[0][3];
            W.m_Pos.y = M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z + M.m[1][3];
            W.m_Pos.z = M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z + M.m[2][3];
        }
        else
            W.m_Pos = v;
        W.m_fRadius = r;
        W.m_Color[0] = L->m_ColorR;
        W.m_Color[1] = L->m_ColorG;
        W.m_Color[2] = L->m_ColorB;
        for (int j = 0; j < 3; j++)
            W.m_Add[j] = 2.0f * W.m_Color[j] - 255.0f;
        W.m_bSolid = bFast || (L->m_Flags & FLAG_SOLIDLIGHT);
        W.m_bFrontOnly = (L->m_Flags & FLAG_DONTLIGHTBACKFACING) != 0;
    }
}

// r_DoesLightTouchPoly: the light's sphere reaches the poly's plane and its
// bounding sphere.
static bool glr_LightTouchesPoly(const GLRWorldLight& L, const WorldPoly* pPoly)
{
    const LTPlane* pPlane = pPoly->m_pSurface->m_pPlane;
    if (pPlane)
    {
        float d = pPlane->m_Normal.x * L.m_Pos.x + pPlane->m_Normal.y * L.m_Pos.y +
                  pPlane->m_Normal.z * L.m_Pos.z - pPlane->m_Dist;
        if (d >= L.m_fRadius || d <= -L.m_fRadius || (L.m_bFrontOnly && d < 0.0f))
            return false;
    }
    float dx = pPoly->m_Center.x - L.m_Pos.x, dy = pPoly->m_Center.y - L.m_Pos.y,
          dz = pPoly->m_Center.z - L.m_Pos.z, rr = L.m_fRadius + pPoly->m_Radius;
    return dx * dx + dy * dy + dz * dz < rr * rr;
}

// The lights that reach the poly, into pOut; how many.
static uint32_t glr_PolyLights(const WorldPoly* pPoly, GLRWorldLight* pOut)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < g_nWLights; i++)
        if (glr_LightTouchesPoly(g_WLights[i], pPoly))
            pOut[n++] = g_WLights[i];
    return n;
}

// A gouraud poly's vertex: its colour plus the lights, as r_LightData's
// texel (r_DrawGouraudDynamicLight).
static void glr_LightVertex(const LTVector& pos, const GLRWorldLight* pLights, uint32_t nLights,
                            uint8_t* pRGB)
{
    float fSat = cvf(CV_LightSaturate) / 63.0f;
    int c[3] = { pRGB[0], pRGB[1], pRGB[2] };
    for (uint32_t l = 0; l < nLights; l++)
    {
        const GLRWorldLight& L = pLights[l];
        float dx = pos.x - L.m_Pos.x, dy = pos.y - L.m_Pos.y, dz = pos.z - L.m_Pos.z;
        float d2 = dx * dx + dy * dy + dz * dz, r2 = L.m_fRadius * L.m_fRadius;
        if (d2 >= r2)
            continue;
        if (L.m_bSolid)
        {
            c[0] = L.m_Color[0];
            c[1] = L.m_Color[1];
            c[2] = L.m_Color[2];
            continue;
        }
        float ramp = (int)((1.0f - d2 / r2) * 63.0f) * fSat;
        if (ramp > 1.0f)
            ramp = 1.0f;
        for (int j = 0; j < 3; j++)
            c[j] += (int)(L.m_Add[j] * ramp);
    }
    for (int j = 0; j < 3; j++)
        pRGB[j] = (uint8_t)(c[j] < 0 ? 0 : c[j] > 255 ? 255 : c[j]);
}

static void glr_DrawPoly(WorldPoly* pPoly, GLRTexture* pTex, bool bLightmaps,
                         const float* pTint, uint8_t nAlpha)
{
    float fU = pTex ? pTex->m_fUMul : 0.0f, fV = pTex ? pTex->m_fVMul : 0.0f;
    bool bFullbright = pTex && (pTex->m_DtxFlags & DTX_FULLBRITE);
    GLRPolyLM* pLM = bLightmaps && !bFullbright ? (GLRPolyLM*)pPoly->m_pLMPage : NULL;
    if (bLightmaps)
        glr_SetLightmapUnit(pLM ? pLM->m_pAtlas : NULL);
    GLRTexture* pDetail = pLM && g_nDetailUnit >= 0 ? glr_DetailOf(pPoly->m_pSurface) : NULL;
    if (g_nDetailUnit >= 0)
        glr_SetDetailUnit(pDetail);

    LTVector P, Q;
    float fInvSize = 0.0f, fOffU = 0.0f, fOffV = 0.0f, fGrid = 1.0f;
    if (pLM)
    {
        glr_LMVectors(pPoly->m_pSurface, &P, &Q);
        fInvSize = 1.0f / pLM->m_pAtlas->m_nSize;
        fOffU = pLM->m_X + 0.5f;
        fOffV = pLM->m_Y + 0.5f;
        fGrid = g_Frame.m_fLMGrid;
    }
    // r_DrawOverlayDetail: the detail texture's coordinates are the base
    // texels rotated and scaled.
    float fDA = 0.0f, fDB = 0.0f, fDC = 0.0f, fDD = 0.0f;
    if (pDetail)
    {
        float fScale = cvf(CV_DetailTextureScale) * pTex->m_fDetailScale;
        fDA = pTex->m_fDetailCos * pDetail->m_fUMul * fScale;
        fDB = -pTex->m_fDetailSin * pDetail->m_fVMul * fScale;
        fDC = pTex->m_fDetailSin * pDetail->m_fUMul * fScale;
        fDD = pTex->m_fDetailCos * pDetail->m_fVMul * fScale;
    }
    bool bDouble = cvi(CV_Saturate) != 0;
    uint8_t r = 255, gg = 255, b = 255;
    if (bFullbright || pLM)
    {
        r = (uint8_t)(255 * pTint[0]);
        gg = (uint8_t)(255 * pTint[1]);
        b = (uint8_t)(255 * pTint[2]);
    }

    GLRWorldLight lights[GLR_MAXWORLDLIGHTS];
    uint32_t nLights = !bFullbright && !pLM && g_nWLights ? glr_PolyLights(pPoly, lights) : 0;

    const WorldPolyVertex* pVerts = pPoly->m_Verts;
    uint32_t nVerts = pPoly->m_nVerts;
    if (cvi(CV_FixTJunc) && pPoly->m_pFixVerts)
    {
        pVerts = pPoly->m_pFixVerts;
        nVerts = pPoly->m_nFixVerts;
    }
    glBegin(GL_TRIANGLE_FAN);
    for (uint32_t v = 0; v < nVerts; v++)
    {
        const WorldPolyVertex& vert = pVerts[v];
        const LTVector& pos = *vert.m_pPos;
        if (bFullbright || pLM)
            glColor4ub(r, gg, b, nAlpha);
        else
        {
            uint8_t rgb[3] = { vert.m_R, vert.m_G, vert.m_B };
            if (nLights)
                glr_LightVertex(pos, lights, nLights, rgb);
            glColor4ub(glr_Saturate(rgb[0], pTint[0], bDouble), glr_Saturate(rgb[1], pTint[1], bDouble),
                       glr_Saturate(rgb[2], pTint[2], bDouble), nAlpha);
        }
        glTexCoord2f(vert.m_U * fU, vert.m_V * fV);
        if (pDetail)
            g_Multi.glMultiTexCoord2f(GL_TEXTURE0_ARB + g_nDetailUnit,
                                      vert.m_U * fDA + vert.m_V * fDB, vert.m_U * fDC + vert.m_V * fDD);
        if (pLM)
        {
            float dx = pos.x - pPoly->m_LMOrigin.x, dy = pos.y - pPoly->m_LMOrigin.y,
                  dz = pos.z - pPoly->m_LMOrigin.z;
            float lu = (dx * P.x + dy * P.y + dz * P.z) / fGrid + fOffU;
            float lv = (dx * Q.x + dy * Q.y + dz * Q.z) / fGrid + fOffV;
            g_Multi.glMultiTexCoord2f(GL_TEXTURE0_ARB + g_nLMUnit, lu * fInvSize, lv * fInvSize);
        }
        glVertex3f(pos.x, pos.y, pos.z);
    }
    glEnd();
}

static const float g_White[3] = { 1, 1, 1 };

// Draw the queue, sorted by texture and lightmap atlas. pBack brings the
// frame's dynamic lights into the queued polys' space (world models).
static void glr_FlushPolys(const float* pTint, uint8_t nAlpha, const LTMatrix* pBack)
{
    qsort(g_pDrawList, g_nDrawList, sizeof(GLRDrawPoly), glr_CompareDrawPoly);
    bool bLightmaps = g_Frame.m_bLightmaps;
    glr_SetupWorldLights(pBack);
    if (bLightmaps)
    {
        GLRWorldLight lights[GLR_MAXWORLDLIGHTS];
        for (uint32_t i = 0; g_nWLights && i < g_nDrawList; i++)
        {
            WorldPoly* pPoly = g_pDrawList[i].m_pPoly;
            GLRTexture* pTex = g_pDrawList[i].m_pTex;
            if (!pPoly->m_pLMPage || (pTex && (pTex->m_DtxFlags & DTX_FULLBRITE)))
                continue;
            uint32_t n = glr_PolyLights(pPoly, lights);
            if (n)
                glr_LightPolyLM(g_Frame.m_pContext, pPoly, lights, n);
        }
        glr_UploadLightmaps(g_Frame.m_pContext);
    }
    glr_ForgetBindings();
    glr_BeginUnits(bLightmaps);
    for (uint32_t i = 0; i < g_nDrawList; i++)
    {
        glr_SetBaseTexture(g_pDrawList[i].m_pTex);
        glr_DrawPoly(g_pDrawList[i].m_pPoly, g_pDrawList[i].m_pTex, bLightmaps, pTint, nAlpha);
    }
    glr_EndUnits();
    g_nDrawList = 0;
    g_nWLights = 0;
}

// ---- visibility -------------------------------------------------------------------

// r_ProcessObject's test (docs/objects.md 2.4); no portal views here.
bool glr_ShouldDraw(LTObject* pObj)
{
    int nType = pObj->m_ObjectType;
    if (nType <= OT_NORMAL || nType >= NUM_OBJECTTYPES || nType == OT_CAMERA || nType == OT_POLYGRID)
        return false;
    if (pObj->m_Flags2 & FLAG2_SKYOBJECT)
        return false;
    return (pObj->m_Flags & FLAG_VISIBLE) != 0;
}

static void glr_ProcessAttachments(LTObject* pObj, uint32_t nDepth)
{
    if (nDepth > 32)
        return;
    for (Attachment* a = pObj->m_pAttachments; a; a = a->m_pNext)
    {
        // The engine places the child at its attachment point.
        LTObject* pChild = g.pStruct->ProcessAttachment(pObj, a);
        if (!pChild || pChild->m_FrameCode == g_Frame.m_ObjectFrameCode)
            continue;
        glr_ProcessAttachments(pChild, nDepth + 1);
        if (glr_ShouldDraw(pChild))
        {
            glr_QueueObject(pChild);
            pChild->m_FrameCode = g_Frame.m_ObjectFrameCode;
        }
    }
}

static void glr_ReallyProcessObject(LTObject* pObj)
{
    glr_QueueObject(pObj);
    if (pObj->m_pAttachments)
        glr_ProcessAttachments(pObj, 0);
}

static void glr_QueueWorldPoly(WorldPoly* pPoly)
{
    if (pPoly->m_FrameCode == g_Frame.m_WorldFrameCode)
        return;
    pPoly->m_FrameCode = g_Frame.m_WorldFrameCode;
    Surface* pSurf = pPoly->m_pSurface;
    if (!pSurf)
        return;
    uint32_t nFlags = pSurf->m_Flags & SURF_FLAGMASK;
    if (nFlags & SURF_INVISIBLE)
        return;
    if (nFlags & SURF_SKY)
    {
        g_Frame.m_nSkyPolys++;      // the sky shows here (never drawn itself)
        return;
    }
    if (cvi(CV_DrawWorld))
        glr_QueueFacingPoly(pPoly, g_Frame.m_Cam.m_Pos);
}

static void cb_Object(LTObject* pObj, void* pUser)
{
    (void)pUser;
    if (glr_ShouldDraw(pObj))
        glr_ReallyProcessObject(pObj);
}

// r_AddRenderObjects: a world tree node's objects, those that may be drawn.
static void cb_AddObjects(LTLink* pHead, LTObject*** ppArray, int* pnCount)
{
    for (LTLink* pCur = pHead->m_pPrev; pCur != pHead; pCur = pCur->m_pPrev)
    {
        LTObject* pObj = (LTObject*)pCur->m_pData;
        if (pObj->m_188)
            break;
        if (glr_ShouldDraw(pObj))
            (*ppArray)[(*pnCount)++] = pObj;
    }
}

// r_ProcessLeaf: the leaf's poly grids and its polygons.
static void cb_Leaf(Leaf* pLeaf, void* pUser)
{
    (void)pUser;
    pLeaf->m_FrameCode = g_Frame.m_WorldFrameCode;
    for (LTLink* pCur = pLeaf->m_Objects.m_pNext; pCur && pCur != &pLeaf->m_Objects; pCur = pCur->m_pNext)
    {
        LTObject* pObj = *(LTObject**)((uint8_t*)pCur->m_pData + 0x18);
        if (pObj && pObj->m_ObjectType == OT_POLYGRID && (pObj->m_Flags & FLAG_VISIBLE) &&
            pObj->m_FrameCode != g_Frame.m_ObjectFrameCode)
        {
            pObj->m_FrameCode = g_Frame.m_ObjectFrameCode;
            glr_ReallyProcessObject(pObj);
        }
    }
    for (uint32_t i = 0; i < pLeaf->m_nPolies; i++)
        if (pLeaf->m_Polies[i])
            glr_QueueWorldPoly(pLeaf->m_Polies[i]);
}

static LTBOOL cb_PortalTest(void* pUserPortal)
{
    (void)pUserPortal;
    return 1;
}

// r_NodeFilter: a world tree node wholly outside the frustum is skipped.
static LTBOOL cb_NodeFilter(WorldTreeNode* pNode)
{
    return glr_SphereVisible(wtn_Center(pNode), wtn_Radius(pNode));
}

// r_IncrementFrameCode: the world's frame code; at its wrap every leaf and
// poly stamp is cleared.
static void glr_IncrementFrameCode(GLRContext* pContext)
{
    MainWorld* pWorld = pContext->m_pWorld;
    if (pContext->m_FrameCode == 0xffff)
    {
        pContext->m_FrameCode = 1;
        for (uint32_t i = 0; i < pWorld->m_Worlds.m_nElements; i++)
        {
            WorldData* pData = mw_World(pWorld, i);
            WorldBsp* pBsp = pData ? pData->m_pWorldBsp : NULL;
            if (!pBsp)
                continue;
            for (uint32_t l = 0; l < pBsp->m_nLeafs; l++)
                pBsp->m_Leafs[l].m_FrameCode = 0;
            for (uint32_t p = 0; p < pBsp->m_nPolies; p++)
                if (pBsp->m_Polies[p])
                    pBsp->m_Polies[p]->m_FrameCode = 0;
        }
    }
    else
        pContext->m_FrameCode++;
}

static WorldBsp* glr_FindBsp(MainWorld* pWorld, uint32_t nFlag)
{
    for (uint32_t i = 0; i < pWorld->m_Worlds.m_nElements; i++)
    {
        WorldData* pData = mw_World(pWorld, i);
        if (pData && pData->m_pWorldBsp && (pData->m_pWorldBsp->m_WorldInfoFlags & nFlag))
            return pData->m_pWorldBsp;
    }
    return NULL;
}

// No PVS (or DrawAll): every poly of the main world, and every object filed
// in the vis BSP's nodes (r_DrawBSP).
static void glr_VisitAll(MainWorld* pWorld)
{
    WorldBsp* pMain = glr_FindBsp(pWorld, WORLDINFO_MAINWORLD);
    if (pMain)
        for (uint32_t i = 0; i < pMain->m_nPolies; i++)
            if (pMain->m_Polies[i])
                glr_QueueWorldPoly(pMain->m_Polies[i]);
    WorldBsp* pVis = glr_FindBsp(pWorld, WORLDINFO_VIS);
    if (!pVis)
        pVis = pMain;
    Node* pNodes = pVis ? (Node*)pVis->m_Nodes : NULL;
    for (uint32_t i = 0; pNodes && i < pVis->m_nNodes; i++)
    {
        LTLink* pHead = (LTLink*)&pNodes[i].m_Objects;
        for (LTLink* pCur = pHead->m_pPrev; pCur && pCur != pHead; pCur = pCur->m_pPrev)
        {
            LTObject* pObj = (LTObject*)pCur->m_pData;
            if (!pObj || pObj->m_188)
                break;
            if (pObj->m_FrameCode != g_Frame.m_ObjectFrameCode && glr_ShouldDraw(pObj))
            {
                pObj->m_FrameCode = g_Frame.m_ObjectFrameCode;
                glr_ReallyProcessObject(pObj);
            }
        }
    }
}

// ---- world models -----------------------------------------------------------------

static void glr_ObjectTint(LTObject* pObj, float* pTint)
{
    pTint[0] = pObj->m_ColorR / 255.0f;
    pTint[1] = pObj->m_ColorG / 255.0f;
    pTint[2] = pObj->m_ColorB / 255.0f;
}

static uint32_t glr_FirstSurfaceFlags(WorldModelInstance* pWM)
{
    WorldBsp* pBsp = pWM->m_pWorldBsp;
    if (!pBsp || !pBsp->m_nPolies || !pBsp->m_Polies[0] || !pBsp->m_Polies[0]->m_pSurface)
        return 0;
    return pBsp->m_Polies[0]->m_pSurface->m_Flags & SURF_FLAGMASK;
}

// r_ProcessWorldModel: which set it goes in.
void glr_ProcessWorldModel(LTObject* pObj)
{
    WorldModelInstance* pWM = (WorldModelInstance*)pObj;
    if (!pWM->m_pWorldBsp || !pWM->m_pWorldBsp->m_nPolies)
        return;
    if (glr_FirstSurfaceFlags(pWM) & SURF_TRANSLUCENT)
        glr_AddTranslucent(pObj, glr_DrawTranslucentWorldModel);
    else if (pObj->m_Flags2 & FLAG2_CHROMAKEY)
        glr_ObjList(VS_WORLDMODELS_CHROMAKEY)->Add(pObj);
    else
        glr_ObjList(VS_WORLDMODELS)->Add(pObj);
}

static LTVector glr_ToLocal(const LTMatrix& M, const LTVector& v)
{
    LTVector r;
    r.x = M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z + M.m[0][3];
    r.y = M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z + M.m[1][3];
    r.z = M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z + M.m[2][3];
    return r;
}

// r_DrawSolidWorldModel: every poly of the model's BSP that faces the
// camera, under the model's matrix, lit like the world.
void glr_DrawSolidWorldModel(LTObject* pObj)
{
    WorldModelInstance* pWM = (WorldModelInstance*)pObj;
    WorldBsp* pBsp = pWM->m_pWorldBsp;
    if (!pBsp || !cvi(CV_DrawWorldModels))
        return;
    float fRadius = *(float*)((uint8_t*)pBsp + 0x14c);
    if (fRadius > 0.0f && !glr_SphereVisible(pObj->m_Pos, fRadius))
        return;
    LTVector vCam = glr_ToLocal(pWM->m_BackTransform, g_Frame.m_Cam.m_Pos);
    for (uint32_t i = 0; i < pBsp->m_nPolies; i++)
    {
        WorldPoly* pPoly = pBsp->m_Polies[i];
        if (!pPoly || !pPoly->m_pSurface || pPoly->m_FrameCode == g_Frame.m_WorldFrameCode)
            continue;
        pPoly->m_FrameCode = g_Frame.m_WorldFrameCode;
        if (pPoly->m_pSurface->m_Flags & (SURF_INVISIBLE | SURF_SKY))
            continue;
        glr_QueueFacingPoly(pPoly, vCam);
    }
    float tint[3];
    glr_ObjectTint(pObj, tint);
    glr_ObjectFog(pObj, true);
    glPushMatrix();
    glr_MultMatrix(pWM->m_Transform);
    glr_FlushPolys(tint, pObj->m_ColorA, &pWM->m_BackTransform);
    glPopMatrix();
    glr_ObjectFog(pObj, false);
}

// Back to front through the model's own BSP (r_RecurseAndDrawWM), with the
// camera in model space (the shipped code uses the world-space camera,
// right only for a model that has not moved).
static void glr_DrawBspBackToFront(Node* pNode, const LTVector& vCam, const float* pTint,
                                   uint8_t nAlpha, int nDepth)
{
    if (!pNode || (pNode->m_Flags & 3) || nDepth > 1024 || !pNode->m_pPoly)
        return;
    WorldPoly* pPoly = pNode->m_pPoly;
    const LTPlane* pPlane = pPoly->m_pSurface->m_pPlane;
    int nSide = (pPlane->m_Normal.x * vCam.x + pPlane->m_Normal.y * vCam.y +
                 pPlane->m_Normal.z * vCam.z - pPlane->m_Dist) >= 0.0f;
    glr_DrawBspBackToFront(pNode->m_Sides[!nSide], vCam, pTint, nAlpha, nDepth + 1);
    if (!(pPoly->m_pSurface->m_Flags & SURF_INVISIBLE))
    {
        GLRTexture* pTex = glr_SurfaceTexture(pPoly->m_pSurface);
        glr_SetBaseTexture(pTex);
        glr_DrawPoly(pPoly, pTex, false, pTint, nAlpha);
    }
    glr_DrawBspBackToFront(pNode->m_Sides[nSide], vCam, pTint, nAlpha, nDepth + 1);
}

void glr_DrawTranslucentWorldModel(LTObject* pObj)
{
    WorldModelInstance* pWM = (WorldModelInstance*)pObj;
    WorldBsp* pBsp = pWM->m_pWorldBsp;
    if (!pBsp || !cvi(CV_DrawWorldModels))
        return;
    bool bAdditive = (glr_FirstSurfaceFlags(pWM) & SURF_ADDITIVE) != 0;
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    if (bAdditive)
        glBlendFunc(GL_ONE, GL_ONE);
    float tint[3];
    glr_ObjectTint(pObj, tint);
    glr_ObjectFog(pObj, true);
    glPushMatrix();
    glr_MultMatrix(pWM->m_Transform);
    LTVector vCam = glr_ToLocal(pWM->m_BackTransform, g_Frame.m_Cam.m_Pos);
    glr_ForgetBindings();
    glr_DrawBspBackToFront((Node*)pBsp->m_RootNode, vCam, tint, pObj->m_ColorA, 0);
    glPopMatrix();
    glr_ObjectFog(pObj, false);
    glPopAttrib();
}

// ---- the sky ----------------------------------------------------------------------

// A sky world model: its BSP as authored (sky brushes do not move), front-
// facing polys only, node poly before its far side (r_DrawSkyWorldModel).
static void glr_DrawSkyBsp(Node* pNode, const LTVector& vCam, const float* pTint, uint8_t nAlpha, int nDepth)
{
    if (!pNode || (pNode->m_Flags & 3) || nDepth > 1024 || !pNode->m_pPoly)
        return;
    WorldPoly* pPoly = pNode->m_pPoly;
    const LTPlane* pPlane = pPoly->m_pSurface->m_pPlane;
    int bFront = (pPlane->m_Normal.x * vCam.x + pPlane->m_Normal.y * vCam.y +
                  pPlane->m_Normal.z * vCam.z - pPlane->m_Dist) > 0.0f;
    if (bFront && !(pPoly->m_pSurface->m_Flags & SURF_INVISIBLE))
    {
        GLRTexture* pTex = glr_SurfaceTexture(pPoly->m_pSurface);
        glr_SetBaseTexture(pTex);
        glr_DrawPoly(pPoly, pTex, false, pTint, nAlpha);
    }
    glr_DrawSkyBsp(pNode->m_Sides[!bFront], vCam, pTint, nAlpha, nDepth + 1);
    glr_DrawSkyBsp(pNode->m_Sides[bFront], vCam, pTint, nAlpha, nDepth + 1);
}

// r_DrawSky: the sky objects in their list order, depth off, seen with the
// main camera's rotation from the camera's place squashed into the sky box
// (r_SetupSkyStuff).
static void glr_DrawSky(SceneDesc* pScene, MainWorld* pWorld)
{
    if (!cvi(CV_DrawSky) || pScene->m_nSkyObjects <= 0 || !g_Frame.m_nSkyPolys)
        return;
    GLRCamera sky = g_Frame.m_Cam;
    LTVector t = { 0.5f, 0.5f, 0.5f };
    LTVector ext = { pWorld->m_ExtentsMax.x - pWorld->m_ExtentsMin.x,
                     pWorld->m_ExtentsMax.y - pWorld->m_ExtentsMin.y,
                     pWorld->m_ExtentsMax.z - pWorld->m_ExtentsMin.z };
    if (ext.x != 0.0f && ext.y != 0.0f && ext.z != 0.0f)
    {
        t.x = (sky.m_Pos.x - pWorld->m_ExtentsMin.x) / ext.x;
        t.y = (sky.m_Pos.y - pWorld->m_ExtentsMin.y) / ext.y;
        t.z = (sky.m_Pos.z - pWorld->m_ExtentsMin.z) / ext.z;
    }
    const SkyDef& def = pScene->m_SkyDef;
    sky.m_Pos.x = def.m_ViewMin.x + (def.m_ViewMax.x - def.m_ViewMin.x) * t.x;
    sky.m_Pos.y = def.m_ViewMin.y + (def.m_ViewMax.y - def.m_ViewMin.y) * t.y;
    sky.m_Pos.z = def.m_ViewMin.z + (def.m_ViewMax.z - def.m_ViewMin.z) * t.z;
    sky.m_fNear = 0.01f;
    sky.m_fScale = cvf(CV_SkyScale) > 0.0f ? cvf(CV_SkyScale) : 1.0f;
    glr_LoadCamera(&sky);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glr_SetFog(true);
    LTVector vSkyCam = sky.m_Pos;
    for (int i = 0; i < pScene->m_nSkyObjects; i++)
    {
        LTObject* pObj = pScene->m_SkyObjects[i];
        if (!pObj || !(pObj->m_Flags & FLAG_VISIBLE))
            continue;
        if (pObj->m_ObjectType == OT_WORLDMODEL)
        {
            WorldModelInstance* pWM = (WorldModelInstance*)pObj;
            if (!pWM->m_pWorldBsp)
                continue;
            // Only a translucent sky model blends (r_SetTranslucentObjectStates);
            // the others are opaque (r_UnsetTranslucentObjectStates), whatever
            // alpha their textures carry.
            uint32_t nFlags = glr_FirstSurfaceFlags(pWM);
            if (nFlags & SURF_TRANSLUCENT)
            {
                glEnable(GL_BLEND);
                if (nFlags & SURF_ADDITIVE)
                    glBlendFunc(GL_ONE, GL_ONE);
                else
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
                glDisable(GL_BLEND);
            float tint[3];
            glr_ObjectTint(pObj, tint);
            glr_ObjectFog(pObj, true);
            glr_ForgetBindings();
            glr_DrawSkyBsp((Node*)pWM->m_pWorldBsp->m_RootNode, vSkyCam, tint, pObj->m_ColorA, 0);
            glr_ObjectFog(pObj, false);
        }
        else
        {
            g_Frame.m_SkyCam = sky;
            glr_DrawSkyObject(pObj);
        }
    }
    glPopAttrib();
    glr_LoadCamera(&g_Frame.m_Cam);
}

// ---- screen passes --------------------------------------------------------------------

static void glr_ScreenQuad(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_QUADS);
    glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// The global light scale darkens the view (dst * scale, as dst*src + src*dst
// with src = scale/2), and the camera's light add brightens it.
static void glr_ScreenPasses(SceneDesc* pScene)
{
    const LTVector& s = pScene->m_GlobalLightScale;
    const LTVector& a = pScene->m_GlobalLightAdd;
    bool bScale = s.x < 0.99f || s.x > 1.01f || s.y < 0.99f || s.y > 1.01f || s.z < 0.99f || s.z > 1.01f;
    bool bAdd = cvi(CV_LightAddPoly) && (a.x >= 0.001f || a.y >= 0.001f || a.z >= 0.001f);
    if (!bScale && !bAdd)
        return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    if (bScale)
    {
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        glColor4f(s.x * 0.5f, s.y * 0.5f, s.z * 0.5f, 1.0f);
        glr_ScreenQuad();
    }
    if (bAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
        glColor4f(a.x, a.y, a.z, 1.0f);
        glr_ScreenQuad();
    }
    glPopAttrib();
}

// ---- RenderScene ---------------------------------------------------------------------

LTBOOL glr_RenderScene(SceneDesc* pScene)
{
    if (!g.hRC || !pScene)
        return 0;
    if (g.bInOptimized2D)
    {
        glr_Print("Error: tried to render 3D while in optimized 2D mode.");
        return 0;
    }
    glr_ReadConsoleVariables();         // the Mac re-reads them every frame
    glr_InitMultitexture();

    GLRContext* pContext = pScene->m_DrawMode == DRAWMODE_NORMAL ? (GLRContext*)pScene->m_pRenderContext : NULL;
    if (pScene->m_DrawMode == DRAWMODE_NORMAL && (!pContext || !pContext->m_pWorld))
        return 0;
    MainWorld* pWorld = pContext ? pContext->m_pWorld : NULL;

    memset(&g_Frame, 0, sizeof(g_Frame));
    g_Frame.m_pScene = pScene;
    g_Frame.m_pContext = pContext;
    glr_SetupCamera(pScene, &g_Frame.m_Cam);
    glr_SetupFrustum(&g_Frame.m_Cam);
    if (pContext)
    {
        glr_IncrementFrameCode(pContext);
        g_Frame.m_WorldFrameCode = pContext->m_FrameCode;
        g_Frame.m_fLMGrid = pWorld->m_LMGridSize > 0.0f ? pWorld->m_LMGridSize : 1.0f;
        g_Frame.m_bLightmaps = g_Multi.m_bOK && pContext->m_pAtlases && cvi(CV_LightMap) &&
                               pContext->m_nContextGen == g.nContextGen;
    }
    // The object frame code stamps LTObject+0x58: the engine's visibility
    // walk and the attachment pass process each object once a frame.
    g_Frame.m_ObjectFrameCode = g.pStruct->IncObjectFrameCode();
    if (g.pStruct->IncCurTextureFrameCode)
        g.pStruct->IncCurTextureFrameCode();
    glr_BeginObjects();

    // What is visible.
    if (pContext)
    {
        if ((pWorld->m_Flags & MAINWORLD_HASVIS) && !cvi(CV_DrawAll))
        {
            VisQueryRequest req;
            memset(&req, 0, sizeof(req));
            req.m_Viewpoint = g_Frame.m_Cam.m_Pos;
            req.m_ViewRadius = 10000.0f;
            req.m_ObjectCB = cb_Object;
            req.m_AddObjects = cb_AddObjects;
            req.m_LeafCB = cb_Leaf;
            req.m_PortalTest = cb_PortalTest;
            req.m_NodeFilter = cb_NodeFilter;
            mw_DoVisQuery(pWorld, &req);
        }
        else
            glr_VisitAll(pWorld);
    }
    else
    {
        for (int i = 0; i < pScene->m_ObjectListSize; i++)
        {
            LTObject* pObj = pScene->m_pObjectList ? pScene->m_pObjectList[i] : NULL;
            if (pObj && pObj->m_ObjectType > OT_NORMAL && pObj->m_ObjectType < NUM_OBJECTTYPES)
                glr_QueueObject(pObj);
        }
    }

    // Draw it.
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glr_LoadCamera(&g_Frame.m_Cam);

    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    // Texels are punched out only in the chromakey sets (d3d.ren's
    // sub_10023eb0/sub_10023f40 around them): solid textures' alpha means
    // nothing, and many 5551 ones (window shutters, the sky) carry 0. The
    // Mac renderer leaves r_Init's alpha test on everywhere.
    glDisable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1666f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glShadeModel(GL_SMOOTH);

    if (pContext)
        glr_DrawSky(pScene, pWorld);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glr_SetFog(false);
    if (pContext)
    {
        if (g_Frame.m_bLightmaps)
            glr_UpdateLightmaps(pContext);
        glr_FlushPolys(g_White, 255, NULL);
    }
    glr_FlushObjects();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    // The screen passes cover the camera's rect.
    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(g_Frame.m_Cam.m_nViewX, g_Frame.m_Cam.m_nViewY, g_Frame.m_Cam.m_nViewW, g_Frame.m_Cam.m_nViewH);
    glr_ScreenPasses(pScene);
    glPopAttrib();
    return 0;
}

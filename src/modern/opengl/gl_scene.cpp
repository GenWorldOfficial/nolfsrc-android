// gl_scene.cpp -- the 3D half: the world context and RenderScene.
//
// The Mac renderer transforms and projects every vertex itself and hands GL
// screen-space vertices; here GL does it, with the projection and view the
// Mac's r_InitFrustrum2 implies (docs/world.md A.3): camera space is
// left-handed, x right, y up, z forward, near plane 7, far plane FarZ.
//
// Stage 2 so far: every visible polygon of the main world, with its texture
// and vertex colours. Visibility, lightmaps, world models, the sky and fog
// follow (README.md).

#include <math.h>

#include "glscene.h"

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
    g_Multi.m_bOK = g_Multi.glActiveTexture && g_Multi.glMultiTexCoord2f && nUnits >= 2;
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

// ---- the camera ------------------------------------------------------------------

struct GLRCamera {
    LTVector m_Pos;
    float    m_R[3][3];                 // camera -> world: columns right, up, forward
    float    m_fTanX, m_fTanY;          // tan of the half field of view
    float    m_fNear, m_fFar;
    int      m_nViewX, m_nViewY, m_nViewW, m_nViewH;   // GL viewport
};

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
}

// Projection and view for GL (column-major), and the viewport.
static void glr_LoadCamera(const GLRCamera* pCam)
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

    // world -> camera = R^T * T(-pos)
    const float (*R)[3] = pCam->m_R;
    const LTVector& p = pCam->m_Pos;
    float V[16];
    for (int row = 0; row < 3; row++)
    {
        V[0 * 4 + row] = R[0][row];
        V[1 * 4 + row] = R[1][row];
        V[2 * 4 + row] = R[2][row];
        V[3 * 4 + row] = -(R[0][row] * p.x + R[1][row] * p.y + R[2][row] * p.z);
    }
    V[3] = V[7] = V[11] = 0.0f;
    V[15] = 1.0f;
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(V);
}

// ---- the world's polygons ------------------------------------------------------------

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
    uintptr_t x = (uintptr_t)((const GLRDrawPoly*)a)->m_pTex;
    uintptr_t y = (uintptr_t)((const GLRDrawPoly*)b)->m_pTex;
    return x < y ? -1 : x > y ? 1 : 0;
}

// Queue one BSP's polygons that face the camera.
static void glr_CollectBsp(WorldBsp* pBsp, const LTVector& vCam)
{
    for (uint32_t i = 0; i < pBsp->m_nPolies; i++)
    {
        WorldPoly* pPoly = pBsp->m_Polies[i];
        if (!pPoly || !pPoly->m_pSurface)
            continue;
        Surface* pSurf = pPoly->m_pSurface;
        uint32_t nFlags = pSurf->m_Flags & SURF_FLAGMASK;
        if (nFlags & (SURF_INVISIBLE | SURF_SKY))
            continue;
        const LTPlane* pPlane = pSurf->m_pPlane;
        if (pPlane && pPlane->m_Normal.x * vCam.x + pPlane->m_Normal.y * vCam.y +
                      pPlane->m_Normal.z * vCam.z - pPlane->m_Dist <= 0.01f)
            continue;                   // facing away (r_ProcessLeaf's plane test)
        GLRTexture* pTex = NULL;
        if (pSurf->m_pTexture)
        {
            glr_TextureId(pSurf->m_pTexture);
            pTex = (GLRTexture*)pSurf->m_pTexture->m_pRenderData;
        }
        glr_QueuePoly(pTex, pPoly);
    }
}

// The lightmap unit: on or off, with this atlas.
static GLRLMAtlas* g_pBoundAtlas;
static void glr_SetLightmapUnit(GLRLMAtlas* pAtlas)
{
    if (pAtlas == g_pBoundAtlas)
        return;
    g_Multi.glActiveTexture(GL_TEXTURE1_ARB);
    if (pAtlas)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, pAtlas->m_Id);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    else
        glDisable(GL_TEXTURE_2D);
    g_Multi.glActiveTexture(GL_TEXTURE0_ARB);
    g_pBoundAtlas = pAtlas;
}

static void glr_DrawQueuedPolys(MainWorld* pWorld, bool bLightmaps)
{
    qsort(g_pDrawList, g_nDrawList, sizeof(GLRDrawPoly), glr_CompareDrawPoly);
    GLRTexture* pBound = (GLRTexture*)-1;
    g_pBoundAtlas = (GLRLMAtlas*)-1;
    if (bLightmaps)
        glr_SetLightmapUnit(NULL);
    float fGrid = pWorld->m_LMGridSize > 0.0f ? pWorld->m_LMGridSize : 1.0f;
    for (uint32_t i = 0; i < g_nDrawList; i++)
    {
        GLRTexture* pTex = g_pDrawList[i].m_pTex;
        WorldPoly* pPoly = g_pDrawList[i].m_pPoly;
        if (pTex != pBound)
        {
            if (pTex)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, pTex->m_Id);
            }
            else
                glDisable(GL_TEXTURE_2D);
            pBound = pTex;
        }
        float fU = pTex ? pTex->m_fUMul : 0.0f, fV = pTex ? pTex->m_fVMul : 0.0f;
        bool bFullbright = pTex && (pTex->m_DtxFlags & DTX_FULLBRITE);

        // A lightmapped poly is base x lightmap with white vertices (the PC's
        // two-pass look, Saturate 0); every other poly is base x its vertex
        // colours (r_SetPolyFunctions, docs/world.md B.6).
        GLRPolyLM* pLM = bLightmaps && !bFullbright ? (GLRPolyLM*)pPoly->m_pLMPage : NULL;
        if (bLightmaps)
            glr_SetLightmapUnit(pLM ? pLM->m_pAtlas : NULL);
        LTVector P, Q;
        float fInvSize = 0.0f, fOffU = 0.0f, fOffV = 0.0f;
        if (pLM)
        {
            glr_LMVectors(pPoly->m_pSurface, &P, &Q);
            fInvSize = 1.0f / pLM->m_pAtlas->m_nSize;
            fOffU = pLM->m_X + 0.5f;
            fOffV = pLM->m_Y + 0.5f;
        }

        glBegin(GL_TRIANGLE_FAN);
        for (uint32_t v = 0; v < pPoly->m_nVerts; v++)
        {
            const WorldPolyVertex& vert = pPoly->m_Verts[v];
            const LTVector& pos = *vert.m_pPos;
            if (bFullbright || pLM)
                glColor4ub(255, 255, 255, 255);
            else
                glColor4ub(vert.m_R, vert.m_G, vert.m_B, 255);
            glTexCoord2f(vert.m_U * fU, vert.m_V * fV);
            if (pLM)
            {
                float dx = pos.x - pPoly->m_LMOrigin.x, dy = pos.y - pPoly->m_LMOrigin.y,
                      dz = pos.z - pPoly->m_LMOrigin.z;
                float lu = (dx * P.x + dy * P.y + dz * P.z) / fGrid + fOffU;
                float lv = (dx * Q.x + dy * Q.y + dz * Q.z) / fGrid + fOffV;
                g_Multi.glMultiTexCoord2f(GL_TEXTURE1_ARB, lu * fInvSize, lv * fInvSize);
            }
            glVertex3f(pos.x, pos.y, pos.z);
        }
        glEnd();
    }
    if (bLightmaps)
        glr_SetLightmapUnit(NULL);
    g_nDrawList = 0;
}

static void glr_DrawWorld(GLRContext* pContext, const GLRCamera* pCam)
{
    MainWorld* pWorld = pContext->m_pWorld;
    // LightMap 0, a world without lightmaps, or no second texture unit: the
    // lightmapped polys fall back to their vertex colours, as in
    // r_SetPolyFunctions.
    bool bLightmaps = g_Multi.m_bOK && pContext->m_pAtlases && cvi(CV_LightMap) &&
                      pContext->m_nContextGen == g.nContextGen;
    if (bLightmaps)
        glr_UpdateLightmaps(pContext);

    uint32_t nWorlds = pWorld->m_Worlds.m_nElements;
    for (uint32_t i = 0; i < nWorlds; i++)
    {
        WorldData* pData = mw_World(pWorld, i);
        if (!pData || !pData->m_pWorldBsp)
            continue;
        if (!(pData->m_pWorldBsp->m_WorldInfoFlags & WORLDINFO_MAINWORLD))
            continue;                   // world models are drawn as objects
        glr_CollectBsp(pData->m_pWorldBsp, pCam->m_Pos);
    }
    glr_DrawQueuedPolys(pWorld, bLightmaps);
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

    GLRCamera cam;
    glr_SetupCamera(pScene, &cam);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glr_LoadCamera(&cam);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1666f);   // r_Init's
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glShadeModel(GL_SMOOTH);

    glr_InitMultitexture();
    if (pScene->m_DrawMode == DRAWMODE_NORMAL && pScene->m_pRenderContext)
    {
        GLRContext* pContext = (GLRContext*)pScene->m_pRenderContext;
        if (pContext->m_pWorld)
            glr_DrawWorld(pContext, &cam);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
    return 0;
}

// gl_sprites.cpp -- sprites (OT_SPRITE), and the world's light grid that
// lights sprites and models.
//
// r_DrawSprite and r_DrawRotateableSprite (docs/objects.md part III.1): the
// engine advances a sprite's animation and the renderer draws its current
// frame, a texture, as a camera-facing quad half the texture's size times
// the object's scale on each side (rotatable sprites: in the object's own
// orientation, from either side). The colour is the object's, plus the
// light grid's at its position unless it is FLAG_NOLIGHT. Sprites are
// translucent: they are drawn far to near after everything solid.

#include <math.h>

#include "globjects.h"

#define FLAG_SPRITEBIAS        0x4
#define FLAG_ROTATEABLESPRITE  0x8
#define FLAG_GLOWSPRITE        0x20

// The light grid, MainWorld+0xfc (r_GetSpriteColor 0x15df70): one 4-byte
// cell (r, g, b, unused) per grid point, sampled trilinearly.
void glr_LightGridSample(const LTVector& pos, float* pOut)
{
    pOut[0] = pOut[1] = pOut[2] = 0.0f;
    MainWorld* pWorld = g_Frame.m_pContext ? g_Frame.m_pContext->m_pWorld : NULL;
    if (!pWorld)
        return;
    const uint8_t* pBase = (const uint8_t*)pWorld;
    const uint8_t* pData = *(const uint8_t* const*)(pBase + 0xfc);
    if (!pData)
        return;
    int32_t nRow = *(const int32_t*)(pBase + 0x104);
    int32_t nMax[3] = { *(const int32_t*)(pBase + 0x110), *(const int32_t*)(pBase + 0x114),
                        *(const int32_t*)(pBase + 0x118) };
    int32_t nSlice = *(const int32_t*)(pBase + 0x11c);
    const float* pInvCell = (const float*)(pBase + 0x12c);
    const float* pOrigin = (const float*)(pBase + 0x138);
    float f[3] = { (pos.x - pOrigin[0]) * pInvCell[0], (pos.y - pOrigin[1]) * pInvCell[1],
                   (pos.z - pOrigin[2]) * pInvCell[2] };
    int idx[3];
    float t[3];
    for (int i = 0; i < 3; i++)
    {
        idx[i] = (int)f[i];
        if (idx[i] < 0) idx[i] = 0;
        if (idx[i] > nMax[i]) idx[i] = nMax[i];
        t[i] = f[i] - floorf(f[i]);
        if (t[i] < 0.0f) t[i] = 0.0f;
        if (t[i] > 1.0f) t[i] = 1.0f;
    }
    const uint8_t* c = pData + ((idx[2] * nSlice + idx[1] * nRow + idx[0]) << 2);
    int dx = 4, dy = nRow * 4, dz = nSlice * 4;
    for (int ch = 0; ch < 3; ch++)
    {
        float c000 = c[ch], c100 = c[dx + ch], c010 = c[dy + ch], c110 = c[dy + dx + ch];
        float c001 = c[dz + ch], c101 = c[dz + dx + ch], c011 = c[dz + dy + ch], c111 = c[dz + dy + dx + ch];
        float x00 = c000 + (c100 - c000) * t[0], x10 = c010 + (c110 - c010) * t[0];
        float x01 = c001 + (c101 - c001) * t[0], x11 = c011 + (c111 - c011) * t[0];
        float y0 = x00 + (x10 - x00) * t[1], y1 = x01 + (x11 - x01) * t[1];
        pOut[ch] = y0 + (y1 - y0) * t[2];
    }
}

#define FLAG_ONLYLIGHTWORLD 0x20     // a light's: it lights the world, not objects

// r_CalcLightAdd (d3d.ren 0x1001e210): the visible lights near the object;
// a light colour below 128 takes light away.
void glr_CalcLightAdd(const LTVector& pos, float* pOut)
{
    if (!cvi(CV_DynamicLight))
        return;
    GLRObjList* pLights = glr_ObjList(VS_LIGHTS);
    for (uint32_t i = 0; i < pLights->m_nObjects; i++)
    {
        LTObject* L = pLights->m_pObjects[i];
        if (L->m_Flags & FLAG_ONLYLIGHTWORLD)
            continue;
        float r;
        memcpy(&r, (const uint8_t*)L + 0x1a8, 4);
        float dx = L->m_Pos.x - pos.x, dy = L->m_Pos.y - pos.y, dz = L->m_Pos.z - pos.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (r * r <= d2)
            continue;
        float s = (1.0f - sqrtf(d2) / r) * 0.7f;
        pOut[0] += (2.0f * L->m_ColorR - 255.0f) * s;
        pOut[1] += (2.0f * L->m_ColorG - 255.0f) * s;
        pOut[2] += (2.0f * L->m_ColorB - 255.0f) * s;
    }
}

// r_GetSpriteColor: the object's colour plus the light at it (added, not
// multiplied), its alpha.
static void glr_SpriteColor(LTObject* pObj, uint8_t* pRGBA)
{
    float add[3] = { 0, 0, 0 };
    if (!(pObj->m_Flags & FLAG_NOLIGHT))
    {
        glr_LightGridSample(pObj->m_Pos, add);
        glr_CalcLightAdd(pObj->m_Pos, add);
    }
    float c[3] = { pObj->m_ColorR + add[0], pObj->m_ColorG + add[1], pObj->m_ColorB + add[2] };
    for (int i = 0; i < 3; i++)
        pRGBA[i] = (uint8_t)(c[i] > 255.0f ? 255.0f : c[i] < 0.0f ? 0.0f : c[i]);
    pRGBA[3] = pObj->m_ColorA;
}

// The sprite's blend (docs/objects.md III.0.5): additive and multiply sprites
// fog towards black.
void glr_ObjectBlend(LTObject* pObj)
{
    static const float black[4] = { 0, 0, 0, 0 };
    if (pObj->m_Flags2 & FLAG2_ADDITIVE)
    {
        glBlendFunc(GL_ONE, GL_ONE);
        glFogfv(GL_FOG_COLOR, black);
    }
    else if (pObj->m_Flags2 & FLAG2_MULTIPLY)
    {
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        glFogfv(GL_FOG_COLOR, black);
    }
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static SharedTexture* glr_SpriteFrame(LTObject* pObj)
{
    // SpriteTracker at +0x1a8: sprite, current animation, current frame
    // (a SharedTexture** into the animation's frames).
    const uint8_t* p = (const uint8_t*)pObj;
    if (!*(void* const*)(p + 0x1ac))
        return NULL;
    SharedTexture** ppFrame = *(SharedTexture** const*)(p + 0x1b0);
    return ppFrame ? *ppFrame : NULL;
}

// Draw one sprite with the camera currently loaded (the main camera, or the
// sky's): pCam gives its position and axes.
static void glr_DrawSpriteWith(LTObject* pObj, const GLRCamera* pCam)
{
    SharedTexture* pFrame = glr_SpriteFrame(pObj);
    if (!pFrame)
        return;
    glr_TextureId(pFrame);
    GLRTexture* pTex = (GLRTexture*)pFrame->m_pRenderData;
    if (!pTex)
        return;

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_FOG_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
    glEnable(GL_BLEND);
    glr_ObjectBlend(pObj);
    glr_ObjectFog(pObj, true);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pTex->m_Id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glr_ForgetBindings();

    uint8_t rgba[4];
    glr_SpriteColor(pObj, rgba);
    glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);

    // Two texels in from every edge (r_DrawSprite 0x15d9c0).
    float W = (float)pTex->m_nBaseWidth, H = (float)pTex->m_nBaseHeight;
    float u0 = W > 4 ? 2.0f / W : 0.0f, u1 = W > 4 ? (W - 2.0f) / W : 1.0f;
    float v0 = H > 4 ? 2.0f / H : 0.0f, v1 = H > 4 ? (H - 2.0f) / H : 1.0f;

    if (pObj->m_Flags & FLAG_ROTATEABLESPRITE)
    {
        // In the object's orientation and scale: corners (+-W, +-H, 0).
        float R[3][3];
        glr_RotationMatrix(pObj->m_Rotation, R);
        LTMatrix M;
        for (int r = 0; r < 3; r++)
        {
            M.m[r][0] = R[r][0] * pObj->m_Scale.x;
            M.m[r][1] = R[r][1] * pObj->m_Scale.y;
            M.m[r][2] = R[r][2] * pObj->m_Scale.z;
        }
        M.m[0][3] = pObj->m_Pos.x;
        M.m[1][3] = pObj->m_Pos.y;
        M.m[2][3] = pObj->m_Pos.z;
        M.m[3][0] = M.m[3][1] = M.m[3][2] = 0.0f;
        M.m[3][3] = 1.0f;
        glPushMatrix();
        if (pObj->m_Flags & FLAG_REALLYCLOSE)
            glLoadIdentity();
        glr_MultMatrix(M);
        glBegin(GL_QUADS);
        glTexCoord2f(u0, v0); glVertex3f(-W, H, 0);
        glTexCoord2f(u1, v0); glVertex3f(W, H, 0);
        glTexCoord2f(u1, v1); glVertex3f(W, -H, 0);
        glTexCoord2f(u0, v1); glVertex3f(-W, -H, 0);
        glEnd();
        glPopMatrix();
    }
    else
    {
        // A billboard in camera space. A really-close object's position is
        // in camera space already.
        LTVector c;
        if (pObj->m_Flags & FLAG_REALLYCLOSE)
            c = pObj->m_Pos;
        else
        {
            LTVector d = { pObj->m_Pos.x - pCam->m_Pos.x, pObj->m_Pos.y - pCam->m_Pos.y,
                           pObj->m_Pos.z - pCam->m_Pos.z };
            const float (*R)[3] = pCam->m_R;
            c.x = (R[0][0] * d.x + R[1][0] * d.y + R[2][0] * d.z) * pCam->m_fScale;
            c.y = (R[0][1] * d.x + R[1][1] * d.y + R[2][1] * d.z) * pCam->m_fScale;
            c.z = (R[0][2] * d.x + R[1][2] * d.y + R[2][2] * d.z) * pCam->m_fScale;
        }
        float fMinZ = (pObj->m_Flags & FLAG_REALLYCLOSE) ? 0.01f : 7.0f;
        if (c.z > fMinZ)
        {
            float hw = W * pObj->m_Scale.x, hh = H * pObj->m_Scale.y;
            if (pObj->m_Flags & FLAG_GLOWSPRITE)
            {
                float k = (c.z - 10.0f) / 490.0f;
                k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
                float s = 0.1f + 1.9f * k;
                hw *= s;
                hh *= s;
            }
            // Sprite bias: drawn 20 units nearer (never nearer than 7), the
            // same size on screen -- the quad scaled towards the eye.
            float k = 1.0f;
            if (pObj->m_Flags & FLAG_SPRITEBIAS)
            {
                float z = c.z - 20.0f;
                if (z < 7.0f)
                    z = 7.0f < c.z ? 7.0f : c.z;
                k = z / c.z;
            }
            glPushMatrix();
            glLoadIdentity();
            glBegin(GL_QUADS);
            glTexCoord2f(u0, v0); glVertex3f((c.x - hw) * k, (c.y + hh) * k, c.z * k);
            glTexCoord2f(u1, v0); glVertex3f((c.x + hw) * k, (c.y + hh) * k, c.z * k);
            glTexCoord2f(u1, v1); glVertex3f((c.x + hw) * k, (c.y - hh) * k, c.z * k);
            glTexCoord2f(u0, v1); glVertex3f((c.x - hw) * k, (c.y - hh) * k, c.z * k);
            glEnd();
            glPopMatrix();
        }
    }
    glr_ObjectFog(pObj, false);
    glPopAttrib();
}

void glr_DrawSprite(LTObject* pObj)
{
    bool bClose = (pObj->m_Flags & FLAG_REALLYCLOSE) != 0;
    if (bClose)
        glr_BeginReallyClose(pObj);
    glr_DrawSpriteWith(pObj, &g_Frame.m_Cam);
    if (bClose)
        glr_EndReallyClose();
}

// r_DrawNoZSprites: over everything, depth ignored.
void glr_DrawNoZSprite(LTObject* pObj)
{
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glr_DrawSprite(pObj);
    glPopAttrib();
}

void glr_DrawSkySprite(LTObject* pObj)
{
    glr_DrawSpriteWith(pObj, &g_Frame.m_SkyCam);
}

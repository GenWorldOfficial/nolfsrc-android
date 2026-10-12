// gl_objects.cpp -- the visible set and the order objects are drawn in.
//
// The Mac renderer's per-type Process functions only sort an object into a
// set of the VisibleSet; r_FlushObjectQueues then draws the sets in a fixed
// order (docs/objects.md 2.3, 2.5): solid world models, solid models, poly
// grids and canvases; the chromakeyed ones; then everything translucent,
// far to near; sprites that ignore the depth buffer last.
//
// World models are drawn by gl_scene.cpp, which owns the world's polygons.
// The other types are stage 3 (README.md): they are sorted here already,
// and draw nothing yet.

#include <math.h>

#include "globjects.h"

static GLRObjList g_Sets[VS_COUNT];

void GLRObjList::Add(LTObject* pObj)
{
    if (m_nObjects == m_nAlloc)
    {
        uint32_t n = m_nAlloc ? m_nAlloc * 2 : 64;
        LTObject** p = (LTObject**)realloc(m_pObjects, n * sizeof(LTObject*));
        if (!p)
            return;
        m_pObjects = p;
        m_nAlloc = n;
    }
    m_pObjects[m_nObjects++] = pObj;
}

GLRObjList* glr_ObjList(GLRObjSet eSet)
{
    return &g_Sets[eSet];
}

// ---- translucent objects -----------------------------------------------------------

struct GLRTranslucent {
    LTObject* m_pObj;
    GLRDrawFn m_fnDraw;
    float     m_fKey;                   // squared distance from the camera
};
static GLRTranslucent* g_pTrans;
static uint32_t g_nTrans, g_nTransAlloc;

void glr_AddTranslucent(LTObject* pObj, GLRDrawFn fnDraw)
{
    if (g_nTrans == g_nTransAlloc)
    {
        uint32_t n = g_nTransAlloc ? g_nTransAlloc * 2 : 64;
        GLRTranslucent* p = (GLRTranslucent*)realloc(g_pTrans, n * sizeof(GLRTranslucent));
        if (!p)
            return;
        g_pTrans = p;
        g_nTransAlloc = n;
    }
    GLRTranslucent& t = g_pTrans[g_nTrans++];
    t.m_pObj = pObj;
    t.m_fnDraw = fnDraw;
    // ObjectDrawList::Sort: a really-close object's position is relative to
    // the camera already.
    LTVector d = pObj->m_Pos;
    if (!(pObj->m_Flags & FLAG_REALLYCLOSE))
    {
        d.x -= g_Frame.m_Cam.m_Pos.x;
        d.y -= g_Frame.m_Cam.m_Pos.y;
        d.z -= g_Frame.m_Cam.m_Pos.z;
    }
    t.m_fKey = d.x * d.x + d.y * d.y + d.z * d.z;
}

// Far to near; really-close objects (drawn in camera space) last.
static int glr_CompareTranslucent(const void* a, const void* b)
{
    const GLRTranslucent* x = (const GLRTranslucent*)a;
    const GLRTranslucent* y = (const GLRTranslucent*)b;
    bool cx = (x->m_pObj->m_Flags & FLAG_REALLYCLOSE) != 0;
    bool cy = (y->m_pObj->m_Flags & FLAG_REALLYCLOSE) != 0;
    if (cx != cy)
        return cx ? 1 : -1;
    return x->m_fKey > y->m_fKey ? -1 : x->m_fKey < y->m_fKey ? 1 : 0;
}

// ---- the set -------------------------------------------------------------------------

void glr_BeginObjects(void)
{
    for (int i = 0; i < VS_COUNT; i++)
        g_Sets[i].Clear();
    g_nTrans = 0;
}

// The per-type Process functions (the Mac handler table, docs/objects.md 2.3).
void glr_QueueObject(LTObject* pObj)
{
    switch (pObj->m_ObjectType)
    {
    case OT_WORLDMODEL:
    case OT_CONTAINER:
        glr_ProcessWorldModel(pObj);
        break;
    case OT_MODEL:
        if (pObj->m_ColorA != 255)
            glr_AddTranslucent(pObj, NULL);
        else if (pObj->m_Flags2 & FLAG2_CHROMAKEY)
            glr_ObjList(VS_MODELS_CHROMAKEY)->Add(pObj);
        else
            glr_ObjList(VS_MODELS)->Add(pObj);
        break;
    case OT_SPRITE:
        if (pObj->m_Flags & FLAG_SPRITE_NOZ)
            glr_ObjList(VS_SPRITES_NOZ)->Add(pObj);
        else
            glr_AddTranslucent(pObj, NULL);
        break;
    case OT_LIGHT:
        glr_ObjList(VS_LIGHTS)->Add(pObj);
        break;
    case OT_POLYGRID:
        if (pObj->m_ColorA != 255)
            glr_AddTranslucent(pObj, NULL);
        else
            glr_ObjList(VS_POLYGRIDS)->Add(pObj);
        break;
    case OT_PARTICLESYSTEM:
    case OT_LINESYSTEM:
        glr_AddTranslucent(pObj, NULL);
        break;
    case OT_CANVAS:
        glr_ObjList(VS_CANVASES)->Add(pObj);
        break;
    default:
        break;
    }
}

static void glr_DrawSet(GLRObjSet eSet, GLRDrawFn fnDraw)
{
    GLRObjList* pList = glr_ObjList(eSet);
    for (uint32_t i = 0; i < pList->m_nObjects; i++)
        if (pList->m_pObjects[i]->m_Flags & FLAG_VISIBLE)
            fnDraw(pList->m_pObjects[i]);
}

void glr_FlushObjects(void)
{
    // Solid.
    glr_DrawSet(VS_WORLDMODELS, glr_DrawSolidWorldModel);

    // Chromakey: texels whose alpha the key cleared are dropped.
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glr_DrawSet(VS_WORLDMODELS_CHROMAKEY, glr_DrawSolidWorldModel);
    glPopAttrib();

    // Translucent, far to near, blended over what is there without writing
    // depth (r_SetTranslucentObjectStates).
    if (g_nTrans)
    {
        if (cvi(CV_DrawSorted))
            qsort(g_pTrans, g_nTrans, sizeof(GLRTranslucent), glr_CompareTranslucent);
        glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (uint32_t i = 0; i < g_nTrans; i++)
            if (g_pTrans[i].m_fnDraw && (g_pTrans[i].m_pObj->m_Flags & FLAG_VISIBLE))
                g_pTrans[i].m_fnDraw(g_pTrans[i].m_pObj);
        glPopAttrib();
    }
}

void glr_DrawSkyObject(LTObject* pObj)
{
    (void)pObj;
}

// globjects.h -- the visible set: the objects a frame draws, by kind, and
// the order they are drawn in (r_FlushObjectQueues; docs/objects.md 2.3-2.5).

#ifndef GLOBJECTS_H
#define GLOBJECTS_H

#include "glscene.h"

enum GLRObjSet {
    VS_WORLDMODELS, VS_WORLDMODELS_CHROMAKEY,
    VS_MODELS, VS_MODELS_CHROMAKEY, VS_CLOSE_MODELS, VS_CLOSE_MODELS_CHROMAKEY,
    VS_SPRITES_NOZ,
    VS_POLYGRIDS, VS_CANVASES,
    VS_LIGHTS,
    VS_COUNT
};

struct GLRObjList {
    LTObject** m_pObjects;
    uint32_t   m_nObjects, m_nAlloc;
    void Add(LTObject* pObj);
    void Clear() { m_nObjects = 0; }
};
GLRObjList* glr_ObjList(GLRObjSet eSet);

// Translucent objects are drawn last, far to near, each by its own function.
typedef void (*GLRDrawFn)(LTObject* pObj);
void glr_AddTranslucent(LTObject* pObj, GLRDrawFn fnDraw);

void glr_BeginObjects(void);                    // empties the set
void glr_QueueObject(LTObject* pObj);           // the type's Process function
void glr_FlushObjects(void);                    // draws the set
void glr_DrawSkyObject(LTObject* pObj);         // a sky sprite or poly grid

// gl_sprites.cpp
void glr_LightGridSample(const LTVector& pos, float* pOut);
void glr_ObjectBlend(LTObject* pObj);
void glr_DrawSprite(LTObject* pObj);
void glr_DrawNoZSprite(LTObject* pObj);
void glr_DrawSkySprite(LTObject* pObj);

// gl_models.cpp
void glr_DrawModel(LTObject* pObj);
void glr_DrawTranslucentModel(LTObject* pObj);
void glr_BeginReallyClose(LTObject* pObj);     // the camera-space view for it
void glr_EndReallyClose(void);

#endif

// gl_scene.cpp -- the 3D half: the world context and RenderScene.
//
// Stage one draws no scene: RenderScene leaves the screen as ClearScreen
// left it. The world comes next (README.md).

#include "glr.h"

// r_CreateContext: the engine binds a loaded world to the renderer and keeps
// what this returns (NULL fails the whole renderer init).
struct GLRContext {
    void* m_pWorld;                 // RenderContextInit's MainWorld
};

void* glr_CreateContext(RenderContextInit* pInit)
{
    GLRContext* pContext = (GLRContext*)calloc(1, sizeof(GLRContext));
    if (pContext && pInit)
        pContext->m_pWorld = pInit->m_pWorld;
    return pContext;
}

void glr_DeleteContext(void* pContext)
{
    free(pContext);
}

LTBOOL glr_RenderScene(SceneDesc* pScene)
{
    (void)pScene;
    if (g.bInOptimized2D)
    {
        glr_Print("Error: tried to render 3D while in optimized 2D mode.");
        return 0;
    }
    return 0;
}

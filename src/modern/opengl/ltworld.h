// ltworld.h -- the engine's scene and world data, as the renderer reads them.
//
// The renderer never loads a world; it walks what the engine loaded. Both
// shipped renderers read these structures directly (and so do we: never
// call the engine's virtuals beyond what is named here, since the PC and Mac
// vtables differ). Layouts and evidence: docs/world.md (parts A and B); the
// offsets are the same on the PC and the Mac.

#ifndef LTWORLD_H
#define LTWORLD_H

#include "ltrender.h"

struct LTRotation { float x, y, z, w; };
struct LTPlane { LTVector m_Normal; float m_Dist; };
struct LTLink { LTLink* m_pPrev; LTLink* m_pNext; void* m_pData; };

// What RenderScene gets (0xec bytes).
struct SkyDef { LTVector m_Min, m_Max, m_ViewMin, m_ViewMax; };
static_assert(sizeof(SkyDef) == 0x30, "SkyDef");

struct SceneDesc {
    int32_t   m_DrawMode;               // 0x00  1 the world from the camera, 2 the object list only
    uint32_t* m_pTicks[6];              // 0x04
    LTVector  m_ModelAdd;               // 0x1c
    LTVector  m_ModelDirAdd;            // 0x28
    void*     m_pRenderContext;         // 0x34  what CreateContext returned
    LTVector  m_Unused38, m_Unused44;   // 0x38  always zero
    LTVector  m_GlobalLightScale;       // 0x50
    LTVector  m_GlobalLightAdd;         // 0x5c  the camera's light add
    float     m_FrameTime;              // 0x68
    SkyDef    m_SkyDef;                 // 0x6c
    LTObject** m_SkyObjects;            // 0x9c
    int32_t   m_nSkyObjects;            // 0xa0
    LTRect    m_Rect;                   // 0xa4  viewport, screen pixels
    float     m_xFov, m_yFov;           // 0xb4  full angles, radians
    float     m_FarZ;                   // 0xbc
    LTVector  m_Pos;                    // 0xc0  camera
    LTRotation m_Rotation;              // 0xcc
    LTObject** m_pObjectList;           // 0xdc  draw mode 2
    int32_t   m_ObjectListSize;         // 0xe0
    void*     m_ModelHookFn;            // 0xe4
    void*     m_ModelHookUser;          // 0xe8
};
static_assert(sizeof(SceneDesc) == 0xec, "SceneDesc");

#define DRAWMODE_NORMAL     1
#define DRAWMODE_OBJECTLIST 2

// ---- the world ---------------------------------------------------------------

struct WorldPoly;
struct Surface;
struct WorldBsp;

struct CMoArrayHdr {                    // the engine's dynamic array
    void*    m_pVtbl;
    void*    m_pArray;
    uint32_t m_nElements;
    uint32_t m_Cache[2];
};
static_assert(sizeof(CMoArrayHdr) == 0x14, "CMoArrayHdr");

struct WorldData {                      // one per BSP world: the main world and every world model
    uint32_t  m_Flags;
    WorldBsp* m_pWorldBsp;              // +0x04 the full geometry
    WorldBsp* m_pVisBsp;                // +0x08 with leaves and PVS, when the file has one
    WorldBsp* m_pRenderBsp;             // +0x0c
};

#define WORLDINFO_VISBSP    0x02        // has a separate vis BSP
#define WORLDINFO_MAINWORLD 0x04
#define WORLDINFO_PHYSICS   0x10
#define WORLDINFO_VIS       0x20

// MainWorld, as far as the renderer reads it.
struct MainWorld {
    void*        m_pVtbl;               // 0x000
    CMoArrayHdr  m_LMAnims;             // 0x004  0x60-byte LMAnim records, name inline
    CMoArrayHdr  m_LMArrays[4];         // 0x018  lightmap data (docs/world.md part C)
    uint32_t     m_68;                  // 0x068
    uint8_t      m_WorldTree[0x7c];     // 0x06c
    LTLink       m_EffectHead;          // 0x0e8
    void*        m_pEffects;            // 0x0f4
    float        m_LMGridSize;          // 0x0f8  lightmap texel size in world units
    uint8_t      m_LightTable[0x48];    // 0x0fc
    LTVector     m_ExtentsMin;          // 0x144
    LTVector     m_ExtentsMax;          // 0x150
    LTVector     m_InvExtents;          // 0x15c
    LTVector     m_BoxMin, m_BoxMax;    // 0x168
    uint32_t     m_Flags;               // 0x180  0x2 has vis data, 0x4 has static lightmaps
    uint32_t     m_184;                 // 0x184
    CMoArrayHdr  m_Worlds;              // 0x188  WorldData*: array +0x18c, count +0x190
};
static_assert(__builtin_offsetof(MainWorld, m_LMGridSize) == 0xf8, "MainWorld");
static_assert(__builtin_offsetof(MainWorld, m_Flags) == 0x180, "MainWorld");
static_assert(__builtin_offsetof(MainWorld, m_Worlds) == 0x188, "MainWorld");

#define MAINWORLD_HASVIS        0x2
#define MAINWORLD_HASLIGHTMAPS  0x4

inline WorldData* mw_World(MainWorld* pWorld, uint32_t i)
{
    return ((WorldData**)pWorld->m_Worlds.m_pArray)[i];
}

struct Node {                           // 0x18
    WorldPoly* m_pPoly;                 // 0x00 splitting poly; plane = its surface's
    Node*      m_Sides[2];              // 0x04 [0] back, [1] front
    void*      m_Objects[2];            // 0x0c object list head (prev, next)
    uint16_t   m_iLeaf;                 // 0x14 0xffff = none
    uint8_t    m_Flags;                 // 0x16 1 = the "in" sentinel, 2 = "out": a walk stops at flags & 3
    uint8_t    m_PlaneType;             // 0x17
};
static_assert(sizeof(Node) == 0x18, "Node");

struct Leaf {
    LTVector   m_Center;                // 0x00
    float      m_Radius;                // 0x0c
    void*      m_LeafLists;             // 0x10
    LTLink     m_Objects;               // 0x14
    WorldPoly** m_Polies;               // 0x20
    uint32_t   m_nPolies;               // 0x24
    float      m_28;                    // 0x28
    uint16_t   m_FrameCode;             // 0x2c
    uint16_t   m_nLeafLists;            // 0x2e
};
static_assert(sizeof(Leaf) == 0x30, "Leaf");

struct WorldBsp {
    void*      m_pVtbl;                 // 0x000
    char       m_WorldName[64];         // 0x004
    uint32_t   m_44;                    // 0x044
    uint32_t   m_WorldInfoFlags;        // 0x048
    uint8_t    m_pad4c[0x68 - 0x4c];
    LTPlane*   m_Planes;                // 0x068
    uint32_t   m_nPlanes;               // 0x06c
    void*      m_Nodes;                 // 0x070
    uint32_t   m_nNodes;                // 0x074
    Surface*   m_Surfaces;              // 0x078
    uint32_t   m_nSurfaces;             // 0x07c
    void*      m_LeafLists;             // 0x080
    uint32_t   m_nLeafLists;            // 0x084
    Leaf*      m_Leafs;                 // 0x088
    uint32_t   m_nLeafs;                // 0x08c
    void**     m_LeafPolyRefs;          // 0x090
    uint32_t   m_nLeafPolyRefs;         // 0x094
    uint32_t   m_VisDataSize;           // 0x098
    void*      m_RootNode;              // 0x09c
    WorldPoly** m_Polies;               // 0x0a0
    uint32_t   m_nPolies;               // 0x0a4
    LTVector*  m_Points;                // 0x0a8
    uint32_t   m_nPoints;               // 0x0ac
    uint8_t    m_padb0[0xc4 - 0xb0];
    LTVector   m_MinBox, m_MaxBox;      // 0x0c4
    uint8_t    m_paddc[0xe8 - 0xdc];
    LTVector   m_WorldTranslation;      // 0x0e8
    uint32_t   m_f4;                    // 0x0f4
    uint16_t   m_WorldIndex;            // 0x0f8
};
static_assert(__builtin_offsetof(WorldBsp, m_Polies) == 0xa0, "WorldBsp");
static_assert(__builtin_offsetof(WorldBsp, m_WorldIndex) == 0xf8, "WorldBsp");

struct Surface {
    void*          m_pVtbl;             // 0x00
    LTVector       O, P, Q;             // 0x04 texture origin and axes (texels per unit)
    uint8_t        m_Nexus[8];          // 0x28
    SharedTexture* m_pTexture;          // 0x30 NULL: flat
    LTPlane*       m_pPlane;            // 0x34
    uint32_t       m_Flags;             // 0x38 SURF_* in the low 29 bits, lightmap plane in the top 3
    uint16_t       m_TextureFlags;      // 0x3c
    uint16_t       m_iTexture;          // 0x3e
    uint16_t       m_FirstPoly;         // 0x40
    uint16_t       m_PortalId;          // 0x42 0x7fff = not a portal
    uint8_t        m_44;                // 0x44
    uint8_t        m_pad45[3];
};
static_assert(sizeof(Surface) == 0x48, "Surface");

#define SURF_SOLID        0x1
#define SURF_INVISIBLE    0x4
#define SURF_TRANSLUCENT  0x8           // a world model whose first surface has it is translucent
#define SURF_SKY          0x10          // marks where the sky shows; never drawn
#define SURF_LIGHTMAP     0x80
#define SURF_PANNINGSKY   0x8000
#define SURF_FLAGMASK     0x1fffffff

struct WorldPolyVertex {                // 0x18
    LTVector* m_pPos;                   // 0x00 -> WorldBsp::m_Points
    float     m_U, m_V;                 // 0x04 base texture, texels (the engine pans these)
    float     m_LMU, m_LMV;             // 0x0c renderer-owned
    uint8_t   m_B, m_G, m_R, m_A;       // 0x14 vertex colour
};
static_assert(sizeof(WorldPolyVertex) == 0x18, "WorldPolyVertex");

struct WorldPoly {
    uint8_t    m_00[0x0c];              // 0x00
    void*      m_pData0c;               // 0x0c
    uint32_t   m_nData10;               // 0x10
    uint16_t   m_Flags;                 // 0x14 renderer-owned
    uint16_t   m_Index;                 // 0x16
    LTVector   m_Center;                // 0x18 bounding sphere
    float      m_Radius;                // 0x24
    Surface*   m_pSurface;              // 0x28
    void*      m_pLights;               // 0x2c
    uint32_t   m_30;                    // 0x30
    LTVector   m_LMOrigin;              // 0x34
    uint16_t   m_NextInSurface;         // 0x40
    uint16_t   m_FrameCode;             // 0x42
    void*      m_pLMPage;               // 0x44 renderer-owned
    uint8_t    m_LMWidth, m_LMHeight;   // 0x48
    uint8_t    m_LMPageX, m_LMPageY;    // 0x4a renderer-owned
    WorldPolyVertex* m_pFixVerts;       // 0x4c
    uint16_t   m_nVerts;                // 0x50
    uint16_t   m_nFixVerts;             // 0x52
    WorldPolyVertex m_Verts[1];         // 0x54 m_nVerts of them
};
static_assert(__builtin_offsetof(WorldPoly, m_pSurface) == 0x28, "WorldPoly");
static_assert(__builtin_offsetof(WorldPoly, m_Verts) == 0x54, "WorldPoly");

#endif

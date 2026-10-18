// gles_device.cpp -- rdev.h on OpenGL ES 3.0: the Android backend.
//
// OpenGL ES has none of the fixed-function pipeline rdev.h is shaped after,
// so this backend keeps that pipeline's state itself and draws with small
// shader programs generated for each combination of it in use (texture
// units and their environments, alpha test, fog, depth clamping). A program
// is made the first time its combination is drawn and kept for the life of
// the context.
//
// Immediate-mode primitives are gathered between rd_Begin and rd_End and
// drawn from a streaming vertex buffer; quads become triangles on the CPU.
//
// Everything draws into an offscreen target at the engine's mode
// (nWidth x nHeight), multisampled when antialiasing is on. rd_Present
// resolves it into a single-sampled copy (what rd_ReadPixels reads as the
// front buffer) and scales that onto the window, keeping its proportions.
// The engine's display mode is therefore an internal resolution: a phone
// can draw at 1280x720 and show it on a 2400x1080 screen.
//
// Depth clamping (the really-close objects: the player's weapon, the menus'
// 3D parts) uses EXT_depth_clamp when the driver has it. rd_NearDepth is done
// in the vertex shader: what lies between the near-depth clip and the
// projection's near plane is given window depth fDepth0..fDepth1 by 1/w,
// which the rasteriser interpolates exactly, so unlike d3d9.ren only a
// triangle that crosses the near plane is approximate, and unlike
// opengl.ren nothing is drawn twice. Geometry nearer than the clip distance
// is clipped with EXT_clip_cull_distance where it exists.
//
// The context is EGL's, on the ANativeWindow* the engine hands over as its
// window (android_platform.cpp). Nothing here is Android-specific: on a
// desktop with EGL and GLES 3 the same file draws on an X11 or Wayland
// window, which is how it can be tried away from a phone.

#include "../rdev.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

const char g_RDDll[] = "gles.ren";
const char g_RDLog[] = "gles.log";
const char g_RDCard[] = "OpenGLES";
RDCaps g_RDCaps;

// The core's console output (mr_main.cpp).
void mr_Print(const char* pFormat, ...);

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT                  0x0040
#endif
#define GLX_COMPRESSED_RGB_S3TC_DXT1        0x83F0
#define GLX_COMPRESSED_RGBA_S3TC_DXT1       0x83F1
#define GLX_COMPRESSED_RGBA_S3TC_DXT3       0x83F2
#define GLX_COMPRESSED_RGBA_S3TC_DXT5       0x83F3
#define GLX_TEXTURE_MAX_ANISOTROPY          0x84FE
#define GLX_MAX_TEXTURE_MAX_ANISOTROPY      0x84FF
#define GLX_DEPTH_CLAMP                     0x864F
#define GLX_CLIP_DISTANCE0                  0x3000

enum { GU_UNITS = 4 };

// ---- the context and its targets -----------------------------------------------

static EGLDisplay s_eglDisplay = EGL_NO_DISPLAY;
static EGLSurface s_eglSurface = EGL_NO_SURFACE;
static EGLContext s_eglContext = EGL_NO_CONTEXT;

static int    s_nWidth, s_nHeight;      // the target's size: the engine's mode
static int    s_nSamples = 1;
static GLuint s_nDrawFBO, s_nDrawColor, s_nDrawDepth;   // what everything draws into
static GLuint s_nFrontFBO, s_nFrontColor;               // the last presented frame
static GLuint s_nReadFBO, s_nReadColor;                 // a back-buffer read, when multisampled

static bool   s_bHwDepthClamp;          // EXT_depth_clamp
static bool   s_bClipDistance;          // EXT_clip_cull_distance

static GLuint s_nVAO, s_nVBO;

static void gu_Log(const char* pFormat, ...)
{
    char buf[512];
    va_list args;
    va_start(args, pFormat);
    vsnprintf(buf, sizeof(buf), pFormat, args);
    va_end(args);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "nolf", "%s", buf);
#endif
    mr_Print("%s", buf);
}

static bool gu_HasExt(const char* pName)
{
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++)
    {
        const char* p = (const char*)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (p && !strcmp(p, pName))
            return true;
    }
    return false;
}

static void gu_DestroyTargets(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLuint fbos[3] = { s_nDrawFBO, s_nFrontFBO, s_nReadFBO };
    GLuint rbs[4] = { s_nDrawColor, s_nDrawDepth, s_nFrontColor, s_nReadColor };
    glDeleteFramebuffers(3, fbos);
    glDeleteRenderbuffers(4, rbs);
    s_nDrawFBO = s_nDrawColor = s_nDrawDepth = 0;
    s_nFrontFBO = s_nFrontColor = 0;
    s_nReadFBO = s_nReadColor = 0;
}

static GLuint gu_ColorTarget(GLuint* pColor, int nSamples)
{
    GLuint nFBO = 0;
    glGenRenderbuffers(1, pColor);
    glBindRenderbuffer(GL_RENDERBUFFER, *pColor);
    if (nSamples > 1)
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, nSamples, GL_RGBA8, s_nWidth, s_nHeight);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, s_nWidth, s_nHeight);
    glGenFramebuffers(1, &nFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, nFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *pColor);
    return nFBO;
}

// The draw target (multisampled when it can be) and the front copy.
static bool gu_CreateTargets(int nSamples)
{
    GLint nMax = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &nMax);
    if (nSamples > nMax)
        nSamples = nMax;
    if (nSamples < 2)
        nSamples = 1;

    for (;;)
    {
        s_nDrawFBO = gu_ColorTarget(&s_nDrawColor, nSamples);
        glGenRenderbuffers(1, &s_nDrawDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, s_nDrawDepth);
        if (nSamples > 1)
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, nSamples, GL_DEPTH24_STENCIL8, s_nWidth, s_nHeight);
        else
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, s_nWidth, s_nHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_nDrawDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
            break;
        gu_DestroyTargets();
        if (nSamples == 1)
        {
            gu_Log("gles.ren: no %dx%d draw target", s_nWidth, s_nHeight);
            return false;
        }
        gu_Log("gles.ren: no %dx multisampled target; antialiasing off", nSamples);
        nSamples = 1;
    }
    s_nSamples = nSamples;

    s_nFrontFBO = gu_ColorTarget(&s_nFrontColor, 1);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        gu_Log("gles.ren: no front copy target");
        gu_DestroyTargets();
        return false;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, s_nDrawFBO);
    return true;
}

static void gu_DestroyContext(void)
{
    if (s_eglDisplay == EGL_NO_DISPLAY)
        return;
    eglMakeCurrent(s_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_eglContext != EGL_NO_CONTEXT)
        eglDestroyContext(s_eglDisplay, s_eglContext);
    if (s_eglSurface != EGL_NO_SURFACE)
        eglDestroySurface(s_eglDisplay, s_eglSurface);
    eglTerminate(s_eglDisplay);
    s_eglDisplay = EGL_NO_DISPLAY;
    s_eglSurface = EGL_NO_SURFACE;
    s_eglContext = EGL_NO_CONTEXT;
}

static bool gu_CreateContext(void* hWnd)
{
    s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_eglDisplay == EGL_NO_DISPLAY || !eglInitialize(s_eglDisplay, NULL, NULL))
    {
        gu_Log("gles.ren: no EGL display");
        s_eglDisplay = EGL_NO_DISPLAY;
        return false;
    }
    // The window only receives the scaled frame: no depth of its own.
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint nConfigs = 0;
    if (!eglChooseConfig(s_eglDisplay, attribs, &config, 1, &nConfigs) || nConfigs < 1)
    {
        gu_Log("gles.ren: no OpenGL ES 3 configuration");
        return false;
    }
    s_eglSurface = eglCreateWindowSurface(s_eglDisplay, config, (EGLNativeWindowType)hWnd, NULL);
    if (s_eglSurface == EGL_NO_SURFACE)
    {
        gu_Log("gles.ren: could not make a surface on the window (EGL error 0x%x)", eglGetError());
        return false;
    }
    const EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    s_eglContext = eglCreateContext(s_eglDisplay, config, EGL_NO_CONTEXT, ctx);
    if (s_eglContext == EGL_NO_CONTEXT ||
        !eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext))
    {
        gu_Log("gles.ren: could not make an OpenGL ES 3 context (EGL error 0x%x)", eglGetError());
        return false;
    }
    return true;
}

// ---- the fixed-function state ------------------------------------------------------

struct GUUnit {
    uint32_t  nTex;
    bool      bEnabled;
    bool      bCombine;                 // the environment is rd_TexCombine's
    RDTexEnv  eEnv;
    RDCombine Combine;
};

struct GUState {
    bool     bBlend, bAlphaTest, bDepthTest, bFog, bScissor, bDepthClamp, bCull;
    RDBlend  eBlendSrc, eBlendDst;
    float    fAlphaRef;
    RDDepthFunc eDepthFunc;
    bool     bDepthMask, bColorMask;
    float    ClearColor[4];
    float    FogColor[3], fFogStart, fFogEnd;
    int      Viewport[4];               // top-left origin, as rdev.h
    float    DepthRange[2];
    int      Scissor[4];
    int      nActive;
    GUUnit   Units[GU_UNITS];
};

static GUState s_S;

// What GL itself was last given, so a draw changes only what differs.
struct GUApplied {
    bool     bValid;
    bool     bBlend, bDepthTest, bScissor, bCull, bHwClamp, bClip;
    GLenum   nBlendSrc, nBlendDst, nDepthFunc;
    bool     bDepthMask, bColorMask;
    int      Viewport[4];
    float    DepthRange[2];
    int      Scissor[4];
};
static GUApplied s_A;
static int       s_nGLActive;           // GL's active texture unit

struct GUAttribFrame {
    uint32_t nMask;
    GUState  S;
};
static std::vector<GUAttribFrame> s_AttribStack;

static float s_NearDepth[4];            // clip, near, depth0, depth1; clip 0: off

// What each texture is, for converting uploads and for REPLACE on RGB.
enum GUKind : uint8_t { GK_NONE, GK_RGBA8, GK_RGB8, GK_RGB565, GK_DXT_ALPHA, GK_DXT_OPAQUE };
static std::vector<uint8_t> s_TexKind;  // indexed by texture name

static GUKind gu_KindOf(uint32_t nTex)
{
    return nTex < s_TexKind.size() ? (GUKind)s_TexKind[nTex] : GK_NONE;
}

static void gu_SetKind(uint32_t nTex, GUKind eKind)
{
    if (!nTex)
        return;
    if (nTex >= s_TexKind.size())
        s_TexKind.resize(nTex + 64, GK_NONE);
    s_TexKind[nTex] = eKind;
}

static void gu_GLActive(int nUnit)
{
    if (s_nGLActive != nUnit)
    {
        glActiveTexture(GL_TEXTURE0 + nUnit);
        s_nGLActive = nUnit;
    }
}

static void gu_ResetState(void)
{
    memset(&s_S, 0, sizeof(s_S));
    s_S.bDepthTest = true;
    s_S.eDepthFunc = RD_LEQUAL;
    s_S.bDepthMask = true;
    s_S.bColorMask = true;
    s_S.eBlendSrc = RD_ONE;
    s_S.eBlendDst = RD_ZERO;
    s_S.ClearColor[3] = 1.0f;
    s_S.fFogEnd = 1.0f;
    s_S.Viewport[2] = s_nWidth;
    s_S.Viewport[3] = s_nHeight;
    s_S.DepthRange[1] = 1.0f;
    s_S.Scissor[2] = s_nWidth;
    s_S.Scissor[3] = s_nHeight;
    for (int i = 0; i < GU_UNITS; i++)
    {
        s_S.Units[i].eEnv = RD_ENV_MODULATE;
        s_S.Units[i].Combine.fScale = 1.0f;
    }
    memset(&s_A, 0, sizeof(s_A));
    s_AttribStack.clear();
    memset(s_NearDepth, 0, sizeof(s_NearDepth));
    s_nGLActive = -1;
    gu_GLActive(0);
}

// ---- matrices ------------------------------------------------------------------------

struct GUMat { float m[16]; };

enum { GU_MV_DEPTH = 32, GU_P_DEPTH = 8, GU_TEX_DEPTH = 8 };
static GUMat    s_MV[GU_MV_DEPTH], s_P[GU_P_DEPTH], s_Tex[GU_UNITS][GU_TEX_DEPTH];
static int      s_nMV, s_nP, s_nTex[GU_UNITS];
static RDMatrixMode s_eMatMode = RD_MODELVIEW;
// Bumped by every change, so each program uploads only what it has not seen.
static uint32_t s_nMVSerial = 1, s_nPSerial = 1, s_TexSerial[GU_UNITS] = { 1, 1, 1, 1 };

static void gu_Identity(GUMat& M)
{
    memset(M.m, 0, sizeof(M.m));
    M.m[0] = M.m[5] = M.m[10] = M.m[15] = 1.0f;
}

// a = a * b, column-major (GL's glMultMatrix).
static void gu_Mul(GUMat& a, const float* b)
{
    GUMat r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
            r.m[c * 4 + row] = a.m[0 * 4 + row] * b[c * 4 + 0] + a.m[1 * 4 + row] * b[c * 4 + 1] +
                               a.m[2 * 4 + row] * b[c * 4 + 2] + a.m[3 * 4 + row] * b[c * 4 + 3];
    a = r;
}

static GUMat& gu_Top(void)
{
    switch (s_eMatMode)
    {
    case RD_PROJECTION: return s_P[s_nP];
    case RD_TEXTURE:    return s_Tex[s_S.nActive][s_nTex[s_S.nActive]];
    default:            return s_MV[s_nMV];
    }
}

static void gu_Touched(void)
{
    switch (s_eMatMode)
    {
    case RD_PROJECTION: s_nPSerial++; break;
    case RD_TEXTURE:    s_TexSerial[s_S.nActive]++; break;
    default:            s_nMVSerial++; break;
    }
}

static void gu_ResetMatrices(void)
{
    s_nMV = s_nP = 0;
    gu_Identity(s_MV[0]);
    gu_Identity(s_P[0]);
    for (int u = 0; u < GU_UNITS; u++)
    {
        s_nTex[u] = 0;
        gu_Identity(s_Tex[u][0]);
        s_TexSerial[u]++;
    }
    s_nMVSerial++;
    s_nPSerial++;
    s_eMatMode = RD_MODELVIEW;
}

// ---- programs ------------------------------------------------------------------------

// Everything that changes a program's source.
struct GUKey {
    uint32_t Units[GU_UNITS];           // 0: the unit is off
    uint32_t nGlobal;                   // alpha test, fog, depth mode, clip distance
    bool operator==(const GUKey& o) const { return !memcmp(this, &o, sizeof(*this)); }
};
struct GUKeyHash {
    size_t operator()(const GUKey& k) const
    {
        uint64_t h = 1469598103934665603ull;
        const uint8_t* p = (const uint8_t*)&k;
        for (size_t i = 0; i < sizeof(k); i++)
            h = (h ^ p[i]) * 1099511628211ull;
        return (size_t)h;
    }
};

enum { GU_DEPTH_NORMAL = 0, GU_DEPTH_CLAMP = 1, GU_DEPTH_NEAR = 2 };

// The unit word: bit 0 on, bits 1-2 mode (0 modulate, 1 replace, 2 combine),
// bit 3 the texture has alpha, then the combine fields 3 bits apart.
static uint32_t gu_UnitKey(const GUUnit& u)
{
    if (!u.bEnabled || !u.nTex)
        return 0;
    GUKind eKind = gu_KindOf(u.nTex);
    bool bAlpha = eKind != GK_RGB8 && eKind != GK_RGB565 && eKind != GK_DXT_OPAQUE;
    uint32_t k = 1u | (bAlpha ? 8u : 0u);
    if (u.bCombine)
    {
        const RDCombine& c = u.Combine;
        int nScale = c.fScale >= 3.0f ? 2 : c.fScale >= 1.5f ? 1 : 0;
        k |= 2u << 1;
        k |= (uint32_t)c.eColorOp << 4 | (uint32_t)c.eColor0 << 7 | (uint32_t)c.eColor1 << 10 |
             (uint32_t)nScale << 13 | (uint32_t)c.eAlphaOp << 16 | (uint32_t)c.eAlpha0 << 19 |
             (uint32_t)c.eAlpha1 << 22;
    }
    else if (u.eEnv == RD_ENV_REPLACE)
        k |= 1u << 1;
    return k;
}

struct GUProgram {
    GLuint   nProg;
    GLint    uMV, uP, uTexM[GU_UNITS];
    GLint    uNear, uDepthRange, uFogColor, uFog, uAlphaRef;
    uint32_t nMVSeen, nPSeen, TexSeen[GU_UNITS];
};

static std::unordered_map<GUKey, GUProgram, GUKeyHash> s_Programs;
static GUKey      s_LastKey;
static GUProgram* s_pLast;

static const char* gu_ArgName(int nArg, bool bAlpha, int nUnit)
{
    static char buf[GU_UNITS][3][24];
    char* p = buf[nUnit][nArg];
    switch (nArg)
    {
    case RD_ARG_TEXTURE: snprintf(p, 24, bAlpha ? "t.a" : "t.rgb"); break;
    case RD_ARG_PRIMARY: snprintf(p, 24, bAlpha ? "vColor.a" : "vColor.rgb"); break;
    default:             snprintf(p, 24, bAlpha ? "prev.a" : "prev.rgb"); break;
    }
    return p;
}

static void gu_AppendOp(std::string& s, int nOp, const char* a0, const char* a1)
{
    switch (nOp)
    {
    case RD_OP_REPLACE:    s += a0; break;
    case RD_OP_MODULATE:   s += a0; s += " * "; s += a1; break;
    case RD_OP_ADD:        s += a0; s += " + "; s += a1; break;
    default:               s += a0; s += " + "; s += a1; s += " - 0.5"; break;
    }
}

static void gu_MakeSource(const GUKey& k, std::string& vs, std::string& fs)
{
    bool bFog = (k.nGlobal & 2) != 0;
    int nDepth = (int)(k.nGlobal >> 2) & 3;
    bool bClip = (k.nGlobal & 16) != 0;
    char line[256];

    vs = "#version 300 es\n";
    if (bClip)
        vs += "#extension GL_EXT_clip_cull_distance : require\n";
    vs += "layout(location = 0) in vec3 aPos;\n"
          "layout(location = 1) in vec4 aColor;\n"
          "layout(location = 2) in vec4 aTex0;\n"
          "layout(location = 3) in vec2 aTex1;\n"
          "layout(location = 4) in vec2 aTex2;\n"
          "layout(location = 5) in vec2 aTex3;\n"
          "uniform mat4 uMV;\n"
          "uniform mat4 uP;\n"
          "uniform vec4 uNear;\n"
          "uniform vec2 uDepthRange;\n"
          "out vec4 vColor;\n"
          "out float vFogZ;\n";
    for (int u = 0; u < GU_UNITS; u++)
        if (k.Units[u])
        {
            snprintf(line, sizeof(line), "uniform mat4 uTexM%d;\nout vec4 vTex%d;\n", u, u);
            vs += line;
        }
    vs += "void main()\n{\n"
          "    vec4 eye = uMV * vec4(aPos, 1.0);\n"
          "    vec4 clip = uP * eye;\n";
    if (nDepth == GU_DEPTH_CLAMP)
        vs += "    clip.z = clamp(clip.z, -clip.w, clip.w);\n";
    else if (nDepth == GU_DEPTH_NEAR)
    {
        // Window depth computed here and drawn with glDepthRange(0, 1): by
        // 1/w from depth0 at the clip distance to depth1 at the near
        // plane, else the projection's own depth in the depth range. Both
        // are linear in 1/w, so z = (2 * depth - 1) * w interpolates
        // exactly. Beyond the far plane z > w still clips.
        vs += "    float w = clip.w;\n"
              "    float win;\n"
              "    if (clip.z < -w)\n"
              "    {\n"
              "        float g = (1.0 / uNear.x - 1.0 / w) / (1.0 / uNear.x - 1.0 / uNear.y);\n"
              "        win = mix(uNear.z, uNear.w, clamp(g, 0.0, 1.0));\n"
              "    }\n"
              "    else\n"
              "        win = mix(uDepthRange.x, uDepthRange.y, clip.z / w * 0.5 + 0.5);\n"
              "    clip.z = (2.0 * win - 1.0) * w;\n";
        if (bClip)
            vs += "    gl_ClipDistance[0] = w - uNear.x;\n";
    }
    vs += "    gl_Position = clip;\n"
          "    vColor = aColor;\n"
          "    vFogZ = abs(eye.z);\n";
    for (int u = 0; u < GU_UNITS; u++)
        if (k.Units[u])
        {
            if (u == 0)
                snprintf(line, sizeof(line), "    vTex0 = uTexM0 * aTex0;\n");
            else
                snprintf(line, sizeof(line), "    vTex%d = uTexM%d * vec4(aTex%d, 0.0, 1.0);\n", u, u, u);
            vs += line;
        }
    vs += "}\n";

    fs = "#version 300 es\n"
         "precision highp float;\n"
         "in vec4 vColor;\n"
         "in float vFogZ;\n"
         "uniform float uAlphaRef;\n"
         "uniform vec3 uFogColor;\n"
         "uniform vec2 uFog;\n"                 // end, 1 / (end - start); 0: no fog
         "out vec4 oColor;\n";
    for (int u = 0; u < GU_UNITS; u++)
        if (k.Units[u])
        {
            snprintf(line, sizeof(line), "uniform sampler2D uTex%d;\nin vec4 vTex%d;\n", u, u);
            fs += line;
        }
    fs += "void main()\n{\n"
          "    vec4 prev = vColor;\n"
          "    vec4 t;\n";
    for (int u = 0; u < GU_UNITS; u++)
    {
        uint32_t w = k.Units[u];
        if (!w)
            continue;                       // an off unit passes its input on
        snprintf(line, sizeof(line), "    t = texture(uTex%d, vTex%d.xy / vTex%d.w);\n", u, u, u);
        fs += line;
        int nMode = (int)(w >> 1) & 3;
        bool bAlpha = (w & 8) != 0;
        if (nMode == 0)
            fs += "    prev = t * prev;\n";
        else if (nMode == 1)
            fs += bAlpha ? "    prev = t;\n" : "    prev = vec4(t.rgb, prev.a);\n";
        else
        {
            int nCOp = (int)(w >> 4) & 7, nC0 = (int)(w >> 7) & 7, nC1 = (int)(w >> 10) & 7;
            int nScale = (int)(w >> 13) & 7;
            int nAOp = (int)(w >> 16) & 7, nA0 = (int)(w >> 19) & 7, nA1 = (int)(w >> 22) & 7;
            std::string c = "    {\n        vec3 c = (";
            gu_AppendOp(c, nCOp, gu_ArgName(nC0, false, u), gu_ArgName(nC1, false, u));
            c += nScale == 2 ? ") * 4.0;\n" : nScale == 1 ? ") * 2.0;\n" : ");\n";
            c += "        float a = ";
            gu_AppendOp(c, nAOp, gu_ArgName(nA0, true, u), gu_ArgName(nA1, true, u));
            c += ";\n        prev = clamp(vec4(c, a), 0.0, 1.0);\n    }\n";
            fs += c;
        }
    }
    if (bFog)
        fs += "    float f = uFog.y > 0.0 ? clamp((uFog.x - vFogZ) * uFog.y, 0.0, 1.0) : 1.0;\n"
              "    prev.rgb = mix(uFogColor, prev.rgb, f);\n";
    if (k.nGlobal & 1)
        fs += "    if (prev.a <= uAlphaRef)\n        discard;\n";
    fs += "    oColor = prev;\n}\n";
}

static GLuint gu_Compile(GLenum eType, const std::string& src)
{
    GLuint n = glCreateShader(eType);
    const char* p = src.c_str();
    glShaderSource(n, 1, &p, NULL);
    glCompileShader(n);
    GLint ok = 0;
    glGetShaderiv(n, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024] = "";
        glGetShaderInfoLog(n, sizeof(log), NULL, log);
        gu_Log("gles.ren: shader did not compile: %s", log);
        glDeleteShader(n);
        return 0;
    }
    return n;
}

static GUProgram* gu_Program(const GUKey& k)
{
    if (s_pLast && k == s_LastKey)
        return s_pLast;
    auto it = s_Programs.find(k);
    if (it == s_Programs.end())
    {
        std::string vs, fs;
        gu_MakeSource(k, vs, fs);
        GUProgram p;
        memset(&p, 0, sizeof(p));
        GLuint nVS = gu_Compile(GL_VERTEX_SHADER, vs);
        GLuint nFS = nVS ? gu_Compile(GL_FRAGMENT_SHADER, fs) : 0;
        if (nVS && nFS)
        {
            p.nProg = glCreateProgram();
            glAttachShader(p.nProg, nVS);
            glAttachShader(p.nProg, nFS);
            glLinkProgram(p.nProg);
            GLint ok = 0;
            glGetProgramiv(p.nProg, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = "";
                glGetProgramInfoLog(p.nProg, sizeof(log), NULL, log);
                gu_Log("gles.ren: program did not link: %s", log);
                glDeleteProgram(p.nProg);
                p.nProg = 0;
            }
        }
        if (nVS)
            glDeleteShader(nVS);
        if (nFS)
            glDeleteShader(nFS);
        if (p.nProg)
        {
            char name[16];
            glUseProgram(p.nProg);
            p.uMV = glGetUniformLocation(p.nProg, "uMV");
            p.uP = glGetUniformLocation(p.nProg, "uP");
            p.uNear = glGetUniformLocation(p.nProg, "uNear");
            p.uDepthRange = glGetUniformLocation(p.nProg, "uDepthRange");
            p.uFogColor = glGetUniformLocation(p.nProg, "uFogColor");
            p.uFog = glGetUniformLocation(p.nProg, "uFog");
            p.uAlphaRef = glGetUniformLocation(p.nProg, "uAlphaRef");
            for (int u = 0; u < GU_UNITS; u++)
            {
                snprintf(name, sizeof(name), "uTexM%d", u);
                p.uTexM[u] = glGetUniformLocation(p.nProg, name);
                snprintf(name, sizeof(name), "uTex%d", u);
                GLint nSampler = glGetUniformLocation(p.nProg, name);
                if (nSampler >= 0)
                    glUniform1i(nSampler, u);
            }
        }
        it = s_Programs.emplace(k, p).first;
    }
    s_LastKey = k;
    s_pLast = &it->second;
    return s_pLast;
}

static void gu_DestroyPrograms(void)
{
    for (auto& it : s_Programs)
        if (it.second.nProg)
            glDeleteProgram(it.second.nProg);
    s_Programs.clear();
    s_pLast = NULL;
}

// ---- applying the state ------------------------------------------------------------

static const GLenum s_BlendGL[] = {
    GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE
};
static const GLenum s_DepthGL[] = { GL_LESS, GL_LEQUAL, GL_ALWAYS };

static void gu_Toggle(GLenum eCap, bool bOn, bool& bApplied)
{
    if (s_A.bValid && bApplied == bOn)
        return;
    if (bOn)
        glEnable(eCap);
    else
        glDisable(eCap);
    bApplied = bOn;
}

// The GL state a clear needs: masks and the scissor.
static void gu_ApplyClearState(void)
{
    gu_Toggle(GL_SCISSOR_TEST, s_S.bScissor, s_A.bScissor);
    if (s_S.bScissor && (!s_A.bValid || memcmp(s_A.Scissor, s_S.Scissor, sizeof(s_A.Scissor))))
    {
        glScissor(s_S.Scissor[0], s_nHeight - s_S.Scissor[1] - s_S.Scissor[3], s_S.Scissor[2], s_S.Scissor[3]);
        memcpy(s_A.Scissor, s_S.Scissor, sizeof(s_A.Scissor));
    }
    if (!s_A.bValid || s_A.bColorMask != s_S.bColorMask)
    {
        GLboolean b = s_S.bColorMask ? GL_TRUE : GL_FALSE;
        glColorMask(b, b, b, b);
        s_A.bColorMask = s_S.bColorMask;
    }
    if (!s_A.bValid || s_A.bDepthMask != s_S.bDepthMask)
    {
        glDepthMask(s_S.bDepthMask ? GL_TRUE : GL_FALSE);
        s_A.bDepthMask = s_S.bDepthMask;
    }
}

static void gu_ApplyDepthRange(float fNear, float fFar)
{
    if (!s_A.bValid || s_A.DepthRange[0] != fNear || s_A.DepthRange[1] != fFar)
    {
        glDepthRangef(fNear, fFar);
        s_A.DepthRange[0] = fNear;
        s_A.DepthRange[1] = fFar;
    }
}

// Everything a draw needs; returns the program, or NULL when none could be made.
static GUProgram* gu_ApplyDrawState(void)
{
    gu_ApplyClearState();

    gu_Toggle(GL_BLEND, s_S.bBlend, s_A.bBlend);
    GLenum nSrc = s_BlendGL[s_S.eBlendSrc], nDst = s_BlendGL[s_S.eBlendDst];
    if (!s_A.bValid || s_A.nBlendSrc != nSrc || s_A.nBlendDst != nDst)
    {
        glBlendFunc(nSrc, nDst);
        s_A.nBlendSrc = nSrc;
        s_A.nBlendDst = nDst;
    }
    gu_Toggle(GL_DEPTH_TEST, s_S.bDepthTest, s_A.bDepthTest);
    GLenum nFunc = s_DepthGL[s_S.eDepthFunc];
    if (!s_A.bValid || s_A.nDepthFunc != nFunc)
    {
        glDepthFunc(nFunc);
        s_A.nDepthFunc = nFunc;
    }
    if (!s_A.bValid || s_A.bCull != s_S.bCull)
    {
        // Screen-clockwise is the front; the target is drawn the right way
        // up, so GL's winding is the screen's.
        if (s_S.bCull)
        {
            glCullFace(GL_BACK);
            glFrontFace(GL_CW);
            glEnable(GL_CULL_FACE);
        }
        else
            glDisable(GL_CULL_FACE);
        s_A.bCull = s_S.bCull;
    }
    if (!s_A.bValid || memcmp(s_A.Viewport, s_S.Viewport, sizeof(s_A.Viewport)))
    {
        glViewport(s_S.Viewport[0], s_nHeight - s_S.Viewport[1] - s_S.Viewport[3], s_S.Viewport[2],
                   s_S.Viewport[3]);
        memcpy(s_A.Viewport, s_S.Viewport, sizeof(s_A.Viewport));
    }

    int nDepth = GU_DEPTH_NORMAL;
    bool bHwClamp = false;
    if (s_S.bDepthClamp)
    {
        if (s_NearDepth[0] > 0.0f && s_NearDepth[1] > s_NearDepth[0])
            nDepth = GU_DEPTH_NEAR;
        else if (s_bHwDepthClamp)
            bHwClamp = true;
        else
            nDepth = GU_DEPTH_CLAMP;
    }
    if (s_bHwDepthClamp)
        gu_Toggle(GLX_DEPTH_CLAMP, bHwClamp, s_A.bHwClamp);
    bool bClip = nDepth == GU_DEPTH_NEAR && s_bClipDistance;
    if (s_bClipDistance)
        gu_Toggle(GLX_CLIP_DISTANCE0, bClip, s_A.bClip);
    if (nDepth == GU_DEPTH_NEAR)
        gu_ApplyDepthRange(0.0f, 1.0f);
    else
        gu_ApplyDepthRange(s_S.DepthRange[0], s_S.DepthRange[1]);
    s_A.bValid = true;

    GUKey k;
    for (int u = 0; u < GU_UNITS; u++)
        k.Units[u] = u < g_RDCaps.nTextureUnits ? gu_UnitKey(s_S.Units[u]) : 0;
    k.nGlobal = (s_S.bAlphaTest ? 1u : 0u) | (s_S.bFog ? 2u : 0u) | (uint32_t)nDepth << 2 | (bClip ? 16u : 0u);
    GUProgram* p = gu_Program(k);
    if (!p->nProg)
        return NULL;
    glUseProgram(p->nProg);

    if (p->nMVSeen != s_nMVSerial)
    {
        glUniformMatrix4fv(p->uMV, 1, GL_FALSE, s_MV[s_nMV].m);
        p->nMVSeen = s_nMVSerial;
    }
    if (p->nPSeen != s_nPSerial)
    {
        glUniformMatrix4fv(p->uP, 1, GL_FALSE, s_P[s_nP].m);
        p->nPSeen = s_nPSerial;
    }
    for (int u = 0; u < GU_UNITS; u++)
        if (p->uTexM[u] >= 0 && p->TexSeen[u] != s_TexSerial[u])
        {
            glUniformMatrix4fv(p->uTexM[u], 1, GL_FALSE, s_Tex[u][s_nTex[u]].m);
            p->TexSeen[u] = s_TexSerial[u];
        }
    if (nDepth == GU_DEPTH_NEAR)
    {
        glUniform4fv(p->uNear, 1, s_NearDepth);
        glUniform2fv(p->uDepthRange, 1, s_S.DepthRange);
    }
    if (s_S.bFog)
    {
        float fSpan = s_S.fFogEnd - s_S.fFogStart;
        glUniform3fv(p->uFogColor, 1, s_S.FogColor);
        glUniform2f(p->uFog, s_S.fFogEnd, fSpan != 0.0f ? 1.0f / fSpan : 0.0f);
    }
    if (s_S.bAlphaTest)
        glUniform1f(p->uAlphaRef, s_S.fAlphaRef);
    return p;
}

// ---- the device ----------------------------------------------------------------------

struct GUVertex {
    float   x, y, z;
    uint8_t c[4];
    float   t0[4];
    float   t[GU_UNITS - 1][2];
};

bool rd_CreateDevice(void* hWnd, uint32_t nWidth, uint32_t nHeight, bool bWindowed, int nSwapInterval,
                     int nSamples)
{
    (void)bWindowed;
    s_nWidth = (int)nWidth;
    s_nHeight = (int)nHeight;
    if (!gu_CreateContext(hWnd))
    {
        gu_DestroyContext();
        return false;
    }
    eglSwapInterval(s_eglDisplay, nSwapInterval);

    memset(&g_RDCaps, 0, sizeof(g_RDCaps));
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &g_RDCaps.nMaxTextureSize);
    GLint nUnits = 1;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &nUnits);
    g_RDCaps.nTextureUnits = nUnits < 1 ? 1 : nUnits > GU_UNITS ? GU_UNITS : nUnits;
    g_RDCaps.bCombine = true;
    // Adreno has no S3TC: the core decodes DXT textures to 32-bit then.
    g_RDCaps.bS3TC = gu_HasExt("GL_EXT_texture_compression_s3tc");
    g_RDCaps.bAniso = gu_HasExt("GL_EXT_texture_filter_anisotropic");
    g_RDCaps.fMaxAniso = 1.0f;
    if (g_RDCaps.bAniso)
        glGetFloatv(GLX_MAX_TEXTURE_MAX_ANISOTROPY, &g_RDCaps.fMaxAniso);
    // NOLF_GLES_NOEXT=1 draws as a driver without either would (tests).
    const char* pNoExt = getenv("NOLF_GLES_NOEXT");
    bool bExt = !pNoExt || !*pNoExt || *pNoExt == '0';
    s_bHwDepthClamp = bExt && gu_HasExt("GL_EXT_depth_clamp");
    s_bClipDistance = bExt && gu_HasExt("GL_EXT_clip_cull_distance");
    g_RDCaps.bDepthClamp = true;        // in the shader when the driver has none
    g_RDCaps.bNearDepth = true;

    if (!gu_CreateTargets(nSamples))
    {
        gu_DestroyContext();
        return false;
    }
    g_RDCaps.nSamples = s_nSamples;

    glGenVertexArrays(1, &s_nVAO);
    glBindVertexArray(s_nVAO);
    glGenBuffers(1, &s_nVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_nVBO);
    const GLsizei nStride = sizeof(GUVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, nStride, (void*)offsetof(GUVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, nStride, (void*)offsetof(GUVertex, c));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, nStride, (void*)offsetof(GUVertex, t0));
    for (int u = 1; u < GU_UNITS; u++)
    {
        glEnableVertexAttribArray(2 + u);
        glVertexAttribPointer(2 + u, 2, GL_FLOAT, GL_FALSE, nStride,
                              (void*)(offsetof(GUVertex, t) + (u - 1) * 2 * sizeof(float)));
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glDisable(GL_DITHER);

    gu_ResetState();
    gu_ResetMatrices();
    s_TexKind.clear();

    // The state every draw starts from: depth test LEQUAL, the rest off,
    // the whole target, cleared to black.
    glBindFramebuffer(GL_FRAMEBUFFER, s_nDrawFBO);
    gu_ApplyDrawState();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    gu_Log("gles.ren: %s, %dx%d, %dx antialiasing, depth clamp %s, clip distance %s", rd_Describe(), s_nWidth,
           s_nHeight, s_nSamples, s_bHwDepthClamp ? "hardware" : "shader", s_bClipDistance ? "yes" : "no");
    return true;
}

void rd_DestroyDevice(void)
{
    if (s_eglContext != EGL_NO_CONTEXT)
    {
        glFinish();
        gu_DestroyPrograms();
        glDeleteBuffers(1, &s_nVBO);
        glDeleteVertexArrays(1, &s_nVAO);
        s_nVBO = s_nVAO = 0;
        gu_DestroyTargets();
    }
    gu_DestroyContext();
    s_TexKind.clear();
}

// The front copy onto the window, scaled to fit with its proportions kept.
void rd_Present(void)
{
    if (s_eglContext == EGL_NO_CONTEXT)
        return;
    glDisable(GL_SCISSOR_TEST);
    s_A.bScissor = false;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_nDrawFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_nFrontFBO);
    glBlitFramebuffer(0, 0, s_nWidth, s_nHeight, 0, 0, s_nWidth, s_nHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EGLint nWinW = s_nWidth, nWinH = s_nHeight;
    eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_WIDTH, &nWinW);
    eglQuerySurface(s_eglDisplay, s_eglSurface, EGL_HEIGHT, &nWinH);
    int nW = nWinW, nH = (int)((int64_t)nWinW * s_nHeight / s_nWidth);
    if (nH > nWinH)
    {
        nH = nWinH;
        nW = (int)((int64_t)nWinH * s_nWidth / s_nHeight);
    }
    int x = (nWinW - nW) / 2, y = (nWinH - nH) / 2;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_nFrontFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    s_A.bColorMask = true;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, s_nWidth, s_nHeight, x, y, x + nW, y + nH, GL_COLOR_BUFFER_BIT,
                      nW == s_nWidth && nH == s_nHeight ? GL_NEAREST : GL_LINEAR);
    eglSwapBuffers(s_eglDisplay, s_eglSurface);

    glBindFramebuffer(GL_FRAMEBUFFER, s_nDrawFBO);
    glClearColor(s_S.ClearColor[0], s_S.ClearColor[1], s_S.ClearColor[2], s_S.ClearColor[3]);
}

const char* rd_Describe(void)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%s (%s)", (const char*)glGetString(GL_RENDERER),
             (const char*)glGetString(GL_VERSION));
    return buf;
}

void rd_PrintInfo(void (*pPrint)(const char* pFormat, ...))
{
    pPrint("GL_VENDOR: %s", (const char*)glGetString(GL_VENDOR));
    pPrint("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));
    pPrint("GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
    pPrint("Target: %dx%d, %dx antialiasing", s_nWidth, s_nHeight, s_nSamples);
    pPrint("Depth clamp: %s; clip distance: %s; S3TC: %s; programs made: %d",
           s_bHwDepthClamp ? "EXT_depth_clamp" : "in the shader", s_bClipDistance ? "yes" : "no",
           g_RDCaps.bS3TC ? "yes" : "no (decoded)", (int)s_Programs.size());
}

// ---- state ------------------------------------------------------------------------------

static void gu_SetCap(RDCap eCap, bool bOn)
{
    switch (eCap)
    {
    case RD_BLEND:       s_S.bBlend = bOn; break;
    case RD_ALPHA_TEST:  s_S.bAlphaTest = bOn; break;
    case RD_DEPTH_TEST:  s_S.bDepthTest = bOn; break;
    case RD_FOG:         s_S.bFog = bOn; break;
    case RD_SCISSOR_TEST: s_S.bScissor = bOn; break;
    case RD_TEXTURE_2D:  s_S.Units[s_S.nActive].bEnabled = bOn; break;
    case RD_DEPTH_CLAMP: s_S.bDepthClamp = bOn; break;
    default: break;
    }
}

void rd_Enable(RDCap eCap)  { gu_SetCap(eCap, true); }
void rd_Disable(RDCap eCap) { gu_SetCap(eCap, false); }
void rd_CullBack(bool bOn)  { s_S.bCull = bOn; }

void rd_BlendFunc(RDBlend eSrc, RDBlend eDst)
{
    s_S.eBlendSrc = eSrc;
    s_S.eBlendDst = eDst;
}

void rd_AlphaGreater(float fRef)     { s_S.fAlphaRef = fRef; }
void rd_DepthFunc(RDDepthFunc eFunc) { s_S.eDepthFunc = eFunc; }
void rd_DepthMask(bool bWrite)       { s_S.bDepthMask = bWrite; }
void rd_ColorMask(bool bWrite)       { s_S.bColorMask = bWrite; }

void rd_FogColor(float r, float g, float b)
{
    s_S.FogColor[0] = r;
    s_S.FogColor[1] = g;
    s_S.FogColor[2] = b;
}

void rd_FogRange(float fStart, float fEnd)
{
    s_S.fFogStart = fStart;
    s_S.fFogEnd = fEnd;
}

void rd_Viewport(int x, int y, int w, int h)
{
    s_S.Viewport[0] = x;
    s_S.Viewport[1] = y;
    s_S.Viewport[2] = w;
    s_S.Viewport[3] = h;
}

void rd_DepthRange(float fNear, float fFar)
{
    s_S.DepthRange[0] = fNear;
    s_S.DepthRange[1] = fFar;
}

void rd_NearDepth(float fClip, float fNear, float fDepth0, float fDepth1)
{
    s_NearDepth[0] = fClip;
    s_NearDepth[1] = fNear;
    s_NearDepth[2] = fDepth0;
    s_NearDepth[3] = fDepth1;
}

void rd_Scissor(int x, int y, int w, int h)
{
    s_S.Scissor[0] = x;
    s_S.Scissor[1] = y;
    s_S.Scissor[2] = w;
    s_S.Scissor[3] = h;
}

// Each unit's binding back into GL after a pop changed them.
static void gu_RebindUnits(void)
{
    for (int u = 0; u < GU_UNITS; u++)
    {
        gu_GLActive(u);
        glBindTexture(GL_TEXTURE_2D, s_S.Units[u].nTex);
    }
    gu_GLActive(s_S.nActive);
}

void rd_PushAttrib(uint32_t nMask)
{
    GUAttribFrame f;
    f.nMask = nMask;
    f.S = s_S;
    s_AttribStack.push_back(f);
}

void rd_PopAttrib(void)
{
    if (s_AttribStack.empty())
        return;
    const GUAttribFrame& f = s_AttribStack.back();
    const GUState& o = f.S;
    uint32_t m = f.nMask;
    if (m & RD_ATTR_COLOR)
    {
        s_S.bBlend = o.bBlend;
        s_S.eBlendSrc = o.eBlendSrc;
        s_S.eBlendDst = o.eBlendDst;
        s_S.bAlphaTest = o.bAlphaTest;
        s_S.fAlphaRef = o.fAlphaRef;
        s_S.bColorMask = o.bColorMask;
        memcpy(s_S.ClearColor, o.ClearColor, sizeof(s_S.ClearColor));
        glClearColor(s_S.ClearColor[0], s_S.ClearColor[1], s_S.ClearColor[2], s_S.ClearColor[3]);
    }
    if (m & RD_ATTR_DEPTH)
    {
        s_S.bDepthTest = o.bDepthTest;
        s_S.eDepthFunc = o.eDepthFunc;
        s_S.bDepthMask = o.bDepthMask;
    }
    if (m & RD_ATTR_FOG)
    {
        s_S.bFog = o.bFog;
        memcpy(s_S.FogColor, o.FogColor, sizeof(s_S.FogColor));
        s_S.fFogStart = o.fFogStart;
        s_S.fFogEnd = o.fFogEnd;
    }
    if (m & RD_ATTR_ENABLE)
    {
        s_S.bBlend = o.bBlend;
        s_S.bAlphaTest = o.bAlphaTest;
        s_S.bDepthTest = o.bDepthTest;
        s_S.bFog = o.bFog;
        s_S.bScissor = o.bScissor;
        s_S.bDepthClamp = o.bDepthClamp;
        s_S.bCull = o.bCull;
        for (int u = 0; u < GU_UNITS; u++)
            s_S.Units[u].bEnabled = o.Units[u].bEnabled;
    }
    if (m & RD_ATTR_TEXTURE)
    {
        s_S.nActive = o.nActive;
        memcpy(s_S.Units, o.Units, sizeof(s_S.Units));
        gu_RebindUnits();
    }
    if (m & RD_ATTR_SCISSOR)
    {
        s_S.bScissor = o.bScissor;
        memcpy(s_S.Scissor, o.Scissor, sizeof(s_S.Scissor));
    }
    if (m & RD_ATTR_VIEWPORT)
    {
        memcpy(s_S.Viewport, o.Viewport, sizeof(s_S.Viewport));
        memcpy(s_S.DepthRange, o.DepthRange, sizeof(s_S.DepthRange));
    }
    s_AttribStack.pop_back();
}

// ---- texture units ------------------------------------------------------------------------

void rd_ActiveTexture(int nUnit)
{
    if (nUnit < 0 || nUnit >= GU_UNITS)
        return;
    s_S.nActive = nUnit;
    gu_GLActive(nUnit);
}

void rd_TexEnv(RDTexEnv eEnv)
{
    GUUnit& u = s_S.Units[s_S.nActive];
    u.bCombine = false;
    u.eEnv = eEnv;
}

void rd_TexCombine(const RDCombine& c)
{
    GUUnit& u = s_S.Units[s_S.nActive];
    u.bCombine = true;
    u.Combine = c;
}

// ---- textures --------------------------------------------------------------------------------

uint32_t rd_GenTexture(void)
{
    GLuint n = 0;
    glGenTextures(1, &n);
    return n;
}

void rd_DeleteTexture(uint32_t nTex)
{
    GLuint n = nTex;
    glDeleteTextures(1, &n);
    gu_SetKind(nTex, GK_NONE);
    for (int u = 0; u < GU_UNITS; u++)
        if (s_S.Units[u].nTex == nTex)
            s_S.Units[u].nTex = 0;      // GL unbinds a deleted texture
}

void rd_BindTexture(uint32_t nTex)
{
    s_S.Units[s_S.nActive].nTex = nTex;
    gu_GLActive(s_S.nActive);
    glBindTexture(GL_TEXTURE_2D, nTex);
}

static std::vector<uint8_t> s_Convert;

// rdev.h's pixels into what the texture holds: RGBA or RGB bytes, or 5:6:5
// words. Returns the data to upload, tightly packed.
static const void* gu_ConvertPixels(RDPixels eSrc, const void* pData, int w, int h, int nRowPixels,
                                    GUKind eKind, GLenum* pFormat, GLenum* pType)
{
    int nRow = nRowPixels ? nRowPixels : w;
    switch (eKind)
    {
    case GK_RGB8:   *pFormat = GL_RGB;  *pType = GL_UNSIGNED_BYTE; break;
    case GK_RGB565: *pFormat = GL_RGB;  *pType = GL_UNSIGNED_SHORT_5_6_5; break;
    default:        *pFormat = GL_RGBA; *pType = GL_UNSIGNED_BYTE; break;
    }
    if (!pData)
        return NULL;
    // Already in the right form and tightly packed.
    if (nRow == w && ((eSrc == RD_PIX_RGBA && eKind == GK_RGBA8) || (eSrc == RD_PIX_RGB && eKind == GK_RGB8) ||
                      (eSrc == RD_PIX_565 && eKind == GK_RGB565)))
        return pData;

    int nSrcBytes = eSrc == RD_PIX_RGB ? 3 : eSrc == RD_PIX_565 ? 2 : 4;
    int nDstBytes = eKind == GK_RGB8 ? 3 : eKind == GK_RGB565 ? 2 : 4;
    s_Convert.resize((size_t)w * h * nDstBytes);
    uint8_t* pOut = s_Convert.data();
    for (int y = 0; y < h; y++)
    {
        const uint8_t* pIn = (const uint8_t*)pData + (size_t)y * nRow * nSrcBytes;
        for (int x = 0; x < w; x++)
        {
            uint8_t r, g, b, a = 255;
            switch (eSrc)
            {
            case RD_PIX_BGRA:
            {
                uint32_t v;
                memcpy(&v, pIn + x * 4, 4);
                a = (uint8_t)(v >> 24);
                r = (uint8_t)(v >> 16);
                g = (uint8_t)(v >> 8);
                b = (uint8_t)v;
                break;
            }
            case RD_PIX_RGBA:
                r = pIn[x * 4];
                g = pIn[x * 4 + 1];
                b = pIn[x * 4 + 2];
                a = pIn[x * 4 + 3];
                break;
            case RD_PIX_RGB:
                r = pIn[x * 3];
                g = pIn[x * 3 + 1];
                b = pIn[x * 3 + 2];
                break;
            default:
            {
                uint16_t v;
                memcpy(&v, pIn + x * 2, 2);
                uint8_t r5 = (uint8_t)(v >> 11), g6 = (uint8_t)((v >> 5) & 63), b5 = (uint8_t)(v & 31);
                r = (uint8_t)(r5 << 3 | r5 >> 2);
                g = (uint8_t)(g6 << 2 | g6 >> 4);
                b = (uint8_t)(b5 << 3 | b5 >> 2);
                break;
            }
            }
            if (eKind == GK_RGB565)
            {
                uint16_t v = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
                memcpy(pOut, &v, 2);
                pOut += 2;
            }
            else
            {
                *pOut++ = r;
                *pOut++ = g;
                *pOut++ = b;
                if (eKind != GK_RGB8)
                    *pOut++ = a;
            }
        }
    }
    return s_Convert.data();
}

void rd_TexImage(int nLevel, RDFormat eFormat, int w, int h, RDPixels ePixels, const void* pData, int nRowPixels)
{
    GUKind eKind = GK_RGBA8;
    GLint nInternal = GL_RGBA8;
    if (eFormat == RD_FMT_RGB8)
    {
        eKind = GK_RGB8;
        nInternal = GL_RGB8;
    }
    else if (eFormat == RD_FMT_RGB)
    {
        // The 16-bit screen copy: kept 5:6:5 when it comes that way.
        eKind = ePixels == RD_PIX_565 ? GK_RGB565 : GK_RGB8;
        nInternal = eKind == GK_RGB565 ? GL_RGB565 : GL_RGB8;
    }
    GLenum nFormat, nType;
    const void* p = gu_ConvertPixels(ePixels, pData, w, h, nRowPixels, eKind, &nFormat, &nType);
    gu_GLActive(s_S.nActive);
    glTexImage2D(GL_TEXTURE_2D, nLevel, nInternal, w, h, 0, nFormat, nType, p);
    if (nLevel == 0)
    {
        gu_SetKind(s_S.Units[s_S.nActive].nTex, eKind);
        s_pLast = NULL;                 // a unit's alpha may have changed
    }
}

void rd_TexSubImage(int nLevel, int x, int y, int w, int h, RDPixels ePixels, const void* pData)
{
    GUKind eKind = gu_KindOf(s_S.Units[s_S.nActive].nTex);
    if (eKind == GK_DXT_ALPHA || eKind == GK_DXT_OPAQUE)
        return;                         // compressed: no partial uploads
    if (eKind == GK_NONE)
        eKind = GK_RGBA8;
    GLenum nFormat, nType;
    const void* p = gu_ConvertPixels(ePixels, pData, w, h, 0, eKind, &nFormat, &nType);
    gu_GLActive(s_S.nActive);
    glTexSubImage2D(GL_TEXTURE_2D, nLevel, x, y, w, h, nFormat, nType, p);
}

void rd_CompressedTexImage(int nLevel, RDFormat eFormat, int w, int h, uint32_t nBytes, const void* pData)
{
    if (!g_RDCaps.bS3TC)
    {
        static bool s_bWarned;
        if (!s_bWarned)
            gu_Log("gles.ren: a DXT texture without S3TC support (the core should have decoded it)");
        s_bWarned = true;
        return;
    }
    GLenum nFormat = eFormat == RD_FMT_DXT1  ? GLX_COMPRESSED_RGB_S3TC_DXT1 :
                     eFormat == RD_FMT_DXT1A ? GLX_COMPRESSED_RGBA_S3TC_DXT1 :
                     eFormat == RD_FMT_DXT3  ? GLX_COMPRESSED_RGBA_S3TC_DXT3 :
                                               GLX_COMPRESSED_RGBA_S3TC_DXT5;
    gu_GLActive(s_S.nActive);
    glCompressedTexImage2D(GL_TEXTURE_2D, nLevel, nFormat, w, h, 0, (GLsizei)nBytes, pData);
    if (nLevel == 0)
    {
        gu_SetKind(s_S.Units[s_S.nActive].nTex, eFormat == RD_FMT_DXT1 ? GK_DXT_OPAQUE : GK_DXT_ALPHA);
        s_pLast = NULL;
    }
}

void rd_TexFilter(RDFilter eMin, RDFilter eMag)
{
    static const GLint filter[] = {
        GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
        GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
    };
    gu_GLActive(s_S.nActive);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter[eMin]);
    // Magnification has no mipmaps: GL takes only NEAREST or LINEAR.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, eMag == RD_NEAREST ? GL_NEAREST : GL_LINEAR);
}

void rd_TexWrap(bool bRepeatS, bool bRepeatT)
{
    gu_GLActive(s_S.nActive);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, bRepeatS ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, bRepeatT ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

void rd_TexMaxLevel(int nLevel)
{
    gu_GLActive(s_S.nActive);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nLevel);
}

void rd_TexAniso(float fAniso)
{
    if (!g_RDCaps.bAniso)
        return;
    gu_GLActive(s_S.nActive);
    glTexParameterf(GL_TEXTURE_2D, GLX_TEXTURE_MAX_ANISOTROPY, fAniso);
}

// ---- matrices ---------------------------------------------------------------------------------

void rd_MatrixMode(RDMatrixMode eMode) { s_eMatMode = eMode; }

void rd_LoadIdentity(void)
{
    gu_Identity(gu_Top());
    gu_Touched();
}

void rd_LoadMatrix(const float* m)
{
    memcpy(gu_Top().m, m, sizeof(float) * 16);
    gu_Touched();
}

void rd_MultMatrix(const float* m)
{
    gu_Mul(gu_Top(), m);
    gu_Touched();
}

void rd_PushMatrix(void)
{
    switch (s_eMatMode)
    {
    case RD_PROJECTION:
        if (s_nP + 1 < GU_P_DEPTH)
        {
            s_P[s_nP + 1] = s_P[s_nP];
            s_nP++;
        }
        break;
    case RD_TEXTURE:
    {
        int u = s_S.nActive;
        if (s_nTex[u] + 1 < GU_TEX_DEPTH)
        {
            s_Tex[u][s_nTex[u] + 1] = s_Tex[u][s_nTex[u]];
            s_nTex[u]++;
        }
        break;
    }
    default:
        if (s_nMV + 1 < GU_MV_DEPTH)
        {
            s_MV[s_nMV + 1] = s_MV[s_nMV];
            s_nMV++;
        }
        break;
    }
}

void rd_PopMatrix(void)
{
    switch (s_eMatMode)
    {
    case RD_PROJECTION: if (s_nP > 0) s_nP--; break;
    case RD_TEXTURE:    if (s_nTex[s_S.nActive] > 0) s_nTex[s_S.nActive]--; break;
    default:            if (s_nMV > 0) s_nMV--; break;
    }
    gu_Touched();
}

void rd_Ortho(double l, double r, double b, double t, double n, double f)
{
    float m[16];
    memset(m, 0, sizeof(m));
    m[0] = (float)(2.0 / (r - l));
    m[5] = (float)(2.0 / (t - b));
    m[10] = (float)(-2.0 / (f - n));
    m[12] = (float)(-(r + l) / (r - l));
    m[13] = (float)(-(t + b) / (t - b));
    m[14] = (float)(-(f + n) / (f - n));
    m[15] = 1.0f;
    rd_MultMatrix(m);
}

// ---- drawing ------------------------------------------------------------------------------------

static std::vector<GUVertex> s_Verts, s_Tris;
static RDPrim   s_ePrim;
static GUVertex s_Cur = { 0, 0, 0, { 255, 255, 255, 255 }, { 0, 0, 0, 1 }, { { 0, 0 }, { 0, 0 }, { 0, 0 } } };

void rd_Begin(RDPrim ePrim)
{
    s_ePrim = ePrim;
    s_Verts.clear();
}

void rd_End(void)
{
    if (s_Verts.empty())
        return;
    GLenum eMode;
    const std::vector<GUVertex>* pVerts = &s_Verts;
    size_t n = s_Verts.size();
    switch (s_ePrim)
    {
    case RD_TRIANGLE_FAN:
        eMode = GL_TRIANGLE_FAN;
        break;
    case RD_LINES:
        eMode = GL_LINES;
        n &= ~(size_t)1;
        break;
    case RD_QUADS:
        // Each quad a, b, c, d as triangles a, b, c and a, c, d.
        eMode = GL_TRIANGLES;
        s_Tris.clear();
        for (size_t i = 0; i + 3 < n; i += 4)
        {
            const GUVertex* q = &s_Verts[i];
            s_Tris.push_back(q[0]);
            s_Tris.push_back(q[1]);
            s_Tris.push_back(q[2]);
            s_Tris.push_back(q[0]);
            s_Tris.push_back(q[2]);
            s_Tris.push_back(q[3]);
        }
        pVerts = &s_Tris;
        n = s_Tris.size();
        break;
    default:
        eMode = GL_TRIANGLES;
        n -= n % 3;
        break;
    }
    if (!n || s_eglContext == EGL_NO_CONTEXT)
        return;
    if (!gu_ApplyDrawState())
        return;
    // Orphaned each time: the driver hands out fresh memory rather than
    // waiting for the previous draw to finish with it.
    glBindBuffer(GL_ARRAY_BUFFER, s_nVBO);
    GLsizeiptr nBytes = (GLsizeiptr)(n * sizeof(GUVertex));
    glBufferData(GL_ARRAY_BUFFER, nBytes, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nBytes, pVerts->data());
    glDrawArrays(eMode, 0, (GLsizei)n);
}

static uint8_t gu_Byte(float f)
{
    return f <= 0.0f ? 0 : f >= 1.0f ? 255 : (uint8_t)(f * 255.0f + 0.5f);
}

void rd_Color4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    s_Cur.c[0] = r;
    s_Cur.c[1] = g;
    s_Cur.c[2] = b;
    s_Cur.c[3] = a;
}

void rd_Color4f(float r, float g, float b, float a)
{
    rd_Color4ub(gu_Byte(r), gu_Byte(g), gu_Byte(b), gu_Byte(a));
}

void rd_TexCoord2f(float s, float t) { rd_TexCoord4f(s, t, 0.0f, 1.0f); }

void rd_TexCoord4f(float s, float t, float r, float q)
{
    s_Cur.t0[0] = s;
    s_Cur.t0[1] = t;
    s_Cur.t0[2] = r;
    s_Cur.t0[3] = q;
}

void rd_MultiTexCoord2f(int nUnit, float s, float t)
{
    if (nUnit == 0)
        rd_TexCoord2f(s, t);
    else if (nUnit > 0 && nUnit < GU_UNITS)
    {
        s_Cur.t[nUnit - 1][0] = s;
        s_Cur.t[nUnit - 1][1] = t;
    }
}

void rd_Vertex2f(float x, float y) { rd_Vertex3f(x, y, 0.0f); }

void rd_Vertex3f(float x, float y, float z)
{
    s_Cur.x = x;
    s_Cur.y = y;
    s_Cur.z = z;
    s_Verts.push_back(s_Cur);
}

// ---- the frame buffer -----------------------------------------------------------------------------

void rd_ClearColor(float r, float g, float b, float a)
{
    s_S.ClearColor[0] = r;
    s_S.ClearColor[1] = g;
    s_S.ClearColor[2] = b;
    s_S.ClearColor[3] = a;
    glClearColor(r, g, b, a);
}

void rd_Clear(uint32_t nWhat)
{
    if (s_eglContext == EGL_NO_CONTEXT)
        return;
    gu_ApplyClearState();
    s_A.bValid = true;
    GLbitfield bits = 0;
    if (nWhat & RD_CLEAR_COLOR)
        bits |= GL_COLOR_BUFFER_BIT;
    if (nWhat & RD_CLEAR_DEPTH)
        bits |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    glClear(bits);
}

// Read as RGBA bytes (what every GLES can read), bottom-up; turned into
// rdev.h's top-down BGRA words or 5:6:5 here.
void rd_ReadPixels(int x, int y, int w, int h, RDPixels ePixels, void* pDest, bool bFront)
{
    if (s_eglContext == EGL_NO_CONTEXT || w <= 0 || h <= 0)
        return;
    GLuint nSource = s_nDrawFBO;
    if (bFront)
        nSource = s_nFrontFBO;
    else if (s_nSamples > 1)
    {
        // A multisampled target cannot be read: resolve it into a scratch
        // copy (not the front one, which holds the last presented frame).
        if (!s_nReadFBO)
            s_nReadFBO = gu_ColorTarget(&s_nReadColor, 1);
        glDisable(GL_SCISSOR_TEST);
        s_A.bScissor = false;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_nDrawFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_nReadFBO);
        glBlitFramebuffer(0, 0, s_nWidth, s_nHeight, 0, 0, s_nWidth, s_nHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        nSource = s_nReadFBO;
    }
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, nSource);
    glReadPixels(x, s_nHeight - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindFramebuffer(GL_FRAMEBUFFER, s_nDrawFBO);

    for (int row = 0; row < h; row++)
    {
        const uint8_t* pIn = rgba.data() + (size_t)(h - 1 - row) * w * 4;
        if (ePixels == RD_PIX_565)
        {
            uint16_t* pOut = (uint16_t*)pDest + (size_t)row * w;
            for (int i = 0; i < w; i++, pIn += 4)
                pOut[i] = (uint16_t)((pIn[0] >> 3) << 11 | (pIn[1] >> 2) << 5 | (pIn[2] >> 3));
        }
        else
        {
            uint32_t* pOut = (uint32_t*)pDest + (size_t)row * w;
            for (int i = 0; i < w; i++, pIn += 4)
                pOut[i] = (uint32_t)pIn[3] << 24 | (uint32_t)pIn[0] << 16 | (uint32_t)pIn[1] << 8 | pIn[2];
        }
    }
}

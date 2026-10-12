// gl_texture.cpp -- the engine's textures (DTX) as GL textures.
//
// rgl_BindTexture in the Mac renderer and d3d.ren's 0x10039fe0 agree: when
// the engine asks, fetch its copy of the data (callback +0x08), upload it,
// keep our object in SharedTexture::m_pRenderData, and give the engine's
// copy back (callback +0x0c) so it is not kept twice. Which mip levels go up
// follows r_SetupTextureSurface: MipmapOffset, the texture group's offset
// and the DTX's own, clamped to what the file has.
//
// 32-bit DTX pixels are ARGB 8888 little-endian words, i.e. B,G,R,A bytes;
// S3TC levels are uploaded compressed when the GL has the extension (the
// Mac renderer never does; d3d.ren does, under S3TCEnable) and decoded here
// otherwise.

#include <math.h>

#include "glr.h"

// ---- S3TC decoding, for a GL without the extension --------------------------------

static void glr_Rgb565(uint16_t c, uint8_t* rgb)
{
    uint32_t r = (c >> 11) & 0x1f, gg = (c >> 5) & 0x3f, b = c & 0x1f;
    rgb[0] = (uint8_t)(r << 3 | r >> 2);
    rgb[1] = (uint8_t)(gg << 2 | gg >> 4);
    rgb[2] = (uint8_t)(b << 3 | b >> 2);
}

// One 4x4 block's colour part into 16 BGRA pixels (alpha set separately
// unless DXT1, where colour 3 may be transparent).
static void glr_DecodeColorBlock(const uint8_t* p, uint8_t out[16][4], bool bDXT1)
{
    uint16_t c0 = p[0] | p[1] << 8, c1 = p[2] | p[3] << 8;
    uint8_t col[4][4];
    glr_Rgb565(c0, col[0]);
    glr_Rgb565(c1, col[1]);
    col[0][3] = col[1][3] = 255;
    if (c0 > c1 || !bDXT1)
    {
        for (int i = 0; i < 3; i++)
        {
            col[2][i] = (uint8_t)((2 * col[0][i] + col[1][i]) / 3);
            col[3][i] = (uint8_t)((col[0][i] + 2 * col[1][i]) / 3);
        }
        col[2][3] = col[3][3] = 255;
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            col[2][i] = (uint8_t)((col[0][i] + col[1][i]) / 2);
            col[3][i] = 0;
        }
        col[2][3] = 255;
        col[3][3] = 0;
    }
    uint32_t bits = p[4] | p[5] << 8 | p[6] << 16 | (uint32_t)p[7] << 24;
    for (int i = 0; i < 16; i++)
    {
        const uint8_t* c = col[(bits >> (2 * i)) & 3];
        out[i][0] = c[2];               // B
        out[i][1] = c[1];               // G
        out[i][2] = c[0];               // R
        out[i][3] = c[3];               // A
    }
}

static void glr_DecodeS3TC(const uint8_t* pData, uint32_t w, uint32_t h, int nType, uint8_t* pOut)
{
    uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    uint32_t nBlock = nType == BPP_S3TC_DXT1 ? 8 : 16;
    for (uint32_t by = 0; by < bh; by++)
    {
        for (uint32_t bx = 0; bx < bw; bx++)
        {
            const uint8_t* p = pData + (by * bw + bx) * nBlock;
            uint8_t px[16][4];
            glr_DecodeColorBlock(nType == BPP_S3TC_DXT1 ? p : p + 8, px, nType == BPP_S3TC_DXT1);
            if (nType == BPP_S3TC_DXT3)
            {
                for (int i = 0; i < 16; i++)
                {
                    uint8_t a = (p[i / 2] >> ((i & 1) * 4)) & 0xf;
                    px[i][3] = (uint8_t)(a << 4 | a);
                }
            }
            else if (nType == BPP_S3TC_DXT5)
            {
                uint8_t a[8];
                a[0] = p[0];
                a[1] = p[1];
                if (a[0] > a[1])
                    for (int i = 1; i < 7; i++)
                        a[i + 1] = (uint8_t)(((7 - i) * a[0] + i * a[1]) / 7);
                else
                {
                    for (int i = 1; i < 5; i++)
                        a[i + 1] = (uint8_t)(((5 - i) * a[0] + i * a[1]) / 5);
                    a[6] = 0;
                    a[7] = 255;
                }
                uint64_t bits = 0;
                for (int i = 0; i < 6; i++)
                    bits |= (uint64_t)p[2 + i] << (8 * i);
                for (int i = 0; i < 16; i++)
                    px[i][3] = a[(bits >> (3 * i)) & 7];
            }
            for (int y = 0; y < 4; y++)
            {
                uint32_t py = by * 4 + y;
                if (py >= h)
                    break;
                for (int x = 0; x < 4; x++)
                {
                    uint32_t pxx = bx * 4 + x;
                    if (pxx < w)
                        memcpy(pOut + (py * w + pxx) * 4, px[y * 4 + x], 4);
                }
            }
        }
    }
}

// ---- upload ---------------------------------------------------------------------

static uint32_t glr_S3TCSize(uint32_t w, uint32_t h, int nType)
{
    uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    return bw * bh * (nType == BPP_S3TC_DXT1 ? 8 : 16);
}

static void glr_ApplyTextureFilter(int nLevels)
{
    bool bLinear = cvi(CV_Bilinear) != 0;
    GLint mag = bLinear ? GL_LINEAR : GL_NEAREST;
    GLint min = mag;
    if (nLevels > 1)
    {
        if (cvi(CV_Trilinear))
            min = bLinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
        else
            min = bLinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    if (g.bAniso)
    {
        float a = cvf(CV_Anisotropic);
        if (a < 1.0f) a = 1.0f;
        if (a > g.fMaxAniso) a = g.fMaxAniso;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, a);
    }
}

static bool glr_Upload(GLRTexture* pTex, TextureData* pData)
{
    const DtxHeader& hdr = pData->m_Header;
    int nType = hdr.m_Extra[2];
    if (nType == 0)
        nType = BPP_32;                 // 0 and 3 both mean 32-bit
    if (nType != BPP_32 && nType != BPP_S3TC_DXT1 && nType != BPP_S3TC_DXT3 && nType != BPP_S3TC_DXT5)
        return false;                   // palettised / 8-bit: no renderer has a path

    int nMips = hdr.m_nMipmaps;
    if (nMips > MAX_DTX_MIPS)
        nMips = MAX_DTX_MIPS;
    if (nMips < 1)
        return false;

    // The first level drawn (r_SetupTextureSurface): the global offset, the
    // group's and the file's own, kept within the chain and the size limit.
    int nFirst = cvi(CV_MipmapOffset) + hdr.m_Extra[3];
    int nGroup = hdr.m_Extra[0];
    if (nGroup >= 0 && nGroup <= 9)
        nFirst += cvi(CV_GroupOffset0 + nGroup);
    if (nFirst < 0)
        nFirst = 0;
    if (nFirst > nMips - 1)
        nFirst = nMips - 1;
    GLint nMax = g.nMaxTextureSize;
    if (cvi(CV_MaxTextureSize) > 0 && cvi(CV_MaxTextureSize) < nMax)
        nMax = cvi(CV_MaxTextureSize);
    while (nFirst < nMips - 1 &&
           ((GLint)pData->m_Mips[nFirst].m_Width > nMax || (GLint)pData->m_Mips[nFirst].m_Height > nMax))
        nFirst++;

    // Which textures keep their alpha (d3d.ren 0x10038a90, Mac
    // r_SetupTextureSurface 0x180628): those the DTX flags give an alpha
    // format -- 4444 (0x80), 5551 (0x100), and fullbright ones, whose alpha
    // marks the fullbright texels -- and with "32BitTextures" all but those
    // that prefer 16 bits (0x2). Every other texture is 565 there, opaque
    // whatever its file's alpha says. Here the alpha is kept at full
    // precision, as Direct3D under Wine also ends up doing: a 1-bit alpha
    // would cut a soft gradient (the menus' bars) down to nothing.
    uint32_t f = hdr.m_IFlags;
    GLint nInternal;
    if ((cvi(CV_32BitTextures) && !(f & 2)) || (f & (0x100 | 0x80 | DTX_FULLBRITE)))
        nInternal = GL_RGBA8;
    else
        nInternal = GL_RGB8;
    bool bCompressed = nType != BPP_32;
    // A compressed texture goes up compressed when its format carries no
    // alpha it should not have: DXT1 as RGB, or anything keeping its alpha.
    bool bUploadCompressed = bCompressed && g.bS3TC && cvi(CV_S3TCEnable) &&
                             (nInternal != GL_RGB8 || nType == BPP_S3TC_DXT1);

    glBindTexture(GL_TEXTURE_2D, pTex->m_Id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    int nLevels = 0;
    uint32_t nBytes = 0;
    uint8_t* pDecode = NULL;
    for (int i = nFirst; i < nMips; i++)
    {
        const TextureMip& mip = pData->m_Mips[i];
        if (!mip.m_pData || !mip.m_Width || !mip.m_Height)
            break;
        if (bUploadCompressed)
        {
            static const GLenum fmt[3] = { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                           GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
                                           GL_COMPRESSED_RGBA_S3TC_DXT5_EXT };
            GLenum nFormat = fmt[nType - BPP_S3TC_DXT1];
            if (nInternal == GL_RGB8)
                nFormat = 0x83F0;       // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
            uint32_t n = glr_S3TCSize(mip.m_Width, mip.m_Height, nType);
            g.glCompressedTexImage2D(GL_TEXTURE_2D, nLevels, nFormat,
                                     mip.m_Width, mip.m_Height, 0, n, mip.m_pData);
            nBytes += n;
        }
        else if (bCompressed)
        {
            uint8_t* p = (uint8_t*)realloc(pDecode, mip.m_Width * mip.m_Height * 4);
            if (!p)
                break;
            pDecode = p;
            glr_DecodeS3TC(mip.m_pData, mip.m_Width, mip.m_Height, nType, pDecode);
            glTexImage2D(GL_TEXTURE_2D, nLevels, nInternal, mip.m_Width, mip.m_Height, 0,
                         GL_BGRA, GL_UNSIGNED_BYTE, pDecode);
            nBytes += mip.m_Width * mip.m_Height * 4;
        }
        else
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mip.m_Pitch ? mip.m_Pitch / 4 : 0);
            glTexImage2D(GL_TEXTURE_2D, nLevels, nInternal, mip.m_Width, mip.m_Height, 0,
                         GL_BGRA, GL_UNSIGNED_BYTE, mip.m_pData);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            nBytes += mip.m_Width * mip.m_Height * 4;
        }
        if (nLevels == 0)
        {
            pTex->m_nWidth = mip.m_Width;
            pTex->m_nHeight = mip.m_Height;
        }
        nLevels++;
    }
    free(pDecode);
    if (!nLevels)
        return false;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nLevels - 1);
    glr_ApplyTextureFilter(nLevels);
    pTex->m_DtxFlags = hdr.m_IFlags;
    pTex->m_nBaseWidth = pData->m_Mips[0].m_Width;
    pTex->m_nBaseHeight = pData->m_Mips[0].m_Height;
    float fScale = (float)(1 << (hdr.m_Extra[4] & 15));
    pTex->m_fUMul = pData->m_Mips[0].m_Width ? fScale / pData->m_Mips[0].m_Width : 0.0f;
    pTex->m_fVMul = pData->m_Mips[0].m_Height ? fScale / pData->m_Mips[0].m_Height : 0.0f;
    // The DTX's detail scale (a float at TextureData+0x26, added to 1) and
    // angle in degrees (an int16 at +0x2a), r_SetupTextureSurface 0x180ab4.
    float fDetail;
    int16_t nAngle;
    memcpy(&fDetail, &hdr.m_Extra[6], 4);
    memcpy(&nAngle, &hdr.m_Extra[10], 2);
    pTex->m_fDetailScale = (fDetail == fDetail && fDetail > -1.0f && fDetail < 100.0f) ? 1.0f + fDetail : 1.0f;
    pTex->m_fDetailCos = cosf(nAngle * 3.14159265f / 180.0f);
    pTex->m_fDetailSin = sinf(nAngle * 3.14159265f / 180.0f);
    pTex->m_bAlpha = nInternal != GL_RGB8;
    pTex->m_nBytes = nBytes;
    return true;
}

// ---- the slots ------------------------------------------------------------------

void glr_BindTexture(SharedTexture* pTexture, LTBOOL bChanged)
{
    if (!pTexture || !g.hRC)
        return;
    GLRTexture* pTex = (GLRTexture*)pTexture->m_pRenderData;
    if (pTex && !bChanged)
        return;

    TextureData* pData = g.pStruct->GetTexture(pTexture);
    if (!pData)
        return;
    bool bNew = !pTex;
    if (bNew)
    {
        pTex = (GLRTexture*)calloc(1, sizeof(GLRTexture));
        if (!pTex)
        {
            g.pStruct->FreeTexture(pTexture);
            return;
        }
        glGenTextures(1, &pTex->m_Id);
    }
    if (glr_Upload(pTex, pData))
    {
        pTexture->m_pRenderData = pTex;
        g.pStruct->m_nBytes[2] += bNew ? pTex->m_nBytes : 0;
    }
    else if (bNew)
    {
        glDeleteTextures(1, &pTex->m_Id);
        free(pTex);
        glr_Debug(1, "texture with unsupported format %d not bound", pData->m_Header.m_Extra[2]);
    }
    g.pStruct->FreeTexture(pTexture);
}

void glr_UnbindTexture(SharedTexture* pTexture)
{
    if (!pTexture)
        return;
    GLRTexture* pTex = (GLRTexture*)pTexture->m_pRenderData;
    if (pTex)
    {
        if (g.hRC)
            glDeleteTextures(1, &pTex->m_Id);
        if (g.pStruct->m_nBytes[2] >= pTex->m_nBytes)
            g.pStruct->m_nBytes[2] -= pTex->m_nBytes;
        free(pTex);
    }
    pTexture->m_pRenderData = NULL;
}

GLuint glr_TextureId(SharedTexture* pTexture)
{
    if (!pTexture)
        return 0;
    if (!pTexture->m_pRenderData)
        glr_BindTexture(pTexture, 0);
    GLRTexture* pTex = (GLRTexture*)pTexture->m_pRenderData;
    return pTex ? pTex->m_Id : 0;
}


#pragma once

#include "e_common.hh"

enum EPixelFormat
{
    PF_ARGB8 = 0,
    PF_DEPTH
};

struct FRenderTarget
{
    int32_t        width; // made signed for easier triangle clipping
    int32_t        height;
    EPixelFormat   pixelFormat;
    unsigned char* pixels;

    static FRenderTarget* Allocate(uint32_t width, uint32_t height, EPixelFormat format);
    static void           Release(FRenderTarget* rt);
};

enum EVertexSemantic
{
    VS_POSITION  = 0, // used, always 3 floats
    VS_TEXCOORD0 = 1, // used, always 2 floats
    VS_TEXCOORD1 = 2, // all below not used
    VS_TEXCOORD2 = 3,
    VS_TEXCOORD3 = 4,
    VS_TEXCOORD4 = 5,
    VS_TEXCOORD5 = 6,
    VS_TEXCOORD6 = 7,
    VS_TEXCOORD7 = 8,

    VS_COUNT
};

struct FVertexFormat {}; // dummy structure, VF is always VS_POSITION+VS_TEXCOORD

struct FVertexBuffer
{
    struct FixedVertex
    {
        float vs_position[3];
        float vs_texcoord[2];
        float vs_normal[3];
    };

    FixedVertex* data;
    size_t       size;

    static FVertexBuffer* Allocate(FixedVertex* data, size_t size); // will NOT take ownership of data
    static void           Release(FVertexBuffer* buffer);
};

struct FIndexBuffer
{
    typedef unsigned int FixedIndex;

    FixedIndex* data;
    size_t      size;

    static FIndexBuffer* Allocate(FixedIndex* data, size_t size); // will NOT take ownership of data
    static void          Release(FIndexBuffer* buffer);
};

enum EDrawMatrix
{
    DM_PROJECTION = 0,
    DM_MODELVIEW  = 1,

    DM_COUNT
};

typedef float TDrawMatrix[16];

// the API
void fglSetRenderTarget(FRenderTarget* rt);
void fglSetDepthStencilTarget(FRenderTarget* rt);
void fglClear(uint32_t color, float depth);
void fglPresent();

// the last set matrix should be EDM_MODELVIEW, because this function caches MVP matrix once modelview matrix is set
void fglSetMatrix(EDrawMatrix matrix, TDrawMatrix drawMatrix);

void fglSetVertexBuffer(FVertexBuffer* vbuf);
void fglSetIndexBuffer(FIndexBuffer* ibuf);

void fglDraw(size_t offset, size_t count);
void fglDrawIndexed(size_t offset, size_t count);

// debug font
void fglDrawDebugText(FRenderTarget* rt, const char* text, int x, int y);

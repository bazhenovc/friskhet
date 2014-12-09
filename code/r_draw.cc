
#include "r_draw.hh"
#include "r_debugfont.hh"
#include "e_profiler.hh"

#include <vector>

// defines and config
//#define F_RASTERIZER_VIZ_COVERAGE
#ifdef F_RASTERIZER_VIZ_COVERAGE
#define FULL_COVERED_COLOR 0x000000FF
#define PARTIALLY_COVERED_COLOR 0x00FF0000
#endif

// utils
static F_INLINE int imin(int x, int y) { return y + ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1))); }
static F_INLINE int imax(int x, int y) { return x - ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1))); }

static F_INLINE int imin3(int x, int y, int z) { return imin(x, imin(y, z)); }
static F_INLINE int imax3(int x, int y, int z) { return imax(x, imax(y, z)); }

static F_INLINE int iclamp(int x, int a, int b) { return imin(imax(x, a), b); }

static F_INLINE int iround(float x) { return static_cast<int>(x); }
//static F_INLINE int iround(float x)
//{
//    int t;
//    __asm
//    {
//        fld x
//        fistp t
//    }
//    return t;
//}

static F_INLINE float fround(int x) { return static_cast<float>(x); }

// pixel formats
typedef uint32_t TPixelARGB8;
typedef float    TPixelDepth;

size_t g_MapPixelFormatSize[] = {
    sizeof(TPixelARGB8),
    sizeof(TPixelDepth)
};

// utilities
struct FPoint2D
{
    float x;
    float y;
};

static F_INLINE FPoint2D operator*(const FPoint2D& pt, float f)
{
    return { pt.x * f, pt.y * f };
}

static F_INLINE FPoint2D operator+(const FPoint2D& pt0, const FPoint2D& pt1)
{
    return { pt0.x + pt1.x, pt0.y + pt1.y };
}

struct FPoint3D
{
    float x;
    float y;
    float z;
};

static F_INLINE FPoint3D operator*(const FPoint3D& pt, float f)
{
    return { pt.x * f, pt.y * f, pt.z * f };
}

static F_INLINE FPoint3D operator/(const FPoint3D& pt, float f)
{
    return { pt.x / f, pt.y / f, pt.z / f };
}

static F_INLINE FPoint3D operator+(const FPoint3D& pt0, const FPoint3D& pt1)
{
    return { pt0.x + pt1.x, pt0.y + pt1.y, pt0.z + pt1.z };
}
static F_INLINE FPoint3D operator-(const FPoint3D& pt0, const FPoint3D& pt1)
{
    return { pt0.x - pt1.x, pt0.y - pt1.y, pt0.z - pt1.z };
}

static F_INLINE FPoint3D operator*(const FPoint3D& pt0, const FPoint3D& pt1)
{
    return { pt0.x * pt1.x, pt0.y * pt1.y, pt0.z * pt1.z };
}

static F_INLINE FPoint3D operator/(const FPoint3D& pt0, const FPoint3D& pt1)
{
    return { pt0.x / pt1.x, pt0.y / pt1.y, pt0.z / pt1.z };
}

struct FPoint4D
{
    float x;
    float y;
    float z;
    float w;
};

static F_INLINE FPoint4D operator*(const FPoint4D& pt, float f)
{
    return { pt.x * f, pt.y * f, pt.z * f, pt.w * f };
}

static F_INLINE FPoint4D operator/(const FPoint4D& pt, float f)
{
    return { pt.x / f, pt.y / f, pt.z / f, pt.w / f };
}

static F_INLINE FPoint4D operator+(const FPoint4D& pt, float f)
{
    return { pt.x + f, pt.y + f, pt.z + f, pt.w + f };
}

static F_INLINE FPoint4D operator*(const FPoint4D& pt0, const FPoint4D& pt1)
{
    return { pt0.x * pt1.x, pt0.y * pt1.y, pt0.z * pt1.z, pt0.w * pt1.w };
}

static F_INLINE FPoint4D operator+(const FPoint4D& pt0, const FPoint4D& pt1)
{
    return { pt0.x + pt1.x, pt0.y + pt1.y, pt0.z + pt1.z, pt0.w + pt1.w };
}

static F_INLINE FPoint4D Mul(const TDrawMatrix& mat, const FPoint4D& pt)
{
    FPoint4D ret;
    ret.x = mat[0] * pt.x + mat[4] * pt.y + mat[8]  * pt.z + mat[12] * pt.w;
    ret.y = mat[1] * pt.x + mat[5] * pt.y + mat[9]  * pt.z + mat[13] * pt.w;
    ret.z = mat[2] * pt.x + mat[6] * pt.y + mat[10] * pt.z + mat[14] * pt.w;
    ret.w = mat[3] * pt.x + mat[7] * pt.y + mat[11] * pt.z + mat[15] * pt.w;
    return ret;
}

static F_INLINE void MMul(TDrawMatrix mat, TDrawMatrix m, TDrawMatrix out) // params are pointers
{
    out[0]  = mat[0] * m[0]  + mat[4] * m[1]  + mat[8]  * m[2]  + mat[12] * m[3];
    out[1]  = mat[1] * m[0]  + mat[5] * m[1]  + mat[9]  * m[2]  + mat[13] * m[3];
    out[2]  = mat[2] * m[0]  + mat[6] * m[1]  + mat[10] * m[2]  + mat[14] * m[3];
    out[3]  = mat[3] * m[0]  + mat[7] * m[1]  + mat[11] * m[2]  + mat[15] * m[3];
    out[4]  = mat[0] * m[4]  + mat[4] * m[5]  + mat[8]  * m[6]  + mat[12] * m[7];
    out[5]  = mat[1] * m[4]  + mat[5] * m[5]  + mat[9]  * m[6]  + mat[13] * m[7];
    out[6]  = mat[2] * m[4]  + mat[6] * m[5]  + mat[10] * m[6]  + mat[14] * m[7];
    out[7]  = mat[3] * m[4]  + mat[7] * m[5]  + mat[11] * m[6]  + mat[15] * m[7];
    out[8]  = mat[0] * m[8]  + mat[4] * m[9]  + mat[8]  * m[10] + mat[12] * m[11];
    out[9]  = mat[1] * m[8]  + mat[5] * m[9]  + mat[9]  * m[10] + mat[13] * m[11];
    out[10] = mat[2] * m[8]  + mat[6] * m[9]  + mat[10] * m[10] + mat[14] * m[11];
    out[11] = mat[3] * m[8]  + mat[7] * m[9]  + mat[11] * m[10] + mat[15] * m[11];
    out[12] = mat[0] * m[12] + mat[4] * m[13] + mat[8]  * m[14] + mat[12] * m[15];
    out[13] = mat[1] * m[12] + mat[5] * m[13] + mat[9]  * m[14] + mat[13] * m[15];
    out[14] = mat[2] * m[12] + mat[6] * m[13] + mat[10] * m[14] + mat[14] * m[15];
    out[15] = mat[3] * m[12] + mat[7] * m[13] + mat[11] * m[14] + mat[15] * m[15];
}

struct IPoint2D
{
    int x;
    int y;
};

struct SSPoint2D // screen-space point
{
    IPoint2D position;
    float    depth;
    FPoint3D texcoord; // (U/Z, V/Z, 1/Z)
};

struct SSTri // screen-space triangle
{
    SSPoint2D v0;
    SSPoint2D v1;
    SSPoint2D v2;
};

// rasterizer
static F_INLINE FPoint3D CalculateBarycentricCoords(const SSTri& tri, const IPoint2D& pt)
{
    int d = ((tri.v1.position.y - tri.v2.position.y) * (tri.v0.position.x - tri.v2.position.x) + (tri.v2.position.x - tri.v1.position.x) * (tri.v0.position.y - tri.v2.position.y));
    float denom = 1.0F / d;

    FPoint3D ret;

    ret.x = ((tri.v1.position.y - tri.v2.position.y) * (pt.x - tri.v2.position.x) + (tri.v2.position.x - tri.v1.position.x) * (pt.y - tri.v2.position.y)) * denom;
    ret.y = ((tri.v2.position.y - tri.v0.position.y) * (pt.x - tri.v2.position.x) + (tri.v0.position.x - tri.v2.position.x) * (pt.y - tri.v2.position.y)) * denom;
    ret.z = (1.0F) - ret.x - ret.y;

    return ret;
}

template <typename F>
static F_INLINE void WritePixel(FRenderTarget*rt, size_t x, size_t y, F pixel)
{
    F* pixels = reinterpret_cast<F*>(rt->pixels);
    pixels[y * rt->width + x] = pixel;
}

template <typename F>
static F_INLINE F GetPixel(FRenderTarget* rt, size_t x, size_t y)
{
    F* pixels = reinterpret_cast<F*>(rt->pixels);
    return pixels[y * rt->width + x];
}

// temporary texture
#define F_ARGB(a, r, g, b) ((a << 24) | (r << 16) | (g << 8) | (b << 0))
const TPixelARGB8 g_grey  = F_ARGB(255, 127, 127, 127);
const TPixelARGB8 g_white = F_ARGB(255, 255, 255, 255);

TPixelARGB8 g_texture[4 * 4] = {
    g_grey,  g_grey,  g_white, g_white,
    g_grey,  g_grey,  g_white, g_white,
    g_white, g_white, g_grey,  g_grey,
    g_white, g_white, g_grey,  g_grey
};

static F_INLINE void WriteTriPixel(FRenderTarget* colorRT, FRenderTarget* depthRT, int x, int y, const SSTri& tri)
{
    FPoint3D blerp = CalculateBarycentricCoords(tri, {x, y});

    float    bdepth    = tri.v0.depth * blerp.x + tri.v1.depth * blerp.y + tri.v2.depth * blerp.z;
    float    depth     = GetPixel<TPixelDepth>(depthRT, x, y);
    FPoint3D btexcoord = tri.v0.texcoord * blerp.x + tri.v1.texcoord * blerp.y + tri.v2.texcoord * blerp.z;
    int tx             = iround(btexcoord.x * 4.0F);
    int ty             = iround(btexcoord.y * 4.0F);
    //TPixelARGB8 color  = GetPixel<TPixelARGB8>(ctx.texture0, tx, ty);
    TPixelARGB8 color  = g_texture[ty * 4 + tx];

    if (bdepth < depth) {
        WritePixel<TPixelDepth>(depthRT, x, y, bdepth);
        WritePixel<TPixelARGB8>(colorRT, x, y, color);
    }
}

static void RasterizeTriangles(FRenderTarget* colorRT, FRenderTarget* depthRT, size_t numTris, SSTri* tris)
{
    F_NAMED_PROFILE(Rasterize_Triangles);

    for (size_t i = 0; i < numTris; ++i) {
        const SSTri& tri = tris[i];

        // 28.4 fixed-point coordinates
        const int Y1 = iround(16.0f * tri.v0.position.y);
        const int Y2 = iround(16.0f * tri.v1.position.y);
        const int Y3 = iround(16.0f * tri.v2.position.y);

        const int X1 = iround(16.0f * tri.v0.position.x);
        const int X2 = iround(16.0f * tri.v1.position.x);
        const int X3 = iround(16.0f * tri.v2.position.x);

        // Deltas
        const int DX12 = X1 - X2;
        const int DX23 = X2 - X3;
        const int DX31 = X3 - X1;

        const int DY12 = Y1 - Y2;
        const int DY23 = Y2 - Y3;
        const int DY31 = Y3 - Y1;

        // Fixed-point deltas
        const int FDX12 = DX12 << 4;
        const int FDX23 = DX23 << 4;
        const int FDX31 = DX31 << 4;

        const int FDY12 = DY12 << 4;
        const int FDY23 = DY23 << 4;
        const int FDY31 = DY31 << 4;

        // Bounding rectangle
        int minx = (imin3(X1, X2, X3) + 0xF) >> 4;
        int maxx = (imax3(X1, X2, X3) + 0xF) >> 4;
        int miny = (imin3(Y1, Y2, Y3) + 0xF) >> 4;
        int maxy = (imax3(Y1, Y2, Y3) + 0xF) >> 4;

        // Block size, standard 8x8 (must be power of two)
        const int q = 8;

        // Start in corner of 8x8 block
        minx &= ~(q - 1);
        miny &= ~(q - 1);

        // Clip
        minx = imax(0, imin(minx, colorRT->width - 1));
        maxx = imax(0, imin(maxx, colorRT->width - 1));

        miny = imax(0, imin(miny, colorRT->height - 1));
        maxy = imax(0, imin(maxy, colorRT->height - 1));

        // Half-edge constants
        int C1 = DY12 * X1 - DX12 * Y1;
        int C2 = DY23 * X2 - DX23 * Y2;
        int C3 = DY31 * X3 - DX31 * Y3;

        // Correct for fill convention
        if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
        if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
        if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

        // Loop through blocks
        for (int y = miny; y < maxy; y += q) {
            for (int x = minx; x < maxx; x += q) {
                // Corners of block
                int x0 = x << 4;
                int x1 = (x + q - 1) << 4;
                int y0 = y << 4;
                int y1 = (y + q - 1) << 4;

                // Evaluate half-space functions
                bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
                bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
                bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
                bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
                int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

                bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
                bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
                bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
                bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
                int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

                bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
                bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
                bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
                bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
                int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

                // Skip block when outside an edge
                if (a == 0x0 || b == 0x0 || c == 0x0) continue;

                // Accept whole block when totally covered
                if (a == 0xF && b == 0xF && c == 0xF) {
                    for (int iy = y; iy < y + q; ++iy) {
                        for (int ix = x; ix < x + q; ++ix) {
                            #ifdef F_RASTERIZER_VIZ_COVERAGE
                            WritePixel<TPixelARGB8>(colorRT, ix, iy, FULL_COVERED_COLOR);
                            #else
                            WriteTriPixel(colorRT, depthRT, ix, iy, tri);
                            #endif
                        }
                    }
                } else { // Partially covered block
                    int CY1 = C1 + DX12 * y0 - DY12 * x0;
                    int CY2 = C2 + DX23 * y0 - DY23 * x0;
                    int CY3 = C3 + DX31 * y0 - DY31 * x0;

                    for (int iy = y; iy < y + q; iy++) {
                        int CX1 = CY1;
                        int CX2 = CY2;
                        int CX3 = CY3;

                        for (int ix = x; ix < x + q; ix++) {
                            if (CX1 > 0 && CX2 > 0 && CX3 > 0) {
                                #ifdef F_RASTERIZER_VIZ_COVERAGE
                                WritePixel<TPixelARGB8>(colorRT, ix, iy, PARTIALLY_COVERED_COLOR);
                                #else
                                WriteTriPixel(colorRT, depthRT, ix, iy, tri);
                                #endif
                            }

                            CX1 -= FDY12;
                            CX2 -= FDY23;
                            CX3 -= FDY31;
                        }

                        CY1 += FDX12;
                        CY2 += FDX23;
                        CY3 += FDX31;
                    }
                }
            }
        }
    }
}

// draw context
struct DrawContext
{
    std::vector<SSTri> screenTris;

    FRenderTarget*     colorRT = nullptr;
    FRenderTarget*     depthRT = nullptr;

    FVertexBuffer*     vertexBuffer = nullptr;
    FIndexBuffer*      indexBuffer  = nullptr;

    TDrawMatrix        matrices[DM_COUNT];
    TDrawMatrix        MVP;

    F_INLINE bool IsValid() const { return colorRT != nullptr && depthRT != nullptr; }
} g_drawContext;

// render targets
FRenderTarget* FRenderTarget::Allocate(uint32_t width, uint32_t height, EPixelFormat format)
{
    FRenderTarget* rt = new FRenderTarget;
    rt->width = width;
    rt->height = height;
    rt->pixelFormat = format;
    rt->pixels = new unsigned char[width * height * g_MapPixelFormatSize[format]];
    return rt;
}

void FRenderTarget::Release(FRenderTarget* rt)
{
    delete rt;
}

// vertex processing
FVertexBuffer* FVertexBuffer::Allocate(FVertexBuffer::FixedVertex* data, size_t size)
{
    FVertexBuffer* ret = new FVertexBuffer;
    ret->data = data;
    ret->size = size;
    return ret;
}

void FVertexBuffer::Release(FVertexBuffer* vbuf)
{
    //delete [] vbuf->data;
    delete vbuf;
}

FIndexBuffer* FIndexBuffer::Allocate(FIndexBuffer::FixedIndex* data, size_t size)
{
    FIndexBuffer* ret = new FIndexBuffer;
    ret->data = data;
    ret->size = size;
    return ret;
}

void FIndexBuffer::Release(FIndexBuffer* ibuf)
{
    //delete [] ibuf->data;
    delete ibuf;
}

struct WSTri // world-space triangle
{
    FVertexBuffer::FixedVertex v0;
    FVertexBuffer::FixedVertex v1;
    FVertexBuffer::FixedVertex v2;
};

static F_INLINE bool ClipTriangle(const SSTri& tri) // not a real clipping
{
    return
        (tri.v0.position.x >= 0 && tri.v0.position.x < g_drawContext.colorRT->width && tri.v0.position.y >= 0 && tri.v0.position.y < g_drawContext.colorRT->height) ||
        (tri.v1.position.x >= 0 && tri.v1.position.x < g_drawContext.colorRT->width && tri.v1.position.y >= 0 && tri.v1.position.y < g_drawContext.colorRT->height) ||
        (tri.v2.position.x >= 0 && tri.v2.position.x < g_drawContext.colorRT->width && tri.v2.position.y >= 0 && tri.v2.position.y < g_drawContext.colorRT->height);
}

static F_INLINE void ProjectTriangle(const WSTri& tri)
{
    FPoint4D v0{ tri.v0.vs_position[0], tri.v0.vs_position[1], tri.v0.vs_position[2], 1.0F };
    FPoint4D v1{ tri.v1.vs_position[0], tri.v1.vs_position[1], tri.v1.vs_position[2], 1.0F };
    FPoint4D v2{ tri.v2.vs_position[0], tri.v2.vs_position[1], tri.v2.vs_position[2], 1.0F };

    v0 = Mul(g_drawContext.MVP, v0);
    v1 = Mul(g_drawContext.MVP, v1);
    v2 = Mul(g_drawContext.MVP, v2);

    const FPoint4D half{ 0.5F, 0.5F, 0.0F, 0.0F };
    const FPoint4D vp  { fround(g_drawContext.colorRT->width), fround(g_drawContext.colorRT->height), 1.0F, 1.0F };

    v0 = ((v0 / v0.w) * 0.5F + half) * vp;
    v1 = ((v1 / v1.w) * 0.5F + half) * vp;
    v2 = ((v2 / v2.w) * 0.5F + half) * vp;

    FPoint3D v0_tex{ tri.v0.vs_texcoord[0], tri.v0.vs_texcoord[1], v0.z };
    FPoint3D v1_tex{ tri.v1.vs_texcoord[0], tri.v1.vs_texcoord[1], v1.z };
    FPoint3D v2_tex{ tri.v2.vs_texcoord[0], tri.v2.vs_texcoord[1], v2.z };

    SSPoint2D p0 = { iround(v0.x), iround(v0.y), v0.z, v0_tex };
    SSPoint2D p1 = { iround(v1.x), iround(v1.y), v1.z, v1_tex };
    SSPoint2D p2 = { iround(v2.x), iround(v2.y), v2.z, v2_tex };

    SSTri stri{p0, p1, p2};
    if (ClipTriangle(stri)) {
        g_drawContext.screenTris.push_back(stri);
    }
}

// FGL interface implementation
void fglSetRenderTarget(FRenderTarget* rt)
{
    g_drawContext.colorRT = rt;
}

void fglSetDepthStencilTarget(FRenderTarget* rt)
{
    g_drawContext.depthRT = rt;
}

void fglClear(uint32_t color, float depth)
{
    if (g_drawContext.IsValid()) {
        std::memset(g_drawContext.colorRT->pixels, color, g_drawContext.colorRT->width * g_drawContext.colorRT->height * sizeof(TPixelARGB8));
        //std::memset(g_drawContext.depthRT->pixels, *((int*)&depth), g_drawContext.depthRT->width * g_drawContext.depthRT->height * sizeof(TPixelDepth));

        float* pixels = reinterpret_cast<float*>(g_drawContext.depthRT->pixels);
        for (ptrdiff_t i = 0; i < g_drawContext.depthRT->width * g_drawContext.depthRT->height; ++i)
            pixels[i] = depth;
    }
}

void fglPresent()
{
    if (g_drawContext.IsValid())
        RasterizeTriangles(g_drawContext.colorRT, g_drawContext.depthRT, g_drawContext.screenTris.size(), g_drawContext.screenTris.data());

    // always clear
    g_drawContext.screenTris.clear();
}

void fglSetMatrix(EDrawMatrix matrix, TDrawMatrix drawMatrix)
{
    std::memcpy(g_drawContext.matrices[matrix], drawMatrix, 16 * sizeof(float));
    if (matrix == DM_MODELVIEW) {
        MMul(g_drawContext.matrices[DM_PROJECTION], drawMatrix, g_drawContext.MVP);
    }
}

void fglSetVertexBuffer(FVertexBuffer* vbuf)
{
    g_drawContext.vertexBuffer = vbuf;
}

void fglSetIndexBuffer(FIndexBuffer* ibuf)
{
    g_drawContext.indexBuffer = ibuf;
}

void fglDraw(size_t offset, size_t count)
{
    F_NAMED_PROFILE(Vertex_Processing);

    for (size_t idx = offset; idx < count; idx += 3) {
        const FVertexBuffer::FixedVertex& v0 = g_drawContext.vertexBuffer->data[idx + 0];
        const FVertexBuffer::FixedVertex& v1 = g_drawContext.vertexBuffer->data[idx + 1];
        const FVertexBuffer::FixedVertex& v2 = g_drawContext.vertexBuffer->data[idx + 2];

        WSTri tri{ v2, v1, v0 };
        ProjectTriangle(tri);
    }
}

void fglDrawIndexed(size_t offset, size_t count)
{
    F_NAMED_PROFILE(Vertex_Processing);

    for (size_t idx = offset; idx < count; idx += 3) {
        const FVertexBuffer::FixedVertex& v0 = g_drawContext.vertexBuffer->data[g_drawContext.indexBuffer->data[idx + 0]];
        const FVertexBuffer::FixedVertex& v1 = g_drawContext.vertexBuffer->data[g_drawContext.indexBuffer->data[idx + 1]];
        const FVertexBuffer::FixedVertex& v2 = g_drawContext.vertexBuffer->data[g_drawContext.indexBuffer->data[idx + 2]];

        WSTri tri{ v2, v1, v0 };
        ProjectTriangle(tri);
    }
}

void fglDrawDebugText(FRenderTarget* rt, const char* text, int x, int y)
{
    int dx = x;
    int dy = y;

    for (const char* c = text; *c; ++c) {
        size_t fontOffset = *c;

        for (int iy = 0; iy < 8; ++iy) {
            for (int ix = 0; ix < 8; ++ix) {
                uint8_t bit = 1 << (7 - ix);
                uint8_t fontPixel = g_debugFont[fontOffset * 8 + iy] & bit ? 255 : 0;

                TPixelARGB8* pixels = reinterpret_cast<TPixelARGB8*>(rt->pixels);
                pixels[(iy + dy) * rt->width + (ix + dx)] = F_ARGB(255, fontPixel, fontPixel, fontPixel);
            }
        }

        dx += 8;
    }
}

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <iostream>
#include <vector>

#include <SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#define GLM_FORCE_PURE
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define WIDTH 640
#define HEIGHT 480

#ifdef _WIN32
#define F_INLINE __forceinline
#else
#define F_INLINE inline
#endif

//#define F_RASTERIZER_VIZ_COVERAGE
#ifdef F_RASTERIZER_VIZ_COVERAGE
#define FULL_COVERED_COLOR 0x000000FF
#define PARTIALLY_COVERED_COLOR 0x00FF0000
#endif

//#define F_PROFILE_RASTERIZER 1
#ifdef F_PROFILE_RASTERIZER
struct FNamedProfiler
{
    std::chrono::high_resolution_clock::time_point tmStart;
    const char* name;

    F_INLINE FNamedProfiler(const char* profName)
        : name(profName)
    {
        tmStart = std::chrono::high_resolution_clock::now();
    }

    F_INLINE ~FNamedProfiler()
    {
        auto tmEnd = std::chrono::high_resolution_clock::now();

        std::cout << "[profile] " << name << " : " << std::chrono::duration_cast<std::chrono::milliseconds>(tmEnd - tmStart).count() << "ms" << std::endl;
    }
};

void F_ClearConsole()
{
#ifdef _WIN32
    COORD topLeft = { 0, 0 };
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(console, topLeft);
#else
    std::cout << "\x1B[2J\x1B[H";
#endif
}

#define F_NAMED_PROFILE(X) FNamedProfiler prof_##X(#X)
#else
#define F_NAMED_PROFILE(X)
#endif

// software rasterizer
typedef uint32_t   FColorARGB;

struct FPoint2D
{
    int       x;
    int       y;
    float     depth;
    glm::vec2 texcoord;
};

typedef glm::vec3 FPoint3D;

uint32_t*  pixels      = nullptr;
float*     depthBuffer = nullptr;

F_INLINE int imin(int x, int y) { return y + ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1))); }
F_INLINE int imax(int x, int y) { return x - ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1))); }

F_INLINE int imin3(int x, int y, int z) { return imin(x, imin(y, z)); }
F_INLINE int imax3(int x, int y, int z) { return imax(x, imax(y, z)); }

F_INLINE int iclamp(int x, int a, int b) { return imin(imax(x, a), b); }

F_INLINE int iround(float x) { return static_cast<int>(x); }
//F_INLINE int iround(float x)
//{
//    int t;
//    __asm
//    {
//        fld x
//        fistp t
//    }
//    return t;
//}

struct FScreenTri
{
    FPoint2D v0;
    FPoint2D v1;
    FPoint2D v2;
};

F_INLINE FPoint3D F_BarycentricCoords(const FScreenTri& tri, const FPoint2D& p)
{
    int d = ((tri.v1.y - tri.v2.y) * (tri.v0.x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (tri.v0.y - tri.v2.y));

    float denom = 1.0F / d;

    FPoint3D ret;

    ret.x = ((tri.v1.y - tri.v2.y) * (p.x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (p.y - tri.v2.y)) * denom;
    ret.y = ((tri.v2.y - tri.v0.y) * (p.x - tri.v2.x) + (tri.v0.x - tri.v2.x) * (p.y - tri.v2.y)) * denom;
    ret.z = (1.0F) - ret.x - ret.y;


    return ret;
}

F_INLINE FColorARGB F_ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) { return (a << 24) | (r << 16) | (g << 8) | (b << 0); }

F_INLINE void F_PutPixel(int ix, int iy, FColorARGB color)
{
    if (ix >= 0 && ix < WIDTH && iy >= 0 && iy < HEIGHT)
        pixels[iy * WIDTH + ix] = color;
}

F_INLINE void  F_WriteDepth(int ix, int iy, float depth) { depthBuffer[iy * WIDTH + ix] = depth; }
F_INLINE float F_GetDepth(int ix, int iy)                { return depthBuffer[iy * WIDTH + ix]; }

const FColorARGB g_grey = F_ARGB(255, 127, 127, 127);
const FColorARGB g_white = F_ARGB(255, 255, 255, 255);

FColorARGB g_texture[4 * 4] = {
    g_grey,  g_grey,  g_white, g_white,
    g_grey,  g_grey,  g_white, g_white,
    g_white, g_white, g_grey,  g_grey,
    g_white, g_white, g_grey,  g_grey
};

F_INLINE void F_PutTriPixel(int ix, int iy, const FScreenTri& tri)
{
    FPoint3D blerp = F_BarycentricCoords(tri, {ix, iy});

    float bdepth   = tri.v0.depth * blerp.x + tri.v1.depth * blerp.y + tri.v2.depth * blerp.z;

    glm::vec2 btex = tri.v0.texcoord * blerp.x + tri.v1.texcoord * blerp.y + tri.v2.texcoord * blerp.z;

    int tx = iround(btex.x * 4);
    int ty = iround(btex.y * 4);

    float depth = F_GetDepth(ix, iy);
    if (bdepth < depth) {
        F_WriteDepth(ix, iy, bdepth);
        F_PutPixel(ix, iy, g_texture[ty * 4 + tx]);
    }
}

void F_RasterizeTris(const FScreenTri* tris, size_t numTris)
{
    F_NAMED_PROFILE(Rasterize_Triangles);

    for (size_t i = 0; i < numTris; ++i) {
        const FScreenTri& tri = tris[i];

        // 28.4 fixed-point coordinates
        const int Y1 = iround(16.0f * tri.v0.y);
        const int Y2 = iround(16.0f * tri.v1.y);
        const int Y3 = iround(16.0f * tri.v2.y);

        const int X1 = iround(16.0f * tri.v0.x);
        const int X2 = iround(16.0f * tri.v1.x);
        const int X3 = iround(16.0f * tri.v2.x);

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
                                F_PutPixel(ix, iy, FULL_COVERED_COLOR);
                                #else
                                F_PutTriPixel(ix, iy, tri);
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
                                F_PutPixel(ix, iy, PARTIALLY_COVERED_COLOR);
                                #else
                                F_PutTriPixel(ix, iy, tri);
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

// vertex processing
struct FVertex
{
    glm::vec3 position;
    glm::vec2 texcoord;
    glm::vec3 normal;
};

struct FTri
{
    FVertex v0;
    FVertex v1;
    FVertex v2;
};

typedef uint32_t FIndex;

#include "cube.hh"

std::vector<FScreenTri> g_screenTris;

glm::mat4 g_mvp;

// not real clipping
F_INLINE bool F_ClipTri(const FScreenTri& tri)
{
    return
        (tri.v0.x >= 0 && tri.v0.x < WIDTH && tri.v0.y >= 0 && tri.v0.y < HEIGHT) ||
        (tri.v1.x >= 0 && tri.v1.x < WIDTH && tri.v1.y >= 0 && tri.v1.y < HEIGHT) ||
        (tri.v2.x >= 0 && tri.v2.x < WIDTH && tri.v2.y >= 0 && tri.v2.y < HEIGHT);
}

F_INLINE void F_ProjectTri(const FTri& tri)
{
    glm::vec4 v0 = glm::vec4(tri.v0.position, 1.0F);
    glm::vec4 v1 = glm::vec4(tri.v1.position, 1.0F);
    glm::vec4 v2 = glm::vec4(tri.v2.position, 1.0F);

    v0 = g_mvp * v0;
    v1 = g_mvp * v1;
    v2 = g_mvp * v2;

    v0 = (v0 / v0.w) * 0.5F + glm::vec4(0.5F);
    v1 = (v1 / v1.w) * 0.5F + glm::vec4(0.5F);
    v2 = (v2 / v2.w) * 0.5F + glm::vec4(0.5F);

    v0 *= glm::vec4(WIDTH, HEIGHT, 1.0F, 1.0F);
    v1 *= glm::vec4(WIDTH, HEIGHT, 1.0F, 1.0F);
    v2 *= glm::vec4(WIDTH, HEIGHT, 1.0F, 1.0F);

    const float half = 0.5F;

    FPoint2D p0 = { iround(v0.x), iround(v0.y), v0.z, tri.v0.texcoord };//, tri.v0.position * half + half };
    FPoint2D p1 = { iround(v1.x), iround(v1.y), v1.z, tri.v1.texcoord };//, tri.v1.position * half + half };
    FPoint2D p2 = { iround(v2.x), iround(v2.y), v2.z, tri.v2.texcoord };//, tri.v2.position * half + half };

    FScreenTri stri{p0, p1, p2};
    if (F_ClipTri(stri))
        g_screenTris.push_back(stri);
}

void F_DrawIndexed(FVertex* vertBuffer, FIndex* indBuffer, size_t numIndices)
{
    for (size_t idx = 0; idx < numIndices; idx += 3) {
        FVertex v0 = vertBuffer[indBuffer[idx + 0]];
        FVertex v1 = vertBuffer[indBuffer[idx + 1]];
        FVertex v2 = vertBuffer[indBuffer[idx + 2]];

        FTri tri{v2, v1, v0};
        F_ProjectTri(tri);
    }
}

void F_Draw(FVertex* vertBuffer, size_t numVertices)
{
    for (size_t idx = 0; idx < numVertices; idx += 3) {
        FVertex v0 = vertBuffer[idx + 0];
        FVertex v1 = vertBuffer[idx + 1];
        FVertex v2 = vertBuffer[idx + 2];

        FTri tri{v2, v1, v0};
        F_ProjectTri(tri);
    }
}

void F_Clear()
{
    g_screenTris.clear();
    memset(pixels, 0, WIDTH * HEIGHT * sizeof(uint32_t));
    for (size_t i = 0; i < WIDTH * HEIGHT; ++i)
        depthBuffer[i] = 1.0F;

    #ifdef F_PROFILE_RASTERIZER
    F_ClearConsole();
    #endif
}

// game loop
glm::vec3 g_cameraPos;

void F_GameStep()
{
    static float time = 0.5F;
    time += 0.005F;
    
    F_Clear();

    F_NAMED_PROFILE(Frame);

    {
        F_NAMED_PROFILE(Vertex_Processing);

        glm::mat4 perspective = glm::perspective(45.0F, float(WIDTH) / float(HEIGHT), 0.01F, 1000.0F) * glm::translate(-g_cameraPos);
        glm::mat4 rotation = glm::mat4_cast(glm::quat(glm::vec3(time, time, time)));

        glm::mat4 modelview[] = {
            glm::translate(glm::vec3( 0.0F,  0.0F, -10.0F)) * rotation,
            glm::translate(glm::vec3(-5.0F,  0.0F, -10.0F)) * rotation,
            glm::translate(glm::vec3( 5.0F,  0.0F, -10.0F)) * rotation,

            glm::translate(glm::vec3( 0.0F,  3.5F, -10.0F)) * rotation,
            glm::translate(glm::vec3(-5.0F,  3.5F, -10.0F)) * rotation,
            glm::translate(glm::vec3( 5.0F,  3.5F, -10.0F)) * rotation,

            glm::translate(glm::vec3( 0.0F, -3.5F, -10.0F)) * rotation,
            glm::translate(glm::vec3(-5.0F, -3.5F, -10.0F)) * rotation,
            glm::translate(glm::vec3( 5.0F, -3.5F, -10.0F)) * rotation,
        };

        for (size_t i = 0; i < 9; ++i) {
            g_mvp = perspective * modelview[i];
            F_DrawIndexed(cubeVertices, cubeIndices, 36);
        }
    }
    
#if 0
    FVertex verts[3] = {
        {
            {0.0F, 0.0F, -1.0F},
            {0.0F, 0.0F, 0.0F},
        },
        {
            {0.0F, 1.0F, -1.0F},
            {0.0F, 0.0F, 0.0F},
        },
        {
            {1.0F, 1.0F, -1.0F},
            {0.0F, 0.0F, 0.0F},
        },
    };
    F_Draw(verts, 3);
#endif

#if 0
    FScreenTri tri{{50, 50}, {50, 150}, {150, 150}};
    g_screenTris.push_back(tri);
#endif

    F_RasterizeTris(g_screenTris.data(), g_screenTris.size());
}

// main loop
int main(int argc, char *argv[])
{
    SDL_Window*   win = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Texture*  rsurface = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return 1;

    // window and renderer
    win = SDL_CreateWindow("Friskhet!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // create surface
    rsurface = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    pixels = new uint32_t[WIDTH * HEIGHT];
    memset(pixels, 0, WIDTH * HEIGHT * 4);

    depthBuffer = new float[WIDTH * HEIGHT];

    // main loop
    bool running = true;
    while (running) {

        SDL_Event ev;
        SDL_PumpEvents();

        while (SDL_HasEvents(0, SDL_LASTEVENT)) {
            if (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, 0, SDL_LASTEVENT)) {
                switch (ev.type) {

                case SDL_QUIT:
                case SDL_WINDOWEVENT_CLOSE:
                    running = false;
                break;

                case SDL_KEYDOWN:
                    ///HandleKeyDown(ev.key.keysym.sym);
                break;

                case SDL_KEYUP:
                    //HandleKeyUp(ev.key.keysym.sym);
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (ev.key.keysym.sym == SDLK_a) g_cameraPos.x -= 1.0F;
                    if (ev.key.keysym.sym == SDLK_d) g_cameraPos.x += 1.0F;
                break;

                case SDL_MOUSEMOTION:
                    //HandleMouseMotion(ev.motion.x, ev.motion.y);
                break;

                case SDL_MOUSEBUTTONUP:
                    //HandleMouseUp(ev.button.button, ev.button.x, ev.button.y);
                break;

                case SDL_MOUSEBUTTONDOWN:
                    //HandleMouseDown(ev.button.button, ev.button.x, ev.button.y);
                break;

                case SDL_MOUSEWHEEL:
                    //HandleMouseWheel(-ev.wheel.y);
                break;

                default: break;
                }
            }
        }

        // process game
        F_GameStep();

        // update screen contents
        {
            F_NAMED_PROFILE(Present);

            SDL_UpdateTexture(rsurface, NULL, pixels, WIDTH * sizeof(uint32_t));

            // clear the screen
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, rsurface, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    }

    delete [] pixels;
    delete [] depthBuffer;

    SDL_DestroyTexture(rsurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    return 0;
}

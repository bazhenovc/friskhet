
#include <SDL.h>
#include "r_draw.hh"

#include "cube.hh"

#define GLM_FORCE_PURE
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define WIDTH 640
#define HEIGHT 480

// render targets and buffers
FRenderTarget* g_colorRT;
FRenderTarget* g_depthRT;

FVertexBuffer* g_cubeVB;
FIndexBuffer*  g_cubeIB;

// game loop
glm::vec3 g_cameraPos;

void F_GameStep()
{
    static float time = 0.5F;
    time += 0.005F;
    
    fglSetRenderTarget(g_colorRT);
    fglSetDepthStencilTarget(g_depthRT);

    fglClear(0x00FFFF00, 1.0F);

    //F_NAMED_PROFILE(Frame);

    {
        //F_NAMED_PROFILE(Vertex_Processing);

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

        fglSetMatrix(DM_PROJECTION, glm::value_ptr(perspective));

        for (size_t i = 0; i < 9; ++i) {
            fglSetMatrix(DM_MODELVIEW, glm::value_ptr(modelview[i]));
            fglSetVertexBuffer(g_cubeVB);
            fglSetIndexBuffer(g_cubeIB);
            fglDrawIndexed(0, 36);
        }
    }

    fglPresent();
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

    g_colorRT = FRenderTarget::Allocate(WIDTH, HEIGHT, PF_ARGB8);
    g_depthRT = FRenderTarget::Allocate(WIDTH, HEIGHT, PF_DEPTH);

    g_cubeVB  = FVertexBuffer::Allocate(cubeVertices, 24);
    g_cubeIB  = FIndexBuffer::Allocate(cubeIndices, 36);

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
            //F_NAMED_PROFILE(Present);

            SDL_UpdateTexture(rsurface, NULL, g_colorRT->pixels, WIDTH * sizeof(uint32_t));

            // clear the screen
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, rsurface, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    }

    FRenderTarget::Release(g_colorRT);
    FRenderTarget::Release(g_depthRT);

    FVertexBuffer::Release(g_cubeVB);
    FIndexBuffer::Release(g_cubeIB);

    SDL_DestroyTexture(rsurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    return 0;
}

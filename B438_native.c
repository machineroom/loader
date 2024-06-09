#include <math.h>
#include <assert.h>
#include <SDL.h>
#include "conc_native.h"
#include "B438.h"

static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_layer;
static SDL_Window *sdl_window;

void sdl_input(void) {
    SDL_Event event;
    SDL_KeyboardEvent key;
    int ret = SDL_WaitEvent(&event);
    if (ret==1) {
        if (event.type == SDL_WINDOWEVENT) {
            switch (event.window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                    exit(0);
            }
        }
    }
}

void sdl_input_thread(void *p) {
    while (1) {
        sdl_input();
    }
}

void write_pixels (int x, int y, int count, int *pixels)
{
    #ifdef __linux__
    static int done=0;
    if (done==0) {
        setupGfx(1,1);
        done = 1;
    }
    #endif

    unsigned int* sdl_pixels;
    int pitch;
    SDL_Rect rect = {x,y,count,1};
    SDL_LockTexture( sdl_layer, &rect, (void**)&sdl_pixels, &pitch );
    int i;
    for (i=0; i < count; i++) {
        // Convert 10bpp to 8bpp
        int r,g,b;
        r = pixels[i]>>2;
        r &= 0xFF;
        g = pixels[i]>>12;
        g &= 0xFF;
        b = pixels[i]>>22;
        b &= 0xFF;
        sdl_pixels[i] = b<<16|g<<8|r;
    }
    SDL_UnlockTexture( sdl_layer );
    SDL_RenderCopy(sdl_renderer, sdl_layer, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);

}

void setupGfx(int true_colour, int test_pattern) {
    // setupGfx & write_pixels are called from different threads and SDL aint happy 'bout that
    int rc = SDL_Init(SDL_INIT_EVERYTHING);
    assert (rc>=0);
    char buf[1024];
    sdl_window = SDL_CreateWindow("SDL",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640,
            480, SDL_WINDOW_SHOWN);
    SDL_GetErrorMsg(buf,sizeof(buf));
    assert (sdl_window != NULL);
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED) ;
    assert (sdl_renderer != NULL);
    sdl_layer = SDL_CreateTexture(sdl_renderer,
                                        SDL_PIXELFORMAT_ABGR8888, 
                                        SDL_TEXTUREACCESS_STREAMING,
                                        640, 480);
    assert (sdl_layer != NULL);
    PRun(PSetup(NULL,sdl_input_thread,0,1,NULL));
}

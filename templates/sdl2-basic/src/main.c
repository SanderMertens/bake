#include <include/${id short}.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

    /* Initialize SDL */
    SDL_Init(SDL_INIT_VIDEO | SDL_WINDOW_OPENGL);

    /* Create SDL Window with support for OpenGL and high resolutions */
    SDL_Window *window = SDL_CreateWindow(
        "Hello ${id}",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800,
        600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        fprintf(stderr, "SDL2 window creation failed for canvas: %s\n",
            SDL_GetError());
        goto stop;
    }

    /* Create SDL renderer */
    SDL_Renderer *display = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!display) {
        fprintf(stderr, "SDL2 renderer creation failed for canvas: %s\n",
            SDL_GetError());
        goto stop;
    }

    int proceed = 1;
    do {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                proceed = 0;
                break;
            }
        }
    } while (proceed);

stop:
    SDL_Quit();
    return 0;
}

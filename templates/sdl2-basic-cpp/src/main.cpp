#include <include/${id base}.h>
#include <iostream>

int main(int argc, char *argv[]) try {

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
        throw std::runtime_error( SDL_GetError());
    }

    /* Create SDL renderer */
    SDL_Renderer *display = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!display) {
        throw std::runtime_error( SDL_GetError());
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

    SDL_Quit();

    return 0;

} catch (const std::exception& e) {
    SDL_Quit();
    std::cout << e.what();
}

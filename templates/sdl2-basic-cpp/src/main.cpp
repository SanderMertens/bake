#include <${id base}.h>
#include <iostream>

#define WIDTH 50
#define HEIGHT 50
#define SPEED 25

int main(int argc, char *argv[]) try {

    /* Initialize SDL */
    SDL_Init(SDL_INIT_VIDEO | SDL_WINDOW_OPENGL);

    /* Create an OpenGL window for drawing */
    SDL_Window *window = SDL_CreateWindow(
        "Hello ${id}!",              /* Window title */
        SDL_WINDOWPOS_UNDEFINED,    /* Window x position */
        SDL_WINDOWPOS_UNDEFINED,    /* Window y position */
        800,                        /* Window width */
        600,                        /* Window height */
        SDL_WINDOW_OPENGL);         /* Flags */
    if (!window) {
        throw std::runtime_error( SDL_GetError());
    }

    /* Create the SDL renderer for using SDL-based drawing functions */
    SDL_Renderer *display = SDL_CreateRenderer(
        window,                       /* Window to render to */
        -1,                           /* Index of device (-1 for autodetect) */      
        SDL_RENDERER_ACCELERATED |    /* Flags */
        SDL_RENDERER_PRESENTVSYNC);
    if (!display) {
        throw std::runtime_error( SDL_GetError());
    }

    /* Main loop */
    int proceed = 1;
    int x = 400, y = 300;

    do {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            /* User closes window */
            if (e.type == SDL_QUIT) {
                proceed = 0;
                break;
            }

            /* Move rectangle if user pressed one of the arrow keys */
            if (e.type == SDL_KEYDOWN) {
                switch( e.key.keysym.sym ){
                    case SDLK_LEFT:
                        x -= SPEED;
                        break;
                    case SDLK_RIGHT:
                        x += SPEED;
                        break;
                    case SDLK_UP:
                        y -= SPEED;
                        break;
                    case SDLK_DOWN:
                        y += SPEED;
                        break;
                    default:
                        break;
                }
            }
        }

        /* Clear the window to a black background */
        SDL_SetRenderDrawColor(display, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(display);

        /* Draw a blue rectangle */
        SDL_Rect r = {.w = WIDTH, .h = HEIGHT, .x = x, .y = y};
        SDL_SetRenderDrawColor(display, 64, 128, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(display, &r);

        /* Present the rectangle to the screen */
        SDL_RenderPresent(display);

    } while (proceed);

} catch (const std::exception& e) {
    SDL_Quit();
    std::cout << e.what();
}

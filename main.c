#include <SDL.h>
#include <SDL_image.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char *window_title = "LD46";
const uint32_t step_size = 16; // 16 milliseconds roughly equates to 60Hz

int main() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    SDL_Window *win = SDL_CreateWindow(window_title, 100, 100, 1280, 720, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Surface *loading_surf = IMG_Load("assets/ball.png");
    SDL_Texture *ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    uint32_t last_step_ticks = 0;
    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        SDL_RenderClear(renderer);

        {
            SDL_Rect dst_rect = {.x = 200, .y = 200, .w = 32, .h = 32};
            SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return EXIT_FAILURE;
}


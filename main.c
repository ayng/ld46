#include <SDL.h>
#include <SDL_image.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char *window_title = "LD46";
const uint32_t step_size = 8; // 8 milliseconds roughly equates to 120Hz
const float steps_per_second = (float)(step_size)*0.001f;
const uint8_t ball_width = 32;
const uint8_t ball_height = 32;

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

    // Position is in pixels relative to the top-left corner of the screen.
    float ball_px = 300.0f;
    float ball_py = 200.0f;
    // Velocity is in pixels/s.
    float ball_vx = 200.0f;
    float ball_vy = 0.0f;
    // Acceleration is in pixels/s/s.
    float ball_ax = 0.0f;
    float ball_ay = 200.0f;

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

        // Step physics simulation.
        ball_vx += steps_per_second * ball_ax;
        ball_vy += steps_per_second * ball_ay;
        ball_px += steps_per_second * ball_vx;
        ball_py += steps_per_second * ball_vy;

        // Render.
        SDL_RenderClear(renderer);
        {
            SDL_Rect dst_rect = {.x = (int)ball_px - ball_width / 2, .y = (int)ball_py - ball_height / 2, .w = ball_width, .h = ball_height};
            SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return EXIT_FAILURE;
}


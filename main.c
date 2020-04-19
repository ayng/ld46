#include <SDL.h>
#include <SDL_image.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char *window_title = "LD46";
const uint32_t screen_width = 1280;
const uint32_t screen_height = 720;
const uint32_t step_size = 8; // 8 milliseconds roughly equates to 120Hz
const float steps_per_second = (float)(step_size)*0.001f;

const float ball_radius = 16.0f;   // pixels
const float player_width = 32.0f;  // pixels
const float player_height = 32.0f; // pixels

const float gravity = 800.0f; // pixels/s/s

const float ball_bounce_vx = 200.0f;       // pixels/s
const float ball_bounce_vy = 600.0f;       // pixels/s
const float player_max_velocity = 300.0f;  // pixels/s
const float player_jump_velocity = 400.0f; // pixels/s

const uint32_t coyote_time = 8; // steps

const float time_to_max_velocity = 18.0f;  // steps
const float time_to_zero_velocity = 18.0f; // steps
const float time_to_pivot = 12.0f;         // steps
const float time_to_squash = 12.0f;        // steps
const float time_to_max_jump = 32.0f;      // steps

const uint32_t max_num_platforms = 32;

typedef struct {
    float px, py, vx, vy;
} body_t;

typedef struct {
    float x, y, w, h;
} platform_t;

bool check_collision_circle_rect(float, float, float, float, float, float, float);
bool check_collision_rect_rect(float, float, float, float, float, float, float, float);

float hold_jump(float);
float accelerate(float);
float decelerate(float);
float pivot(float);

int main() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    SDL_Window *win = SDL_CreateWindow(window_title, 100, 100, screen_width, screen_height, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Surface *loading_surf;

    loading_surf = IMG_Load("assets/ball.png");
    SDL_Texture *ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/player.png");
    SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/player_jumping.png");
    SDL_Texture *player_jumping_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/platform.png");
    SDL_Texture *platform_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/player_platform.png");
    SDL_Texture *player_platform_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    body_t ball = {
        .px = 440.0f + ball_radius,
        .py = 400.0f,
        .vx = 0.0f,
        .vy = -300.0f,
    };

    body_t player = {
        .px = 440.0f,
        .py = 600.0f,
        .vx = 0.0f,
        .vy = 0.0f,
    };

    platform_t platforms[max_num_platforms];
    memset(platforms, 0, max_num_platforms * sizeof(platform_t));
    platforms[0].x = 400.0f;
    platforms[0].y = 680.0f;
    platforms[0].h = 16.0f;
    platforms[0].w = 128.0f;

    platforms[1].x = 600.0f;
    platforms[1].y = 680.0f - 1.0f * player_height;
    platforms[1].h = 16.0f;
    platforms[1].w = 128.0f;
    platforms[2].x = 600.0f;
    platforms[2].y = 680.0f - 2.0f * player_height;
    platforms[2].h = 16.0f;
    platforms[2].w = 128.0f;
    platforms[3].x = 600.0f;
    platforms[3].y = 680.0f - 3.0f * player_height;
    platforms[3].h = 16.0f;
    platforms[3].w = 128.0f;
    platforms[4].x = 600.0f;
    platforms[4].y = 680.0f - 4.0f * player_height;
    platforms[4].h = 16.0f;
    platforms[4].w = 128.0f;
    platforms[5].x = 600.0f;
    platforms[5].y = 680.0f - 5.0f * player_height;
    platforms[5].h = 16.0f;
    platforms[5].w = 128.0f;

    uint32_t last_step_ticks = 0;
    float last_ball_px = 0.0f;
    float last_ball_py = 0.0f;
    float last_player_px = 0.0f;
    float last_player_py = 0.0f;

    bool left_pressed = false;
    bool right_pressed = false;
    bool jump_pressed = false;
    bool player_on_ground = false;
    bool player_carrying_ball = false;
    bool player_jumping = false;

    float player_carry_offset = 0.0f;
    uint32_t ball_carry_time = 0;
    uint32_t air_time = 0;

    platform_t *player_platform = NULL;

    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    left_pressed = true;
                    break;
                case SDLK_RIGHT:
                    right_pressed = true;
                    break;
                case SDLK_SPACE:
                    jump_pressed = true;
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    left_pressed = false;
                    break;
                case SDLK_RIGHT:
                    right_pressed = false;
                    break;
                case SDLK_SPACE:
                    jump_pressed = false;
                    break;
                default:
                    break;
                }
            }
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        // Step ball.
        last_ball_px = ball.px;
        last_ball_py = ball.py;
        ball.vy += steps_per_second * gravity;
        ball.px += steps_per_second * ball.vx;
        ball.py += steps_per_second * ball.vy;

        // Step player.
        last_player_px = player.px;
        last_player_py = player.py;
        if (left_pressed ^ right_pressed) {
            if (left_pressed) {
                if (player.vx > 0.0f) {
                    player.vx = pivot(player.vx);
                } else {
                    player.vx = -accelerate(-player.vx);
                }
            } else {
                if (player.vx < 0.0f) {
                    player.vx = -pivot(-player.vx);
                } else {
                    player.vx = accelerate(player.vx);
                }
            }
        } else {
            if (player.vx > 0.0f) {
                player.vx = decelerate(player.vx);
            } else {
                player.vx = -decelerate(-player.vx);
            }
        }
        if (jump_pressed) {
            if (player_on_ground || air_time < coyote_time) {
                player_on_ground = false;
                player_jumping = true;
            }
            if (air_time < time_to_max_jump) {
                player.vy = -hold_jump(-player.vy);
            }
        }
        player.vy += steps_per_second * gravity;
        player.px += steps_per_second * player.vx;
        player.py += steps_per_second * player.vy;

        // Squash ball.
        if (player_carrying_ball) {
            if (ball_carry_time < time_to_squash) {
                ball.px = player.px + player_carry_offset;
                ball.py = player.py - ball_radius;
                ball_carry_time++;
            } else {
                ball.py = player.py - ball_radius;
                ball.vy = -ball_bounce_vy;
                if (left_pressed ^ right_pressed) {
                    ball.vx = left_pressed ? -ball_bounce_vx : ball_bounce_vx;
                } else {
                    ball.vx = 0.0f;
                }
                player_carrying_ball = false;
                ball_carry_time = 0;
            }
        }

        // Check for collision between ball and player.
        {
            bool collision = check_collision_circle_rect(ball.px, ball.py, ball_radius, player.px, player.py, player_width, player_height);
            if (collision && last_ball_py < player.py && ball.vy > 0) {
                // Enter carry state.
                player_carry_offset = ball.px - player.px;
                player_carrying_ball = true;
            }
        }

        // Check for collision between ball and platform or player and platform.
        player_platform = NULL;
        for (int i = 0; i < max_num_platforms; i++) {
            platform_t *platform = &platforms[i];
            {
                bool collision = check_collision_circle_rect(ball.px, ball.py, ball_radius, platform->x, platform->y, platform->w, platform->h);
                if (collision && last_ball_py < platform->y && ball.vy > 0) {
                    ball.py = platform->y - ball_radius;
                    ball.vy = -ball.vy;
                }
            }
            {
                bool collision = check_collision_rect_rect(player.px, player.py, player_width, player_height, platform->x, platform->y, platform->w, platform->h);
                if (collision && last_player_py + player_height - 0.001f < platform->y && player.vy > 0) {
                    player_platform = platform;
                    player.py = platform->y - player_height;
                    player.vy = 0.0f;
                    player_on_ground = true;
                    player_jumping = false;
                }
            }
        }
        if (player_platform == NULL) {
            player_on_ground = false;
        }

        // Increment air time counter.
        if (!player_on_ground) {
            air_time++;
        } else {
            air_time = 0;
        }

        // Render.
        SDL_RenderClear(renderer);
        for (int i = 0; i < max_num_platforms; i++) {
            platform_t *platform = &platforms[i];
            SDL_Rect dst_rect = {.x = (int)platform->x, .y = (int)platform->y, .w = (int)platform->w, .h = (int)platform->h};
            if (platform == player_platform) {
                SDL_RenderCopy(renderer, player_platform_texture, NULL, &dst_rect);
            } else {
                SDL_RenderCopy(renderer, platform_texture, NULL, &dst_rect);
            }
        }
        {
            SDL_Rect dst_rect = {.x = (int)(ball.px - ball_radius), .y = (int)(ball.py - ball_radius), .w = (int)(ball_radius * 2), .h = (int)(ball_radius * 2)};
            if (player_carrying_ball) {
                float pct = (float)ball_carry_time / (float)time_to_squash;
                pct *= 2;
                pct -= 1;
                pct = pct * pct * pct * pct;
                pct = 1 - pct;
                dst_rect.x -= pct * ball_radius / 2;
                dst_rect.w += pct * ball_radius;
                dst_rect.y += pct * ball_radius * 5 / 8;
                dst_rect.h -= pct * ball_radius * 5 / 8;
            }
            SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
        }
        {
            SDL_Rect dst_rect = {.x = (int)player.px, .y = (int)player.py, .w = (int)player_width, .h = (int)player_height};
            if (player_jumping && jump_pressed && air_time < time_to_max_jump) {
                SDL_RenderCopy(renderer, player_jumping_texture, NULL, &dst_rect);
            } else {
                SDL_RenderCopy(renderer, player_texture, NULL, &dst_rect);
            }
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return EXIT_FAILURE;
}

bool check_collision_rect_rect(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    bool x = bx <= ax + aw && ax <= bx + bw;
    bool y = by <= ay + ah && ay <= by + bh;
    return x && y;
}

bool check_collision_circle_rect(float cx, float cy, float cr, float rx, float ry, float rw, float rh) {
    if (!check_collision_rect_rect(cx - cr, cy - cr, 2 * cr, 2 * cr, rx, ry, rw, rh)) {
        return false;
    }

    // Check which of 9 zones the circle is in:
    //
    //    top left | top    | top right
    // -----------------------------------
    //        left | rect   | right
    // -----------------------------------
    // bottom left | bottom | bottom right
    //
    // rect, left, top, right, bottom: definitely colliding
    // top left, top right, bottom left, bottom right: maybe, but need to further check if a corner of the rect is contained in the circle

    // Short-circuit for rect, left, top, right, bottom.
    if (cx < rx) {
        if (ry <= cy && cy < ry + rh) {
            return true;
        }
    } else if (cx < rx + rw) {
        return true;
    } else {
        if (ry <= cy && cy < ry + rh) {
            return true;
        }
    }

    // Extra check for corner containment in case circle is in a diagonal zone.
    float d0 = (rx - cx) * (rx - cx) + (ry - cy) * (ry - cy);
    float d1 = (rx + rw - cx) * (rx + rw - cx) + (ry - cy) * (ry - cy);
    float d2 = (rx - cx) * (rx - cx) + (ry + rh - cy) * (ry + rh - cy);
    float d3 = (rx + rw - cx) * (rx + rw - cx) + (ry + rh - cy) * (ry + rh - cy);
    float rr = cr * cr;

    return d0 < rr || d1 < rr || d2 < rr || d3 < rr;
}

float square(float x) {
    return x * x;
}

float quadric(float x) {
    return x * x * x * x;
}

float quadrt(float x) {
    return sqrt(sqrt(x));
}

float quintic(float x) {
    return x * x * x * x * x;
}

float quintic_root(float x) {
    return pow(x, .2f);
}

float identity(float x) {
    return x;
}

// Return a value larger than or equal to velocity. Positive values only.
float hold_jump(float velocity) {
    return fmin(player_jump_velocity, player_jump_velocity * quintic_root(quintic(velocity / player_jump_velocity) + 1.0f / time_to_max_jump));
}

// Return a value larger than or equal to velocity. Positive values only.
float accelerate(float velocity) {
    return fmin(player_max_velocity, player_max_velocity * square(sqrt(velocity / player_max_velocity) + 1.0f / time_to_max_velocity));
}

// Return a value less than velocity that approaches zero. Positive values only.
float decelerate(float velocity) {
    return fmax(0.0f, velocity - player_max_velocity / time_to_zero_velocity);
}

// Return a value less than velocity that approaches zero. Positive values only.
float pivot(float velocity) {
    return fmax(0.0f, velocity - player_max_velocity / time_to_pivot);
}


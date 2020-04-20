#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const char *window_title = "LD46";
const uint32_t screen_width = 1280;
const uint32_t screen_height = 720;
const uint32_t step_size = 8; // 8 milliseconds roughly equates to 120Hz
const float steps_per_second = (float)(step_size)*0.001f;

const float ball_radius = 10.0f;   // pixels
const float player_width = 32.0f;  // pixels
const float player_height = 32.0f; // pixels
const float brick_width = 64.0f;   // pixels
const float brick_height = 16.0f;  // pixels

const float gravity = 800.0f;       // pixels/s/s
const float fast_gravity = 2400.0f; // pixels/s/s

const float ball_bounce_vx = 200.0f;                        // pixels/s
const float ball_bounce_vy = 640.0f;                        // pixels/s
const float ball_light_bounce_vx = 80.0f;                   // pixels/s
const float player_max_velocity = 300.0f;                   // pixels/s
const float player_terminal_velocity = 600.0f;              // pixels/s
const float player_jump_velocity = 440.0f;                  // pixels/s
const float player_max_jump_height = 10.0f * player_height; // pixels

const float ball_bounce_attenuation = 0.95f;
const float jump_release_attenuation = 0.9f;

const float ball_no_bounce_velocity = 120.0f; // pixels/s

const uint32_t coyote_time = 12;        // steps
const uint32_t time_to_buffer_jump = 8; // steps
const uint32_t max_time = 65535;        // steps

const float time_to_max_velocity = 18.0f;  // steps
const float time_to_zero_velocity = 18.0f; // steps
const float time_to_pivot = 12.0f;         // steps
const float time_to_squash = 16.0f;        // steps
const float time_to_max_jump = 64.0f;      // steps

const float camera_focus_bottom_margin = 128.0f;
const float camera_move_factor = 0.04f;

const uint32_t max_num_bricks = 256;

typedef struct {
    float px, py, vx, vy;
} body_t;

typedef struct {
    float x, y;
} brick_t;

bool check_collision_circle_rect(float, float, float, float, float, float, float);
bool check_collision_rect_rect(float, float, float, float, float, float, float, float);

float accelerate(float);
float decelerate(float);
float pivot(float);

float rand_range(float, float);
float positive_fmod(float, float);

int main() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return EXIT_FAILURE;
    }

    if (TTF_Init()) {
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

    SDL_SetRenderDrawColor(renderer, 32, 32, 64, 255);

    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Surface *loading_surf;

    loading_surf = IMG_Load("assets/ball3.png");
    SDL_Texture *ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/ball_squash.png");
    SDL_Texture *ball_squash_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/guy2.png");
    SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/guy2_jump.png");
    SDL_Texture *player_jump_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/guy2_fall.png");
    SDL_Texture *player_fall_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/brick2.png");
    SDL_Texture *brick_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    TTF_Font *font = TTF_OpenFont("assets/Hack-Regular.ttf", 24);
    assert(font != NULL);

    int glyph_width, glyph_height;
    TTF_SizeText(font, "a", &glyph_width, &glyph_height);
    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Texture *number_textures[10];
    for (int i = 0; i < 10; i++) {
        loading_surf = TTF_RenderGlyph_Blended(font, '0' + i, text_color);
        number_textures[i] = SDL_CreateTextureFromSurface(renderer, loading_surf);
    }

init:;

    srand(time(NULL));
    float start_x = rand_range(128.0f, screen_width - 128.0f);
    float start_y = 128.0f;

    body_t ball = {
        .px = start_x,
        .py = start_y + 256.0f,
    };

    body_t player = {
        .px = start_x - player_width / 2,
        .py = start_y + 128.0f,
    };

    brick_t bricks[max_num_bricks];
    memset(bricks, 0, max_num_bricks * sizeof(brick_t));
    bricks[0].x = start_x - brick_width / 2.0f;
    bricks[0].y = start_y;
    bricks[1].x = start_x - brick_width * 3.0f / 2.0f;
    bricks[1].y = start_y;
    bricks[2].x = start_x + brick_width / 2.0f;
    bricks[2].y = start_y;

    float last_x = start_x;
    float last_y = start_y;
    for (int i = 3; i < 255; i += 3) {
        float x = rand_range(0.0f, 1.0f) > 0.5f ? rand_range(last_x + 3.0f * brick_width, last_x + 6.0f * brick_width) : rand_range(last_x - 9.0f * brick_width, last_x - 6.0f * brick_width);
        float y = last_y + rand_range(1.5f * player_height, 2.0f * player_height);
        last_x = x;
        last_y = y;
        bricks[i].x = last_x - brick_width / 2.0f;
        bricks[i].y = last_y;
        bricks[i + 1].x = last_x - brick_width * 3.0f / 2.0f;
        bricks[i + 1].y = last_y;
        bricks[i + 2].x = last_x + brick_width / 2.0f;
        bricks[i + 2].y = last_y;
    }

    int next_brick = 0;

    uint32_t last_step_ticks = 0;
    float last_ball_px = 0.0f;
    float last_ball_py = 0.0f;
    float last_player_px = 0.0f;
    float last_player_py = 0.0f;

    bool left_pressed = false;
    bool right_pressed = false;
    bool down_pressed = false;
    bool player_on_ground = false;
    bool player_carrying_ball = false;
    bool player_jumping = false;
    bool ball_bouncing = false;

    bool left_pressed_entering_carry_state = false;
    bool right_pressed_entering_carry_state = false;

    float player_carry_offset = 0.0f;
    float stored_ball_vx = 0.0f;
    float stored_ball_vy = 0.0f;
    float stored_ball_py = 0.0f;

    uint32_t ball_carry_time = 0;
    uint32_t ball_bounce_time = 0;
    uint32_t air_time = 0;
    uint32_t jump_time = 0;
    uint32_t time_since_jump_press = max_time;
    uint32_t time_since_jump_release = max_time - 1;

    float camera_y = 0.0f;
    float camera_focus_y = bricks[0].y;

    brick_t *player_brick = NULL;
    brick_t *hit_brick = NULL;

    bool game_over = false;

    uint32_t score = 0;

    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
            // Ignore key repeats.
            if (e.key.repeat) {
                goto after_handle_input;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_a:
                    left_pressed = true;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    right_pressed = true;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    down_pressed = true;
                    break;
                case SDLK_r:
                    goto init;
                    break;
                case SDLK_SPACE:
                    time_since_jump_press = 0;
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                case SDLK_a:
                    left_pressed = false;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    right_pressed = false;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    down_pressed = false;
                    break;
                case SDLK_SPACE:
                    time_since_jump_release = 0;
                    break;
                default:
                    break;
                }
            }
        }
    after_handle_input:;

        if (game_over) {
            continue;
        }

        uint32_t ticks = SDL_GetTicks();
        if (ticks - last_step_ticks < step_size) {
            continue;
        }
        last_step_ticks = ticks - ticks % step_size;

        // Step ball.
        last_ball_px = ball.px;
        last_ball_py = ball.py;
        if (!ball_bouncing) {
            ball.vy -= steps_per_second * gravity;
        }
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
        // Initiate jump if possible.
        if (time_since_jump_press < time_to_buffer_jump) {
            // Jump has just been pressed or is buffered.
            if (player_on_ground || air_time < coyote_time) {
                // Player is able to jump.
                player.vy = player_jump_velocity;
                player_jumping = true;
            }
        }
        if (jump_time > time_to_max_jump) {
            // Max jump has been reached.
            player_jumping = false;
        }
        if (!player_jumping && down_pressed) {
            player.vy -= steps_per_second * fast_gravity;
        } else {
            player.vy -= steps_per_second * gravity;
        }

        player.px += steps_per_second * player.vx;
        player.py += steps_per_second * player.vy;

        // Squash ball.
        if (player_carrying_ball) {
            ball.py = player.py + player_height + ball_radius;
            if (ball_carry_time < time_to_squash) {
                ball.px = player.px + player_carry_offset;
                ball_carry_time++;
            } else {
                ball.vy = ball_bounce_vy;
                if (left_pressed ^ right_pressed) {
                    if (left_pressed) {
                        if (left_pressed_entering_carry_state) {
                            ball.vx = -ball_bounce_vx;
                        } else {
                            ball.vx = -ball_light_bounce_vx;
                        }
                    } else if (right_pressed) {
                        if (right_pressed_entering_carry_state) {
                            ball.vx = ball_bounce_vx;
                        } else {
                            ball.vx = ball_light_bounce_vx;
                        }
                    }
                } else {
                    ball.vx = 0.0f;
                }
                right_pressed_entering_carry_state = false;
                left_pressed_entering_carry_state = false;
                player_carrying_ball = false;
                ball_carry_time = 0;
            }
        }
        if (ball_bouncing) {
            if (ball_bounce_time < time_to_squash) {
                ball_bounce_time++;
            } else {
                ball.vx = stored_ball_vx;
                ball.vy = stored_ball_vy;
                ball.py = stored_ball_py;
                ball_bouncing = false;
                ball_bounce_time = 0;
                // Break brick.
                hit_brick->x = 0;
                hit_brick->y = 0;
                hit_brick = NULL;
                score++;
            }
        }

        // Check if ball falls off the bottom of screen.
        if (ball.py + ball_radius < camera_y) {
            game_over = true;
        }

        // Check for collision between ball and player.
        if (!player_carrying_ball) {
            bool collision =
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height);
            if (collision && last_ball_py > player.py + player_height && ball.vy < 0) {
                // Enter carry state.
                player_carry_offset = ball.px - player.px;
                left_pressed_entering_carry_state = left_pressed;
                right_pressed_entering_carry_state = right_pressed;
                player_carrying_ball = true;
            }
        }

        // Check for collision between ball and brick or player and brick.
        player_brick = NULL;
        for (int i = 0; i < max_num_bricks; i++) {
            brick_t *brick = &bricks[i];
            {
                bool collision =
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                    check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                                positive_fmod(brick->x, (float)screen_width) - screen_width, brick->y, brick_width, brick_height);
                if (collision && last_ball_py - ball_radius + 0.001f > brick->y + brick_height && ball.vy < 0) {
                    ball.py = brick->y + brick_height + ball_radius;
                    ball_bouncing = true;
                    stored_ball_vx = ball.vx;
                    stored_ball_vy = -ball_bounce_attenuation * ball.vy;
                    ball.vx = 0.0f;
                    ball.vy = 0.0f;
                    stored_ball_py = ball.py;
                    hit_brick = brick;
                }
            }
            {
                bool collision =
                    check_collision_rect_rect(positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height,
                                              positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                    check_collision_rect_rect(positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height,
                                              positive_fmod(brick->x, (float)screen_width) - screen_width, brick->y, brick_width, brick_height);
                if (collision && last_player_py + 0.001f > brick->y + brick_height && player.vy < 0) {
                    camera_focus_y = fmax(camera_focus_y, brick->y);
                    player_brick = brick;
                    player.py = brick->y + brick_height;
                    player.vy = 0.0f;
                    player_on_ground = true;
                    player_jumping = false;
                }
            }
        }
        if (player_brick == NULL) {
            player_on_ground = false;
        }

        // Move camera.
        float camera_target_y = camera_focus_y - camera_focus_bottom_margin;
        if (fabs(camera_y - camera_target_y) > 0.001f) {
            camera_y = (1.0f - camera_move_factor) * camera_y + camera_move_factor * camera_target_y;
        }

        // Increment counters.
        if (!player_on_ground) {
            air_time++;
            if (player_jumping) {
                jump_time++;
            }
        } else {
            air_time = 0;
            jump_time = 0;
        }
        if (time_since_jump_press < max_time) {
            time_since_jump_press++;
        }
        if (time_since_jump_release < max_time - 1) {
            time_since_jump_release++;
        }

        // Render.
        SDL_RenderClear(renderer);
        for (int i = 0; i < max_num_bricks; i++) {
            brick_t *brick = &bricks[i];
            if (brick->x == 0 && brick->y == 0) {
                continue;
            }
            SDL_Rect dst_rect = {.x = (int)brick->x, .y = screen_height - (int)(brick->y + brick_height - camera_y), .w = (int)brick_width, .h = (int)brick_height};
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;
            SDL_RenderCopy(renderer, brick_texture, NULL, &dst_rect);
            SDL_RenderCopy(renderer, brick_texture, NULL, &wrap_rect);
        }
        {
            SDL_Rect dst_rect = {.x = (int)(ball.px - ball_radius), .y = screen_height - (int)(ball.py + ball_radius - camera_y), .w = (int)(ball_radius * 2), .h = (int)(ball_radius * 2)};
            if (player_carrying_ball || ball_bouncing) {
                const int ball_squash_width = 2.0f * ball_radius + 4.0f * 4.0f;
                float x = ball.px - (float)ball_squash_width / 2.0f;
                dst_rect.w = ball_squash_width;
                dst_rect.x = x;
            }
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;
            if (player_carrying_ball || ball_bouncing) {
                SDL_RenderCopy(renderer, ball_squash_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, ball_squash_texture, NULL, &wrap_rect);
            } else {
                SDL_RenderCopy(renderer, ball_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, ball_texture, NULL, &wrap_rect);
            }
        }
        {
            SDL_Rect dst_rect = {.x = (int)player.px, .y = screen_height - (int)(player.py + player_height - camera_y), .w = (int)player_width, .h = (int)player_height};
            dst_rect.x = positive_fmod(dst_rect.x, screen_width);
            SDL_Rect wrap_rect = dst_rect;
            wrap_rect.x -= screen_width;
            if (player_on_ground || air_time < coyote_time) {
                SDL_RenderCopy(renderer, player_texture, NULL, &dst_rect);
                SDL_RenderCopy(renderer, player_texture, NULL, &wrap_rect);
            } else {
                if (player_jumping) {
                    SDL_RenderCopy(renderer, player_jump_texture, NULL, &dst_rect);
                    SDL_RenderCopy(renderer, player_jump_texture, NULL, &wrap_rect);
                } else {
                    SDL_RenderCopy(renderer, player_fall_texture, NULL, &dst_rect);
                    SDL_RenderCopy(renderer, player_fall_texture, NULL, &wrap_rect);
                }
            }
        }
        {
            int digit = score;
            int i = 0;
            do {
                SDL_Rect dst_rect = {screen_width - glyph_width * (i + 1), screen_height - glyph_height, glyph_width, glyph_height};
                SDL_RenderCopy(renderer, number_textures[digit % 10], NULL, &dst_rect);
                digit /= 10;
                i++;
            } while (digit > 0);
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    TTF_Quit();

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

float rand_range(float min, float max) {
    float r = (float)rand() / (float)RAND_MAX;
    return min + r * (max - min);
}

float positive_fmod(float x, float mod) {
    float xm = fmod(x, mod);
    if (xm < 0) {
        return xm + mod;
    }
    return xm;
}


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

const char *window_title = "LD46 - Icy Mountain Hot Potato";
const uint32_t screen_width = 1280;
const uint32_t screen_height = 720;
const float seconds_per_frame = 1.0f / 60.0f;

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
const float player_jump_velocity = 500.0f;                  // pixels/s
const float player_max_jump_height = 10.0f * player_height; // pixels

const float ball_bounce_attenuation = 0.95f;
const float jump_release_attenuation = 0.9f;

const float ball_no_bounce_velocity = 120.0f; // pixels/s

const uint32_t coyote_time = 6;         // steps
const uint32_t time_to_buffer_jump = 8; // steps
const uint32_t max_time = 65535;        // steps

const float time_to_max_velocity = 9.0f;  // steps
const float time_to_zero_velocity = 9.0f; // steps
const float time_to_pivot = 6.0f;         // steps
const float time_to_squash = 8.0f;        // steps
const float time_to_max_jump = 32.0f;     // steps

const float camera_focus_bottom_margin = 128.0f;
const float camera_move_factor = 0.04f;

#define MAX_NUM_BRICKS 256

const char *game_over_text = " press R to restart ";

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

int next_brick;

uint32_t last_step_ticks;
float last_ball_px;
float last_ball_py;
float last_player_px;
float last_player_py;

bool left_pressed;
bool right_pressed;
bool down_pressed;
bool reset_pressed;
bool jump_pressed;
bool player_on_ground;
bool player_carrying_ball;
bool player_jumping;
bool ball_bouncing;

bool left_pressed_entering_carry_state;
bool right_pressed_entering_carry_state;

float player_carry_offset;
float stored_ball_vx;
float stored_ball_vy;
float stored_ball_py;

uint32_t ball_carry_time;
uint32_t ball_bounce_time;
uint32_t air_time;
uint32_t jump_time;
uint32_t time_since_jump_press;
uint32_t time_since_jump_release;

float camera_y;
float camera_focus_y;

brick_t *player_brick;
brick_t *hit_brick;

bool game_over;

uint32_t high_score;
uint32_t score;

struct timeval tv;

body_t ball, player;

brick_t bricks[MAX_NUM_BRICKS];

SDL_Window *win;
SDL_Renderer *renderer;
SDL_Surface *loading_surf;
SDL_Texture *ball_texture, *ball_squash_texture, *player_texture, *player_jump_texture, *player_fall_texture, *brick_texture;
SDL_Texture *white_number_textures[10];
SDL_Texture *yellow_number_textures[10];
SDL_Texture *game_over_text_texture;
Mix_Chunk *sfx_jump, *sfx_game_over, *sfx_bounce_start, *sfx_bounce_end, *sfx_brick_break;
TTF_Font *font;

int glyph_width, glyph_height;
int game_over_text_width, game_over_text_height;

bool should_quit = false;

void init() {
    gettimeofday(&tv, NULL);
    srand((uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000);
    //float start_x = rand_range(128.0f, screen_width - 128.0f);
    float start_x = 34.0f;
    float start_y = 128.0f;

    ball = (body_t){
        .px = start_x,
        .py = start_y + player_height * 6.0f,
    };

    player = (body_t){
        .px = start_x - player_width * 0.5f,
        .py = start_y + player_height * 2.0f,
    };

    memset(bricks, 0, MAX_NUM_BRICKS * sizeof(brick_t));

    bricks[0].x = start_x - brick_width / 2.0f;
    bricks[0].y = start_y;
    //bricks[1].x = start_x - brick_width * 3.0f / 2.0f;
    //bricks[1].y = start_y;
    bricks[2].x = start_x - brick_width * 3.0f / 2.0f + 16.0f;
    bricks[2].y = start_y + 64.0f;

    float last_x = start_x;
    float last_y = start_y;

    {
        int i = 3;
        float x = rand_range(start_x + 3.0f * brick_width, start_x + 6.0f * brick_width);
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

    {
        int i = 6;
        float x = rand_range(start_x - 9.0f * brick_width, start_x - 6.0f * brick_width);
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

    {
        int i = 9;
        float x = rand_range(start_x + 6.0f * brick_width, start_x + 9.0f * brick_width);
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

    for (int i = 12; i < 255; i += 3) {
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

    next_brick = 0;

    last_step_ticks = 0;
    last_ball_px = 0.0f;
    last_ball_py = 0.0f;
    last_player_px = 0.0f;
    last_player_py = 0.0f;

    left_pressed = false;
    right_pressed = false;
    down_pressed = false;
    player_on_ground = false;
    player_carrying_ball = false;
    player_jumping = false;
    ball_bouncing = false;
    left_pressed_entering_carry_state = false;
    right_pressed_entering_carry_state = false;

    player_carry_offset = 0.0f;
    stored_ball_vx = 0.0f;
    stored_ball_vy = 0.0f;
    stored_ball_py = 0.0f;
    ball_carry_time = 0;
    ball_bounce_time = 0;
    air_time = 0;
    jump_time = 0;
    time_since_jump_press = max_time;
    time_since_jump_release = max_time - 1;

    camera_y = 0.0f;
    camera_focus_y = bricks[0].y;

    player_brick = NULL;
    hit_brick = NULL;

    game_over = false;

    score = 0;
}

void one_iter() {
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            should_quit = true;
            return;
        }
    }
    const Uint8 *keystates = SDL_GetKeyboardState(NULL);
    left_pressed = keystates[SDL_SCANCODE_A] || keystates[SDL_SCANCODE_LEFT];
    right_pressed = keystates[SDL_SCANCODE_D] || keystates[SDL_SCANCODE_RIGHT];
    down_pressed = keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN];

    if (!reset_pressed && keystates[SDL_SCANCODE_R]) {
        reset_pressed = keystates[SDL_SCANCODE_R];
        init();
        return;
    }

    if (reset_pressed && !keystates[SDL_SCANCODE_R]) {
        reset_pressed = keystates[SDL_SCANCODE_R];
    }

    if (!jump_pressed && keystates[SDL_SCANCODE_SPACE]) {
        jump_pressed = keystates[SDL_SCANCODE_SPACE];
        time_since_jump_press = 0;
    }

    if (jump_pressed && !keystates[SDL_SCANCODE_SPACE]) {
        jump_pressed = keystates[SDL_SCANCODE_SPACE];
        time_since_jump_release = 0;
    }

    if (game_over) {
        return;
    }

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
        if (!player_jumping && (player_on_ground || air_time < coyote_time)) {
            // Player is able to jump.
            player.vy = player_jump_velocity;
            player_jumping = true;
            Mix_PlayChannel(-1, sfx_jump, 0);
        }
    }
    if (jump_time > time_to_max_jump) {
        // Max jump has been reached.
        player_jumping = false;
    }
    if (!player_jumping && down_pressed) {
        player.vy -= seconds_per_frame * fast_gravity;
    } else {
        player.vy -= seconds_per_frame * gravity;
    }
    player.px += seconds_per_frame * player.vx;
    player.py += seconds_per_frame * player.vy;

    // Step ball.
    last_ball_px = ball.px;
    last_ball_py = ball.py;
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
            Mix_PlayChannel(-1, sfx_bounce_end, 0);
        }
    } else if (ball_bouncing) {
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
            Mix_PlayChannel(-1, sfx_brick_break, 0);
            score++;
            if (score > high_score) {
                high_score = score;
            }
        }
    } else {
        ball.vy -= seconds_per_frame * gravity;
        ball.px += seconds_per_frame * ball.vx;
        ball.py += seconds_per_frame * ball.vy;
    }

    // Check if ball falls off the bottom of screen.
    if (ball.py + ball_radius < camera_y) {
        game_over = true;
        Mix_PlayChannel(-1, sfx_game_over, 0);
    }

    // Check for collision between ball and player.
    if (!player_carrying_ball) {
        bool collision =
            check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                        positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
            check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width) - screen_width, ball.py, ball_radius,
                                        positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height) ||
            check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                        positive_fmod(player.px, (float)screen_width) - screen_width, player.py, player_width, player_height);
        if (collision && last_ball_py > player.py + player_height && ball.vy <= 0.0f) {
            // Enter carry state.
            player_carry_offset = ball.px - player.px;
            left_pressed_entering_carry_state = left_pressed;
            right_pressed_entering_carry_state = right_pressed;
            player_carrying_ball = true;
            // Cancel bounce if needed.
            if (ball_bouncing) {
                ball_bouncing = false;
                ball_bounce_time = 0;
                // Break brick.
                hit_brick->x = 0;
                hit_brick->y = 0;
                hit_brick = NULL;
                Mix_PlayChannel(-1, sfx_brick_break, 0);
                score++;
                if (score > high_score) {
                    high_score = score;
                }
            }

            Mix_PlayChannel(-1, sfx_bounce_start, 0);
        }
    }

    // Check for collision between ball and brick or player and brick.
    player_brick = NULL;
    for (int i = 0; i < MAX_NUM_BRICKS; i++) {
        brick_t *brick = &bricks[i];
        if (brick->x == 0 && brick->y == 0) {
            continue;
        }
        if (brick->y + brick_height < camera_y) {
            // Off-screen bricks don't have collision.
            continue;
        }
        if (!player_carrying_ball) {
            bool collision =
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width), ball.py, ball_radius,
                                            positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                check_collision_circle_rect(positive_fmod(ball.px, (float)screen_width) - screen_width, ball.py, ball_radius,
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
                Mix_PlayChannel(-1, sfx_bounce_start, 0);
            }
        }
        {
            bool collision =
                check_collision_rect_rect(positive_fmod(player.px, (float)screen_width), player.py, player_width, player_height,
                                          positive_fmod(brick->x, (float)screen_width), brick->y, brick_width, brick_height) ||
                check_collision_rect_rect(positive_fmod(player.px, (float)screen_width) - screen_width, player.py, player_width, player_height,
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
    for (int i = 0; i < MAX_NUM_BRICKS; i++) {
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
            SDL_Rect dst_rect = {screen_width - glyph_width * (i + 1), screen_height - 2.0f * glyph_height, glyph_width, glyph_height};
            SDL_RenderCopy(renderer, white_number_textures[digit % 10], NULL, &dst_rect);
            digit /= 10;
            i++;
        } while (digit > 0);
    }
    {
        int digit = high_score;
        int i = 0;
        do {
            SDL_Rect dst_rect = {screen_width - glyph_width * (i + 1), screen_height - glyph_height, glyph_width, glyph_height};
            SDL_RenderCopy(renderer, yellow_number_textures[digit % 10], NULL, &dst_rect);
            digit /= 10;
            i++;
        } while (digit > 0);
    }
    if (game_over) {
        SDL_Rect dst_rect = {screen_width * 0.5f - game_over_text_width * 0.5f, screen_height * 0.5f - game_over_text_height * 0.5f, game_over_text_width, game_over_text_height};
        SDL_RenderCopy(renderer, game_over_text_texture, NULL, &dst_rect);
    }
    SDL_RenderPresent(renderer);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return EXIT_FAILURE;
    }

    if (TTF_Init()) {
        return EXIT_FAILURE;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)) {
        return EXIT_FAILURE;
    }

    Mix_Volume(-1, MIX_MAX_VOLUME / 4);

    win = SDL_CreateWindow(window_title, 100, 100, screen_width, screen_height, 0);
    if (win == NULL) {
        return EXIT_FAILURE;
    }

    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawColor(renderer, 32, 32, 64, 255);

    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    loading_surf = IMG_Load("assets/ball3.png");
    printf("%s\n", IMG_GetError());
    assert(loading_surf != NULL);
    ball_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    assert(ball_texture != NULL);
    loading_surf = IMG_Load("assets/ball_squash.png");
    ball_squash_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/guy2.png");
    player_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/guy2_jump.png");
    player_jump_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    loading_surf = IMG_Load("assets/guy2_fall.png");
    player_fall_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    loading_surf = IMG_Load("assets/brick2.png");
    brick_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);
    assert(brick_texture != NULL);

    font = TTF_OpenFont("assets/Hack-Regular.ttf", 24);
    assert(font != NULL);

    TTF_SizeText(font, "a", &glyph_width, &glyph_height);
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {232, 234, 74, 255};
    SDL_Color black = {0, 0, 0, 255};
    for (int i = 0; i < 10; i++) {
        loading_surf = TTF_RenderGlyph_Blended(font, '0' + i, white);
        white_number_textures[i] = SDL_CreateTextureFromSurface(renderer, loading_surf);
        loading_surf = TTF_RenderGlyph_Blended(font, '0' + i, yellow);
        yellow_number_textures[i] = SDL_CreateTextureFromSurface(renderer, loading_surf);
    }

    TTF_SizeText(font, game_over_text, &game_over_text_width, &game_over_text_height);
    loading_surf = TTF_RenderText_Shaded(font, game_over_text, white, black);
    game_over_text_texture = SDL_CreateTextureFromSurface(renderer, loading_surf);

    sfx_jump = Mix_LoadWAV("assets/jump.wav");
    assert(sfx_jump != NULL);
    sfx_game_over = Mix_LoadWAV("assets/game_over.wav");
    assert(sfx_game_over != NULL);
    sfx_bounce_start = Mix_LoadWAV("assets/bounce_start.wav");
    assert(sfx_bounce_start != NULL);
    sfx_bounce_end = Mix_LoadWAV("assets/bounce_end.wav");
    assert(sfx_bounce_end != NULL);
    sfx_brick_break = Mix_LoadWAV("assets/kick3.wav");
    assert(sfx_brick_break != NULL);

    init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(one_iter, 0, 1);
#else
    while (!should_quit) {
        one_iter();
        SDL_Delay(16);
    }
#endif

    Mix_FreeChunk(sfx_jump);
    Mix_FreeChunk(sfx_game_over);
    Mix_FreeChunk(sfx_bounce_start);
    Mix_FreeChunk(sfx_bounce_end);
    Mix_FreeChunk(sfx_brick_break);
    Mix_CloseAudio();
    Mix_Quit();

    IMG_Quit();

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return EXIT_SUCCESS;
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


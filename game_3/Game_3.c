#include "Game_3.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "PWM.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

#define COURT_TOP        20
#define COURT_BOTTOM     186
#define COURT_LEFT       5
#define COURT_RIGHT      235
#define NET_X            120
#define NET_TOP          146
#define FLOOR_Y          COURT_BOTTOM
#define FLOOR_BOTTOM     240

#define PLAYER_START_X   50
#define PLAYER_Y         185
#define PLAYER_SPEED     3
#define PLAYER_MIN_X     (COURT_LEFT + 18)
#define PLAYER_MAX_X     (NET_X - 18)

#define OPPONENT_X       196
#define OPPONENT_SPEED   2
#define OPPONENT_MIN_X   (NET_X + 18)
#define OPPONENT_MAX_X   (COURT_RIGHT - 18)
#define OPPONENT_HOME_X  OPPONENT_X
#define OPPONENT_LEAD_FRAMES  3

#define SHUTTLE_R        4
#define FP_SCALE         10   // positions stored as int*10, no floats

#define PLAYER_SERVE_X_OFFSET    12
#define PLAYER_SERVE_Y_OFFSET   (-28)
#define OPPONENT_SERVE_X_OFFSET  12
#define OPPONENT_SERVE_Y_OFFSET (-28)
#define OPPONENT_SERVE_DELAY_MS  900

#define RESET_CENTER_X   NET_X
#define RESET_CENTER_Y   100
#define GAME_OVER_SCORE  5

// shuttle physics (fixed-point)
#define SFP_INIT_DX      30
#define SFP_INIT_DY    (-50)
#define SFP_HIT_DX       26
#define SFP_HIT_DY     (-48)
#define SFP_GRAVITY_UP   2
#define SFP_GRAVITY_DOWN 4
#define SFP_DRAG_INTERVAL 2
#define SFP_MAX_DX       60
#define SFP_MIN_DX       20
#define SFP_MAX_DY       60

#define SFP_BOUND_L  ((COURT_LEFT   + SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_R  ((COURT_RIGHT  - SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_T  ((COURT_TOP    + SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_B  ((COURT_BOTTOM - SHUTTLE_R) * FP_SCALE)
#define SFP_NET_X    (NET_X * FP_SCALE)

#define FRAME_TIME_MS    33
#define WALK_ANIM_SPEED  6
#define SWING_DURATION   10
#define HIT_COOLDOWN_FRAMES      8
#define OPPONENT_AI_REACT_FRAMES 3
#define START_PROMPT_BLINK_MS    360

// hit box offsets
#define PLAYER_HIT_X0_OFFSET  (-8)
#define PLAYER_HIT_X1_OFFSET  14
#define PLAYER_HIT_Y0_OFFSET  (-40)
#define PLAYER_HIT_Y1_OFFSET  0

#define OPPONENT_HIT_X0_OFFSET  (-14)
#define OPPONENT_HIT_X1_OFFSET  8
#define OPPONENT_HIT_Y0_OFFSET  (-40)
#define OPPONENT_HIT_Y1_OFFSET  0

// Player animation states
typedef enum {
    ANIM_IDLE  = 0,
    ANIM_WALK,
    ANIM_SWING
} AnimState;

typedef enum {
    GAME_STATE_START = 0,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    int16_t x;
    int16_t y;
    AnimState state;
    uint8_t anim_frame;
    uint8_t frame_count;
    uint8_t swing_timer;
    int8_t facing;  // +1 right, -1 left
} Stickman;

// Shuttlecock (badminton ball)
// x, y = position, dx dy = speed
// dy < 0 means moving up
typedef struct {
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;
    uint8_t drag_tick;
} Shuttlecock;

static void play_short_beep(uint32_t* stop_tick, uint32_t freq_hz, uint8_t volume, uint32_t duration_ms);

static void draw_racket_head(int16_t wx, int16_t wy, int16_t adx, int16_t ady, uint8_t col)
{
    int16_t s = (adx < 0 ? -adx : adx);
    if ((ady < 0 ? -ady : ady) > s) s = (ady < 0 ? -ady : ady);
    if (s == 0) { s = 1; adx = 1; }

    int16_t cx = wx + adx * 3 / s;
    int16_t cy = wy + ady * 3 / s;
    int16_t ux = adx * 5 / s, uy = ady * 5 / s;
    int16_t nx = -ady * 2 / s, ny = adx * 2 / s;

    LCD_Draw_Line(cx-ux,       cy-uy,       cx-ux/2+nx, cy-uy/2+ny, col);
    LCD_Draw_Line(cx-ux/2+nx,  cy-uy/2+ny,  cx+ux/2+nx, cy+uy/2+ny, col);
    LCD_Draw_Line(cx+ux/2+nx,  cy+uy/2+ny,  cx+ux,      cy+uy,      col);
    LCD_Draw_Line(cx+ux,       cy+uy,       cx+ux/2-nx, cy+uy/2-ny, col);
    LCD_Draw_Line(cx+ux/2-nx,  cy+uy/2-ny,  cx-ux/2-nx, cy-uy/2-ny, col);
    LCD_Draw_Line(cx-ux/2-nx,  cy-uy/2-ny,  cx-ux,      cy-uy,      col);
}

static void draw_net(void)
{
    int16_t back_x = NET_X - 8;
    int16_t front_x = NET_X + 2;
    int16_t top_back_y = NET_TOP + 8;
    int16_t top_front_y = NET_TOP;

    LCD_Draw_Line(back_x,  top_back_y,  back_x,  FLOOR_Y, 0);
    LCD_Draw_Line(front_x, top_front_y, front_x, FLOOR_Y, 0);

    for (int16_t x = back_x + 1; x <= front_x - 1; x++) {
        int16_t y_top = top_back_y + (int16_t)(((top_front_y - top_back_y) * (x - back_x)) / (front_x - back_x));
        LCD_Draw_Line(x, y_top, x, FLOOR_Y - 1, 13);
    }

    LCD_Draw_Line(back_x - 1, top_back_y - 1, front_x + 1, top_front_y - 1, 0);
    LCD_Draw_Line(back_x - 1, top_back_y,     front_x + 1, top_front_y,     0);
    LCD_Draw_Line(back_x,     top_back_y,     front_x,     top_front_y,     13);

    for (int16_t y = top_front_y + 2; y <= FLOOR_Y - 2; y += 4) {
        LCD_Draw_Line(back_x + 1, y + 8, front_x - 1, y, 13);
    }
    for (int16_t y = top_front_y + 1; y <= FLOOR_Y - 2; y += 4) {
        LCD_Draw_Line(back_x + 1, y, front_x - 1, y + 8, 13);
    }
    for (int16_t x = back_x + 2; x <= front_x - 2; x += 2) {
        LCD_Draw_Line(x, top_front_y + 2, x, FLOOR_Y - 1, 14);
    }

    LCD_Draw_Rect(back_x - 1,  FLOOR_Y - 1, 3, 12, 0, true);
    LCD_Draw_Rect(front_x - 1, FLOOR_Y - 1, 3, 26, 0, true);
    LCD_Draw_Line(back_x - 4,  FLOOR_Y + 10, back_x + 4,  FLOOR_Y + 10, 0);
    LCD_Draw_Line(front_x - 4, FLOOR_Y + 24, front_x + 4, FLOOR_Y + 24, 0);
    LCD_Draw_Line(front_x + 1, top_front_y + 1, front_x + 1, FLOOR_Y + 25, 13);
}

static void draw_court(void)
{
    // Draw floor
    LCD_Draw_Rect(0, FLOOR_Y, 240, FLOOR_BOTTOM - FLOOR_Y, 10, true);

    // Court boundary lines
    LCD_Draw_Line(0, FLOOR_Y, 239, FLOOR_Y, 0);
    LCD_Draw_Line(COURT_LEFT, COURT_TOP, COURT_LEFT, FLOOR_Y, 0);
    LCD_Draw_Line(0, FLOOR_BOTTOM - 1, COURT_LEFT, FLOOR_Y, 0);

    // Service lines
    for (int16_t i = 0; i < 10; i++) {
        int16_t lx = 75 + i * 2;
        int16_t ly = 236 - i * 5;
        LCD_Draw_Rect(lx, ly, 2, 4, 6, true);
    }
    for (int16_t i = 0; i < 10; i++) {
        int16_t rx = 205 - i * 2;
        int16_t ry = 236 - i * 5;
        LCD_Draw_Rect(rx, ry, 2, 4, 6, true);
    }

    draw_net();
}

static void erase_rect_background(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    if (x0 < 0)   x0 = 0;
    if (y0 < 0)   y0 = 0;
    if (x1 > 239) x1 = 239;
    if (y1 > 239) y1 = 239;
    if (x0 > x1 || y0 > y1) return;

    if (y0 < FLOOR_Y) {
        int16_t wy1 = (y1 < (FLOOR_Y - 1)) ? y1 : (FLOOR_Y - 1);
        LCD_Draw_Rect((uint16_t)x0, (uint16_t)y0, (uint16_t)(x1-x0+1), (uint16_t)(wy1-y0+1), 13, true);
    }
    if (y1 >= FLOOR_Y) {
        int16_t fy0 = (y0 > FLOOR_Y) ? y0 : FLOOR_Y;
        LCD_Draw_Rect((uint16_t)x0, (uint16_t)fy0, (uint16_t)(x1-x0+1), (uint16_t)(y1-fy0+1), 10, true);
    }
}

static void erase_stickman(int16_t px, int16_t py)
{
    // Swing needs bigger erase box
    erase_rect_background(px - 35, py - 50, 71, 53);
}

static void erase_shuttle(int16_t sx, int16_t sy)
{
    // Shuttle erase box
    erase_rect_background(sx - 10, sy - 8, 21, 17);
}

// Draw player
// IDLE = standing, WALK = moving, SWING = hitting
// px py = feet position
// facing: +1 right, -1 left
static void draw_stickman_at(int16_t px, int16_t py,
                              int8_t facing, AnimState state,
                              uint8_t anim_frame)
{
    const uint8_t body_colour = 0;

    // Head
    LCD_Draw_Circle(px, py - 38, 5, 1, true);
    LCD_Draw_Circle(px, py - 38, 5, body_colour, false);

    // Body
    LCD_Draw_Line(px,     py - 32, px,     py - 14, body_colour);
    LCD_Draw_Line(px + 1, py - 32, px + 1, py - 14, body_colour);

    // Arms
    if (state == ANIM_SWING) {
        int16_t ra_ex = px + facing * 18;
        int16_t ra_ey = py - 40;
        LCD_Draw_Line(px,     py - 26, ra_ex,     ra_ey, body_colour);
        LCD_Draw_Line(px + 1, py - 26, ra_ex + 1, ra_ey, body_colour);
        draw_racket_head(ra_ex, ra_ey, ra_ex - px, ra_ey - (py - 26), body_colour);
        LCD_Draw_Line(px,     py - 26, px - facing * 10,     py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 10 + 1, py - 18, body_colour);
    } else if (state == ANIM_WALK) {
        int16_t arm_swing = (anim_frame == 0) ? 8 : -8;
        LCD_Draw_Line(px,     py - 26, px - facing * 8,      py - 20 + arm_swing / 2, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 8 + 1,  py - 20 + arm_swing / 2, body_colour);
        LCD_Draw_Line(px,     py - 26, px + facing * 10,     py - 20 - arm_swing / 2, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px + facing * 10 + 1, py - 20 - arm_swing / 2, body_colour);
        draw_racket_head(px + facing * 10, py - 20 - arm_swing / 2,
                         facing * 10, 6 - arm_swing / 2, body_colour);
    } else {
        LCD_Draw_Line(px,     py - 26, px - facing * 9,      py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 9 + 1,  py - 18, body_colour);
        LCD_Draw_Line(px,     py - 26, px + facing * 9,      py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px + facing * 9 + 1,  py - 18, body_colour);
        draw_racket_head(px + facing * 9, py - 18, facing * 9, 8, body_colour);
    }

    if (state == ANIM_WALK) {
        if (anim_frame == 0) {
            LCD_Draw_Line(px,     py - 14, px + facing * 10,     py - 2, body_colour);
            LCD_Draw_Line(px + 1, py - 14, px + facing * 10 + 1, py - 2, body_colour);
            LCD_Draw_Line(px,     py - 14, px - facing * 4,      py,     body_colour);
            LCD_Draw_Line(px + 1, py - 14, px - facing * 4 + 1,  py,     body_colour);
        } else {
            LCD_Draw_Line(px,     py - 14, px + facing * 4,      py,     body_colour);
            LCD_Draw_Line(px + 1, py - 14, px + facing * 4 + 1,  py,     body_colour);
            LCD_Draw_Line(px,     py - 14, px - facing * 10,     py - 2, body_colour);
            LCD_Draw_Line(px + 1, py - 14, px - facing * 10 + 1, py - 2, body_colour);
        }
    } else {
        LCD_Draw_Line(px,     py - 14, px - 6,     py, body_colour);
        LCD_Draw_Line(px + 1, py - 14, px - 6 + 1, py, body_colour);
        LCD_Draw_Line(px,     py - 14, px + 6,     py, body_colour);
        LCD_Draw_Line(px + 1, py - 14, px + 6 + 1, py, body_colour);
    }
}



static void draw_shuttlecock(int16_t sx, int16_t sy, int16_t dx, int16_t dy, int8_t held_facing)
{
    int8_t dir = (dx > 0) ? 1 : -1;
    int8_t tilt = 0;
    int16_t cork_x;
    int16_t cork_y;
    int16_t band_x;
    int16_t skirt_x0;
    int16_t skirt_x1;

    if (dx == 0 && held_facing != 0) {
        dir = (int8_t)(-held_facing);
    }

    if (dy > 12) {
        tilt = 1;
    } else if (dy < -12) {
        tilt = -1;
    }

    cork_x = sx + 5 * dir;
    cork_y = sy + tilt;
    band_x = cork_x - 3 * dir;
    skirt_x0 = band_x;
    skirt_x1 = cork_x - 8 * dir;

    LCD_Draw_Line(skirt_x0, sy - 3 + tilt, skirt_x1, sy - 4 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy - 1 + tilt, skirt_x1, sy - 1 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy + 1 + tilt, skirt_x1, sy + 2 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy + 3 + tilt, skirt_x1, sy + 4 + tilt, 1);

    LCD_Draw_Line(band_x, sy - 3 + tilt, band_x - 3 * dir, sy - 1 + tilt, 0);
    LCD_Draw_Line(band_x, sy + tilt,     band_x - 4 * dir, sy + 1 + tilt, 0);
    LCD_Draw_Line(band_x, sy + 3 + tilt, band_x - 3 * dir, sy + 4 + tilt, 0);

    LCD_Draw_Line(band_x,       sy - 3 + tilt, band_x,       sy + 3 + tilt, 0);
    LCD_Draw_Line(band_x + dir, sy - 3 + tilt, band_x + dir, sy + 3 + tilt, 0);

    LCD_Draw_Circle(cork_x, cork_y, 2, 1, true);
    LCD_Draw_Line(cork_x - dir, cork_y - 1, cork_x + dir, cork_y + 1, 0);
}

// Score display
static void draw_hud(uint8_t sl, uint8_t sr)
{
    char buf[16];
    sprintf(buf, "%d - %d", sl, sr);
    LCD_printString(buf, 100, 4, 1, 2);
}

static void draw_start_screen(bool show_prompt)
{
    LCD_Fill_Buffer(13);
    draw_court();
    draw_stickman_at(OPPONENT_X, PLAYER_Y, -1, ANIM_IDLE, 0);
    draw_stickman_at(PLAYER_START_X, PLAYER_Y, 1, ANIM_IDLE, 0);
    draw_shuttlecock(RESET_CENTER_X, RESET_CENTER_Y, 0, 0, 0);

    LCD_printString("Stickman Badminton", 12, 36, 1, 2);
    if (show_prompt) {
        LCD_printString("press button to play", 0, 126, 1, 2);
        LCD_printString("press button to play", 1, 126, 1, 2);
    }
}

static void draw_game_over(void)
{
    LCD_printString("GAME OVER", 66, 88, 1, 2);
}

static void reset_shuttle_for_player_serve(Shuttlecock* shuttle, const Stickman* player)
{
    shuttle->x = (player->x + player->facing * PLAYER_SERVE_X_OFFSET) * FP_SCALE;
    shuttle->y = (player->y + PLAYER_SERVE_Y_OFFSET) * FP_SCALE;
    shuttle->dx = 0;
    shuttle->dy = 0;
    shuttle->drag_tick = 0;
}

static void reset_shuttle_for_opponent_serve(Shuttlecock* shuttle, const Stickman* opponent)
{
    shuttle->x = (opponent->x + opponent->facing * OPPONENT_SERVE_X_OFFSET) * FP_SCALE;
    shuttle->y = (opponent->y + OPPONENT_SERVE_Y_OFFSET) * FP_SCALE;
    shuttle->dx = 0;
    shuttle->dy = 0;
    shuttle->drag_tick = 0;
}

static void play_short_beep(uint32_t* stop_tick, uint32_t freq_hz, uint8_t volume, uint32_t duration_ms)
{
    buzzer_tone(&buzzer_cfg, freq_hz, volume);
    *stop_tick = HAL_GetTick() + duration_ms;
}

static void play_badminton_hit(uint32_t* stop_tick)
{
    int16_t freq;

    // Stop other sounds
    *stop_tick = 0;

    // Make hit sound
    for (freq = 1000; freq > 150; freq -= 100) {
        buzzer_tone(&buzzer_cfg, (uint32_t)freq, 45);
        HAL_Delay(5);
    }

    buzzer_off(&buzzer_cfg);
}

// Main game function
MenuState Game3_Run(void)
{
    GameState game_state = GAME_STATE_START;

    Stickman player = {
        .x = PLAYER_START_X,
        .y = PLAYER_Y,
        .state = ANIM_IDLE,
        .anim_frame = 0,
        .frame_count = 0,
        .swing_timer = 0,
        .facing = 1
    };

    Stickman opponent = {
        .x = OPPONENT_X,
        .y = PLAYER_Y,
        .state = ANIM_IDLE,
        .anim_frame = 0,
        .frame_count = 0,
        .swing_timer = 0,
        .facing = -1
    };

    Shuttlecock shuttle = {
        .x = 0,
        .y = 0,
        .dx = SFP_INIT_DX,
        .dy = SFP_INIT_DY,
        .drag_tick = 0
    };

    uint8_t score_left  = 0;
    uint8_t score_right = 0;
    uint8_t prev_score_left  = 0;
    uint8_t prev_score_right = 0;
    uint8_t hit_cooldown = 0;
    bool waiting_for_serve = true;
    bool serve_from_player = true;
    uint32_t buzzer_stop_tick = 0;
    uint32_t opponent_serve_tick = 0;
    int16_t opponent_target_x = OPPONENT_HOME_X;
    uint8_t opponent_ai_tick = 0;

    reset_shuttle_for_player_serve(&shuttle, &player);

    int16_t prev_px = player.x;
    int16_t prev_py = player.y;
    int16_t prev_ox = opponent.x;
    int16_t prev_oy = opponent.y;
    int16_t prev_sx = (int16_t)(shuttle.x / FP_SCALE);
    int16_t prev_sy = (int16_t)(shuttle.y / FP_SCALE);

    draw_start_screen(((HAL_GetTick() / START_PROMPT_BLINK_MS) % 2) == 0);
    LCD_Refresh(&cfg0);

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        if (current_input.btn3_pressed) {
            break;  // Exit to menu
        }

        if (buzzer_stop_tick != 0 && (int32_t)(HAL_GetTick() - buzzer_stop_tick) >= 0) {
            buzzer_off(&buzzer_cfg);
            buzzer_stop_tick = 0;
        }

        if (game_state == GAME_STATE_START) {
            draw_start_screen(((frame_start / START_PROMPT_BLINK_MS) % 2) == 0);
            LCD_Refresh(&cfg0);

            if (current_input.btn2_pressed || current_input.btn4_pressed) {
                buzzer_off(&buzzer_cfg);
                buzzer_stop_tick = 0;

                player.x = PLAYER_START_X;
                player.y = PLAYER_Y;
                player.state = ANIM_IDLE;
                player.anim_frame = 0;
                player.frame_count = 0;
                player.swing_timer = 0;
                player.facing = 1;

                opponent.x = OPPONENT_X;
                opponent.y = PLAYER_Y;
                opponent.state = ANIM_IDLE;
                opponent.anim_frame = 0;
                opponent.frame_count = 0;
                opponent.swing_timer = 0;
                opponent.facing = -1;

                score_left = 0;
                score_right = 0;
                prev_score_left = 0;
                prev_score_right = 0;
                hit_cooldown = 0;
                waiting_for_serve = true;
                serve_from_player = true;
                opponent_serve_tick = 0;

                reset_shuttle_for_player_serve(&shuttle, &player);
                opponent_target_x = OPPONENT_HOME_X;
                opponent_ai_tick = 0;

                prev_px = player.x;
                prev_py = player.y;
                prev_ox = opponent.x;
                prev_oy = opponent.y;
                prev_sx = (int16_t)(shuttle.x / FP_SCALE);
                prev_sy = (int16_t)(shuttle.y / FP_SCALE);

                LCD_Fill_Buffer(13);
                draw_hud(score_left, score_right);
                draw_court();
                draw_stickman_at(opponent.x, opponent.y, opponent.facing, opponent.state, opponent.anim_frame);
                draw_stickman_at(player.x, player.y, player.facing, player.state, player.anim_frame);
                draw_shuttlecock(prev_sx, prev_sy, shuttle.dx, shuttle.dy, player.facing);
                LCD_Refresh(&cfg0);
                play_short_beep(&buzzer_stop_tick, 900, 40, 45);
                game_state = GAME_STATE_PLAYING;
            }

            uint32_t elapsed = HAL_GetTick() - frame_start;
            if (elapsed < FRAME_TIME_MS) {
                HAL_Delay(FRAME_TIME_MS - elapsed);
            }
            continue;
        }

        if (game_state == GAME_STATE_GAME_OVER) {
            if (current_input.btn2_pressed || current_input.btn4_pressed) {
                buzzer_off(&buzzer_cfg);
                buzzer_stop_tick = 0;

                player.x = PLAYER_START_X;
                player.y = PLAYER_Y;
                player.state = ANIM_IDLE;
                player.anim_frame = 0;
                player.frame_count = 0;
                player.swing_timer = 0;
                player.facing = 1;

                opponent.x = OPPONENT_X;
                opponent.y = PLAYER_Y;
                opponent.state = ANIM_IDLE;
                opponent.anim_frame = 0;
                opponent.frame_count = 0;
                opponent.swing_timer = 0;
                opponent.facing = -1;

                score_left = 0;
                score_right = 0;
                prev_score_left = 0;
                prev_score_right = 0;
                hit_cooldown = 0;
                waiting_for_serve = true;
                serve_from_player = true;
                opponent_serve_tick = 0;

                reset_shuttle_for_player_serve(&shuttle, &player);
                opponent_target_x = OPPONENT_HOME_X;
                opponent_ai_tick = 0;

                prev_px = player.x;
                prev_py = player.y;
                prev_ox = opponent.x;
                prev_oy = opponent.y;
                prev_sx = (int16_t)(shuttle.x / FP_SCALE);
                prev_sy = (int16_t)(shuttle.y / FP_SCALE);

                LCD_Fill_Buffer(13);
                draw_hud(score_left, score_right);
                draw_court();
                draw_stickman_at(opponent.x, opponent.y, opponent.facing, opponent.state, opponent.anim_frame);
                draw_stickman_at(player.x, player.y, player.facing, player.state, player.anim_frame);
                draw_shuttlecock(prev_sx, prev_sy, shuttle.dx, shuttle.dy, player.facing);
                LCD_Refresh(&cfg0);
                play_short_beep(&buzzer_stop_tick, 900, 40, 45);
                game_state = GAME_STATE_PLAYING;
            }

            uint32_t elapsed = HAL_GetTick() - frame_start;
            if (elapsed < FRAME_TIME_MS) {
                HAL_Delay(FRAME_TIME_MS - elapsed);
            }
            continue;
        }

        if (current_input.btn2_pressed && player.state != ANIM_SWING) {
            player.state = ANIM_SWING;
            player.swing_timer = SWING_DURATION;

            if (waiting_for_serve && serve_from_player) {
                reset_shuttle_for_player_serve(&shuttle, &player);
                shuttle.dx = SFP_INIT_DX;
                shuttle.dy = SFP_INIT_DY;
                shuttle.drag_tick = 0;
                play_badminton_hit(&buzzer_stop_tick);
                waiting_for_serve = false;
            }
        }

        {
            bool player_is_moving = false;
            Direction dir = joystick_data.direction;

            if (dir == E || dir == NE || dir == SE) {
                player.x += PLAYER_SPEED;
                player.facing = 1;
                player_is_moving = true;
                if (player.x > PLAYER_MAX_X) player.x = PLAYER_MAX_X;
            } else if (dir == W || dir == NW || dir == SW) {
                player.x -= PLAYER_SPEED;
                player.facing = -1;
                player_is_moving = true;
                if (player.x < PLAYER_MIN_X) player.x = PLAYER_MIN_X;
            }

            if (player.state == ANIM_SWING) {
                player.swing_timer--;
                if (player.swing_timer == 0) {
                    player.state = player_is_moving ? ANIM_WALK : ANIM_IDLE;
                }
            } else {
                player.state = player_is_moving ? ANIM_WALK : ANIM_IDLE;
            }
        }

        if (waiting_for_serve) {
            if (serve_from_player) {
                reset_shuttle_for_player_serve(&shuttle, &player);
            } else {
                reset_shuttle_for_opponent_serve(&shuttle, &opponent);
                if (opponent_serve_tick != 0 && (int32_t)(HAL_GetTick() - opponent_serve_tick) >= 0) {
                    opponent.state = ANIM_SWING;
                    opponent.swing_timer = SWING_DURATION;
                    shuttle.dx = -SFP_INIT_DX;
                    shuttle.dy = SFP_INIT_DY;
                    shuttle.drag_tick = 0;
                    play_badminton_hit(&buzzer_stop_tick);
                    waiting_for_serve = false;
                    opponent_serve_tick = 0;
                }
            }
        }

        {
            bool opponent_is_moving = false;

            if (opponent_ai_tick == 0) {
                int16_t shuttle_screen_x = (int16_t)(shuttle.x / FP_SCALE);
                opponent_target_x = OPPONENT_HOME_X;

                if (!waiting_for_serve && shuttle.dx > 0 && shuttle.x >= SFP_NET_X) {
                    opponent_target_x = shuttle_screen_x + (int16_t)((shuttle.dx / FP_SCALE) * OPPONENT_LEAD_FRAMES);
                }

                if (opponent_target_x < OPPONENT_MIN_X) opponent_target_x = OPPONENT_MIN_X;
                if (opponent_target_x > OPPONENT_MAX_X) opponent_target_x = OPPONENT_MAX_X;
            }
            opponent_ai_tick++;
            if (opponent_ai_tick >= OPPONENT_AI_REACT_FRAMES) {
                opponent_ai_tick = 0;
            }

            opponent.y = PLAYER_Y;

            if (opponent.x < opponent_target_x) {
                opponent.x += OPPONENT_SPEED;
                if (opponent.x > opponent_target_x) opponent.x = opponent_target_x;
                opponent_is_moving = true;
            } else if (opponent.x > opponent_target_x) {
                opponent.x -= OPPONENT_SPEED;
                if (opponent.x < opponent_target_x) opponent.x = opponent_target_x;
                opponent_is_moving = true;
            }

            if (opponent.state != ANIM_SWING) {
                opponent.state = opponent_is_moving ? ANIM_WALK : ANIM_IDLE;
            }
        }

        if (opponent.state == ANIM_SWING) {
            opponent.swing_timer--;
            if (opponent.swing_timer == 0) {
                opponent.state = ANIM_IDLE;
            }
        }

        if (!waiting_for_serve) {
            if (shuttle.dy < 0) {
                shuttle.dy += SFP_GRAVITY_UP;
            } else {
                shuttle.dy += SFP_GRAVITY_DOWN;
            }
            if (shuttle.dy > SFP_MAX_DY) shuttle.dy = SFP_MAX_DY;

            shuttle.drag_tick++;
            if (shuttle.drag_tick >= SFP_DRAG_INTERVAL) {
                shuttle.drag_tick = 0;
                if (shuttle.dx > SFP_MIN_DX) {
                    shuttle.dx--;
                } else if (shuttle.dx < -SFP_MIN_DX) {
                    shuttle.dx++;
                }
            }

            if (hit_cooldown > 0) {
                hit_cooldown--;
            }

            if (hit_cooldown == 0) {
                int16_t shuttle_left = shuttle.x - SHUTTLE_R * FP_SCALE;
                int16_t shuttle_right = shuttle.x + SHUTTLE_R * FP_SCALE;
                int16_t shuttle_top = shuttle.y - SHUTTLE_R * FP_SCALE;
                int16_t shuttle_bottom = shuttle.y + SHUTTLE_R * FP_SCALE;

                int16_t player_hit_x0 = player.x + player.facing * PLAYER_HIT_X0_OFFSET;
                int16_t player_hit_x1 = player.x + player.facing * PLAYER_HIT_X1_OFFSET;
                int16_t player_left_px = (player_hit_x0 < player_hit_x1) ? player_hit_x0 : player_hit_x1;
                int16_t player_right_px = (player_hit_x0 > player_hit_x1) ? player_hit_x0 : player_hit_x1;
                int16_t player_left = player_left_px * FP_SCALE;
                int16_t player_right = player_right_px * FP_SCALE;
                int16_t player_top = (player.y + PLAYER_HIT_Y0_OFFSET) * FP_SCALE;
                int16_t player_bottom = (player.y + PLAYER_HIT_Y1_OFFSET) * FP_SCALE;

                int16_t opponent_left = (opponent.x + OPPONENT_HIT_X0_OFFSET) * FP_SCALE;
                int16_t opponent_right = (opponent.x + OPPONENT_HIT_X1_OFFSET) * FP_SCALE;
                int16_t opponent_top = (opponent.y + OPPONENT_HIT_Y0_OFFSET) * FP_SCALE;
                int16_t opponent_bottom = (opponent.y + OPPONENT_HIT_Y1_OFFSET) * FP_SCALE;

                if (player.state == ANIM_SWING &&
                    shuttle.dx < 0 &&
                    shuttle_right >= player_left &&
                    shuttle_left <= player_right &&
                    shuttle_bottom >= player_top &&
                    shuttle_top <= player_bottom) {
                    shuttle.dx = SFP_HIT_DX;
                    shuttle.dy = SFP_HIT_DY;
                    shuttle.x = player_right + (SHUTTLE_R + 1) * FP_SCALE;
                    shuttle.drag_tick = 0;
                    hit_cooldown = HIT_COOLDOWN_FRAMES;
                    play_badminton_hit(&buzzer_stop_tick);
                }

                if (shuttle.dx > 0 &&
                    shuttle_right >= opponent_left &&
                    shuttle_left <= opponent_right &&
                    shuttle_bottom >= opponent_top &&
                    shuttle_top <= opponent_bottom) {
                    opponent.state = ANIM_SWING;
                    opponent.swing_timer = SWING_DURATION;
                    shuttle.dx = -SFP_HIT_DX;
                    shuttle.dy = SFP_HIT_DY;
                    shuttle.x = opponent_left - (SHUTTLE_R + 1) * FP_SCALE;
                    shuttle.drag_tick = 0;
                    hit_cooldown = HIT_COOLDOWN_FRAMES;
                    play_badminton_hit(&buzzer_stop_tick);
                }
            }

            shuttle.x += shuttle.dx;
            shuttle.y += shuttle.dy;

            if (shuttle.y < SFP_BOUND_T) {
                shuttle.y = SFP_BOUND_T;
                if (shuttle.dy < 0) shuttle.dy = SFP_GRAVITY_UP;
            }

            {
                int16_t shuttle_left = shuttle.x - SHUTTLE_R * FP_SCALE;
                int16_t shuttle_right = shuttle.x + SHUTTLE_R * FP_SCALE;
                int16_t shuttle_top = shuttle.y - SHUTTLE_R * FP_SCALE;
                int16_t shuttle_bottom = shuttle.y + SHUTTLE_R * FP_SCALE;
                int16_t net_left_fp = (NET_X - 8) * FP_SCALE;
                int16_t net_right_fp = (NET_X + 2) * FP_SCALE;
                int16_t net_top_fp = NET_TOP * FP_SCALE;

                if (shuttle_right >= net_left_fp &&
                    shuttle_left <= net_right_fp &&
                    shuttle_bottom >= net_top_fp &&
                    shuttle_top <= SFP_BOUND_B) {
                    if (shuttle.dx > 0) {
                        score_right++;
                    } else if (shuttle.dx < 0) {
                        score_left++;
                    }

                    serve_from_player = (shuttle.dx < 0);
                    if (serve_from_player) {
                        reset_shuttle_for_player_serve(&shuttle, &player);
                        opponent_serve_tick = 0;
                    } else {
                        reset_shuttle_for_opponent_serve(&shuttle, &opponent);
                        opponent_serve_tick = HAL_GetTick() + OPPONENT_SERVE_DELAY_MS;
                    }
                    hit_cooldown = 0;
                    waiting_for_serve = true;
                }
            }

            if (shuttle.x < SFP_BOUND_L) {
                score_right++;
                serve_from_player = false;
                reset_shuttle_for_opponent_serve(&shuttle, &opponent);
                hit_cooldown = 0;
                waiting_for_serve = true;
                opponent_serve_tick = HAL_GetTick() + OPPONENT_SERVE_DELAY_MS;
            }
            if (shuttle.x > SFP_BOUND_R) {
                score_left++;
                serve_from_player = true;
                reset_shuttle_for_player_serve(&shuttle, &player);
                hit_cooldown = 0;
                waiting_for_serve = true;
                opponent_serve_tick = 0;
            }

            if (shuttle.y > SFP_BOUND_B) {
                if (shuttle.x < SFP_NET_X) {
                    score_right++;
                } else {
                    score_left++;
                }

                if (shuttle.x < SFP_NET_X) {
                    serve_from_player = false;
                    reset_shuttle_for_opponent_serve(&shuttle, &opponent);
                    opponent_serve_tick = HAL_GetTick() + OPPONENT_SERVE_DELAY_MS;
                } else {
                    serve_from_player = true;
                    reset_shuttle_for_player_serve(&shuttle, &player);
                    opponent_serve_tick = 0;
                }
                hit_cooldown = 0;
                waiting_for_serve = true;
            }

            if (score_left >= GAME_OVER_SCORE || score_right >= GAME_OVER_SCORE) {
                shuttle.x = RESET_CENTER_X * FP_SCALE;
                shuttle.y = RESET_CENTER_Y * FP_SCALE;
                shuttle.dx = 0;
                shuttle.dy = 0;
                shuttle.drag_tick = 0;
                waiting_for_serve = true;
                opponent_serve_tick = 0;
                game_state = GAME_STATE_GAME_OVER;
            }
        }

        if (waiting_for_serve) {
            hit_cooldown = 0;
        }

        // Walk animation
        if (player.state == ANIM_WALK) {
            player.frame_count++;
            if (player.frame_count >= WALK_ANIM_SPEED) {
                player.frame_count = 0;
                player.anim_frame  = !player.anim_frame;
            }
        } else {
            player.frame_count = 0;
        }

        if (opponent.state == ANIM_WALK) {
            opponent.frame_count++;
            if (opponent.frame_count >= WALK_ANIM_SPEED) {
                opponent.frame_count = 0;
                opponent.anim_frame  = !opponent.anim_frame;
            }
        } else {
            opponent.frame_count = 0;
        }

        int16_t new_sx = (int16_t)(shuttle.x / FP_SCALE);
        int16_t new_sy = (int16_t)(shuttle.y / FP_SCALE);

        erase_stickman(prev_px, prev_py);
        erase_stickman(prev_ox, prev_oy);
        erase_shuttle(prev_sx, prev_sy);

        draw_net();
        LCD_Draw_Line(0, FLOOR_Y, 239, FLOOR_Y, 0);
        LCD_Draw_Line(COURT_LEFT, COURT_TOP, COURT_LEFT, FLOOR_Y, 0);
        LCD_Draw_Line(0, FLOOR_BOTTOM - 1, COURT_LEFT, FLOOR_Y, 0);

        if (score_left != prev_score_left || score_right != prev_score_right) {
            LCD_Draw_Rect(80, 0, 80, 18, 13, true);
            draw_hud(score_left, score_right);
            prev_score_left = score_left;
            prev_score_right = score_right;
        }

        if (game_state == GAME_STATE_GAME_OVER) {
            draw_game_over();
        }

        draw_stickman_at(opponent.x, opponent.y, opponent.facing, opponent.state, opponent.anim_frame);
        draw_stickman_at(player.x, player.y, player.facing, player.state, player.anim_frame);
        draw_shuttlecock(new_sx, new_sy, shuttle.dx, shuttle.dy,
                         waiting_for_serve ? (serve_from_player ? player.facing : opponent.facing) : 0);

        prev_px = player.x;
        prev_py = player.y;
        prev_ox = opponent.x;
        prev_oy = opponent.y;
        prev_sx = new_sx;
        prev_sy = new_sy;

        LCD_Refresh(&cfg0);

        uint32_t elapsed = HAL_GetTick() - frame_start;
        if (elapsed < FRAME_TIME_MS) {
            HAL_Delay(FRAME_TIME_MS - elapsed);
        }
    }

    buzzer_off(&buzzer_cfg);
    return MENU_STATE_HOME;
}

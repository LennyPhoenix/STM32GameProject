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

extern ST7789V2_cfg_t   cfg0;
extern Buzzer_cfg_t     buzzer_cfg;
extern Joystick_cfg_t   joystick_cfg;
extern Joystick_t       joystick_data;

/* ──────────────────────────────────────────────
 * Layout constants
 * ────────────────────────────────────────────── */
#define COURT_TOP        20
#define COURT_BOTTOM     186   /* floor line: orange floor is below this */
#define COURT_LEFT       5
#define COURT_RIGHT      235
#define NET_X            120
#define NET_TOP          146   /* lower net height to match reference ratio */
#define FLOOR_Y          COURT_BOTTOM
#define FLOOR_BOTTOM     240

#define PLAYER_START_X   50
#define PLAYER_Y         185   /* feet at floor line */
#define PLAYER_SPEED     3
#define PLAYER_MIN_X     (COURT_LEFT + 18)
#define PLAYER_MAX_X     (NET_X - 18)

#define OPPONENT_X       196
#define OPPONENT_SPEED   2
#define OPPONENT_MIN_X   (NET_X + 18)
#define OPPONENT_MAX_X   (COURT_RIGHT - 18)
#define OPPONENT_HOME_X  OPPONENT_X
#define OPPONENT_LEAD_FRAMES  3

#define SHUTTLE_R         4

/* Fixed-point scale: positions and velocities stored ×FP_SCALE.
 * Velocity unit = (pixels/frame) × FP_SCALE.
 * Integer math only — no floats needed. */
#define FP_SCALE          10

/* Serve start relative to the player's racket hand */
#define PLAYER_SERVE_X_OFFSET   12
#define PLAYER_SERVE_Y_OFFSET  (-28)
#define OPPONENT_SERVE_X_OFFSET  12
#define OPPONENT_SERVE_Y_OFFSET  (-28)
#define OPPONENT_SERVE_DELAY_MS  900

#define RESET_CENTER_X       NET_X
#define RESET_CENTER_Y       100
#define GAME_OVER_SCORE      5

/* Initial velocity: faster overall and a little more lift to clear the net. */
#define SFP_INIT_DX        30   /* +3.0 px/frame toward opponent    */
#define SFP_INIT_DY       (-50) /* -5.0 px/frame upward             */

/* Every racket contact keeps the rally a bit faster and higher. */
#define SFP_HIT_DX         26   /* +2.6 px/frame after hit          */
#define SFP_HIT_DY       (-48)  /* -4.8 px/frame upward after hit   */

/* Simplified badminton gravity:
 * Rising phase still slows the shuttle quickly enough that it peaks inside
 * the court instead of flying straight off-screen. */
#define SFP_GRAVITY_UP     2
#define SFP_GRAVITY_DOWN   4

/* Air drag: reduce |dx| by 1 FP unit every N frames */
#define SFP_DRAG_INTERVAL  2

/* Speed limits in FP units */
#define SFP_MAX_DX         60   /* 6.0 px/frame max horizontal      */
#define SFP_MIN_DX         20   /* 2.0 px/frame floor — no stall    */
#define SFP_MAX_DY         60   /* 6.0 px/frame max downward        */

/* Court boundaries (fixed-point) */
#define SFP_BOUND_L  ((COURT_LEFT   + SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_R  ((COURT_RIGHT  - SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_T  ((COURT_TOP    + SHUTTLE_R) * FP_SCALE)
#define SFP_BOUND_B  ((COURT_BOTTOM - SHUTTLE_R) * FP_SCALE)
#define SFP_NET_X    (NET_X * FP_SCALE)

#define FRAME_TIME_MS    33     /* ~30 FPS */
#define WALK_ANIM_SPEED  6      /* frames per walk step */
#define SWING_DURATION   10     /* frames for one swing */
#define HIT_COOLDOWN_FRAMES  8
#define OPPONENT_AI_REACT_FRAMES  3

/* Pong-style collision boxes around each stickman.
 * The shuttle bounces when its AABB overlaps one of these boxes. */
#define PLAYER_HIT_X0_OFFSET  (-8)
#define PLAYER_HIT_X1_OFFSET  14
#define PLAYER_HIT_Y0_OFFSET  (-40)
#define PLAYER_HIT_Y1_OFFSET  0

#define OPPONENT_HIT_X0_OFFSET  (-14)
#define OPPONENT_HIT_X1_OFFSET  8
#define OPPONENT_HIT_Y0_OFFSET  (-40)
#define OPPONENT_HIT_Y1_OFFSET  0

/* ──────────────────────────────────────────────
 * Stickman animation state
 * ────────────────────────────────────────────── */
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
    int16_t   x;
    int16_t   y;           /* feet position */
    AnimState state;
    uint8_t   anim_frame;  /* 0 or 1, used by walk */
    uint8_t   frame_count; /* counts up for anim timing */
    uint8_t   swing_timer; /* counts down during swing */
    int8_t    facing;      /* +1 = right, -1 = left */
} Stickman;

/* ──────────────────────────────────────────────
 * Shuttlecock — integer fixed-point (×FP_SCALE).
 * x, y        : screen position × FP_SCALE
 * dx, dy      : velocity (px/frame) × FP_SCALE
 * drag_tick   : frames since last drag step
 * dy < 0 = moving upward (screen y grows downward)
 * ────────────────────────────────────────────── */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;
    uint8_t drag_tick;
} Shuttlecock;

static void play_short_beep(uint32_t* stop_tick, uint32_t freq_hz, uint8_t volume, uint32_t duration_ms);

static void draw_racket_head(int16_t wrist_x, int16_t wrist_y,
                             int16_t arm_dx, int16_t arm_dy,
                             uint8_t border_colour)
{
    int16_t abs_dx = (arm_dx < 0) ? -arm_dx : arm_dx;
    int16_t abs_dy = (arm_dy < 0) ? -arm_dy : arm_dy;
    int16_t scale = (abs_dx > abs_dy) ? abs_dx : abs_dy;
    int16_t ux;
    int16_t uy;
    int16_t nx;
    int16_t ny;
    int16_t center_x;
    int16_t center_y;
    int16_t tip_back_x;
    int16_t tip_back_y;
    int16_t tip_front_x;
    int16_t tip_front_y;
    int16_t top_back_x;
    int16_t top_back_y;
    int16_t top_front_x;
    int16_t top_front_y;
    int16_t bot_back_x;
    int16_t bot_back_y;
    int16_t bot_front_x;
    int16_t bot_front_y;

    if (scale == 0) {
        scale = 1;
        arm_dx = 1;
    }

    ux = (int16_t)((arm_dx * 5) / scale);
    uy = (int16_t)((arm_dy * 5) / scale);
    nx = (int16_t)((-arm_dy * 2) / scale);
    ny = (int16_t)(( arm_dx * 2) / scale);

    center_x = wrist_x + (int16_t)((arm_dx * 3) / scale);
    center_y = wrist_y + (int16_t)((arm_dy * 3) / scale);

    tip_back_x  = center_x - ux;
    tip_back_y  = center_y - uy;
    tip_front_x = center_x + ux;
    tip_front_y = center_y + uy;

    top_back_x  = center_x - ux / 2 + nx;
    top_back_y  = center_y - uy / 2 + ny;
    top_front_x = center_x + ux / 2 + nx;
    top_front_y = center_y + uy / 2 + ny;
    bot_back_x  = center_x - ux / 2 - nx;
    bot_back_y  = center_y - uy / 2 - ny;
    bot_front_x = center_x + ux / 2 - nx;
    bot_front_y = center_y + uy / 2 - ny;

    LCD_Draw_Line(tip_back_x, tip_back_y, top_back_x, top_back_y, border_colour);
    LCD_Draw_Line(top_back_x, top_back_y, top_front_x, top_front_y, border_colour);
    LCD_Draw_Line(top_front_x, top_front_y, tip_front_x, tip_front_y, border_colour);
    LCD_Draw_Line(tip_front_x, tip_front_y, bot_front_x, bot_front_y, border_colour);
    LCD_Draw_Line(bot_front_x, bot_front_y, bot_back_x, bot_back_y, border_colour);
    LCD_Draw_Line(bot_back_x, bot_back_y, tip_back_x, tip_back_y, border_colour);

}

/* ──────────────────────────────────────────────
 * Court drawing
 * ────────────────────────────────────────────── */
static void draw_net(void)
{
    /* Slight perspective: rear pole (left) and front pole (right). */
    const int16_t back_x      = NET_X - 8;
    const int16_t front_x     = NET_X + 2;
    const int16_t top_back_y  = NET_TOP + 8;
    const int16_t top_front_y = NET_TOP;

    /* Side borders of mesh panel. */
    LCD_Draw_Line(back_x,  top_back_y,  back_x,  FLOOR_Y, 0);
    LCD_Draw_Line(front_x, top_front_y, front_x, FLOOR_Y, 0);

    /* Mesh panel fill matches the wall so only the frame and threads stand out. */
    for (int16_t x = back_x + 1; x <= front_x - 1; x++) {
        int16_t y_top = top_back_y + (int16_t)(((top_front_y - top_back_y) * (x - back_x)) / (front_x - back_x));
        LCD_Draw_Line(x, y_top, x, FLOOR_Y - 1, 13);
    }

    /* Top tape: dark border, keep it subtle (no bright white slash). */
    LCD_Draw_Line(back_x - 1, top_back_y - 1, front_x + 1, top_front_y - 1, 0);
    LCD_Draw_Line(back_x - 1, top_back_y,     front_x + 1, top_front_y,     0);
    LCD_Draw_Line(back_x,     top_back_y,     front_x,     top_front_y,     13);

    /* Mesh: two diagonal families to resemble badminton net weave. */
    for (int16_t y = top_front_y + 2; y <= FLOOR_Y - 2; y += 4) {
        LCD_Draw_Line(back_x + 1, y + 8, front_x - 1, y, 13);
    }
    for (int16_t y = top_front_y + 1; y <= FLOOR_Y - 2; y += 4) {
        LCD_Draw_Line(back_x + 1, y, front_x - 1, y + 8, 13);
    }

    /* Subtle vertical thread accents for finer detail. */
    for (int16_t x = back_x + 2; x <= front_x - 2; x += 2) {
        LCD_Draw_Line(x, top_front_y + 2, x, FLOOR_Y - 1, 14);
    }

    /* Rear and front poles (front pole longer, like reference). */
    LCD_Draw_Rect(back_x - 1,  FLOOR_Y - 1, 3, 12, 0, true);
    LCD_Draw_Rect(front_x - 1, FLOOR_Y - 1, 3, 26, 0, true);

    /* Pole feet / bases. */
    LCD_Draw_Line(back_x - 4,  FLOOR_Y + 10, back_x + 4,  FLOOR_Y + 10, 0);
    LCD_Draw_Line(front_x - 4, FLOOR_Y + 24, front_x + 4, FLOOR_Y + 24, 0);

    /* Front-pole highlight to reduce jagged look on LCD. */
    LCD_Draw_Line(front_x + 1, top_front_y + 1, front_x + 1, FLOOR_Y + 25, 13);
}

static void draw_court(void)
{
    /* Floor in gold-ish tone like reference. */
    LCD_Draw_Rect(0, FLOOR_Y, 240, FLOOR_BOTTOM - FLOOR_Y, 10, true);

    /* Wall/floor boundary and left wall corner lines. */
    LCD_Draw_Line(0, FLOOR_Y, 239, FLOOR_Y, 0);
    LCD_Draw_Line(COURT_LEFT, COURT_TOP, COURT_LEFT, FLOOR_Y, 0);
    LCD_Draw_Line(0, FLOOR_BOTTOM - 1, COURT_LEFT, FLOOR_Y, 0);

    /* Dashed service lines with perspective-like slant. */
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

/* ──────────────────────────────────────────────
 * Restore the net over any area that was erased.
 * Called after partial-erase rects that may overlap the net x-range.
 * ────────────────────────────────────────────── */
static void redraw_net(void)
{
    draw_net();
}

/* ──────────────────────────────────────────────
 * Erase helpers: overwrite bounding box with background colour.
 * Use clipped rectangles so negative coords near screen edges do not
 * wrap into large uint16 values and skip erasing.
 * ────────────────────────────────────────────── */
static void fill_rect_clamped(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour)
{
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    if (x0 < 0)   x0 = 0;
    if (y0 < 0)   y0 = 0;
    if (x1 > 239) x1 = 239;
    if (y1 > 239) y1 = 239;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    LCD_Draw_Rect((uint16_t)x0,
                  (uint16_t)y0,
                  (uint16_t)(x1 - x0 + 1),
                  (uint16_t)(y1 - y0 + 1),
                  colour,
                  true);
}

/* Fill erase box with the correct static background layers.
 * Upper region uses wall grey, floor region uses gold tone. */
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
    if (x0 > x1 || y0 > y1) {
        return;
    }

    /* Wall part (above floor line) */
    if (y0 < FLOOR_Y) {
        int16_t wall_y1 = (y1 < (FLOOR_Y - 1)) ? y1 : (FLOOR_Y - 1);
        fill_rect_clamped(x0, y0, (x1 - x0 + 1), (wall_y1 - y0 + 1), 13);
    }

    /* Floor part (on or below floor line) */
    if (y1 >= FLOOR_Y) {
        int16_t floor_y0 = (y0 > FLOOR_Y) ? y0 : FLOOR_Y;
        fill_rect_clamped(x0, floor_y0, (x1 - x0 + 1), (y1 - floor_y0 + 1), 10);
    }
}

static void erase_stickman(int16_t px, int16_t py)
{
    /* Swing pose reaches higher than idle/walk: use a larger bbox. */
    erase_rect_background(px - 35, py - 50, 71, 53);
}

static void erase_shuttle(int16_t sx, int16_t sy)
{
    /* Shuttle sprite is wider than the old line marker. */
    erase_rect_background(sx - 10, sy - 8, 21, 17);
}

/* ──────────────────────────────────────────────
 * Stickman drawing
 *
 * Poses:
 *   IDLE  – both arms hanging, racket loosely at side
 *   WALK  – legs alternate (2 frames), arms swing opposite
 *   SWING – racket arm swings up toward shuttlecock
 *
 * px, py = feet position
 * facing  +1 = right (toward net), -1 = left
 * ────────────────────────────────────────────── */
static void draw_stickman_at(int16_t px, int16_t py,
                              int8_t facing, AnimState state,
                              uint8_t anim_frame)
{
    const uint8_t body_colour = 0;

    /* ── HEAD ── */
    LCD_Draw_Circle(px, py - 38, 5, 1, true);
    LCD_Draw_Circle(px, py - 38, 5, body_colour, false);

    /* ── BODY ── */
    LCD_Draw_Line(px,     py - 32, px,     py - 14, body_colour);
    LCD_Draw_Line(px + 1, py - 32, px + 1, py - 14, body_colour);

    /* ── ARMS ── */
    if (state == ANIM_SWING) {
        /* Racket arm: sweeps diagonally upward on the facing side */
        int16_t ra_ex = px + facing * 18;
        int16_t ra_ey = py - 40;
        LCD_Draw_Line(px,     py - 26, ra_ex,     ra_ey, body_colour);
        LCD_Draw_Line(px + 1, py - 26, ra_ex + 1, ra_ey, body_colour);
        /* Racket head: long axis follows the arm direction. */
        draw_racket_head(ra_ex, ra_ey, ra_ex - px, ra_ey - (py - 26), body_colour);
        /* Other arm: swings back for balance */
        LCD_Draw_Line(px,     py - 26, px - facing * 10,     py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 10 + 1, py - 18, body_colour);
    } else if (state == ANIM_WALK) {
        /* Arms swing opposite to legs — anim_frame drives it */
        int16_t arm_swing = (anim_frame == 0) ? 8 : -8;
        /* back arm */
        LCD_Draw_Line(px,     py - 26, px - facing * 8,      py - 20 + arm_swing / 2, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 8 + 1,  py - 20 + arm_swing / 2, body_colour);
        /* racket arm on the facing side */
        LCD_Draw_Line(px,     py - 26, px + facing * 10,     py - 20 - arm_swing / 2, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px + facing * 10 + 1, py - 20 - arm_swing / 2, body_colour);
        /* Racket head stays aligned with the racket arm. */
        draw_racket_head(px + facing * 10, py - 20 - arm_swing / 2,
                         facing * 10, 6 - arm_swing / 2, body_colour);
    } else {
        /* IDLE: arms relaxed, racket loosely at side */
        LCD_Draw_Line(px,     py - 26, px - facing * 9,      py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px - facing * 9 + 1,  py - 18, body_colour);
        LCD_Draw_Line(px,     py - 26, px + facing * 9,      py - 18, body_colour);
        LCD_Draw_Line(px + 1, py - 26, px + facing * 9 + 1,  py - 18, body_colour);
        /* Racket head stays aligned with the racket arm. */
        draw_racket_head(px + facing * 9, py - 18, facing * 9, 8, body_colour);
    }

    /* ── LEGS ── */
    if (state == ANIM_WALK) {
        if (anim_frame == 0) {
            /* facing-side leg forward, other leg back */
            LCD_Draw_Line(px,     py - 14, px + facing * 10,     py - 2, body_colour);
            LCD_Draw_Line(px + 1, py - 14, px + facing * 10 + 1, py - 2, body_colour);
            LCD_Draw_Line(px,     py - 14, px - facing * 4,      py,     body_colour);
            LCD_Draw_Line(px + 1, py - 14, px - facing * 4 + 1,  py,     body_colour);
        } else {
            /* swap */
            LCD_Draw_Line(px,     py - 14, px + facing * 4,      py,     body_colour);
            LCD_Draw_Line(px + 1, py - 14, px + facing * 4 + 1,  py,     body_colour);
            LCD_Draw_Line(px,     py - 14, px - facing * 10,     py - 2, body_colour);
            LCD_Draw_Line(px + 1, py - 14, px - facing * 10 + 1, py - 2, body_colour);
        }
    } else {
        /* IDLE / SWING: legs slightly apart */
        LCD_Draw_Line(px,     py - 14, px - 6,     py, body_colour);
        LCD_Draw_Line(px + 1, py - 14, px - 6 + 1, py, body_colour);
        LCD_Draw_Line(px,     py - 14, px + 6,     py, body_colour);
        LCD_Draw_Line(px + 1, py - 14, px + 6 + 1, py, body_colour);
    }
}



/* ──────────────────────────────────────────────
 * Shuttlecock
 * ────────────────────────────────────────────── */
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

    /* Feather body: slimmer white skirt. */
    LCD_Draw_Line(skirt_x0, sy - 3 + tilt, skirt_x1, sy - 4 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy - 1 + tilt, skirt_x1, sy - 1 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy + 1 + tilt, skirt_x1, sy + 2 + tilt, 1);
    LCD_Draw_Line(skirt_x0, sy + 3 + tilt, skirt_x1, sy + 4 + tilt, 1);

    /* Feather separators near the band. */
    LCD_Draw_Line(band_x, sy - 3 + tilt, band_x - 3 * dir, sy - 1 + tilt, 0);
    LCD_Draw_Line(band_x, sy + tilt,     band_x - 4 * dir, sy + 1 + tilt, 0);
    LCD_Draw_Line(band_x, sy + 3 + tilt, band_x - 3 * dir, sy + 4 + tilt, 0);

    /* Band around the shuttle base. */
    LCD_Draw_Line(band_x,       sy - 3 + tilt, band_x,       sy + 3 + tilt, 0);
    LCD_Draw_Line(band_x + dir, sy - 3 + tilt, band_x + dir, sy + 3 + tilt, 0);

    /* Cork / head. */
    LCD_Draw_Circle(cork_x, cork_y, 2, 1, true);
    LCD_Draw_Line(cork_x - dir, cork_y - 1, cork_x + dir, cork_y + 1, 0);
}

/* ──────────────────────────────────────────────
 * HUD
 * ────────────────────────────────────────────── */
static void draw_hud(uint8_t sl, uint8_t sr)
{
    char buf[16];
    sprintf(buf, "%d - %d", sl, sr);
    LCD_printString(buf, 100, 4, 1, 2);
}

static void draw_start_screen(void)
{
    LCD_Fill_Buffer(13);
    LCD_Draw_Rect(20, 44, 200, 112, 1, true);
    LCD_Draw_Rect(20, 44, 200, 112, 0, false);
    LCD_printString("STICKMAN", 52, 64, 0, 2);
    LCD_printString("BADMINTON", 46, 90, 0, 2);
    LCD_printString("PRESS JOYSTICK", 42, 126, 0, 1);
    LCD_printString("OR BUTTON TO START", 32, 142, 0, 1);
}

static void draw_game_over(uint8_t player_score, uint8_t opponent_score)
{
    char score_buf[20];
    const char* winner = (player_score >= GAME_OVER_SCORE) ? "PLAYER WINS" : "OPPONENT WINS";

    LCD_Draw_Rect(28, 78, 184, 82, 13, true);
    LCD_Draw_Rect(28, 78, 184, 82, 0, false);
    LCD_printString("GAME OVER", 56, 88, 1, 2);
    LCD_printString((char*)winner, 52, 112, 1, 1);
    sprintf(score_buf, "%d - %d", player_score, opponent_score);
    LCD_printString(score_buf, 94, 126, 1, 2);
    LCD_printString("PRESS JOYSTICK", 50, 142, 1, 1);
    LCD_printString("OR BUTTON TO RESTART", 34, 154, 1, 1);
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Mirror the shuttle horizontally, but also reapply a small upward arc so
 * it feels more like badminton than a flat Pong reflection. */
static void bounce_from_player(Shuttlecock* shuttle, int16_t player_right_fp)
{
    shuttle->dx = clamp_i16(SFP_HIT_DX, SFP_MIN_DX, SFP_MAX_DX);
    shuttle->dy = SFP_HIT_DY;
    shuttle->x = player_right_fp + (SHUTTLE_R + 1) * FP_SCALE;
    shuttle->drag_tick = 0;
}

static void bounce_from_opponent(Shuttlecock* shuttle, int16_t opponent_left_fp)
{
    shuttle->dx = (int16_t)(-clamp_i16(SFP_HIT_DX, SFP_MIN_DX, SFP_MAX_DX));
    shuttle->dy = SFP_HIT_DY;
    shuttle->x = opponent_left_fp - (SHUTTLE_R + 1) * FP_SCALE;
    shuttle->drag_tick = 0;
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

static void reset_shuttle_to_center(Shuttlecock* shuttle)
{
    shuttle->x = RESET_CENTER_X * FP_SCALE;
    shuttle->y = RESET_CENTER_Y * FP_SCALE;
    shuttle->dx = 0;
    shuttle->dy = 0;
    shuttle->drag_tick = 0;
}

static bool start_or_restart_pressed(Direction dir)
{
    return (dir != CENTRE) || current_input.btn2_pressed || current_input.btn4_pressed;
}

static int8_t get_held_shuttle_facing(bool waiting_for_serve, bool serve_from_player,
                                      const Stickman* player, const Stickman* opponent)
{
    if (!waiting_for_serve) {
        return 0;
    }

    return serve_from_player ? player->facing : opponent->facing;
}

static void reset_match_state(Stickman* player, Stickman* opponent, Shuttlecock* shuttle,
                              uint8_t* score_left, uint8_t* score_right,
                              uint8_t* prev_score_left, uint8_t* prev_score_right,
                              uint8_t* hit_cooldown, bool* waiting_for_serve,
                              bool* serve_from_player, uint32_t* opponent_serve_tick)
{
    player->x           = PLAYER_START_X;
    player->y           = PLAYER_Y;
    player->state       = ANIM_IDLE;
    player->anim_frame  = 0;
    player->frame_count = 0;
    player->swing_timer = 0;
    player->facing      = 1;

    opponent->x           = OPPONENT_X;
    opponent->y           = PLAYER_Y;
    opponent->state       = ANIM_IDLE;
    opponent->anim_frame  = 0;
    opponent->frame_count = 0;
    opponent->swing_timer = 0;
    opponent->facing      = -1;

    *score_left        = 0;
    *score_right       = 0;
    *prev_score_left   = 0;
    *prev_score_right  = 0;
    *hit_cooldown      = 0;
    *waiting_for_serve = true;
    *serve_from_player = true;
    *opponent_serve_tick = 0;

    reset_shuttle_for_player_serve(shuttle, player);
}

static void draw_play_scene(const Stickman* player, const Stickman* opponent,
                            const Shuttlecock* shuttle, uint8_t score_left, uint8_t score_right,
                            bool waiting_for_serve, bool serve_from_player)
{
    LCD_Fill_Buffer(13);
    draw_hud(score_left, score_right);
    draw_court();
    draw_stickman_at(opponent->x, opponent->y, opponent->facing,
                     opponent->state, opponent->anim_frame);
    draw_stickman_at(player->x, player->y, player->facing,
                     player->state, player->anim_frame);
    draw_shuttlecock((int16_t)(shuttle->x / FP_SCALE), (int16_t)(shuttle->y / FP_SCALE),
                     shuttle->dx, shuttle->dy,
                     get_held_shuttle_facing(waiting_for_serve, serve_from_player, player, opponent));
}

static void sync_previous_draw_state(const Stickman* player, const Stickman* opponent,
                                     const Shuttlecock* shuttle,
                                     int16_t* prev_px, int16_t* prev_py,
                                     int16_t* prev_ox, int16_t* prev_oy,
                                     int16_t* prev_sx, int16_t* prev_sy,
                                     AnimState* prev_player_state, AnimState* prev_opponent_state,
                                     uint8_t* prev_player_anim, uint8_t* prev_opponent_anim,
                                     int8_t* prev_player_facing, int8_t* prev_opponent_facing)
{
    *prev_px = player->x;
    *prev_py = player->y;
    *prev_ox = opponent->x;
    *prev_oy = opponent->y;
    *prev_sx = (int16_t)(shuttle->x / FP_SCALE);
    *prev_sy = (int16_t)(shuttle->y / FP_SCALE);
    *prev_player_state = player->state;
    *prev_opponent_state = opponent->state;
    *prev_player_anim = player->anim_frame;
    *prev_opponent_anim = opponent->anim_frame;
    *prev_player_facing = player->facing;
    *prev_opponent_facing = opponent->facing;
}

static void enter_playing_state(GameState* game_state,
                                Stickman* player, Stickman* opponent, Shuttlecock* shuttle,
                                uint8_t* score_left, uint8_t* score_right,
                                uint8_t* prev_score_left, uint8_t* prev_score_right,
                                uint8_t* hit_cooldown, bool* waiting_for_serve,
                                bool* serve_from_player, uint32_t* opponent_serve_tick,
                                int16_t* opponent_target_x, uint8_t* opponent_ai_tick,
                                uint32_t* buzzer_stop_tick,
                                int16_t* prev_px, int16_t* prev_py,
                                int16_t* prev_ox, int16_t* prev_oy,
                                int16_t* prev_sx, int16_t* prev_sy,
                                AnimState* prev_player_state, AnimState* prev_opponent_state,
                                uint8_t* prev_player_anim, uint8_t* prev_opponent_anim,
                                int8_t* prev_player_facing, int8_t* prev_opponent_facing)
{
    buzzer_off(&buzzer_cfg);
    *buzzer_stop_tick = 0;

    reset_match_state(player, opponent, shuttle,
                      score_left, score_right,
                      prev_score_left, prev_score_right,
                      hit_cooldown, waiting_for_serve,
                      serve_from_player, opponent_serve_tick);

    *opponent_target_x = OPPONENT_HOME_X;
    *opponent_ai_tick = 0;

    sync_previous_draw_state(player, opponent, shuttle,
                             prev_px, prev_py, prev_ox, prev_oy, prev_sx, prev_sy,
                             prev_player_state, prev_opponent_state,
                             prev_player_anim, prev_opponent_anim,
                             prev_player_facing, prev_opponent_facing);

    draw_play_scene(player, opponent, shuttle, *score_left, *score_right,
                    *waiting_for_serve, *serve_from_player);
    LCD_Refresh(&cfg0);
    play_short_beep(buzzer_stop_tick, 900, 40, 45);
    *game_state = GAME_STATE_PLAYING;
}

static void launch_player_serve(Shuttlecock* shuttle)
{
    shuttle->dx = SFP_INIT_DX;
    shuttle->dy = SFP_INIT_DY;
    shuttle->drag_tick = 0;
}

static void launch_opponent_serve(Shuttlecock* shuttle)
{
    shuttle->dx = (int16_t)(-SFP_INIT_DX);
    shuttle->dy = SFP_INIT_DY;
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

    /* Cancel any pending one-shot beep so this sweep owns the buzzer. */
    *stop_tick = 0;

    /* Fast descending sweep: close to the Arduino reference sound. */
    for (freq = 1000; freq > 150; freq -= 100) {
        buzzer_tone(&buzzer_cfg, (uint32_t)freq, 45);
        HAL_Delay(5);
    }

    buzzer_off(&buzzer_cfg);
}

/* ──────────────────────────────────────────────
 * Main entry point
 * ────────────────────────────────────────────── */
MenuState Game3_Run(void)
{
    GameState game_state = GAME_STATE_START;

    Stickman player = {
        .x           = PLAYER_START_X,
        .y           = PLAYER_Y,
        .state       = ANIM_IDLE,
        .anim_frame  = 0,
        .frame_count = 0,
        .swing_timer = 0,
        .facing      = 1    /* face right toward net */
    };

    Stickman opponent = {
        .x           = OPPONENT_X,
        .y           = PLAYER_Y,
        .state       = ANIM_IDLE,
        .anim_frame  = 0,
        .frame_count = 0,
        .swing_timer = 0,
        .facing      = -1   /* face left toward player */
    };

    Shuttlecock shuttle = {
        .x         = 0,
        .y         = 0,
        .dx        = SFP_INIT_DX,
        .dy        = SFP_INIT_DY,
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

    /* Previous positions for partial-erase rendering */
    int16_t prev_px = player.x;
    int16_t prev_py = player.y;
    int16_t prev_ox = opponent.x;
    int16_t prev_oy = opponent.y;
    int16_t prev_sx = (int16_t)(shuttle.x / FP_SCALE);
    int16_t prev_sy = (int16_t)(shuttle.y / FP_SCALE);
    AnimState prev_player_state = player.state;
    AnimState prev_opponent_state = opponent.state;
    uint8_t prev_player_anim = player.anim_frame;
    uint8_t prev_opponent_anim = opponent.anim_frame;
    int8_t prev_player_facing = player.facing;
    int8_t prev_opponent_facing = opponent.facing;

    draw_start_screen();
    LCD_Refresh(&cfg0);

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        /* ── INPUT ── */
        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        if (current_input.btn3_pressed) {
            break;
        }

        if (buzzer_stop_tick != 0 && (int32_t)(HAL_GetTick() - buzzer_stop_tick) >= 0) {
            buzzer_off(&buzzer_cfg);
            buzzer_stop_tick = 0;
        }

        if (game_state == GAME_STATE_START) {
            if (start_or_restart_pressed(joystick_data.direction)) {
                enter_playing_state(&game_state,
                                    &player, &opponent, &shuttle,
                                    &score_left, &score_right,
                                    &prev_score_left, &prev_score_right,
                                    &hit_cooldown, &waiting_for_serve,
                                    &serve_from_player, &opponent_serve_tick,
                                    &opponent_target_x, &opponent_ai_tick,
                                    &buzzer_stop_tick,
                                    &prev_px, &prev_py, &prev_ox, &prev_oy,
                                    &prev_sx, &prev_sy,
                                    &prev_player_state, &prev_opponent_state,
                                    &prev_player_anim, &prev_opponent_anim,
                                    &prev_player_facing, &prev_opponent_facing);
            }

            uint32_t elapsed = HAL_GetTick() - frame_start;
            if (elapsed < FRAME_TIME_MS) {
                HAL_Delay(FRAME_TIME_MS - elapsed);
            }
            continue;
        }

        if (game_state == GAME_STATE_GAME_OVER) {
            if (start_or_restart_pressed(joystick_data.direction)) {
                enter_playing_state(&game_state,
                                    &player, &opponent, &shuttle,
                                    &score_left, &score_right,
                                    &prev_score_left, &prev_score_right,
                                    &hit_cooldown, &waiting_for_serve,
                                    &serve_from_player, &opponent_serve_tick,
                                    &opponent_target_x, &opponent_ai_tick,
                                    &buzzer_stop_tick,
                                    &prev_px, &prev_py, &prev_ox, &prev_oy,
                                    &prev_sx, &prev_sy,
                                    &prev_player_state, &prev_opponent_state,
                                    &prev_player_anim, &prev_opponent_anim,
                                    &prev_player_facing, &prev_opponent_facing);
            }

            uint32_t elapsed = HAL_GetTick() - frame_start;
            if (elapsed < FRAME_TIME_MS) {
                HAL_Delay(FRAME_TIME_MS - elapsed);
            }
            continue;
        }

        /* BT2: trigger swing. If waiting to serve, launch from the player's hand. */
        if (current_input.btn2_pressed && player.state != ANIM_SWING) {
            player.state       = ANIM_SWING;
            player.swing_timer = SWING_DURATION;

            if (waiting_for_serve && serve_from_player) {
                reset_shuttle_for_player_serve(&shuttle, &player);
                launch_player_serve(&shuttle);
                play_badminton_hit(&buzzer_stop_tick);
                waiting_for_serve = false;
            }
        }

        /* ── UPDATE ── */
        {
            bool player_is_moving = false;
            Direction dir = joystick_data.direction;

            /* Movement stays available even while the player is swinging. */
            if (dir == E || dir == NE || dir == SE) {
                player.x      += PLAYER_SPEED;
                player.facing  = 1;
                player_is_moving = true;
                if (player.x > PLAYER_MAX_X) player.x = PLAYER_MAX_X;
            } else if (dir == W || dir == NW || dir == SW) {
                player.x      -= PLAYER_SPEED;
                player.facing  = -1;
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
                    launch_opponent_serve(&shuttle);
                    play_badminton_hit(&buzzer_stop_tick);
                    waiting_for_serve = false;
                    opponent_serve_tick = 0;
                }
            }
        }

        /* Simple opponent AI: return to home when idle, and lead the shuttle a
         * little when it is traveling toward the opponent. */
        {
            bool opponent_is_moving = false;

            if (opponent_ai_tick == 0) {
                int16_t shuttle_screen_x = (int16_t)(shuttle.x / FP_SCALE);
                opponent_target_x = OPPONENT_HOME_X;

                if (!waiting_for_serve && shuttle.dx > 0 && shuttle.x >= SFP_NET_X) {
                    opponent_target_x = shuttle_screen_x + (int16_t)((shuttle.dx / FP_SCALE) * OPPONENT_LEAD_FRAMES);
                }

                opponent_target_x = clamp_i16(opponent_target_x, OPPONENT_MIN_X, OPPONENT_MAX_X);
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

        /* Opponent swing animation countdown */
        if (opponent.state == ANIM_SWING) {
            opponent.swing_timer--;
            if (opponent.swing_timer == 0) {
                opponent.state = ANIM_IDLE;
            }
        }

        /* ── SHUTTLE PHYSICS (integer fixed-point) ── */
        if (!waiting_for_serve) {

        /* Step 1 — Gravity.
         * Add SFP_GRAVITY to dy every frame.
         * dy starts negative (upward); gravity makes it less negative
         * each frame until it goes positive and the shuttle falls.
         * This produces the parabolic badminton arc. */
        if (shuttle.dy < 0) {
            shuttle.dy += SFP_GRAVITY_UP;
        } else {
            shuttle.dy += SFP_GRAVITY_DOWN;
        }
        if (shuttle.dy > SFP_MAX_DY) shuttle.dy = SFP_MAX_DY;

        /* Step 2 — Air drag (horizontal).
         * Every SFP_DRAG_INTERVAL frames, reduce |dx| by 1 FP unit.
         * This makes the shuttle slow down horizontally over time.
         * The SFP_MIN_DX floor ensures it never stalls completely. */
        shuttle.drag_tick++;
        if (shuttle.drag_tick >= SFP_DRAG_INTERVAL) {
            shuttle.drag_tick = 0;
            if      (shuttle.dx >  SFP_MIN_DX) shuttle.dx--;
            else if (shuttle.dx < -SFP_MIN_DX) shuttle.dx++;
            /* Within the ±MIN_DX floor: leave it unchanged */
        }

        if (hit_cooldown > 0) {
            hit_cooldown--;
        }

        /* Step 2b — Hit checks.
         * Pong-style AABB overlap: if the shuttle box overlaps the player or
         * opponent box, reverse only the horizontal velocity. */
        if (hit_cooldown == 0) {
            int16_t shuttle_left   = shuttle.x - SHUTTLE_R * FP_SCALE;
            int16_t shuttle_right  = shuttle.x + SHUTTLE_R * FP_SCALE;
            int16_t shuttle_top    = shuttle.y - SHUTTLE_R * FP_SCALE;
            int16_t shuttle_bottom = shuttle.y + SHUTTLE_R * FP_SCALE;

            int16_t player_hit_x0  = player.x + player.facing * PLAYER_HIT_X0_OFFSET;
            int16_t player_hit_x1  = player.x + player.facing * PLAYER_HIT_X1_OFFSET;
            int16_t player_left_px = (player_hit_x0 < player_hit_x1) ? player_hit_x0 : player_hit_x1;
            int16_t player_right_px = (player_hit_x0 > player_hit_x1) ? player_hit_x0 : player_hit_x1;
            int16_t player_left    = player_left_px * FP_SCALE;
            int16_t player_right   = player_right_px * FP_SCALE;
            int16_t player_top     = (player.y + PLAYER_HIT_Y0_OFFSET) * FP_SCALE;
            int16_t player_bottom  = (player.y + PLAYER_HIT_Y1_OFFSET) * FP_SCALE;

            int16_t opponent_left   = (opponent.x + OPPONENT_HIT_X0_OFFSET) * FP_SCALE;
            int16_t opponent_right  = (opponent.x + OPPONENT_HIT_X1_OFFSET) * FP_SCALE;
            int16_t opponent_top    = (opponent.y + OPPONENT_HIT_Y0_OFFSET) * FP_SCALE;
            int16_t opponent_bottom = (opponent.y + OPPONENT_HIT_Y1_OFFSET) * FP_SCALE;

            if (player.state == ANIM_SWING &&
                shuttle.dx < 0 &&
                shuttle_right >= player_left &&
                shuttle_left <= player_right &&
                shuttle_bottom >= player_top &&
                shuttle_top <= player_bottom) {
                bounce_from_player(&shuttle, player_right);
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
                bounce_from_opponent(&shuttle, opponent_left);
                hit_cooldown = HIT_COOLDOWN_FRAMES;
                play_badminton_hit(&buzzer_stop_tick);
            }
        }

        /* Step 3 — Move. */
        shuttle.x += shuttle.dx;
        shuttle.y += shuttle.dy;

        /* Step 4 — Top ceiling: clamp and kill upward momentum.
         * Do NOT reflect — just let gravity take over from here. */
        if (shuttle.y < SFP_BOUND_T) {
            shuttle.y = SFP_BOUND_T;
            if (shuttle.dy < 0) shuttle.dy = SFP_GRAVITY_UP; /* start falling */
        }

        /* Step 4b — Net fault: if the shuttle reaches the net body, the side
         * that hit it loses the rally. */
        {
            int16_t shuttle_left   = shuttle.x - SHUTTLE_R * FP_SCALE;
            int16_t shuttle_right  = shuttle.x + SHUTTLE_R * FP_SCALE;
            int16_t shuttle_top    = shuttle.y - SHUTTLE_R * FP_SCALE;
            int16_t shuttle_bottom = shuttle.y + SHUTTLE_R * FP_SCALE;
            int16_t net_left_fp    = (NET_X - 8) * FP_SCALE;
            int16_t net_right_fp   = (NET_X + 2) * FP_SCALE;
            int16_t net_top_fp     = NET_TOP * FP_SCALE;

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

        /* Step 5 — Side out: no Pong-style wall bounce.
         * If shuttle flies out, give point to opposite side and reset. */
        if (shuttle.x < SFP_BOUND_L) {
            score_right++;
            serve_from_player = false;
            reset_shuttle_for_opponent_serve(&shuttle, &opponent);
            hit_cooldown      = 0;
            waiting_for_serve = true;
            opponent_serve_tick = HAL_GetTick() + OPPONENT_SERVE_DELAY_MS;
        }
        if (shuttle.x > SFP_BOUND_R) {
            score_left++;
            serve_from_player = true;
            reset_shuttle_for_player_serve(&shuttle, &player);
            hit_cooldown      = 0;
            waiting_for_serve = true;
            opponent_serve_tick = 0;
        }

        /* Step 6 — Ground: score, then reset.
         * Left half  → opponent scores (shuttle missed by player).
         * Right half → player scores (opponent missed). */
        if (shuttle.y > SFP_BOUND_B) {
            if (shuttle.x < SFP_NET_X)
                score_right++;
            else
                score_left++;

            if (shuttle.x < SFP_NET_X) {
                serve_from_player = false;
                reset_shuttle_for_opponent_serve(&shuttle, &opponent);
                opponent_serve_tick = HAL_GetTick() + OPPONENT_SERVE_DELAY_MS;
            } else {
                serve_from_player = true;
                reset_shuttle_for_player_serve(&shuttle, &player);
                opponent_serve_tick = 0;
            }
            hit_cooldown      = 0;
            waiting_for_serve = true;
        }

        if (score_left >= GAME_OVER_SCORE || score_right >= GAME_OVER_SCORE) {
            reset_shuttle_to_center(&shuttle);
            waiting_for_serve = true;
            opponent_serve_tick = 0;
            game_state = GAME_STATE_GAME_OVER;
        }
        }

        if (waiting_for_serve) {
            hit_cooldown = 0;
        }

        /* Walk animation ticker */
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

        /* ── RENDER: partial erase ──
         * Only touch pixels that actually changed.
         * Unchanged rows are skipped by LCD_Refresh (track_changes). */

        int16_t new_sx = (int16_t)(shuttle.x / FP_SCALE);
        int16_t new_sy = (int16_t)(shuttle.y / FP_SCALE);
        bool player_changed =
            (player.x != prev_px) || (player.y != prev_py) ||
            (player.state != prev_player_state) ||
            (player.anim_frame != prev_player_anim) ||
            (player.facing != prev_player_facing);
        bool opponent_changed =
            (opponent.x != prev_ox) || (opponent.y != prev_oy) ||
            (opponent.state != prev_opponent_state) ||
            (opponent.anim_frame != prev_opponent_anim) ||
            (opponent.facing != prev_opponent_facing);
        bool shuttle_changed =
            (new_sx != prev_sx) || (new_sy != prev_sy);

        if (waiting_for_serve) {
            if (serve_from_player && player_changed) {
                shuttle_changed = true;
            }
            if (!serve_from_player && opponent_changed) {
                shuttle_changed = true;
            }
        }

        bool needs_static_restore = player_changed || opponent_changed || shuttle_changed;

        /* 1. Erase only objects that actually changed */
        if (player_changed) {
            erase_stickman(prev_px, prev_py);
        }
        if (opponent_changed) {
            erase_stickman(prev_ox, prev_oy);
        }
        if (shuttle_changed) {
            erase_shuttle(prev_sx, prev_sy);
        }

        /* 2. Restore net over any pixels the erase rects may have wiped */
        if (needs_static_restore) {
            redraw_net();

            /* 2b. Restore static court lines that erase boxes can cross. */
            LCD_Draw_Line(0, FLOOR_Y, 239, FLOOR_Y, 0);
            LCD_Draw_Line(COURT_LEFT, COURT_TOP, COURT_LEFT, FLOOR_Y, 0);
            LCD_Draw_Line(0, FLOOR_BOTTOM - 1, COURT_LEFT, FLOOR_Y, 0);
        }

        /* 3. Redraw HUD only when score changes */
        if (score_left != prev_score_left || score_right != prev_score_right) {
            LCD_Draw_Rect(80, 0, 80, 18, 13, true);   /* erase HUD text area */
            draw_hud(score_left, score_right);
            prev_score_left  = score_left;
            prev_score_right = score_right;
        }

        if (game_state == GAME_STATE_GAME_OVER) {
            draw_game_over(score_left, score_right);
        }

        /* 4. Draw new positions */
        if (opponent_changed) {
            draw_stickman_at(opponent.x, opponent.y, opponent.facing,
                     opponent.state, opponent.anim_frame);
        }
        if (player_changed) {
            draw_stickman_at(player.x, player.y, player.facing,
                     player.state, player.anim_frame);
        }
        if (shuttle_changed) {
            draw_shuttlecock(new_sx, new_sy, shuttle.dx, shuttle.dy,
                             get_held_shuttle_facing(waiting_for_serve, serve_from_player,
                                                     &player, &opponent));
        }

        /* 5. Save positions for next frame's erase */
        prev_px = player.x;
        prev_py = player.y;
        prev_ox = opponent.x;
        prev_oy = opponent.y;
        prev_sx = new_sx;
        prev_sy = new_sy;
        prev_player_state = player.state;
        prev_opponent_state = opponent.state;
        prev_player_anim = player.anim_frame;
        prev_opponent_anim = opponent.anim_frame;
        prev_player_facing = player.facing;
        prev_opponent_facing = opponent.facing;

        LCD_Refresh(&cfg0);

        /* ── FRAME GATE ── */
        uint32_t elapsed = HAL_GetTick() - frame_start;
        if (elapsed < FRAME_TIME_MS) {
            HAL_Delay(FRAME_TIME_MS - elapsed);
        }
    }

    buzzer_off(&buzzer_cfg);
    return MENU_STATE_HOME;
}

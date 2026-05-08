#include "game_1.h"
#include "Buzzer.h"
#include "InputHandler.h"
#include "LCD.h"
#include "Menu.h"
#include "PWM.h"
#include "aabb.h"
#include "entity.h"
#include "player_entity.h"
#include "stm32l4xx_hal.h"
#include "system.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;       // LED PWM control
extern Buzzer_cfg_t buzzer_cfg; // Buzzer control

static uint32_t frame_counter = 0;

// Frame rate for this game (in milliseconds)
#define GAME1_FRAME_TIME_MS 25 // 20 FPS

void add_block(world_t *world, size_t *world_size, int32_t x) {
  entity_t *block = new_entity(world, world_size);

  aabb_t *aabb = init_aabb(block);
  aabb->x = x;
  aabb->y = 0;
  aabb->width = 32;
  aabb->height = 64;
}

MenuState Game1_Run(void) {
  // Initialize game state
  frame_counter = 0;

  // Play a brief startup sound
  buzzer_tone(&buzzer_cfg, 1000, 30); // 1kHz at 30% volume
  HAL_Delay(50);                      // Brief beep duration
  buzzer_off(&buzzer_cfg);            // Stop the buzzer

  MenuState exit_state = MENU_STATE_HOME; // Default: return to menu

  world_t world = NULL;
  size_t world_size = 0;

  entity_t *game = new_entity(&world, &world_size);
  entity_t *player = new_player(&world, &world_size);
  add_block(&world, &world_size, 240);
  add_block(&world, &world_size, 480);
  add_block(&world, &world_size, 720);
  add_block(&world, &world_size, 960);

  // Game's own loop - runs until exit condition
  while (1) {
    uint32_t frame_start = HAL_GetTick();

    LCD_Fill_Buffer(0);

    Input_Read();

    run_systems(&world, &world_size, frame_counter);

    LCD_Refresh(&cfg0);

    // Frame timing - wait for remainder of frame time
    uint32_t frame_time = HAL_GetTick() - frame_start;
    if (frame_time < GAME1_FRAME_TIME_MS) {
      HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
    }

    frame_counter++;
  }

  return exit_state; // Tell main where to go next
}

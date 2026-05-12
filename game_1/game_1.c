#include "game_1.h"
#include "Buzzer.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "LCD.h"
#include "Menu.h"
#include "PWM.h"
#include "aabb.h"
#include "entity.h"
#include "game_entity.h"
#include "gravity.h"
#include "physics.h"
#include "platform.h"
#include "player.h"
#include "stm32l4xx_hal.h"
#include "system.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;           // LED PWM control
extern Buzzer_cfg_t buzzer_cfg;     // Buzzer control
extern Joystick_cfg_t joystick_cfg; // Joystick configuration
extern Joystick_t joystick_data;    // Current joystick readings

static uint32_t frame_counter = 0;

// Frame rate for this game (in milliseconds)
#define GAME1_FRAME_TIME_MS 50 // 20 FPS

void new_game(world_t *world, size_t *world_size) {
  // delete all existing entities
  ITER_ENTITIES(*world, *world_size, entity,
                { delete_entity(entity, *world, *world_size); });

  // init new game entity, should be in slot 0!
  entity_t *game = new_entity(world, world_size);
  init_game_component(game);

  // instantiate player entity
  new_player(world, world_size);

  // add a couple platforms to start
  new_platform(world, world_size, 30, 50);
  new_platform(world, world_size, 240, 20);
}

MenuState Game1_Run(void) {
  // Initialize game state
  frame_counter = 0;

  // Play a brief startup sound
  buzzer_tone(&buzzer_cfg, 1000, 30); // 1kHz at 30% volume
  HAL_Delay(50);                      // Brief beep duration
  buzzer_off(&buzzer_cfg);            // Stop the buzzer

  MenuState exit_state = MENU_STATE_HOME; // Default: return to menu

  // initialise world
  world_t world = NULL;
  size_t world_size = 0;

  new_game(&world, &world_size);

  while (1) {
    uint32_t frame_start = HAL_GetTick();

    // clear screen - TODO should be a system
    LCD_Fill_Buffer(0);

    // read input - TODO should be a system
    Input_Read();
    Joystick_Read(&joystick_cfg, &joystick_data);

    // store user input in player - TODO should be a system
    // run all systems
    run_systems(&world, &world_size, frame_counter);

    LCD_Refresh(&cfg0);

    // wait for remainder of frame time
    uint32_t frame_time = HAL_GetTick() - frame_start;
    if (frame_time < GAME1_FRAME_TIME_MS) {
      HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
    }

    frame_counter++;
  }

  return exit_state;
}

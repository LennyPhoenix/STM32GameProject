#ifndef GAME_2_H
#define GAME_2_H

#include "Menu.h"
#include <stdint.h>


/**
 * @brief Game 2 - Student can implement their own game here
 * 
 * Placeholder for Student 2's game implementation.
 * This structure allows multiple students to work on separate games
 * while sharing common utilities from the shared/ folder.
 * 
 * The menu system calls this function when Game 2 is selected.
 * The function runs its own loop and returns when the game exits.
 * 
 * @return MenuState - Where to go next (typically MENU_STATE_HOME for menu)
 */

MenuState Game2_Run(void);
void pixel_coords (void);
void welcome_screen (void);
void instruction_screen (void);
void draw_gamescreen (void);
void draw_checkerboard (void);
void draw_infobar (void);
void draw_cursor (void);
void place_bombs (void);
void calculate_numbers (void);
void place_crosses (void);
void dig (void);
void game_lost (void);
void game_won (void);
void reset_game (void); 

#endif // GAME_2_H

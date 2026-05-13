#include "Game_2.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "PWM.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

extern ST7789V2_cfg_t cfg0;  // LCD configuration from main.c
extern Joystick_cfg_t joystick_cfg;  // Joystick configuration
extern Joystick_t joystick_data;     // Current joystick readings
extern Buzzer_cfg_t buzzer_cfg;  // Buzzer control


/**
 * @brief Game 2 Implementation - Student can modify
 * 
 * EXAMPLE: Shows how to use the Buzzer for sound effects
 * This is a placeholder with a bouncing animation.
 * Replace this with your actual game logic!
 */


// Frame rate for this game (in milliseconds) - runs slower than Game 1
#define GAME2_FRAME_TIME_MS 50  // ~20 FPS (different from Game 1!)
#define GRID_ROWS 12            // Checkerboard with 20x20 pixel squares in an 11x12 grid
#define GRID_COLS 12
#define CELL_H 20
#define CELL_W 20
#define NUM_BOMBS_AND_CROSSES 20            // Number of bombs and crosses can be adjusted easily for future developments

static uint8_t cursor_x = 0; 
static uint8_t cursor_y = 1;            // Start cursor in the top left square of the board
static uint16_t pixel_x = 0; 
static uint16_t pixel_y = 0;

static uint8_t bomb_grid[GRID_ROWS][GRID_COLS] = {0};
static uint8_t cross_grid[GRID_ROWS][GRID_COLS] = {0};
static uint8_t crosses_left =  NUM_BOMBS_AND_CROSSES; 
static uint8_t number_grid[GRID_ROWS][GRID_COLS] = {0}; 
static uint8_t revealed_grid[GRID_ROWS][GRID_COLS] = {0};
static uint8_t first_dig = 0; 
static uint8_t marked_bomb = 0; 
static uint8_t game_over = 0; 
static uint8_t player_won = 0; 
static uint16_t note_delay_ms = 150; 


const uint8_t Cross[18][18] = {
    {255, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 255},
    {0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0},
    {255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255},
    {255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255},
    {255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255},
    {255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255},
    {255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255},
    {255, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255},
    {255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255},
    {0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0},
    {255, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 255}
};

const uint8_t Sad_Face[8][8] = {
    {2, 0, 0, 255, 255, 0, 0, 0},
    {0, 0, 0, 255, 255, 0, 0, 0},
    {0, 0, 0, 255, 255, 0, 0, 0},
    {255, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 255, 255, 255, 255, 255},
    {255, 0, 0, 0, 0, 0, 0, 255},
    {0, 0, 255, 255, 255, 255, 0, 0},
    {0, 255, 255, 255, 255, 255, 255, 0}
};

MenuState Game2_Run(void) {
    
    // Set palette and reset the screen
    LCD_Fill_Buffer(0);
    LCD_Set_Palette(2); 

    // Turn LED off
    //PWM_SetDuty(PWM_cfg_t *cfg, uint8_t duty_percent);
    
    srand(HAL_GetTick());           // Setting the seed to use rand() correctly

    // Play a brief startup sound
    buzzer_tone(&buzzer_cfg, 1200, 30);  // 1.2kHz at 30% volume
    HAL_Delay(50);  // Brief beep duration
    buzzer_off(&buzzer_cfg);  // Stop the buzzer

    // Print welcome and instruction screen
    printf("Skipping ahead\n"); 
    welcome_screen();
    instruction_screen(); 

    pixel_coords();

    MenuState exit_state = MENU_STATE_HOME;  // Default: return to menu
    
    // Game's own loop - runs until exit condition
    while (1) {
        uint32_t frame_start = HAL_GetTick();
        
        // Read input
        Input_Read();

        // Read Joystick
        Joystick_Read(&joystick_cfg, &joystick_data);
        
        // Check if button was pressed to return to menu
        if (current_input.btn3_pressed) {
            exit_state = MENU_STATE_HOME; 
            break; 
        }
        
        while (game_over) {

            Input_Read();
            Joystick_Read(&joystick_cfg,  &joystick_data);

            if (current_input.btn3_pressed) {

                printf("entered other if\n"); 
                reset_game(); 
                game_over = 0; 

                printf("Going to exit to menu now\n");
                exit_state = MENU_STATE_HOME; 
                break; 
            }

        }
        
        // UPDATE: Game logic

        Direction dir = joystick_data.direction;

        if (dir == N) {
            if (cursor_y > 1) cursor_y--;      // Set the minimum value to 1 so the cursor does not go over the information bar
            pixel_coords();
            //printf("%i\n", cursor_y);
        } 
        else if (dir == S) {
            if (cursor_y < GRID_ROWS - 1) cursor_y++;
            pixel_coords();
            //printf("%i\n", cursor_y);
        }
        else if (dir == W) {
            if (cursor_x > 0) cursor_x--;
            pixel_coords();
            //printf("%i\n", cursor_x);
        }
        else if (dir == E) {
            if (cursor_x < (GRID_COLS - 1)) cursor_x++;     // The range needs to be 0-11 so that the cursor does not go off the screen
            pixel_coords();
            //printf("%i\n", cursor_x);
        }

        draw_gamescreen();
        draw_cursor();
        LCD_Refresh(&cfg0);
        
        if (current_input.btn4_pressed) {
            //printf("Button 4 is being pressed\n"); 
            dig();
        }

        if (current_input.btn2_pressed) {
            place_crosses(); 
        }
        
        // Frame timing - wait for remainder of frame time
        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS) {
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }
    
    return exit_state;  // Tell main where to go next
}

void pixel_coords (void) {                  // Convert the cursor coordinates into pixel coordinates
    pixel_x = cursor_x * CELL_W;
    pixel_y = ((cursor_y - 1) * CELL_H) + 20;
}

void welcome_screen (void) {

    printf("Entered welcome\n");

    LCD_Fill_Buffer(4); 
    LCD_printString("Welcome", 40, 50, 0, 3);
    LCD_Refresh(&cfg0);
    HAL_Delay(500); 
    LCD_printString("to", 50, 90, 0, 3);
    LCD_Refresh(&cfg0);
    HAL_Delay(500);
    LCD_printString("Minesweeper", 23, 138, 10, 3);
    LCD_printString("Minesweeper", 25, 140, 11, 3);
    LCD_Refresh(&cfg0);
    HAL_Delay(2000); 
}

void instruction_screen (void) {

    LCD_Fill_Buffer(4);

    LCD_printString("Press the button", 10, 20, 0, 2);
    LCD_printString("on the breadboard", 10, 40, 0, 2);
    LCD_printString("to mark bombs.", 10, 60, 0, 2);

    LCD_printString("Press the blue", 10, 90, 0, 2);
    LCD_printString("button to dig.", 10, 110, 0, 2);

    LCD_printString("Press the joystick", 10, 140, 0, 2);
    LCD_printString("to return to ", 10, 160, 0, 2);
    LCD_printString("the main menu.", 10, 180, 0, 2);

    LCD_Refresh(&cfg0);
    HAL_Delay(5000); 
}

void draw_gamescreen (void) {               // Draw the main gameplay screen
    draw_infobar();
    draw_checkerboard();
}

void draw_infobar (void) {                  // Display leftover crosses above the minefield

    LCD_Draw_Rect(0, 0, 240, 20, 4, 1);

    char cross_buffer[100];
    sprintf(cross_buffer, "Crosses Left: %i", crosses_left);
    LCD_printString (cross_buffer, 5, 3, 0, 2);
}

void draw_checkerboard (void) {                     // Draw checkered minefield maintaining crosses and revealed patches
    for (int col = 0; col < GRID_COLS; col++) {
        for (int row = 1; row < GRID_ROWS; row++) {        

            uint16_t colour; 

            if ((row + col) % 2 == 0) {         // Alternates the colours to make a checkered pattern
                colour = 11;
            } else {
                colour = 10;
            }

            int x = col * CELL_W;               // Make sure the squares are drawn next to each other and not on top
            int y = row * CELL_H;

            LCD_Draw_Rect(x, y, CELL_W, CELL_H, colour, 1);

            if (cross_grid[row][col]){
                LCD_Draw_Sprite_Colour_Scaled((x + 1), (y + 1), 18, 18, (uint8_t*) Cross, 3, 1);
            }

            if (revealed_grid[row][col]){
                char buffer[4];
                sprintf(buffer, "%i", number_grid[row][col]);
                LCD_Draw_Rect(x, y, CELL_W, CELL_H, 5, 1);
                LCD_printString(buffer, x + 5, y + 3, 2, 2);
            }
        }
    }
}

void draw_cursor (void) {                       // Draw a cursor to allow the user to select squares 
    LCD_Draw_Rect(pixel_x, pixel_y, CELL_W, CELL_H, 15, 0);
}

void place_bombs (void) {                   // Randomly place bombs making sure they are not in revealed squares

    int bombs_placed = 0;

    while (bombs_placed < NUM_BOMBS_AND_CROSSES) {

        int row = (rand() % (GRID_ROWS - 1)) + 1;           //
        int col = rand() % GRID_COLS;

        if (bomb_grid[row][col] == 0 && revealed_grid[row][col] == 0) {
            bomb_grid[row][col] = 1; 
            bombs_placed++;
            printf("Bomb: %i, %i\n", row, col);
        }
    }
}

void calculate_numbers (void) {                     // Calculate the numbers supposed to be on each square depending on the number of bombs surrounding them

    for (int col = 0; col < GRID_COLS; col++) {

        for (int row = 1; row < GRID_ROWS; row++) {

            if (bomb_grid[row][col] == 1) {

                for (int r = row - 1; r <= row + 1; r++) {
                    for (int c = col -1; c <= col + 1; c++) {

                        if (r <= 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS) {
                            continue; 
                        }
                        if (r == row && c == col){
                            continue; 
                        }
                        if (bomb_grid[r][c] == 1){
                            continue;
                        }

                        number_grid[r][c]++; 
                    }
                } 
            }
        }
    }
}

void dig (void) {                       // Checking if the user has dug up a bomb or revealing the number below

    if (first_dig == 0) {               // Initial dig revealing extra squares and placing bombs afterwards

        first_dig = 1;

        for (int row = -1; row <= 1; row++) {
            for (int col = -1; col <= 1; col++) {
                
                int r = cursor_y + row; 
                int c = cursor_x + col; 

                if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS){
                    continue; 
                }

                revealed_grid[r][c] = 1; 
            }
        }

        place_bombs();
        calculate_numbers();
    }

    if (cross_grid[cursor_y][cursor_x]) {
        return;
    }

    else if (bomb_grid[cursor_y][cursor_x] == 1) {
        game_lost();
    }

    else {                  
        revealed_grid[cursor_y][cursor_x] = 1;      // Hit dirt 
    }
}

void place_crosses (void) {                     // Crosses are placed to mark where the bombs might be and by placing them correctly you win
    if (cross_grid[cursor_y][cursor_x] == 0) {

        if (crosses_left > 0) {

            cross_grid[cursor_y][cursor_x] = 1; 
            //LCD_Draw_Sprite_Colour_Scaled((pixel_x + 1), (pixel_y + 1), 18, 18, (uint8_t*) Cross, 3, 1);
            crosses_left--;
            // printf("Flag: %i, %i\n", cursor_y, cursor_x);

            if (bomb_grid[cursor_y][cursor_x] == 1){
                marked_bomb++; 
            }

            if (marked_bomb == NUM_BOMBS_AND_CROSSES){
                game_won(); 
            }
        } 
        else {
            LCD_Fill_Buffer(0);
            LCD_printString("You have", 50, 50, 2, 3);
            LCD_printString("no crosses", 40, 90, 2, 3);
            LCD_printString("left!", 80, 130, 2, 3);

            LCD_Refresh(&cfg0);

            HAL_Delay(700); 
        }
    }

    else {
        cross_grid[cursor_y][cursor_x] = 0;

        /*uint16_t colour = (cursor_x + cursor_y) % 2 ? 11 : 10; 
        LCD_Draw_Rect(pixel_x, pixel_y, 20, 20, colour, 1);*/
        crosses_left++; 
        //printf("No Flag: %i, %i\n", cursor_y, cursor_x);
    }

    //LCD_Refresh(&cfg0);
}

void game_lost (void) {

    game_over = 1; 

    LCD_Fill_Buffer(0);
    LCD_printString("Game Over", 15, 50, 2, 4);
    LCD_Draw_Sprite_Colour_Scaled(60, 100, 8, 8, (uint8_t*) Sad_Face, 14, 15);
    LCD_Refresh(&cfg0); 
    
    buzzer_note(&buzzer_cfg, NOTE_D4, 50); 
    HAL_Delay(1000);
    buzzer_note(&buzzer_cfg, NOTE_CS4, 50); 
    HAL_Delay(1000); 
    buzzer_note(&buzzer_cfg, NOTE_C4, 50); 
    HAL_Delay(1000);

    buzzer_off(&buzzer_cfg);
}

void game_won (void) {

    game_over = 1; 
    
    LCD_Fill_Buffer(13); 
    LCD_printString("Congratulations!", 30, 50, 2, 2);
    LCD_printString("YOU HAVE", 50, 90, 2, 3);
    LCD_printString("WON", 90, 130, 2, 3);
    LCD_printString(":D", 100, 170, 2, 3);
    LCD_Refresh(&cfg0);

    for (int i = 0; i < 3; i++) {
        buzzer_note(&buzzer_cfg, NOTE_E6, 50); 
        HAL_Delay(note_delay_ms);
        buzzer_note(&buzzer_cfg, NOTE_F6, 50); 
        HAL_Delay(note_delay_ms); 
        buzzer_note(&buzzer_cfg, NOTE_G6, 50); 
        HAL_Delay(note_delay_ms);
    }
 
    buzzer_off(&buzzer_cfg); 
}

void reset_game (void) {                // Reset all counters and variables to their intial state

    cursor_x = 0; 
    cursor_y = 1;            
    pixel_x = 0; 
    pixel_y = 0;

    memset(bomb_grid, 0, sizeof(bomb_grid));
    memset(cross_grid, 0, sizeof(cross_grid));
    memset(number_grid, 0, sizeof(number_grid));
    memset(revealed_grid, 0, sizeof(revealed_grid));

    first_dig = 0; 
    crosses_left = NUM_BOMBS_AND_CROSSES; 
    marked_bomb = 0; 
    game_over = 0; 
    player_won = 0; 
}

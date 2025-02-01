/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "font6x9.h"
#include "hagl.h"
#include "rgb565.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint8_t sequence[50];       // Maximum sequence length
static uint8_t sequence_length;    // Current sequence length
static uint8_t current_level = 0;  // Game level
static uint8_t current_sequence_check_index = 0; // Current index for sequence checking

#define MAX_PLAYERS 20

static char player_global_usernames[MAX_PLAYERS][20] = { "" };  // Global array of player usernames
static uint8_t player_global_scores[MAX_PLAYERS] = {0};         // Global array of player scores

uint8_t uart_rx_buffer;
uint32_t game_state = 0;                  // Current game state
uint8_t player_count = 0;                 // Number of players
char player_names[4][20];                 // Names of up to 4 players
uint8_t current_player = 0;               // Index of the current player
uint8_t color_sequence[50];               // Color sequence
uint8_t player_scores[4] = {0, 0, 0, 0};    // Player scores

hagl_backend_t *display;

#define LINE_MAX_LENGTH 20

static char line_buffer[LINE_MAX_LENGTH] = {0};
static uint8_t line_length = 0;

void line_append(uint8_t value) {
    if (value == '\r' || value == '\n') {
        // Line ending character received
        if (line_length > 0) {
            // Add string terminator at the end of the line
            line_buffer[line_length] = '\0';

            // Process the line
            if (game_state == 5) {
                strncpy(player_names[current_player], line_buffer, LINE_MAX_LENGTH);
                player_names[current_player][LINE_MAX_LENGTH - 1] = '\0';  // Ensure null termination

                // Display the username on screen
                wchar_t username_label[40];
                swprintf(username_label, sizeof(username_label) / sizeof(wchar_t),
                         L"Player %u: %s", current_player + 1, player_names[current_player]);
                hagl_put_text(display, username_label, 10, 40 + (current_player * 20), WHITE, font6x9);
                lcd_copy();

                // Check if this is the last player
                if (current_player + 1 < player_count) {
                    current_player++;
                    char message[50];  // Message buffer
                    snprintf(message, sizeof(message), "Type Player %u username:\r\n", current_player + 1);
                    HAL_UART_Transmit(&huart2, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
                } else {
                    HAL_UART_Transmit(&huart2, (uint8_t *)"All usernames selected!\r\n", 27, HAL_MAX_DELAY);

                    for (uint8_t i = 0; i < player_count; i++) {
                        char message[50];  // Message buffer
                        snprintf(message, sizeof(message), "Player %u: %s\r\n", i + 1, player_names[i]);
                        HAL_UART_Transmit(&huart2, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
                    }

                    current_player = 0;

                    all_leds_off_but(1);
                    display_players_turn_text();

                    game_state = 6;  // Change game state
                }
            }
            // Reset the buffer
            line_length = 0;
        }
    } else {
        // Append character to buffer if space is available
        if (line_length < LINE_MAX_LENGTH - 1) {
            line_buffer[line_length++] = value;
        } else {
            // Buffer overflow, resetting
            line_length = 0;
        }
    }
}

uint8_t received_char;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // Add the received character to the line buffer
        line_append(received_char);

        // Restart reception in interrupt mode
        HAL_UART_Receive_IT(&huart2, &received_char, 1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    // Button handling
    uint32_t currentTime = HAL_GetTick();

    char buffer[64];
    sprintf(buffer, "Game state: %lu, GPIO_Pin: %u\r\n", game_state, GPIO_Pin);
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);

    if (game_state == 2 || game_state == 10) {
        if (GPIO_Pin == BT4_Pin) {
            game_state = 1;
            HAL_UART_Transmit(&huart2, (uint8_t *)"Back to Menu\r\n", 14, HAL_MAX_DELAY);
            all_leds_on_but(4);
            display_menu();
            return;
        }
    } else if (game_state == 1) {  // Menu
        if (GPIO_Pin == BT1_Pin) {
            game_state = 3;
            all_leds_on();
            display_player_count_selection();
            HAL_UART_Transmit(&huart2, (uint8_t *)"Player Count Selection\r\n", 25, HAL_MAX_DELAY);
        } else if (GPIO_Pin == BT2_Pin) {
            game_state = 2;
            HAL_UART_Transmit(&huart2, (uint8_t *)"Global Ranking selected\r\n", 23, HAL_MAX_DELAY);
            all_leds_off_but(4);
            display_global_ranking();
        } else if (GPIO_Pin == BT3_Pin) {
            game_state = 10;
            HAL_UART_Transmit(&huart2, (uint8_t *)"Credits selected\r\n", 19, HAL_MAX_DELAY);
            all_leds_off_but(4);
            display_credits();
        }
    } else if (game_state == 3) {  // Player Count Selection
        if (GPIO_Pin == BT1_Pin) {
            player_count = 1;
            game_state = 4;
            all_leds_off_but(1);
            HAL_UART_Transmit(&huart2, (uint8_t *)"1 Player selected\r\n", 20, HAL_MAX_DELAY);
        } else if (GPIO_Pin == BT2_Pin) {
            player_count = 2;
            game_state = 4;
            all_leds_off_but(2);
            HAL_UART_Transmit(&huart2, (uint8_t *)"2 Players selected\r\n", 21, HAL_MAX_DELAY);
        } else if (GPIO_Pin == BT3_Pin) {
            player_count = 3;
            game_state = 4;
            all_leds_off_but(3);
            HAL_UART_Transmit(&huart2, (uint8_t *)"3 Players selected\r\n", 21, HAL_MAX_DELAY);
        } else if (GPIO_Pin == BT4_Pin) {
            player_count = 4;
            game_state = 4;
            all_leds_off_but(4);
            HAL_UART_Transmit(&huart2, (uint8_t *)"4 Players selected\r\n", 21, HAL_MAX_DELAY);
        }

        if (GPIO_Pin == BT1_Pin || GPIO_Pin == BT2_Pin || GPIO_Pin == BT3_Pin || GPIO_Pin == BT4_Pin) {
            display_player_count_selected();
            all_leds_off();
            HAL_Delay(1000);
            display_player_introduction();
            game_state = 5;
            current_player = 0;
            HAL_UART_Transmit(&huart2, (uint8_t *)"Type Player 1 username:\r\n", 26, HAL_MAX_DELAY);
        }
    } else if (game_state == 6) {
        if (GPIO_Pin == BT1_Pin) {
            game_state = 7;
            all_leds_off();
            game_system();
        }
    } else if (game_state == 8) {
        uint8_t expected_led = sequence[current_sequence_check_index];
        uint8_t pressed_led = 0;

        // Map GPIO_Pin to LED number
        if (GPIO_Pin == BT1_Pin)
            pressed_led = 0;
        else if (GPIO_Pin == BT2_Pin)
            pressed_led = 1;
        else if (GPIO_Pin == BT3_Pin)
            pressed_led = 2;
        else if (GPIO_Pin == BT4_Pin)
            pressed_led = 3;

        // Check if the pressed LED matches the expected one
        if (pressed_led == expected_led) {
            current_sequence_check_index++;

            display_replay_sequence(current_level, current_sequence_check_index + 1, sequence_length);

            if (current_sequence_check_index >= sequence_length) {
                current_sequence_check_index = 0;
                game_state = 7;
                game_system();
            }
        } else {
            update_global_score(player_names[current_player], current_level - 1);
            update_score(current_level - 1);

            display_wrong();
            all_leds_blink(6, 100);
            HAL_Delay(2000);

            current_level = 0;
            if (current_player + 1 < player_count) {
                current_player += 1;
                all_leds_off_but(1);
                display_players_turn_text();

                game_state = 6;
            } else {
                HAL_UART_Transmit(&huart2, (uint8_t *)"Game finished!\r\n", 21, HAL_MAX_DELAY);
                game_state = 9;
                all_leds_off_but(4);
                display_game_ranking();
            }
        }
    } else if (game_state == 9) {
        if (GPIO_Pin == BT4_Pin) {
            game_state = 1;
            all_leds_on_but(4);
            display_menu();
        }
    }

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define MAX_RADIUS 80
#define STEP 3  // Increase the radius by this step during animation

void generate_sequence(uint8_t level) {
    sequence_length = level + 2;  // Sequence length increases with level

    for (uint8_t i = 0; i < sequence_length; i++) {
        sequence[i] = rand() % 4;  // Random color (0-3)
    }
}

void play_sequence(uint32_t base_delay) {
    for (uint8_t i = 0; i < sequence_length; i++) {
        // Turn on the corresponding LED based on the sequence color
        switch (sequence[i]) {
            case 0:
                HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
                break;
            case 1:
                HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
                break;
            case 2:
                HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
                break;
            case 3:
                HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
                break;
        }

        HAL_Delay(base_delay);  // Delay

        // Turn off all LEDs using the helper function
        all_leds_off();

        HAL_Delay(base_delay / 2);  // Short delay before the next LED
    }
}

void game_system() {
    if (game_state != 7) return;
    current_sequence_check_index = 0;

    lcd_clear(BLACK);

    // Display message to memorize the sequence
    display_remember_sequence();

    HAL_Delay(1000);

    // Generate a new sequence
    current_sequence_check_index = 0;
    current_level += 1;
    generate_sequence(current_level);

    // Display the sequence using the LEDs
    uint32_t base_delay = 1000 - (current_level * 100);  // Reduce delay with each level
    play_sequence(base_delay);

    // Display message prompting the user to repeat the sequence
    game_state = 8;
    display_replay_sequence();
}

void all_leds_blink(uint8_t times, uint32_t delay_ms) {
    for (uint8_t i = 0; i < times; i++) {
        // Turn on all LEDs using the helper function
        all_leds_on();
        HAL_Delay(delay_ms);

        // Turn off all LEDs using the helper function
        all_leds_off();
        HAL_Delay(delay_ms);
    }
}

void all_leds_on() {
    // Turn on all LEDs
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
}

void all_leds_on_but(int excluded_led) {
    // Turn off all LEDs
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);

    // Turn on all LEDs except the specified one
    if (excluded_led != 1) {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    }
    if (excluded_led != 2) {
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    }
    if (excluded_led != 3) {
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    }
    if (excluded_led != 4) {
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
    }
}

void all_leds_off() {
    // Turn off all LEDs
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
}

void all_leds_off_but(int num) {
    // Turn off all LEDs
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);

    // Turn on one LED based on its number
    switch (num) {
        case 1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
            break;
        case 2:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
            break;
        case 3:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
            break;
        case 4:
            HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
            break;
        default:
            // Do not turn on any LED if the number is invalid
            break;
    }
}

void animate_circles() {
    lcd_clear(BLACK);
    hagl_color_t colors[] = {YELLOW, GREEN, RED, BLUE};
    int16_t corner_positions[4][2] = {
        {0, 0},
        {LCD_WIDTH - 1, 0},
        {0, LCD_HEIGHT - 1},
        {LCD_WIDTH - 1, LCD_HEIGHT - 1}
    };

    // Initial radius for each circle
    int16_t radius[4] = {0, 0, 0, 0};

    // Animate the growth of the circles
    while (radius[0] < MAX_RADIUS) {
        for (uint16_t i = 0; i < 4; i++) {
            int16_t x0 = corner_positions[i][0];
            int16_t y0 = corner_positions[i][1];
            // Increase the radius for each circle
            radius[i] = (radius[i] + STEP > MAX_RADIUS) ? MAX_RADIUS : radius[i] + STEP;
            hagl_color_t color = colors[i];
            // Draw a filled circle
            hagl_fill_circle(display, x0, y0, radius[i], color);
        }

        // Draw the "Simon Says" text
        hagl_put_text(display, L"Simon Says", 49, 59, WHITE, font6x9);

        // Update the screen
        lcd_copy();
    }

    // After the animation, display the final state
    lcd_copy();

    HAL_Delay(500);
}

void display_menu() {
    lcd_clear(BLACK);

    // Colors for the menu option circles
    hagl_color_t colors[] = {RED, YELLOW, GREEN};

    // Y positions for the menu options
    int16_t positions_y[] = {20, 40, 60};

    // Display menu options with colored circles
    const wchar_t *menu_texts[] = {L"Start Game", L"Global Ranking", L"Credits"};

    for (uint8_t i = 0; i < 3; i++) {
        // Draw a small circle
        hagl_fill_circle(display, 10, positions_y[i] + 5, 5, colors[i]);

        // Display text next to the circle
        hagl_put_text(display, menu_texts[i], 20, positions_y[i], WHITE, font6x9);
    }

    lcd_copy();
}

void display_player_count_selection() {
    // Colors for the player options
    hagl_color_t colors[] = {RED, YELLOW, GREEN, BLUE};

    // X positions for the options (evenly divided horizontally)
    int16_t positions_x[4] = {
        LCD_WIDTH / 8,      // 1/8 of width for red
        LCD_WIDTH / 8 * 3,  // 3/8 of width for yellow
        LCD_WIDTH / 8 * 5,  // 5/8 of width for green
        LCD_WIDTH / 8 * 7   // 7/8 of width for blue
    };

    // Common Y position for all options (at the bottom of the screen)
    int16_t position_y = LCD_HEIGHT - 40;

    // Clear the screen
    lcd_clear(BLACK);

    // Display header text
    hagl_put_text(display, L"Select how many players", LCD_WIDTH / 2 - 70, 10, WHITE, font6x9);
    hagl_put_text(display, L"will play the game", LCD_WIDTH / 2 - 55, 20, WHITE, font6x9);

    // Display the player count options
    for (uint16_t i = 0; i < 4; i++) {
        // Draw small circles in the corresponding colors
        hagl_fill_circle(display, positions_x[i], position_y, 15, colors[i]);

        // Draw numbers ("1", "2", "3", "4") under each circle
        wchar_t label[2];  // Wide character array
        swprintf(label, sizeof(label) / sizeof(wchar_t), L"%u", i + 1);
        hagl_put_text(display, label, positions_x[i] - 3, position_y + 20, WHITE, font6x9);
    }

    // Update the screen
    lcd_copy();
}

void display_player_count_selected() {
    // Clear the screen
    lcd_clear(BLACK);

    // Display header indicating the selected number of players
    hagl_put_text(display, L"You selected", LCD_WIDTH / 2 - 40, 10, WHITE, font6x9);

    // Display the number of players
    wchar_t player_count_label[20];
    if (player_count > 1) {
        swprintf(player_count_label, sizeof(player_count_label) / sizeof(wchar_t), L"%u players", player_count);
    } else {
        swprintf(player_count_label, sizeof(player_count_label) / sizeof(wchar_t), L"1 player");
    }
    hagl_put_text(display, player_count_label, LCD_WIDTH / 2 - 30, 30, WHITE, font6x9);

    // Update the screen
    lcd_copy();
}

void display_player_introduction() {
    // Clear the screen
    lcd_clear(BLACK);

    // Display header prompting for usernames via UART
    hagl_put_text(display, L"Define usernames (UART):", LCD_WIDTH / 2 - 70, 10, WHITE, font6x9);

    for (uint8_t i = 0; i < player_count; i++) {
        wchar_t player_label[20];
        swprintf(player_label, sizeof(player_label) / sizeof(wchar_t), L"Player %u:", i + 1);
        hagl_put_text(display, player_label, 10, 40 + (i * 20), WHITE, font6x9);
    }

    // Update the screen
    lcd_copy();
}

void display_players_turn_text() {
    lcd_clear(BLACK);

    // Display the first line "It's now"
    hagl_put_text(display, L"It's now", LCD_WIDTH / 2 - (8 * 6 / 2), 10, WHITE, font6x9);

    // Display the player's name centered
    wchar_t player_turn[50];
    swprintf(player_turn, sizeof(player_turn) / sizeof(wchar_t), L"%s's", player_names[current_player]);
    hagl_put_text(display, player_turn, LCD_WIDTH / 2 - (wcslen(player_turn) * 6 / 2), 25, WHITE, font6x9);

    // Display the third line "turn"
    hagl_put_text(display, L"turn", LCD_WIDTH / 2 - (4 * 6 / 2), 40, WHITE, font6x9);

    // Display the text "Click"
    hagl_put_text(display, L"Click", LCD_WIDTH / 2 - (5 * 6 / 2), LCD_HEIGHT - 43, WHITE, font6x9);

    // Draw a red circle
    hagl_fill_circle(display, LCD_WIDTH / 2, LCD_HEIGHT - 25, 5, RED);

    // Display the text "when you're ready"
    hagl_put_text(display, L"when you're ready", LCD_WIDTH / 2 - (16 * 6 / 2), LCD_HEIGHT - 15, WHITE, font6x9);

    lcd_copy();
}

void display_remember_sequence() {
    lcd_clear(BLACK);

    // Display "Remember the sequence" text
    hagl_put_text(display, L"Remember the sequence", LCD_WIDTH / 2 - (21 * 6 / 2), 30, WHITE, font6x9);

    // Display "Memorize the order of LEDs" text
    hagl_put_text(display, L"Memorize the order of LEDs", LCD_WIDTH / 2 - (26 * 6 / 2), 50, WHITE, font6x9);

    lcd_copy();
}

void display_replay_sequence() {
    lcd_clear(BLACK);

    // Display "Your turn!" text
    hagl_put_text(display, L"Your turn!", LCD_WIDTH / 2 - (10 * 6 / 2), 20, WHITE, font6x9);

    // Display "Repeat the sequence" text
    hagl_put_text(display, L"Repeat the sequence", LCD_WIDTH / 2 - (19 * 6 / 2), 40, WHITE, font6x9);

    // Display the level number
    wchar_t level_text[20];
    swprintf(level_text, sizeof(level_text) / sizeof(wchar_t), L"Level: %u", current_level);
    hagl_put_text(display, level_text, LCD_WIDTH / 2 - (wcslen(level_text) * 6 / 2), 60, WHITE, font6x9);

    // Display the step information
    wchar_t step_text[30];
    swprintf(step_text, sizeof(step_text) / sizeof(wchar_t), L"Step: %u/%u", current_sequence_check_index, sequence_length);
    hagl_put_text(display, step_text, LCD_WIDTH / 2 - (wcslen(step_text) * 6 / 2), 80, WHITE, font6x9);

    lcd_copy();
}

void display_wrong() {
    lcd_clear(BLACK);

    // Display "WRONG" text at the center of the screen
    hagl_put_text(display, L"WRONG", LCD_WIDTH / 2 - (5 * 6 / 2), LCD_HEIGHT / 2 - 18, RED, font6x9);

    // Display "Completed levels: <level>" text below "WRONG"
    wchar_t level_text[20];
    swprintf(level_text, sizeof(level_text) / sizeof(wchar_t), L"Completed levels: %u", current_level - 1);
    hagl_put_text(display, level_text, LCD_WIDTH / 2 - (wcslen(level_text) * 6 / 2), LCD_HEIGHT / 2, WHITE, font6x9);

    lcd_copy();
}

void update_global_score(const char *username, uint8_t score) {
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        // If the user is found, update the score only if the new score is higher
        if (strcmp(player_global_usernames[i], username) == 0) {
            if (score > player_global_scores[i]) {
                player_global_scores[i] = score;
            }
            return;
        }
    }

    // If the user does not exist, find an empty slot and add the user
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (player_global_usernames[i][0] == '\0') {  // Empty slot
            strncpy(player_global_usernames[i], username, sizeof(player_global_usernames[i]) - 1);
            player_global_usernames[i][sizeof(player_global_usernames[i]) - 1] = '\0';  // Ensure safe string termination
            player_global_scores[i] = score;
            return;
        }
    }
}

void update_score(uint8_t score) {
    player_scores[current_player] = score;
}

void display_global_ranking() {
    lcd_clear(BLACK);

    // Temporary arrays for sorting
    char sorted_usernames[MAX_PLAYERS][20];
    uint8_t sorted_scores[MAX_PLAYERS];
    memcpy(sorted_usernames, player_global_usernames, sizeof(player_global_usernames));
    memcpy(sorted_scores, player_global_scores, sizeof(player_global_scores));

    // Bubble sort (from highest to lowest score)
    for (uint8_t i = 0; i < MAX_PLAYERS - 1; i++) {
        for (uint8_t j = 0; j < MAX_PLAYERS - i - 1; j++) {
            if (sorted_scores[j] < sorted_scores[j + 1]) {
                // Swap scores
                uint8_t temp_score = sorted_scores[j];
                sorted_scores[j] = sorted_scores[j + 1];
                sorted_scores[j + 1] = temp_score;

                // Swap usernames
                char temp_username[20];
                strncpy(temp_username, sorted_usernames[j], sizeof(temp_username));
                strncpy(sorted_usernames[j], sorted_usernames[j + 1], sizeof(sorted_usernames[j]));
                strncpy(sorted_usernames[j + 1], temp_username, sizeof(sorted_usernames[j + 1]));
            }
        }
    }

    // Display ranking header
    hagl_put_text(display, L"Global Ranking", LCD_WIDTH / 2 - (14 * 6 / 2), 5, WHITE, font6x9);

    // Display the list of players and their scores
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (sorted_usernames[i][0] != '\0') {  // Check if the user exists
            wchar_t ranking_entry[40];
            swprintf(ranking_entry, sizeof(ranking_entry) / sizeof(wchar_t), L"%s: %u", sorted_usernames[i], sorted_scores[i]);
            hagl_put_text(display, ranking_entry, 10, 20 + (i * 15), WHITE, font6x9);
        }
    }

    // Display centered "Click" text
    hagl_put_text(display, L"Click", LCD_WIDTH / 2 - (5 * 6 / 2), LCD_HEIGHT - 43, WHITE, font6x9);

    // Draw a blue circle below "Click"
    hagl_fill_circle(display, LCD_WIDTH / 2, LCD_HEIGHT - 25, 5, BLUE);

    // Display centered "to get back to menu" text below the circle
    hagl_put_text(display, L"to get back to menu", LCD_WIDTH / 2 - (19 * 6 / 2), LCD_HEIGHT - 15, WHITE, font6x9);

    lcd_copy();
}

void display_game_ranking() {
    lcd_clear(BLACK);

    // Temporary arrays for sorting
    char sorted_names[4][20];
    uint8_t sorted_scores[4];
    memcpy(sorted_names, player_names, sizeof(player_names));
    memcpy(sorted_scores, player_scores, sizeof(player_scores));

    // Bubble sort (from highest to lowest score)
    for (uint8_t i = 0; i < player_count - 1; i++) {
        for (uint8_t j = 0; j < player_count - i - 1; j++) {
            if (sorted_scores[j] < sorted_scores[j + 1]) {
                // Swap scores
                uint8_t temp_score = sorted_scores[j];
                sorted_scores[j] = sorted_scores[j + 1];
                sorted_scores[j + 1] = temp_score;

                // Swap player names
                char temp_name[20];
                strncpy(temp_name, sorted_names[j], sizeof(temp_name));
                strncpy(sorted_names[j], sorted_names[j + 1], sizeof(sorted_names[j]));
                strncpy(sorted_names[j + 1], temp_name, sizeof(sorted_names[j + 1]));
            }
        }
    }

    // Display ranking header
    hagl_put_text(display, L"Game Ranking", LCD_WIDTH / 2 - (12 * 6 / 2), 5, WHITE, font6x9);

    // Display the sorted list of players and their scores
    for (uint8_t i = 0; i < player_count; i++) {
        if (sorted_names[i][0] != '\0') {  // Check if the user exists
            wchar_t ranking_entry[40];
            swprintf(ranking_entry, sizeof(ranking_entry) / sizeof(wchar_t), L"%s: %u", sorted_names[i], sorted_scores[i]);
            uint16_t text_width = wcslen(ranking_entry) * 6;  // Text width in pixels
            hagl_put_text(display, ranking_entry, LCD_WIDTH / 2 - (text_width / 2), 20 + (i * 15), WHITE, font6x9);
        }
    }

    // Display centered "Click" text
    hagl_put_text(display, L"Click", LCD_WIDTH / 2 - (5 * 6 / 2), LCD_HEIGHT - 43, WHITE, font6x9);

    // Draw a blue circle below "Click"
    hagl_fill_circle(display, LCD_WIDTH / 2, LCD_HEIGHT - 25, 5, BLUE);

    // Display centered "to get back to menu" text below the circle
    hagl_put_text(display, L"to get back to menu", LCD_WIDTH / 2 - (19 * 6 / 2), LCD_HEIGHT - 15, WHITE, font6x9);

    lcd_copy();
}

void display_credits() {
    lcd_clear(BLACK);

    // Display the centered "Credits" title
    hagl_put_text(display, L"Credits", LCD_WIDTH / 2 - (7 * 6 / 2), 5, WHITE, font6x9);

    // Display centered lines of text with spacing below "Credits"
    hagl_put_text(display, L"Marcin Kondrat", LCD_WIDTH / 2 - (14 * 6 / 2), 17, WHITE, font6x9);
    hagl_put_text(display, L"Jowita Ochrymiuk", LCD_WIDTH / 2 - (16 * 6 / 2), 28, WHITE, font6x9);
    hagl_put_text(display, L"Projekt zaliczeniowy", LCD_WIDTH / 2 - (20 * 6 / 2), 39, WHITE, font6x9);
    hagl_put_text(display, L"Architektura Komputerów", LCD_WIDTH / 2 - (23 * 6 / 2), 50, WHITE, font6x9);
    hagl_put_text(display, L"Politechnika Białostocka", LCD_WIDTH / 2 - (24 * 6 / 2), 61, WHITE, font6x9);
    hagl_put_text(display, L"2025", LCD_WIDTH / 2 - (4 * 6 / 2), 72, WHITE, font6x9);

    // Display centered "Click" text
    hagl_put_text(display, L"Click", LCD_WIDTH / 2 - (5 * 6 / 2), LCD_HEIGHT - 43, WHITE, font6x9);

    // Draw a blue circle below "Click"
    hagl_fill_circle(display, LCD_WIDTH / 2, LCD_HEIGHT - 25, 5, BLUE);

    // Display centered "to get back to menu" text below the circle
    hagl_put_text(display, L"to get back to menu", LCD_WIDTH / 2 - (19 * 6 / 2), LCD_HEIGHT - 15, WHITE, font6x9);

    lcd_copy();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &received_char, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    lcd_init();

    display = hagl_init();

    animate_circles();
    game_state = 1;
    all_leds_on_but(4);
    display_menu();
    while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

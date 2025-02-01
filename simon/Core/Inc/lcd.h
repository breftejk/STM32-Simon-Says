#pragma once

#include <stdint.h>

#define LCD_WIDTH	160
#define LCD_HEIGHT	128

//Definicje kolorów z odwróconą kolejnością bajtów
#define BLACK			0x0000
#define RED			0x00f8
#define GREEN			0xe007
#define BLUE			0x1f00
#define YELLOW			0xe0ff
#define MAGENTA			0x1ff8
#define CYAN			0xff07
#define WHITE			0xffff

void lcd_clear(uint16_t color);
void lcd_set_rotation(uint8_t rotation);
void lcd_init(void); // Inicjalizacja wyświetlacza
void lcd_put_pixel(void *_surface, int16_t x, int16_t y, uint16_t color);
void lcd_copy(void); // Przesłanie zawartości bufora

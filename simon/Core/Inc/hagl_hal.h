#ifndef INC_HAGL_HAL_H_
#define INC_HAGL_HAL_H_

#include "lcd.h"
#include "hagl/backend.h"

#define DISPLAY_WIDTH 	(LCD_WIDTH)
#define DISPLAY_HEIGHT 	(LCD_HEIGHT)
#define DISPLAY_DEPTH 	16

void hagl_hal_init(hagl_backend_t *backend);

#endif /* INC_HAGL_HAL_H_ */

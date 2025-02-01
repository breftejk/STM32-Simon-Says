#include "hagl/backend.h"
#include "hagl_hal.h"

void hagl_hal_init(hagl_backend_t *backend)
{
    backend->width = DISPLAY_WIDTH;
    backend->height = DISPLAY_HEIGHT;
    backend->depth = DISPLAY_DEPTH;
    backend->put_pixel = lcd_put_pixel;
}

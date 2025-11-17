
#include <lvgl.h>
#include <zmk/endpoints.h>

#define LVGL_BACKGROUND                                                                            \
    IS_ENABLED(CONFIG_ZMK_DISPLAY_INVERT) ? lv_color_black() : lv_color_white()
#define LVGL_FOREGROUND                                                                            \
    IS_ENABLED(CONFIG_ZMK_DISPLAY_INVERT) ? lv_color_white() : lv_color_black()

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH  DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)

#if CONFIG_LV_COLOR_DEPTH_1 == 1
#define BYTES_PER_PIXEL 1
#define MONOCHROME
#elif CONFIG_LV_COLOR_DEPTH_8 == 1
#define BYTES_PER_PIXEL 1
#elif CONFIG_LV_COLOR_DEPTH_16 == 1
#define BYTES_PER_PIXEL 2
#elif CONFIG_LV_COLOR_DEPTH_32 == 1
#define BYTES_PER_PIXEL 4
#else
#error "No display pixel format defined, is your board supported?"
#endif
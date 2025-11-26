#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <lvgl.h>
#include "mod_status.h"
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>

#define ZMK_LED_NUMLOCK_BIT BIT(0)
#define ZMK_LED_CAPSLOCK_BIT BIT(1)
#define ZMK_LED_SCROLLLOCK_BIT BIT(2)

#include <fonts.h>
#include <util.h>
#include <dimensions.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SYMBOLS_COUNT 7

struct hid_indicators_status_state {
    zmk_hid_indicators_t flags;  // HID Indicator Status Bit Mask
} hid_state;

static void update_mod_status(struct zmk_widget_mod_status *widget)
{
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    char text[SYMBOLS_COUNT * 4 * 2 + 1] = "";
    int idx = 0;
    char *syms[SYMBOLS_COUNT] = {NULL};
    int n = 0;

    if (hid_state.flags & ZMK_LED_CAPSLOCK_BIT)
        syms[n++] = "󰘲"; 
    if (hid_state.flags & ZMK_LED_NUMLOCK_BIT)
        syms[n++] = ""; 
    if (hid_state.flags & ZMK_LED_SCROLLLOCK_BIT)
        syms[n++] = "S"; 
    if (mods & (MOD_LCTL | MOD_RCTL))
        syms[n++] = "󰘴";
    if (mods & (MOD_LSFT | MOD_RSFT))
        syms[n++] = "󰘶"; 
    if (mods & (MOD_LALT | MOD_RALT))
        syms[n++] = "󰘵"; 
    if (mods & (MOD_LGUI | MOD_RGUI))
        syms[n++] = ""; 

    for (int i = 0; i < n; ++i)
    { 
        if (i > 0)
            idx += snprintf(&text[idx], sizeof(text) - idx, " ");
        idx += snprintf(&text[idx], sizeof(text) - idx, "%s", syms[i]);
    }

    lv_label_set_text(widget->obj, idx ? text : "");
}

static void mod_status_timer_cb(struct k_timer *timer)
{
    struct zmk_widget_mod_status *widget = k_timer_user_data_get(timer);
    hid_state.flags = zmk_hid_indicators_get_current_profile();
    update_mod_status(widget);
}

static struct k_timer mod_status_timer;

int zmk_widget_mod_status_init(struct zmk_widget_mod_status *widget, lv_obj_t *parent, lv_point_t size)
{
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, size.x, size.y);
    lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
#if (GRID_CELL_HEIGHT * L_MOD_ROW_CNT) < 20
    lv_obj_set_style_text_font(widget->obj, &nerd_12, 0);
#elif (GRID_CELL_HEIGHT * L_MOD_ROW_CNT) < 24
    lv_obj_set_style_text_font(widget->obj, &nerd_20, 0);
#else
    lv_obj_set_style_text_font(widget->obj, &nerd_24, 0);
#endif
    lv_label_set_text(widget->obj, "HELLO!");

    // k_timer_init(&mod_status_timer, mod_status_timer_cb, NULL);
    // k_timer_user_data_set(&mod_status_timer, widget);
    // k_timer_start(&mod_status_timer, K_MSEC(100), K_MSEC(100));

    return 0;
}

lv_obj_t *zmk_widget_mod_status_obj(struct zmk_widget_mod_status *widget)
{
    return widget->obj;
}

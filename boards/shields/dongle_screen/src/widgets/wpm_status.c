/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#include "wpm_status.h"
#include <fonts.h>
#include <util.h>
#include <dimensions.h>

#define SYMBOLS_COUNT 2

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_status_state
{
    int wpm;
};

static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);

    return (struct wpm_status_state){
        .wpm = ev ? ev->state : 0};
}

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    int idx = 0;
    char text[SYMBOLS_COUNT * 4 + 1] = "";
    
    if (state.wpm > 150 && state.wpm < 9999)
    {
        idx += snprintf(&text[idx], sizeof(text) - idx, "󰓅");
    }
    else if (state.wpm > 100 && state.wpm < 9999)
    {
        idx += snprintf(&text[idx], sizeof(text) - idx, "󰾅");
    }
    else
    {
        idx += snprintf(&text[idx], sizeof(text) - idx, "󰾆");
    }
    snprintf(&text[idx], sizeof(text) - idx, "%03i", state.wpm);
    lv_label_set_text(widget->obj, text);
}

static void wpm_status_update_cb(struct wpm_status_state state)
{
    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_wpm(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

// output_status.c
int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent, lv_point_t size)
{
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, size.x, size.y);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_border_side(widget->obj, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_border_width(widget->obj, 1, 0);
    lv_obj_set_style_border_color(widget->obj, LVGL_FOREGROUND, 0);
#if GRID_CELL_HEIGHT < 20
    lv_obj_set_style_text_font(widget->obj, &nerd_12, 0);
#else
    lv_obj_set_style_text_font(widget->obj, &nerd_20, 0);
#endif

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}

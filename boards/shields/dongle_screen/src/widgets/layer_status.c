/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include <util.h>
#include <dimensions.h>

#include "layer_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_status_state
{
    uint8_t index;
    const char *label;
};

static void set_layer_symbol(lv_obj_t *label, struct layer_status_state state)
{
    if (state.label == NULL)
    {
        char text[7] = {};

        sprintf(text, "%i", state.index);

        lv_label_set_text(label, text);
    }
    else
    {
        char text[13] = {};

        snprintf(text, sizeof(text), "%s", state.label);

        lv_label_set_text(label, text);
    }
}

static void layer_status_update_cb(struct layer_status_state state)
{
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_symbol(widget->obj, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
{
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent, lv_point_t size)
{    
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, size.x, size.y);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
#if (GRID_CELL_HEIGHT * L_LAYER_ROW_CNT) < 20
    lv_obj_set_style_text_font(widget->obj, &nerd_12, 0);
#elif (GRID_CELL_HEIGHT * L_LAYER_ROW_CNT) < 24
    lv_obj_set_style_text_font(widget->obj, &nerd_20, 0);
#elif (GRID_CELL_HEIGHT * L_LAYER_ROW_CNT) < 32
    lv_obj_set_style_text_font(widget->obj, &nerd_24, 0);
#else
    lv_obj_set_style_text_font(widget->obj, &nerd_24, 0);
#endif
    lv_label_set_text(widget->obj, "󰼭");
    // lv_obj_set_style_border_side(widget->obj, LV_BORDER_SIDE_FULL, 0);
    // lv_obj_set_style_border_width(widget->obj, 1, 0);
    // lv_obj_set_style_border_color(widget->obj, LVGL_FOREGROUND, 0);
    sys_slist_append(&widgets, &widget->node);

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget)
{
    return widget->obj;
}
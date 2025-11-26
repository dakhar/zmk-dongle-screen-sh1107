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
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"

#include <fonts.h>
#include <util.h>
#include <dimensions.h>

#define SYMBOLS_COUNT 3

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state
{
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static struct output_status_state get_state(const zmk_event_t *_eh)
{
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),                     // 0 = USB , 1 = BLE
        .active_profile_index = zmk_ble_active_profile_index(),            // 0-3 BLE profiles
        .active_profile_connected = zmk_ble_active_profile_is_connected(), // 0 = not connected, 1 = connected
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),        // 0 =  BLE not bonded, 1 = bonded
        .usb_is_hid_ready = zmk_usb_is_hid_ready()};                       // 0 = not ready, 1 = ready
}

static void set_status_symbol(struct zmk_widget_output_status *widget, struct output_status_state state)
{
    int idx = 0;
    char text[SYMBOLS_COUNT * 4 + 1] = "";
#if (GRID_CELL_HEIGHT * L_OUT_ROW_CNT) < 20
    static const char *sym_usb = "";
    static const char *sym_unbonded[] = {"1[F]","2[F]","3[F]","4[F]","5[F]"} ;
    static const char *sym_bonded[] = {"1[D]","2[D]","3[D]","4[D]","5[D]"} ;
    static const char *sym_connected[] = {"1[C]","2[C]","3[C]","4[C]","5[C]"} ;
#else
    static const char *sym_usb = "󰕓";
    static const char *sym_unbonded[] = {"󰎦","󰎩","󰎬","󰎮","󰎰"} ;
    static const char *sym_bonded[] = {"󰎥","󰎨","󰎫","󰎲","󰎯"} ;
    static const char *sym_connected[] = {"󰎤","󰎧","󰎪","󰎭","󰎱"} ;
#endif
    char *syms[SYMBOLS_COUNT] = {NULL};
    int n = 0;
    switch (state.selected_endpoint.transport) {
        case ZMK_TRANSPORT_USB:
            syms[n++] = sym_usb;
            break;
        case ZMK_TRANSPORT_BLE:
            if (! state.usb_is_hid_ready) {
                syms[n++] = "󱇰";
            }
            if (state.active_profile_bonded) {
                if (state.active_profile_connected) {
                    syms[n++] = "󰂱";
                    syms[n++] = sym_connected[state.selected_endpoint.ble.profile_index];
                } else {
                    syms[n++] = "󰂲";
                    syms[n++] = sym_bonded[state.selected_endpoint.ble.profile_index];
                }
            } else {
                syms[n++] = "󰂳";
                syms[n++] = sym_unbonded[state.selected_endpoint.ble.profile_index];
            }
        break;
    }
    
    if (n <= SYMBOLS_COUNT) {
        for (int i = 0; i < n; ++i) {
            if (syms[i] == NULL) continue;
            idx += snprintf(&text[idx], sizeof(text) - idx, "%s", syms[i]);
        }
    }
    lv_label_set_text(widget->obj, idx ? text : "");
}

static void output_status_update_cb(struct output_status_state state)
{
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_status_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

// output_status.c
int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent, lv_point_t size)
{
    widget->obj = lv_label_create(parent);
    lv_obj_set_size(widget->obj, size.x, size.y);
    lv_obj_align(widget->obj, LV_ALIGN_TOP_RIGHT, (- size.x), 0);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_RIGHT, 0);

#if (GRID_CELL_HEIGHT * L_OUT_ROW_CNT) < 20
    lv_obj_set_style_text_font(widget->obj, &nerd_12, 0);
#else
    lv_obj_set_style_text_font(widget->obj, &nerd_20, 0);
#endif
    lv_obj_set_style_border_side(widget->obj, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_border_width(widget->obj, 1, 0);
    lv_obj_set_style_border_color(widget->obj, LVGL_FOREGROUND, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget)
{
    return widget->obj;
}

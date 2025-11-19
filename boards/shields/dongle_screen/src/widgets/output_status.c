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

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static lv_coord_t *widget_row_dsc;
static lv_coord_t *widget_col_dsc;

lv_point_t selection_line_points[] = {{0, 0}, {13, 0}}; // will be replaced with lv_point_precise_t

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
    
    char *transport_text[6] = {NULL};
    switch (state.selected_endpoint.transport) {
        case ZMK_TRANSPORT_USB:
            transport_text[0] = "";
            break;
        case ZMK_TRANSPORT_BLE:
            transport_text[0] = "󰂲";
#ifdef ZMK_SPLIT_ROLE_CENTRAL
            transport_text[1] = "󰎦";
            transport_text[2] = "󰎩";
            transport_text[3] = "󰎬";
            transport_text[4] = "󰎮";
            transport_text[5] = "󰎰";
#endif
            if (state.active_profile_bonded) {
                if (state.active_profile_connected) {
                    transport_text[1] = "󰂱";
#ifdef ZMK_SPLIT_ROLE_CENTRAL
                    switch (state.selected_endpoint.ble.profile_index) {
                    case 0:
                        transport_text[1] = "󰎤";
                        break;
                    case 1:
                        transport_text[2] = "󰎧";
                        break;
                    case 2:
                        transport_text[3] = "󰎪";
                        break;
                    case 3:
                        transport_text[4] = "󰎭";
                        break;
                    case 4:
                        transport_text[5] = "󰎱";
                        break;
                    }
#endif
                }
            } else {
                transport_text[1] = "󰂳";
#ifdef ZMK_SPLIT_ROLE_CENTRAL
                switch (state.selected_endpoint.ble.profile_index) {
                case 0:
                    transport_text[1] = "󰎥";
                    break;
                case 1:
                    transport_text[2] = "󰎨";
                    break;
                case 2:
                    transport_text[3] = "󰎫";
                    break;
                case 3:
                    transport_text[4] = "󰎲";
                    break;
                case 4:
                    transport_text[5] = "󰎯";
                    break;
                }
#endif
            }
            break;
    }
    lv_obj_set_style_text_font(widget->transport_label, &nerd_24, 0);
    lv_obj_set_style_text_align(widget->transport_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(widget->transport_label, transport_text);
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
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, size.x, size.y);
    lv_obj_set_layout(widget->obj, LV_LAYOUT_GRID);

    widget_row_dsc = lv_mem_alloc(3 * sizeof(lv_coord_t));
    if (!widget_row_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return -1;
    }
    for (uint8_t i = 0; i < 2; i++) {
        widget_row_dsc[i] = size.y / 2; 
    }
    widget_row_dsc[2] = LV_GRID_TEMPLATE_LAST;

    widget_col_dsc = lv_mem_alloc(3 * sizeof(lv_coord_t));
    if (!widget_col_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return -1;
    }
    for (uint8_t i = 0; i < 2; i++) {
        widget_col_dsc[i] = size.x / 2; 
    }
    widget_col_dsc[2] = LV_GRID_TEMPLATE_LAST;  // Terminator
    lv_obj_set_style_grid_column_dsc_array(widget->obj, widget_col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(widget->obj, widget_row_dsc, 0);

    widget->transport_label = lv_label_create(widget->obj);
    lv_obj_set_style_text_font(widget->transport_label, &nerd_24, 0);
    lv_obj_set_grid_cell(widget->transport_label, 
                            LV_GRID_ALIGN_CENTER, 0, 2,
                            LV_GRID_ALIGN_CENTER, 0, 1);

    widget->ble_label = lv_label_create(widget->obj);
    lv_obj_set_grid_cell(widget->ble_label,
                            LV_GRID_ALIGN_CENTER, 0, 2,
                            LV_GRID_ALIGN_CENTER, 1, 1);

    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget)
{
    return widget->obj;
}

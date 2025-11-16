/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/split/central.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"
#include <util.h>

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
#define BAT_COUNT (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET)
#define LABEL_HEIGHT 20
#define NRG_METER_W 25
#define NRG_METER_H 4
#define BORDER_SZ   1
#define X_OFFSET    2
#define BAT_WIDTH   X_OFFSET + BORDER_SZ + NRG_METER_W + BORDER_SZ
#define BAT_HEIGHT BORDER_SZ + NRG_METER_H + BORDER_SZ
#define BITS_PER_PIXEL 4
/*
  *******************
***                 *
***                 *
  *******************
*/

#define PALETTE_SIZE 5
static lv_color_t palette[PALETTE_SIZE];
static void init_palette(void) {
    // Вычисляем цвета с учётом инверсии
    lv_color_t bg = IS_ENABLED(CONFIG_ZMK_DISPLAY_INVERT) 
                    ? lv_color_black()
                    : lv_color_white();
    lv_color_t fg = IS_ENABLED(CONFIG_ZMK_DISPLAY_INVERT)
                    ? lv_color_white()
                    : lv_color_black();
    // Заполняем палитру
    palette[0] = bg;
    palette[1] = fg;
    palette[2] = lv_palette_main(LV_PALETTE_GREEN);
    palette[3] = lv_palette_main(LV_PALETTE_YELLOW);
    palette[4] = lv_palette_main(LV_PALETTE_RED);
}

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    bool initialized;
    uint8_t *buffer;
    lv_obj_t *symbol;
    lv_obj_t *label;
} battery_objects[BAT_COUNT];
    
// Peripheral reconnection tracking
// ZMK sends battery events with level < 1 when peripherals disconnect
static int8_t last_battery_levels[BAT_COUNT];

static void init_peripheral_tracking(void) {
    for (int i = 0; i < BAT_COUNT; i++) {
        last_battery_levels[i] = -1; // -1 indicates never seen before
    }
}

static bool is_peripheral_reconnecting(uint8_t source, uint8_t new_level) {
    if (source >= BAT_COUNT) {
        return false;
    }
    
    int8_t previous_level = last_battery_levels[source];
    
    // Reconnection detected if:
    // 1. Previous level was < 1 (disconnected/unknown) AND
    // 2. New level is >= 1 (valid battery level)
    bool reconnecting = (previous_level < 1) && (new_level >= 1);
    
    if (reconnecting) {
        LOG_INF("Peripheral %d reconnection: %d%% -> %d%% (was %s)", 
                source, previous_level, new_level, 
                previous_level == -1 ? "never seen" : "disconnected");
    }
    
    return reconnecting;
}

static void draw_battery(struct battery_state state, struct battery_object *battery) {
    // if (!battery || !battery->symbol) return;
    // if (state.level < 1 || state.level > 100) return;

    // int filled_width = (NRG_METER_W * state.level + 50) / 100;  // Округление
    // filled_width = LV_CLAMP(0, filled_width, NRG_METER_W);
    
    lv_color_t bg_color;
    lv_color_t fg_color;
    lv_color_t meter_color;
    
    bg_color.full = 0; 
    fg_color.full = 1; 
    if (state.level > 30) {
        meter_color.full = 2;  // Green
    } else if (state.level > 10) {
        meter_color.full = 3;  // Yellow
    } else {
        meter_color.full = 4;  // Red
    }
    
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = fg_color;
    // rect_dsc.border_color = fg_color;
    // rect_dsc.border_width = BORDER_SZ;
    // rect_dsc.radius = 2;
    
    // if (!battery->initialized) {
    //     // Draw battery contact
    //     int contact_y;
    //     int contact_h = BAT_HEIGHT - 4;
    //     if (contact_h % 2 == 0) {
    //         if (contact_h >= 2) {
    //             contact_y = contact_h / 2;
    //         } else {
    //             contact_y = 1;
    //             contact_h = 2;
    //         }
    //     } else {
    //         if (contact_h > 1) {
    //             contact_y = contact_h / 2;
    //         } else if (contact_h == 1) {
    //             contact_y = 1;
    //             contact_h = 3;
    //         } else {
    //             contact_y = 0;
    //             contact_h = BAT_HEIGHT;
    //         }
    //     }
    //     lv_canvas_draw_rect(battery->symbol, 0, contact_y, X_OFFSET, contact_h, &rect_dsc);
    //     lv_canvas_draw_rect(battery->symbol, X_OFFSET, 0, BAT_WIDTH, BAT_HEIGHT, &rect_dsc);
    //     battery->initialized = true;
    // }
    
    // Fill energy meter
    // if (filled_width > 1) {
    //     rect_dsc.bg_color = fg_color;
    //     rect_dsc.border_color = fg_color;
    //     lv_canvas_draw_rect(battery->symbol, X_OFFSET + BORDER_SZ, BORDER_SZ, filled_width, NRG_METER_H, &rect_dsc);
    // }
    lv_canvas_draw_rect(battery->symbol, 0, 0, BAT_WIDTH, BAT_HEIGHT, &rect_dsc);
    // 7. Обновляем только изменённую область
    lv_obj_invalidate(battery->symbol);
}

static void draw_label(struct battery_state state, struct battery_object *battery) {
    if (!battery || !battery->label) return;
    if (state.level <= 0)
    {
        lv_obj_set_style_text_color(battery->label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(battery->label, "X");
    } else if (state.level < 10) {
        lv_obj_set_style_text_color(battery->label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text_fmt(battery->label, "%4u", state.level);
    } else if (state.level < 30) {
        lv_obj_set_style_text_color(battery->label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_label_set_text_fmt(battery->label, "%4u", state.level);
    } else {
        lv_obj_set_style_text_color(battery->label, LVGL_FOREGROUND, 0);
        lv_label_set_text_fmt(battery->label, "%4u", state.level);
    }
    lv_obj_invalidate(battery->label);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= BAT_COUNT) {
        return;
    }
    // Check for reconnection using the existing battery level mechanism
    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    // if (last_battery_levels[state.source] != state.level) {
        last_battery_levels[state.source] = state.level;
    // } else {
    //     return;
    // }
    // Wake screen on reconnection
    if (reconnecting) {
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0    
        LOG_INF("Peripheral %d reconnected (battery: %d%%), requesting screen wake", 
                state.source, state.level);
#else 
        LOG_INF("Peripheral %d reconnected (battery: %d%%)", 
                state.source, state.level);
#endif
    }
    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;
    
    draw_battery(state, &battery_objects[state.source]);
    draw_label(state, &battery_objects[state.source]);
    
    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(symbol);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);

}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state) {
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) { 
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTER */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    init_palette();
    lv_coord_t parent_width = lv_obj_get_width(parent);
    // Set table rows height
    static lv_coord_t row_dsc[] = {LABEL_HEIGHT, BAT_HEIGHT, LV_GRID_TEMPLATE_LAST};
    // Set table rows width
    lv_coord_t *col_dsc = lv_mem_alloc((BAT_COUNT + 1) * sizeof(lv_coord_t));
    if (!col_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return -1;
    }
    for (uint8_t i = 0; i < BAT_COUNT; i++) {
        col_dsc[i] = parent_width / BAT_COUNT; 
    }
    col_dsc[BAT_COUNT] = LV_GRID_TEMPLATE_LAST;
    
// Create table
    widget->obj = lv_obj_create(parent);
    lv_obj_set_style_grid_column_dsc_array(widget->obj, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(widget->obj, row_dsc, 0);
    lv_obj_set_size(widget->obj, parent_width, LABEL_HEIGHT + BAT_HEIGHT);
    lv_obj_center(widget->obj);
    lv_obj_set_layout(widget->obj, LV_LAYOUT_GRID);
    
    // Add batteries into table
    for (int i = 0; i < BAT_COUNT; i++) {
        struct battery_object *battery = &battery_objects[i];
        // Add lable to first row
        battery->label = lv_label_create(widget->obj);
        lv_obj_set_grid_cell(battery->label, LV_GRID_ALIGN_CENTER, i, 1,
                            LV_GRID_ALIGN_CENTER, 0, 1);
        lv_obj_add_flag(battery->label, LV_OBJ_FLAG_HIDDEN);
        
        // Add canvas to second row
        const int buf_size = (BAT_WIDTH * BAT_HEIGHT * BITS_PER_PIXEL) / 8 + 1;  // bytes
        battery->buffer = lv_mem_alloc(buf_size); 
        if (!battery->buffer) {
            LV_LOG_ERROR("Canvas buffer allocation failed!");
            return -1;
        }
        battery->symbol = lv_canvas_create(widget->obj);
        lv_canvas_set_buffer(battery->symbol, battery->buffer, 
                                BAT_WIDTH, BAT_HEIGHT, 
                                LV_IMG_CF_INDEXED_4BIT); // 4-bit index
        for (int c = 0; c < PALETTE_SIZE; c++) {
            lv_img_buf_set_palette(lv_canvas_get_img(battery->symbol), c, palette[c]);
        }
        lv_obj_set_grid_cell(battery->symbol, LV_GRID_ALIGN_CENTER, i, 1,
                            LV_GRID_ALIGN_CENTER, 1, 1);
        lv_obj_add_flag(battery->symbol, LV_OBJ_FLAG_HIDDEN);

    }

    sys_slist_append(&widgets, &widget->node);

    // Initialize peripheral tracking
    init_peripheral_tracking();

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
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
#define LABEL_W 25
#define NRG_METER_W 4
#define NRG_METER_H 25
#define BORDER_SZ   1
#define CONTACT_L    2

#if NRG_METER_W >= NRG_METER_H
#define BATTERY_W   (CONTACT_L + BORDER_SZ + NRG_METER_W + BORDER_SZ)
#define BATTERY_H   (BORDER_SZ + NRG_METER_H + BORDER_SZ)
#define CANVAS_W    ((BATTERY_W < LABEL_W) ? LABEL_W : BATTERY_W)
#else
#define BATTERY_W   (BORDER_SZ + NRG_METER_W + BORDER_SZ)
#define BATTERY_H   (BORDER_SZ + NRG_METER_H + BORDER_SZ + CONTACT_L)
#define CANVAS_W    (BATTERY_W + LABEL_W)
#endif

#if CONFIG_LV_COLOR_DEPTH == 8
#define BYTES_PER_PIXEL 1
#elif CONFIG_LV_COLOR_DEPTH == 16
#define BYTES_PER_PIXEL 2
#elif CONFIG_LV_COLOR_DEPTH == 32
#define BYTES_PER_PIXEL 4
#else 
#define BYTES_PER_PIXEL 1
#define MONOCHROME true
#endif


static lv_draw_rect_dsc_t rect_shell;
static lv_draw_rect_dsc_t rect_meter;
static lv_draw_rect_dsc_t rect_contact;
static lv_draw_label_dsc_t label_dsc;

static void init_descriptors(void) {
    
    lv_draw_rect_dsc_init(&rect_shell);
    rect_shell.bg_color = LVGL_BACKGROUND;
    rect_shell.bg_opa = LV_OPA_COVER;
    rect_shell.border_width = BORDER_SZ;
    rect_shell.border_color = LVGL_FOREGROUND;
    rect_shell.border_side = LV_BORDER_SIDE_FULL;

    lv_draw_rect_dsc_init(&rect_meter);
    rect_meter.bg_opa = LV_OPA_COVER;

    lv_draw_rect_dsc_init(&rect_contact);
    rect_contact.bg_color = LVGL_FOREGROUND;
    rect_contact.bg_opa = LV_OPA_COVER;
    rect_contact.border_width = BORDER_SZ;
    rect_contact.border_color = LVGL_BACKGROUND;
    rect_contact.border_side = (NRG_METER_W < NRG_METER_H) ? \
                    (LV_BORDER_SIDE_LEFT || LV_BORDER_SIDE_RIGHT) : \
                    (LV_BORDER_SIDE_TOP || LV_BORDER_SIDE_BOTTOM);
                    
    lv_draw_label_dsc_init(&label_dsc);
}

static int label_h = BATTERY_H;
static void calc_label_h(lv_obj_t *obj) {
    const lv_font_t *font = label_dsc.font ? label_dsc.font : lv_theme_get_font_normal(obj);
    label_h = font->line_height;
}

static int canvas_h = BATTERY_H;
static void calc_canvas_h(lv_obj_t *obj) {
    if (NRG_METER_W >= NRG_METER_H) {
        canvas_h = BATTERY_H + label_h;
    } else {
        canvas_h = ((BATTERY_H < label_h) ? label_h : BATTERY_H);
    }
    
}

static lv_coord_t *widget_row_dsc;
static lv_coord_t *widget_col_dsc;

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    bool *initialized;
    uint8_t *buffer;
    lv_obj_t *canvas;
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

static void draw_battery(struct battery_state state, struct battery_object battery) {
    assert(NRG_METER_W > 0 && NRG_METER_H > 0);
    assert(BATTERY_W > CONTACT_L && BATTERY_H > CONTACT_L);
    if (!battery.canvas) return;
    lv_color_t meter_color;
    lv_color_t text_color;
    char level_str[4];
    int text_y = (canvas_h - label_h) / 2;

    if (MONOCHROME) {
        meter_color = LVGL_FOREGROUND;
        text_color = LVGL_FOREGROUND;
    } else if (state.level > 30) {
        meter_color = lv_palette_main(LV_PALETTE_GREEN);
        text_color = LVGL_FOREGROUND;
    } else if (state.level > 10) {
        meter_color = lv_palette_main(LV_PALETTE_YELLOW);
        text_color = LVGL_FOREGROUND;
    } else {
        meter_color = lv_palette_main(LV_PALETTE_RED);
        text_color = lv_palette_main(LV_PALETTE_RED);
    }
    rect_meter.bg_color = meter_color;
    label_dsc.color = text_color;

    int len = snprintf(level_str, sizeof(level_str), "%d", state.level);
    if (len < 0 || len >= sizeof(level_str) || state.level < 1 || state.level > 100) {
        strcpy(level_str, "X");
    }

    lv_canvas_fill_bg(battery.canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    // Fill energy meter
    const int meter_width = LV_CLAMP(0, (NRG_METER_W * state.level + 50) / 100, NRG_METER_W);
    const int meter_height = LV_CLAMP(0, (NRG_METER_H * state.level + 50) / 100, NRG_METER_H);
    
    if (NRG_METER_W >= NRG_METER_H)
    {
        lv_canvas_draw_text (battery.canvas, 0, 0, LABEL_W, &label_dsc, level_str); 
        if (state.level < 1 || state.level > 100) return;
        lv_canvas_draw_rect(battery.canvas, 0, label_h, CONTACT_L, BATTERY_H, &rect_contact);
        lv_canvas_draw_rect(battery.canvas, CONTACT_L, label_h, BATTERY_W - CONTACT_L, BATTERY_H, &rect_shell);
        lv_canvas_draw_rect(battery.canvas, CONTACT_L + BORDER_SZ, label_h + BORDER_SZ, meter_width, NRG_METER_H, &rect_meter);
    } else {
        lv_canvas_draw_text (battery.canvas, BATTERY_W, text_y, LABEL_W, &label_dsc, level_str);
        if (state.level < 1 || state.level > 100) return;
        lv_canvas_draw_rect(battery.canvas, 0, 0, BATTERY_W, BATTERY_H - CONTACT_L, &rect_contact);
        lv_canvas_draw_rect(battery.canvas, 0, CONTACT_L, BATTERY_W, BATTERY_H - CONTACT_L, &rect_shell);
        lv_canvas_draw_rect(battery.canvas, BORDER_SZ, BORDER_SZ, NRG_METER_W, NRG_METER_H - meter_height, &rect_meter);
    }
    
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= BAT_COUNT) {
        return;
    }
    
    // Check for reconnection using the existing battery level mechanism
    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    
    // Update our tracking
    last_battery_levels[state.source] = state.level;


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
    lv_obj_t *canvas = battery_objects[state.source].canvas;

    draw_battery(state, battery_objects[state.source]);
    
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(canvas);

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
    init_descriptors();
    calc_label_h();
    calc_canvas_h();

    lv_coord_t parent_width = lv_obj_get_width(parent);
    
    widget_row_dsc = lv_mem_alloc(2 * sizeof(lv_coord_t));
    if (!widget_row_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return -1;
    }
    widget_row_dsc[0] = canvas_h;
    widget_row_dsc[1] = LV_GRID_TEMPLATE_LAST;

    widget_col_dsc = lv_mem_alloc((BAT_COUNT + 1) * sizeof(lv_coord_t));
    if (!widget_col_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return -1;
    }
    for (uint8_t i = 0; i < BAT_COUNT; i++) {
        widget_col_dsc[i] = parent_width / BAT_COUNT; 
    }
    widget_col_dsc[BAT_COUNT] = LV_GRID_TEMPLATE_LAST;  // Terminator
    
    widget->obj = lv_obj_create(parent);
    lv_obj_set_style_grid_column_dsc_array(widget->obj, widget_col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(widget->obj, widget_row_dsc, 0);
    lv_obj_set_size(widget->obj, parent_width, canvas_h);
    lv_obj_center(widget->obj);
    lv_obj_set_layout(widget->obj, LV_LAYOUT_GRID);

    for (int i = 0; i < BAT_COUNT; i++) {
        struct battery_object *battery = &battery_objects[i];
        
        const int buf_size = (CANVAS_W * canvas_h * BYTES_PER_PIXEL);  // bytes
        battery->buffer = lv_mem_alloc(buf_size); 
        if (!battery->buffer) {
            LV_LOG_ERROR("Canvas buffer allocation failed!");
            return -1;
        }
        battery->canvas = lv_canvas_create(widget->obj);
        lv_obj_set_grid_cell(battery->canvas, LV_GRID_ALIGN_CENTER, i, 1,
                            LV_GRID_ALIGN_CENTER, 0, 1);
        lv_canvas_set_buffer(battery->canvas, battery->buffer, CANVAS_W, canvas_h, LV_IMG_CF_TRUE_COLOR);
        lv_obj_add_flag(battery->canvas, LV_OBJ_FLAG_HIDDEN);
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
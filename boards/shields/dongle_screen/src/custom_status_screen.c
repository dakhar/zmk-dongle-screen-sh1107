/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include <util.h>

struct widget_layout
{
    uint8_t col, row, colspan, rowspan;
};

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
#include "widgets/wpm_status.h"
static struct zmk_widget_wpm_status wpm_status_widget;
static struct widget_layout wpm_layout = {L_WPM_COL,L_WPM_ROW,L_WPM_COL_CNT,L_WPM_ROW_CNT};
#endif

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
#include "widgets/output_status.h"
static struct zmk_widget_output_status output_status_widget;
static struct widget_layout output_layout = {L_OUT_COL,L_OUT_ROW,L_OUT_COL_CNT,L_OUT_ROW_CNT};
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
#include "widgets/layer_status.h"
static struct zmk_widget_layer_status layer_status_widget;
static struct widget_layout layer_layout = {L_LAYER_COL,L_LAYER_ROW,L_LAYER_COL_CNT,L_LAYER_ROW_CNT};
#endif

//Modifiers status layout
#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
#include "widgets/mod_status.h"
static struct zmk_widget_mod_status mod_widget;
static struct widget_layout mod_layout = {L_MOD_COL,L_MOD_ROW,L_MOD_COL_CNT,L_MOD_ROW_CNT};
#endif

//Battery status layout
#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
#include "widgets/battery_status.h"
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
static struct widget_layout battery_layout = {L_BAT_COL,L_BAT_ROW,L_BAT_COL_CNT,L_BAT_ROW_CNT};
#endif

typedef void (*widget_init_func)(void *, lv_obj_t *, lv_point_t);
typedef lv_obj_t *(*widget_obj_getter)(void *);

static void init_widget(
    void *widget,
    lv_obj_t *parent,
    struct widget_layout *layout,
    widget_init_func init_func,
    widget_obj_getter obj_getter)
{
    // Calculate physical dimensions
    lv_point_t size = {
        .x = layout->colspan * GRID_CELL_WIDTH,
        .y = layout->rowspan * GRID_CELL_HEIGHT};

    // Init widget
    init_func(widget, parent, size);

    // Set grid position
    lv_obj_set_grid_cell(
        obj_getter(widget),
        LV_GRID_ALIGN_CENTER, layout->col, layout->colspan,
        LV_GRID_ALIGN_CENTER, layout->row, layout->rowspan);
}


#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

lv_style_t global_style;
static lv_coord_t *screen_row_dsc;
static lv_coord_t *screen_col_dsc;

lv_obj_t *zmk_display_status_screen()
{
    lv_style_init(&global_style);
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);
    if (!screen) {
        LV_LOG_ERROR("Failed to create screen");
        return NULL;
    }
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(screen, LVGL_BACKGROUND, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_style_set_text_color(&global_style, LVGL_FOREGROUND);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    screen_row_dsc = lv_mem_alloc(ROW_COUNT * sizeof(lv_coord_t));
    if (!screen_row_dsc)
    {
        LV_LOG_ERROR("Memory allocation failed!");
        return NULL;
    }
    for (uint8_t i = 0; i < ROW_COUNT; i++)
    {
        screen_row_dsc[i] = GRID_CELL_HEIGHT;
    }
    screen_row_dsc[ROW_COUNT] = LV_GRID_TEMPLATE_LAST; // Terminator

    screen_col_dsc = lv_mem_alloc(COL_COUNT * sizeof(lv_coord_t));
    if (!screen_col_dsc)
    {
        LV_LOG_ERROR("Memory allocation failed!");
        return NULL;
    }
    for (uint8_t i = 0; i < COL_COUNT; i++)
    {
        screen_col_dsc[i] = GRID_CELL_WIDTH;
    }
    screen_col_dsc[COL_COUNT] = LV_GRID_TEMPLATE_LAST; // Terminator
    
    lv_obj_set_layout(screen, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(screen, screen_col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(screen, screen_row_dsc, 0);

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
    init_widget(
        &wpm_status_widget,
        screen,
        &wpm_layout,
        zmk_widget_wpm_status_init,
        zmk_widget_wpm_status_obj);
#endif

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
    init_widget(
        &output_status_widget,
        screen,
        &output_layout,
        zmk_widget_output_status_init,
        zmk_widget_output_status_obj);
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
    init_widget(
        &layer_status_widget,
        screen,
        &layer_layout,
        zmk_widget_layer_status_init,
        zmk_widget_layer_status_obj);
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
    init_widget(
        &mod_widget,
        screen,
        &mod_layout,
        zmk_widget_mod_status_init,
        zmk_widget_mod_status_obj);
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
    init_widget(
        &dongle_battery_status_widget,
        screen,
        &battery_layout,
        zmk_widget_dongle_battery_status_init,
        zmk_widget_dongle_battery_status_obj);
#endif
    return screen;
}
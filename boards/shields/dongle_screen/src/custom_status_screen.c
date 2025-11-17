/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include <util.h>

#define ROW_COUNT 6
#define COL_COUNT 8

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
#include "widgets/output_status.h"
static struct zmk_widget_output_status output_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
#include "widgets/layer_status.h"
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
#include "widgets/battery_status.h"
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
#include "widgets/wpm_status.h"
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
#include "widgets/mod_status.h"
static struct zmk_widget_mod_status mod_widget;
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

lv_style_t global_style;
static lv_coord_t *screen_row_dsc;
static lv_coord_t *screen_col_dsc;

lv_obj_t *zmk_display_status_screen()
{
    lv_style_init(&global_style);
    lv_obj_t *screen;
    lv_point_t size;

    screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(screen, LVGL_BACKGROUND, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_style_set_text_font(&global_style, &lv_font_unscii_8); // ToDo: Font is not recognized
    lv_style_set_text_color(&global_style, LVGL_FOREGROUND);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);
    const int cell_w = DISPLAY_WIDTH / ROW_COUNT;
    const int cell_h = DISPLAY_HEIGHT / COL_COUNT;
    screen_row_dsc = lv_mem_alloc(ROW_COUNT * sizeof(lv_coord_t));
    if (!screen_row_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return NULL;
    }
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
        screen_row_dsc[i] = cell_h; 
    }
    screen_row_dsc[ROW_COUNT] = LV_GRID_TEMPLATE_LAST;  // Terminator

    screen_col_dsc = lv_mem_alloc(COL_COUNT * sizeof(lv_coord_t));
    if (!screen_col_dsc) {
        LV_LOG_ERROR("Memory allocation failed!");
        return NULL;
    }
    for (uint8_t i = 0; i < COL_COUNT; i++) {
        screen_col_dsc[i] = cell_w; 
    }
    screen_col_dsc[COL_COUNT] = LV_GRID_TEMPLATE_LAST;  // Terminator
    lv_obj_set_layout(screen, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(screen, screen_col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(screen, screen_row_dsc, 0);
 

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
    size.x = 3 * cell_w;
    size.y = 1 * cell_h;
    zmk_widget_wpm_status_init(&wpm_status_widget, screen, size);
    lv_obj_set_grid_cell(zmk_widget_wpm_status_obj(&wpm_status_widget), 
                            LV_GRID_ALIGN_CENTER, 0, 3,
                            LV_GRID_ALIGN_CENTER, 0, 1);
#endif

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
    size.x = (COL_COUNT-3) * cell_w;
    size.y = 1 * cell_h;
    zmk_widget_output_status_init(&output_status_widget, screen, size);
    lv_obj_set_grid_cell(zmk_widget_output_status_obj(&output_status_widget), 
                            LV_GRID_ALIGN_CENTER, 3, COL_COUNT-3,
                            LV_GRID_ALIGN_CENTER, 0, 1);
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
    size.x = (COL_COUNT) * cell_w;
    size.y = 2 * cell_h;
    zmk_widget_layer_status_init(&layer_status_widget, screen, size);
    lv_obj_set_grid_cell(zmk_widget_layer_status_obj(&layer_status_widget), 
                            LV_GRID_ALIGN_CENTER, 0, COL_COUNT,
                            LV_GRID_ALIGN_CENTER, 1, 2);
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
    size.x = (COL_COUNT) * cell_w;
    size.y = 1 * cell_h;
    zmk_widget_mod_status_init(&mod_widget, screen, size);
    lv_obj_set_grid_cell(zmk_widget_mod_status_obj(&mod_widget), 
                            LV_GRID_ALIGN_CENTER, 0, COL_COUNT,
                            LV_GRID_ALIGN_CENTER, ROW_COUNT-2, 1);
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
    size.x = (COL_COUNT) * cell_w;
    size.y = 1 * cell_h;
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen, size);
    lv_obj_set_grid_cell(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), 
                            LV_GRID_ALIGN_CENTER, 0, COL_COUNT,
                            LV_GRID_ALIGN_CENTER, ROW_COUNT-1, 1);
#endif

    return screen;
}
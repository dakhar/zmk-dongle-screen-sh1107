/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include <util.h>

#define ROW_COUNT 6
#define COL_COUNT 8
#define GRID_CELL_WIDTH (DISPLAY_WIDTH / COL_COUNT)
#define GRID_CELL_HEIGHT (DISPLAY_HEIGHT / ROW_COUNT)
#define BATTERY_ROW (ROW_COUNT - 1)
#define MODIFIER_ROW (ROW_COUNT - 2)

typedef void (*widget_init_func)(void*, lv_obj_t*, lv_point_t);
typedef lv_obj_t* (*widget_obj_getter)(void*);

static void init_status_widget(
    void *widget,                        // указатель на структуру виджета
    lv_obj_t *parent,                  // родительский контейнер (screen)
    uint8_t col, uint8_t row,         // позиция в сетке
    uint8_t colspan, uint8_t rowspan,   // размеры в ячейках сетки
    widget_init_func init_func,       // функция инициализации виджета
    widget_obj_getter obj_getter       // функция получения lv_obj_t из виджета
) {
    // Расчёт физических размеров
    const int cell_w = DISPLAY_WIDTH / COL_COUNT;
    const int cell_h = DISPLAY_HEIGHT / ROW_COUNT;
    lv_point_t size = {
        .x = colspan * cell_w,
        .y = rowspan * cell_h
    };

    // Инициализация виджета
    init_func(widget, parent, size);

    // Установка позиции в сетке
    lv_obj_set_grid_cell(
        obj_getter(widget),
        LV_GRID_ALIGN_CENTER, col, colspan,
        LV_GRID_ALIGN_CENTER, row, rowspan
    );
}
/**
 * Заполняет все ячейки сетки текстовыми метками с координатами (R1C1, R1C2 и т.д.)
 * @param parent Родительский объект LVGL (экран или контейнер с сеткой)
 * @param rows Количество строк в сетке
 * @param cols Количество столбцов в сетке
 * @param cell_width Ширина ячейки в пикселях
 * @param cell_height Высота ячейки в пикселях
 */
static void fill_grid_with_coordinates(lv_obj_t *parent, uint8_t rows, uint8_t cols, int cell_width, int cell_height) {
    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < cols; col++) {
            // Создаём текстовый лейбл для ячейки
            lv_obj_t *label = lv_label_create(parent);
            
            // Формируем текст: "R{row+1}C{col+1}" (индексация с 1)
            char text[10];
            snprintf(text, sizeof(text), "R%dC%d", row + 1, col + 1);
            lv_label_set_text(label, text);
            
            // Выравниваем текст по центру ячейки
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            
            // Устанавливаем позицию и размер через сетку
            lv_obj_set_grid_cell(label,
                LV_GRID_ALIGN_CENTER, col, 1,  // колонка, ширина в ячейках
                LV_GRID_ALIGN_CENTER, row, 1); // строка, высота в ячейках

            // Дополнительно: можно задать стиль (цвет, шрифт)
            // lv_obj_set_style_text_color(label, lv_color_white(), 0);
            // lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        }
    }
}

struct widget_layout {
    uint8_t col, row, colspan, rowspan;
    bool active_config;
};

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
    const int cell_w = DISPLAY_WIDTH / COL_COUNT;
    const int cell_h = DISPLAY_HEIGHT / ROW_COUNT;
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
    fill_grid_with_coordinates(screen, ROW_COUNT, COL_COUNT, GRID_CELL_WIDTH, GRID_CELL_HEIGHT);

// #if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
//     init_status_widget(
//         &wpm_status_widget,
//         screen,
//         0, 0,      // col, row
//         3, 1,      // colspan, rowspan
//         zmk_widget_wpm_status_init,
//         zmk_widget_wpm_status_obj
//     );
// #endif

// #if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
//     init_status_widget(
//         &output_status_widget,
//         screen,
//         3, 0,      // col, row
//         COL_COUNT-3, 1,      // colspan, rowspan
//         zmk_widget_output_status_init,
//         zmk_widget_output_status_obj
//     );
// #endif

// #if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
//     init_status_widget(
//         &layer_status_widget,
//         screen,
//         0, 1,      // col, row
//         COL_COUNT, 2,      // colspan, rowspan
//         zmk_widget_layer_status_init,
//         zmk_widget_layer_status_obj
//     );
// #endif

// #if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
//     init_status_widget(
//         &mod_widget,
//         screen,
//         0, MODIFIER_ROW,      // col, row
//         COL_COUNT, 1,      // colspan, rowspan
//         zmk_widget_mod_status_init,
//         zmk_widget_mod_status_obj
//     );
// #endif

// #if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
//     init_status_widget(
//         &dongle_battery_status_widget,
//         screen,
//         0, 0,      // col, row
//         COL_COUNT, 1,      // colspan, rowspan
//         zmk_widget_dongle_battery_status_init,
//         zmk_widget_dongle_battery_status_obj
//     );
// #endif

    return screen;
}
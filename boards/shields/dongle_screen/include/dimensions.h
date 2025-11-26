#pragma once

#define START_ROW 0
#define START_COL 0
#define COL_COUNT 2

#define L_WPM_COL START_COL
#define L_WPM_ROW START_ROW
#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
#define L_WPM_COL_CNT ((COL_COUNT + 1) / 2)
#define L_WPM_ROW_CNT 1
#else
#define L_WPM_COL_CNT 0
#define L_WPM_ROW_CNT 0
#endif

#define L_OUT_COL  L_WPM_COL_CNT
#define L_OUT_ROW  L_WPM_ROW
#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
#define L_OUT_COL_CNT   (COL_COUNT - L_WPM_COL_CNT)
#define L_OUT_ROW_CNT   L_WPM_ROW_CNT
#else
#define L_OUT_COL_CNT  0
#define L_OUT_ROW_CNT  0
#endif

#define L_LAYER_COL     START_COL
#define L_LAYER_ROW     (MAX(L_WPM_ROW_CNT, L_OUT_ROW_CNT))
#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
#define L_LAYER_COL_CNT COL_COUNT
#define L_LAYER_ROW_CNT 2
#else
#define L_LAYER_COL_CNT 0
#define L_LAYER_ROW_CNT 0
#endif

//Modifiers status layout
#define L_MOD_COL       START_COL
#define L_MOD_ROW       (L_LAYER_ROW + L_LAYER_ROW_CNT)
#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
#define L_MOD_COL_CNT   COL_COUNT
#define L_MOD_ROW_CNT   1
#else
#define L_MOD_COL_CNT   0
#define L_MOD_ROW_CNT   0
#endif

//Battery status layout
#define L_BAT_COL       START_COL
#define L_BAT_ROW       (L_MOD_ROW + L_MOD_ROW_CNT)
#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
#define L_BAT_COL_CNT   COL_COUNT
#define L_BAT_ROW_CNT   1
#else
#define L_BAT_COL_CNT   0
#define L_BAT_ROW_CNT   0
#endif

#define ROW_COUNT (MAX(L_WPM_ROW_CNT,L_OUT_ROW_CNT) + L_LAYER_ROW_CNT + L_MOD_ROW_CNT + L_BAT_ROW_CNT )
#if ROW_COUNT > 10
#error "Too many rows, consider reducing widgets"
#endif

#if (L_WPM_COL_CNT + L_OUT_COL_CNT) > COL_COUNT || L_LAYER_COL_CNT > COL_COUNT || L_MOD_COL_CNT > COL_COUNT || L_BAT_COL_CNT > COL_COUNT
#error "Column count limit!!!"
#endif

#define GRID_CELL_WIDTH (DISPLAY_WIDTH / COL_COUNT)
#define GRID_CELL_HEIGHT (DISPLAY_HEIGHT / ROW_COUNT)
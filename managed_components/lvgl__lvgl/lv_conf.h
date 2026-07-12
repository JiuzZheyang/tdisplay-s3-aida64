/* lv_conf.h - LVGL v9 minimal config for T-Display-S3 (170x320 ST7789) */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_PATH             ""

#define LV_COLOR_DEPTH           16
#define LV_COLOR_16_SWAP         1
#define LV_MEM_SIZE              (32 * 1024)
#define LV_USE_OS                LV_OS_FREERTOS
#define LV_DISP_DEF_REFR_RATE    60

/* Fonts - must enable all used sizes */
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_24   1

/*hal*/
#define LV_USE_FILESYSTEM        0
#define LV_USE_ANIMATION         1
#define LV_USE_LABEL             1
#define LV_USE_RECT              1
#define LV_USE_ARC               1
#define LV_USE_BAR               1
#define LV_USE_BTN               1
#define LV_USE_IMG               1
#define LV_USE_LINE              1
#define LV_USE_ROLLER            1

#endif

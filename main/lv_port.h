#ifndef LV_PORT_H
#define LV_PORT_H

#include "esp_lcd_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_WIDTH  170
#define LCD_HEIGHT 320

void lv_port_init(esp_lcd_panel_handle_t panel_handle);

// LVGL 对象访问锁：在非 LVGL 任务（如 SSE 回调任务）里修改 UI 时必须加锁，
// 否则会与 LVGL 渲染任务并发改同一对象导致更新丢失/错乱。
void lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);

#endif

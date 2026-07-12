#include "lv_port.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "lv_port";

static TaskHandle_t lvgl_task_handle = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;

static void lvgl_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    int x_start = area->x1;
    int x_end = area->x2 + 1;
    int y_start = area->y1;
    int y_end = area->y2 + 1;

    // 字节序交换已在面板层处理：lcd_i80.c 中 panel_dev_config.data_endian
    // = LCD_RGB_DATA_ENDIAN_LITTLE（等价于旧版 swap_color_bytes=1），
    // ST7789 通过 RAMCTRL 命令进入小端模式，故此处不再手动交换，避免双重交换。
    esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5));
        lvgl_port_lock(0);
        lv_task_handler();
        lvgl_port_unlock();
    }
}

void lvgl_port_lock(int timeout_ms)
{
    if (lvgl_mutex) {
        if (timeout_ms == 0) {
            xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        } else {
            xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(timeout_ms));
        }
    }
}

void lvgl_port_unlock(void)
{
    if (lvgl_mutex) {
        xSemaphoreGive(lvgl_mutex);
    }
}

void lv_port_init(esp_lcd_panel_handle_t panel_handle)
{
    lvgl_mutex = xSemaphoreCreateMutex();

    lv_init();

    // Create display buffer
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_color_t *buf1 = heap_caps_malloc(LCD_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_display_set_buffers(disp, buf1, NULL, LCD_WIDTH * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_callback);
    lv_display_set_user_data(disp, panel_handle);

    // 注意：旋转已在面板硬件层完成（lcd_i80.c 的 esp_lcd_panel_swap_xy）。
    // 若在 LVGL 层再调用 lv_display_set_rotation(90)，会使 LVGL 逻辑分辨率变为
    // 320x170，而 partial 绘制缓冲仍按 170 宽分配，渲染时越界踩坏堆中 LVGL 对象，
    // 导致对象指针变成 0xffffffff、首次渲染即 LoadProhibited 崩溃。故此处不旋转。

    // Start LVGL task
    xTaskCreatePinnedToCore(lvgl_task, "LVGL", 10240, NULL, 5, &lvgl_task_handle, 1);

    ESP_LOGI(TAG, "LVGL initialized: 170x320 portrait, own task");
}

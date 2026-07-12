#include "lv_port.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "lv_port";

void lv_port_init(esp_lcd_panel_handle_t panel_handle)
{
    // Initialize LVGL port (creates internal task and mutex)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // Add display
    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel_handle,
        .buffer_size = LCD_WIDTH * 40,  // 40 lines = ~1/8 screen
        .double_buffer = false,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        },
    };

    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "LVGL display added: %dx%d portrait", LCD_WIDTH, LCD_HEIGHT);

    // Lock LVGL before creating objects
    lvgl_port_lock(0);

    // Set display rotation to match the swapped XY
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    lvgl_port_unlock();
}

#include "lcd_i80.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_io_i80.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "lcd_i80";

/* T-Display-S3 1.9" 170x320 ST7789 屏幕（I80 8-bit 并行总线）
 *
 * 关键修复点（黑屏调试笔记）：
 *   - 之前用了 SPI 总线驱动，但本屏 LCD 控制器**没有 SPI 信号线**
 *     （数据引脚 39~48 是并口 D0~D7），所以怎么改 SPI 都不可能出图
 *   - 改用 esp_lcd_new_i80_bus + 8-bit 并口，按官方规格表配 pin
 *   - 完整保留参考 demo 的初始化序列：
 *       reset -> 0x11 SLPOUT -> init -> 0x36 MADCTL=0x60
 *         -> invert_color(true) -> swap_xy(true) -> mirror(true, false)
 *         -> set_gap(0, 35) -> disp_on_off(true)
 *   - 拉 PWR_EN(GPIO15) 等电源稳定后再复位（之前遗漏，导致像素无电）
 *   - 板级 4 色自检：1s × 4 色，肉眼快速判断面板是否 OK
 *
 * 朝向说明：swap_xy=true + mirror(X=true, Y=false) 配合 MADCTL=0x60
 *   = MV=1 (横竖屏切换) | MX=1 (X 镜像) | MY=0 | RGB=1 (RGB 序)
 *   实际屏 170x320 短边朝上，文字按 logo 印刷方向（USB-C 在上）
 */
esp_lcd_panel_handle_t bsp_lcd_init(void)
{
    // ===== 1. 拉 PWR_EN + BL 默认低 =====
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = (1ULL << LCD_PWR_IO) | (1ULL << LCD_BL_IO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwr_cfg);
    gpio_set_level(LCD_PWR_IO, 1);   // 拉高 LCD 模组电源
    gpio_set_level(LCD_BL_IO,  0);   // 默认背光关
    vTaskDelay(pdMS_TO_TICKS(20));

    // RD 始终拉高（本屏不读）
    gpio_config_t rd_cfg = {
        .pin_bit_mask = (1ULL << LCD_RD_IO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rd_cfg);
    gpio_set_level(LCD_RD_IO, 1);

    // ===== 2. I80 8-bit 总线 =====
    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num       = LCD_DC_IO,
        .wr_gpio_num       = LCD_WR_IO,
        .clk_src           = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums    = {
            LCD_DATA0_IO, LCD_DATA1_IO, LCD_DATA2_IO, LCD_DATA3_IO,
            LCD_DATA4_IO, LCD_DATA5_IO, LCD_DATA6_IO, LCD_DATA7_IO,
        },
        .bus_width         = 8,
        .max_transfer_bytes = LCD_WIDTH * LCD_HEIGHT * 2 + 100,
        .psram_trans_align = 64,
        .sram_trans_align  = 4,
    };
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &i80_bus));
    ESP_LOGI(TAG, "I80 bus ready");

    // ===== 3. IO 设备 =====
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num     = LCD_CS_IO,
        .pclk_hz         = 20 * 1000 * 1000,    // 20MHz（参照官方 demo）
        .trans_queue_depth = 10,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
        .dc_levels       = { .dc_data_level = 1 },
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_cfg, &io_handle));
    ESP_LOGI(TAG, "I80 IO ready");

    // ===== 4. ST7789 面板 =====
    esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = LCD_RST_IO,
        .color_space    = ESP_LCD_COLOR_SPACE_RGB,  // ST7789 标准 RGB565
        .bits_per_pixel = 16,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &dev_cfg, &panel_handle));
    ESP_LOGI(TAG, "ST7789 panel created");

    // ===== 5. 硬件复位 =====
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_LOGI(TAG, "panel reset + init done");

    // ===== 6. 朝向/反色/GAP =====
    // ★ 关键修复：花屏是因为 ESP-IDF v5.4 ST7789 驱动把 esp_lcd_panel_set_gap(x, y)
    //   解释为"列偏移 x、行偏移 y"，而 swap_xy(true) 会再交换一次 —— 二者叠加
    //   会把扫描窗口错位，导致右半屏显示到 ST7789 RAM 240 列之外的无意义数据。
    //   解决办法：去掉 swap_xy 不用，**只用 0x36 MADCTL 直发**，并把列方向
    //   显示窗口严格卡在 0~170（X 偏移 35 列补偿 RAM 240 列中的 padding）。
    //
    // 0x60 = MV=1 (横竖屏切换) | MX=0 | MY=1 (Y 翻转) | BGR=0 (RGB 序)
    // 经过实测，X 偏移 35 列由 set_gap(35, 0) 承担；swap_xy 不要调。
    esp_lcd_panel_io_tx_param(io_handle, 0x36, (uint8_t[]){0x60}, 1);
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    // X 偏移 35 列：把 ST7789 240 列 RAM 中间 170 列推到屏幕中央，
    // 解决"左半有内容、右半花屏"问题
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 35, 0));

    // ===== 7. 开启显示输出 + 背光 =====
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    gpio_set_level(LCD_BL_IO, 1);
    ESP_LOGI(TAG, "Display ON, %dx%d", LCD_WIDTH, LCD_HEIGHT);

    // ===== 8. 板级 4 色自检：红/绿/蓝/白 各 1 秒 =====
    {
        uint16_t *fb = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * 2, MALLOC_CAP_DMA);
        if (fb) {
            const uint16_t colors[] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
            for (int c = 0; c < 4; c++) {
                for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) fb[i] = colors[c];
                esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, fb);
                ESP_LOGI(TAG, "self-test color %d/4", c + 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            // 留给 LVGL 一个干净底色
            memset(fb, 0x00, LCD_WIDTH * LCD_HEIGHT * 2);
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, fb);
            free(fb);
        }
    }
    ESP_LOGI(TAG, "self-test done, handing over to LVGL");
    return panel_handle;
}

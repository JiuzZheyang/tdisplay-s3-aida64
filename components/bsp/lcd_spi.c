#include "lcd_spi.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "lcd_spi";

esp_lcd_panel_handle_t bsp_lcd_init(void)
{
    // GPIO initialize
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LCD_DC_IO) | (1ULL << LCD_RST_IO) | (1ULL << LCD_BL_IO),
    };
    gpio_config(&bk_gpio_config);

    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_SPI_SCK_IO,
        .mosi_io_num = LCD_SPI_MOSI_IO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Attach LCD to SPI
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_SPI_CS_IO,
        .dc_gpio_num = LCD_DC_IO,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,  // 40MHz
        .trans_queue_depth = 2,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .swap_color_bytes = 1,
        },
    };

    esp_lcd_panel_io_handle_t io_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &io_handle));

    // ST7789 panel config
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_IO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    esp_lcd_panel_handle_t panel_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // Reset
    gpio_set_level(LCD_RST_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST_IO, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Portrait: swap_xy=true, mirror x/y to correct orientation
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    // Fill black to clear any garbage
    uint16_t *frame = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * 2, MALLOC_CAP_DMA);
    if (frame) {
        memset(frame, 0x00, LCD_WIDTH * LCD_HEIGHT * 2);
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, frame);
        free(frame);
    }

    // Backlight on
    gpio_set_level(LCD_BL_IO, 1);

    ESP_LOGI(TAG, "T-Display-S3 LCD initialized: 170x320 SPI ST7789");
    return panel_handle;
}

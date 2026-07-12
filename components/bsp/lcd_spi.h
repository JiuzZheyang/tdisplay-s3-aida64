#ifndef BSP_LCD_SPI_H
#define BSP_LCD_SPI_H

#include "esp_lcd_types.h"
#include "driver/spi_master.h"

// T-Display-S3 pinout for SPI ST7789
#define LCD_SPI_HOST       SPI2_HOST
#define LCD_SPI_CS_IO      7
#define LCD_SPI_SCK_IO     6
#define LCD_SPI_MOSI_IO    5
#define LCD_DC_IO          4
#define LCD_RST_IO         8
#define LCD_BL_IO          38

#define LCD_WIDTH  170
#define LCD_HEIGHT 320

esp_lcd_panel_handle_t bsp_lcd_init(void);

#endif

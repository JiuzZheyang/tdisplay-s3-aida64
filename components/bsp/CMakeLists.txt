idf_component_register(SRCS "lcd_spi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_timer freertos)

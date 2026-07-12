idf_component_register(SRCS "main.c" "lv_port.c" "aida64.c" "ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_timer freertos esp_wifi nvs_flash esp_lvgl_port lvgl)

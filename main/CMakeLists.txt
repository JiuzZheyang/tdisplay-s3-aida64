idf_component_register(SRCS "main.c" "lv_port.c" "aida64.c" "ui.c" "boot_anim.c" "wifi_ui.c" "wifi_manager.c" "font_cn_18.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_lcd esp_timer freertos esp_wifi nvs_flash esp_netif esp_event lvgl lwip esp_lvgl_port esp_driver_ledc esp_driver_gpio esp_http_server nvs)

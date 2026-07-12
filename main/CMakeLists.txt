idf_component_register(SRCS "main.c" "lv_port.c" "aida64.c" "ui.c"
    INCLUDE_DIRS "." "../managed_components/lvgl__lvgl"
    REQUIRES bsp esp_lcd esp_timer freertos esp_wifi nvs_flash esp_netif esp_event lwip)

# Let LVGL find its own lv_conf.h via LV_CONF_PATH
target_compile_options(${COMPONENT_LIB} PRIVATE -DLV_CONF_PATH=\"lv_conf.h\")

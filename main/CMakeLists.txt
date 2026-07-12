idf_component_register(SRCS "main.c" "lv_port.c" "aida64.c" "ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_lcd esp_timer freertos esp_wifi nvs_flash esp_netif esp_event lwip)

# Let LVGL find lv_conf.h via LV_CONF_PATH
# "lv_conf.h" will be found in managed_components/lvgl__lvgl via REQUIRES include path
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_PATH="lv_conf.h")

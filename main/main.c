#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_ip4_addr.h"

#include "lcd_spi.h"
#include "lv_port.h"
#include "aida64.h"
#include "ui.h"

static const char *TAG = "main";

#define WIFI_SSID     "Ji"
#define WIFI_PASSWORD "wcg12345@"
#define AIDA64_SERVER "192.168.1.121"

static bool aida64_started = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (!aida64_started) {
            aida64_started = true;
            aida64_monitor_start(AIDA64_SERVER);
        }
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    memcpy(wifi_config.sta.ssid, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, connecting to %s...", WIFI_SSID);
}

void app_main(void)
{
    ESP_LOGI(TAG, "T-Display-S3 AIDA64 Monitor");
    ESP_LOGI(TAG, "Server: %s:7789", AIDA64_SERVER);

    ESP_ERROR_CHECK(nvs_flash_init());

    // Init LCD (SPI ST7789 170x320)
    esp_lcd_panel_handle_t panel_handle = bsp_lcd_init();

    // Init LVGL
    lv_port_init(panel_handle);

    // Init UI
    ui_init();

    // Init WiFi, AIDA64 starts when IP is obtained
    wifi_init();

    ESP_LOGI(TAG, "All init done!");
}

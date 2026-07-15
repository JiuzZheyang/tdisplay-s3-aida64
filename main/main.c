#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lcd_i80.h"
#include "lv_port.h"
#include "lvgl.h"
#include "wifi_manager.h"
#include "wifi_ui.h"
#include "boot_anim.h"
#include "backlight.h"
#include "button.h"
#include "ui.h"
#include "aida64.h"

static const char *TAG = "main";

static bool s_aida64_started = false;

/* 按键回调：切换亮度 */
static void on_key_pressed(void)
{
    backlight_cycle();
    ui_show_brightness_toast(backlight_get_percent());
}

/* ===================== WiFi 回调 ===================== */

static void on_sta_connected(const char *ip_str)
{
    ESP_LOGI(TAG, "STA connected: %s", ip_str);

    /* 隐藏配网 UI（如果正在显示） */
    wifi_ui_hide();

    /* 启动 AIDA64 监控 */
    if (!s_aida64_started) {
        s_aida64_started = true;
        aida64_monitor_start(NULL);
    }

    /* 通知开机动画 WiFi 已就绪，允许提前结束 */
    boot_anim_signal_ready();
}

static void on_sta_disconnected(void)
{
    ESP_LOGW(TAG, "STA disconnected");
}

static void on_ap_started(void)
{
    ESP_LOGI(TAG, "AP mode started");
}

/* ===================== main ===================== */

void app_main(void)
{
    ESP_LOGI(TAG, "T-Display-S3 AIDA64 Monitor");

    /* NVS + 事件处理程序 */
    wifi_manager_init();

    /* 注册 WiFi 回调（必须在 start 之前） */
    wifi_manager_on_sta_connected(on_sta_connected);
    wifi_manager_on_sta_disconnected(on_sta_disconnected);
    wifi_manager_on_ap_started(on_ap_started);

    /* BSP LCD */
    esp_lcd_panel_handle_t panel_handle = bsp_lcd_init();

    /* 背光 */
    backlight_init();

    /* LVGL */
    lv_port_init(panel_handle);

    /* 开机动画 + 主 UI */
    boot_anim_init();
    ui_init();

    /* 按键 */
    button_init();
    button_register_brightness_cb(ui_show_brightness_toast);

    /* 根据 NVS 配置启动 WiFi */
    wifi_manager_start();

    if (wifi_manager_get_mode() == WM_MODE_AP) {
        /* 未配置：显示配网 UI，WiFi 连上后 on_sta_connected 会切走 */
        wifi_ui_show_ap_mode(wifi_manager_get_ap_ssid());
    } else {
        /* 已配置：播放开机动画，WiFi 连上后自动切换到主 UI */
        boot_anim_play(ui_get_main_screen(), 2000, 6000);
    }

    ESP_LOGI(TAG, "All init done!");
}

#include "button.h"
#include "lcd_i80.h"        /* LCD_BOOT_IO 定义 */
#include "backlight.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "button";

#define BOOT_BTN_IO        LCD_BOOT_IO      /* GPIO 0 */
#define POLL_PERIOD_MS     10
#define DEBOUNCE_MS        20
#define PRESS_THRESHOLD_MS 50               /* 持续按下超过此值才确认（防抖毛刺） */

static button_brightness_cb_t s_brightness_cb = NULL;
static TaskHandle_t s_task_handle = NULL;

void button_register_brightness_cb(button_brightness_cb_t cb)
{
    s_brightness_cb = cb;
}

static void button_task(void *arg)
{
    (void)arg;
    bool last_pressed = false;
    int  press_stable_ms = 0;
    bool fired = false;                     /* 本次按下是否已触发（防连发） */

    while (1) {
        /* 0 = 按下（拉低），1 = 松开（上拉） */
        bool pressed = (gpio_get_level(BOOT_BTN_IO) == 0);

        if (pressed) {
            press_stable_ms += POLL_PERIOD_MS;
        } else {
            press_stable_ms = 0;
            fired = false;
        }

        /* 上升沿（松开后）不做事；下降沿（首次按下）经过去抖后触发一次 */
        if (pressed && !fired && press_stable_ms >= DEBOUNCE_MS) {
            /* 再确认一次，避免极短噪声 */
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            if (gpio_get_level(BOOT_BTN_IO) == 0) {
                fired = true;
                int percent = backlight_cycle();
                ESP_LOGI(TAG, "BOOT pressed, brightness=%d%%", percent);
                if (s_brightness_cb) {
                    s_brightness_cb(percent);
                }
            } else {
                press_stable_ms = 0;
            }
        }

        last_pressed = pressed;
        (void)last_pressed;
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}

void button_init(void)
{
    /* GPIO 0 在 ESP32-S3 上电阶段会被 ROM bootloader 用作 strap，
     * 进入 app 后可安全配为输入 + 上拉，作为普通按键使用。 */
    gpio_config_t io_cfg = {
        .pin_bit_mask  = (1ULL << BOOT_BTN_IO),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    BaseType_t ok = xTaskCreatePinnedToCore(button_task, "btn", 4096,
                                            NULL, 3, &s_task_handle, 0);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_FAIL);

    ESP_LOGI(TAG, "button init: BOOT on GPIO%u, poll=%ums debounce=%ums",
             BOOT_BTN_IO, POLL_PERIOD_MS, DEBOUNCE_MS);
}

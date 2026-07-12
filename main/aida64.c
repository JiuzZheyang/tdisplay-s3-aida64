#include "aida64.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "AIDA64";

extern void set_monitor_connect_state(bool connected);
extern void set_monitor_param(int cpu_rate, int cpu_temp, int mem_rate, int mem_use);

static TaskHandle_t aida64_task_handle = NULL;
static EventGroupHandle_t aida64_event_group = NULL;
static esp_http_client_handle_t http_handle = NULL;
static bool is_http_connect = false;

#define AIDA64_CONNECT_BIT BIT0

static bool aida64_monitor_parse(char *data, aida64_data_t *aida64_data)
{
    if (!data) return false;

    char *p = NULL;

    // CPU_Rate
    p = strstr(data, "CPU_Rate");
    if (p) {
        sscanf(p, "CPU_Rate %d", &aida64_data->cpu_rate);
    } else {
        return false;
    }

    // CPU_Temp
    p = strstr(data, "CPU_Temp");
    if (p) {
        sscanf(p, "CPU_Temp %d", &aida64_data->cpu_temp);
    } else {
        return false;
    }

    // MEM_Rate
    p = strstr(data, "MEM_Rate");
    if (p) {
        sscanf(p, "MEM_Rate %d", &aida64_data->mem_rate);
    } else {
        return false;
    }

    // MEM_Use
    p = strstr(data, "MEM_Use");
    if (p) {
        sscanf(p, "MEM_Use %d", &aida64_data->mem_use);
    } else {
        return false;
    }

    return true;
}

static esp_err_t aida64_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            is_http_connect = true;
            set_monitor_connect_state(true);
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER: key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA: len=%d", evt->data_len);
            // SSE data - NUL-terminate and parse
            if (evt->data_len > 0) {
                char *buf = (char *)evt->data;
                buf[evt->data_len] = '\0';
                aida64_data_t data;
                if (aida64_monitor_parse(buf, &data)) {
                    set_monitor_param(data.cpu_rate, data.cpu_temp, data.mem_rate, data.mem_use);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            is_http_connect = false;
            set_monitor_connect_state(false);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void aida64_monitor_task(void *param)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s/sse", (char *)param);

    while (1) {
        xEventGroupClearBits(aida64_event_group, AIDA64_CONNECT_BIT);

        esp_http_client_config_t config = {
            .url = url,
            .event_handler = aida64_http_event_handler,
        };

        http_handle = esp_http_client_init(&config);
        esp_http_client_set_header(http_handle, "Accept", "text/event-stream");

        is_http_connect = false;

        while (1) {
            esp_err_t err = esp_http_client_perform(http_handle);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "HTTP perform failed: %s, reconnecting...", esp_err_to_name(err));
                break;
            }
            esp_http_client_close(http_handle);
        }

        esp_http_client_cleanup(http_handle);
        http_handle = NULL;
        is_http_connect = false;
        set_monitor_connect_state(false);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void aida64_monitor_start(const char *ip)
{
    if (aida64_event_group == NULL) {
        aida64_event_group = xEventGroupCreate();
    }

    if (aida64_task_handle == NULL) {
        xTaskCreatePinnedToCore(aida64_monitor_task, "AIDA64", 8192, (void *)ip, 3, &aida64_task_handle, 1);
    }
}

void aida64_monitor_stop(void)
{
    if (http_handle) {
        esp_http_client_close(http_handle);
        esp_http_client_cleanup(http_handle);
        http_handle = NULL;
    }
    if (aida64_task_handle) {
        vTaskDelete(aida64_task_handle);
        aida64_task_handle = NULL;
    }
}

int aida64_monitor_isconnect(void)
{
    return is_http_connect;
}

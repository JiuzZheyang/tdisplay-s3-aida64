#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "web_pages.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_mgr";

/* NVS */
#define NVS_NS        "wifi"
#define NVS_KEY_SSID  "sta_ssid"
#define NVS_KEY_PASS  "sta_pass"
#define NVS_KEY_AIDA  "aida64_ip"
#define NVS_KEY_CONF  "configured"

/* AP 配置 */
#define AP_SSID_PREFIX  "AIDA64-"
#define AP_PASSWORD     "12345678"
#define AP_CHANNEL      1
#define AP_IP           "192.168.4.1"
#define AP_GW           "192.168.4.1"
#define AP_NETMASK      "255.255.255.0"
#define DEFAULT_AIDA64  "192.168.1.121"

/* STA 连接超时（ms），超时后自动切换 AP 配网 */
#define STA_CONNECT_TIMEOUT_MS  10000

/* 内部状态 */
static wm_mode_t s_mode = WM_MODE_AP;
static char s_sta_ip[32] = {0};
static char s_aida64_ip[32] = {0};
static char s_ap_ssid[48] = {0};

/* 回调 */
static wm_sta_cb_t      s_cb_sta_conn = NULL;
static wm_sta_disc_cb_t s_cb_sta_disc = NULL;
static wm_ap_cb_t       s_cb_ap_start = NULL;

/* HTTP 服务器 */
static httpd_handle_t s_httpd = NULL;

/* NVS */
static nvs_handle_t s_nvs = 0;

/* STA 连接超时检测 */
static TaskHandle_t s_sta_timeout_task = NULL;

/* ===================== NVS（内部封装） ===================== */

static bool wm_nvs_open(void)
{
    if (s_nvs) return true;
    if (nvs_open(NVS_NS, NVS_READWRITE, &s_nvs) != ESP_OK) { s_nvs = 0; return false; }
    return true;
}

static void wm_nvs_close(void)
{
    if (s_nvs) { nvs_close(s_nvs); s_nvs = 0; }
}

/* ===================== HTTP 服务器 ===================== */

static esp_err_t http_status_handler(httpd_req_t *req)
{
    char buf[512];
    wifi_ap_record_t info;
    int rssi = -100;
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) rssi = info.rssi;

    char sta_ssid_buf[32] = "--";
    if (s_mode == WM_MODE_STA) {
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK)
            snprintf(sta_ssid_buf, sizeof(sta_ssid_buf), "%s", (char*)cfg.sta.ssid);
    }

    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"mode\":\"%s\","
        "\"ap_ssid\":\"%s\","
        "\"sta_ssid\":\"%s\","
        "\"sta_ip\":\"%s\","
        "\"aida64_ip\":\"%s\","
        "\"rssi\":%d"
        "}",
        s_mode == WM_MODE_STA ? "STA" : "AP",
        s_ap_ssid,
        s_mode == WM_MODE_STA ? sta_ssid_buf : "--",
        s_sta_ip[0] ? s_sta_ip : "--",
        s_aida64_ip[0] ? s_aida64_ip : DEFAULT_AIDA64,
        rssi
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

static esp_err_t http_config_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "");
    buf[ret] = '\0';

    char ssid[64] = {0}, pass[128] = {0}, aida[32] = {0};

    /* 简单 JSON 解析 */
    for (char *p = buf; *p; ) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        char key[32] = {0};
        for (int i = 0; i < 31 && p[i+1] && p[i+1] != '"'; i++) key[i] = p[i+1];
        while (*p && *p != ':') p++;
        if (!*p) break;
        p++;
        while (*p && (*p == ' ' || *p == '"')) p++;
        char val[128] = {0};
        for (int i = 0; i < 127 && p[i] && p[i] != '"' && p[i] != ',' && p[i] != '}'; i++) val[i] = p[i];

        if (!strcmp(key, "sta_ssid"))  snprintf(ssid, sizeof(ssid),  "%s", val);
        if (!strcmp(key, "sta_pass"))  snprintf(pass, sizeof(pass),  "%s", val);
        if (!strcmp(key, "aida64_ip")) snprintf(aida,  sizeof(aida),   "%s", val);
        while (*p && *p != ',') p++;
        if (*p) p++;
    }

    if (!ssid[0]) {
        snprintf(buf, sizeof(buf), "{\"success\":false,\"error\":\"ssid required\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, buf, strlen(buf));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "config: ssid=%s aida=%s", ssid, aida);
    if (wifi_manager_save_and_reboot(ssid, pass, aida)) {
        snprintf(buf, sizeof(buf), "{\"success\":true}");
    } else {
        snprintf(buf, sizeof(buf), "{\"success\":false,\"error\":\"save failed\"}");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t http_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

static void start_http_server(void)
{
    if (s_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 4096;

    httpd_uri_t h[] = {
        { .uri = "/",          .method = HTTP_GET,  .handler = http_root_handler,  .user_ctx = NULL },
        { .uri = "/status",    .method = HTTP_GET,  .handler = http_status_handler, .user_ctx = NULL },
        { .uri = "/api/config",.method = HTTP_POST, .handler = http_config_handler, .user_ctx = NULL },
    };

    if (httpd_start(&s_httpd, &cfg) == ESP_OK) {
        for (int i = 0; i < 3; i++) httpd_register_uri_handler(s_httpd, &h[i]);
        ESP_LOGI(TAG, "HTTP server started");
    } else ESP_LOGE(TAG, "HTTP server start failed");
}

static void stop_http_server(void)
{
    if (s_httpd) { httpd_stop(s_httpd); s_httpd = NULL; }
}

/* ===================== STA 超时检测 ===================== */

static void sta_timeout_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));
    if (s_mode != WM_MODE_STA || s_sta_ip[0] == '\0') {
        ESP_LOGW(TAG, "STA connect timeout (%dms), switching to AP mode", STA_CONNECT_TIMEOUT_MS);
        wifi_manager_start_ap_mode();
    }
    s_sta_timeout_task = NULL;
    vTaskDelete(NULL);
}

/* ===================== 事件处理 ===================== */

static void wifi_event_handler(void *arg, esp_event_base_t ev_base,
                               int32_t ev_id, void *ev_data)
{
    (void)arg; (void)ev_base;

    if (ev_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (ev_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected");
        s_sta_ip[0] = '\0';
        if (s_cb_sta_disc) s_cb_sta_disc();
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }
    else if (ev_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)ev_data;
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "STA got IP: %s", s_sta_ip);
        s_mode = WM_MODE_STA;
        if (s_sta_timeout_task) {
            vTaskDelete(s_sta_timeout_task);
            s_sta_timeout_task = NULL;
        }
        if (s_cb_sta_conn) s_cb_sta_conn(s_sta_ip);
    }
    else if (ev_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "AP: client connected");
    }
    else if (ev_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "AP: client disconnected");
    }
}

/* ===================== AP 启动 ===================== */

static void wifi_ap_start(void)
{
    if (s_mode == WM_MODE_AP) return;

    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    esp_netif_ip_info_t ip_info = {
        .ip      = {.addr = esp_ip4addr_aton(AP_IP)},
        .gw      = {.addr = esp_ip4addr_aton(AP_GW)},
        .netmask = {.addr = esp_ip4addr_aton(AP_NETMASK)},
    };
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len = strlen(s_ap_ssid),
            .channel  = AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .password = AP_PASSWORD,
            .max_connection = 4,
            .pmf_cfg = { .required = false },
        },
    };

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_mode = WM_MODE_AP;
    ESP_LOGI(TAG, "AP started: SSID=%s", s_ap_ssid);
    if (s_cb_ap_start) s_cb_ap_start();
}

/* ===================== STA 启动 ===================== */

static void wifi_sta_start(const char *ssid, const char *pass)
{
    if (s_mode == WM_MODE_STA) return;

    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_cfg = {
        .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK, .pmf_cfg = { .required = false } },
    };
    snprintf((char*)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), "%s", ssid);
    if (pass && pass[0])
        snprintf((char*)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), "%s", pass);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_mode = WM_MODE_STA;
    s_sta_ip[0] = '\0';
    ESP_LOGI(TAG, "STA starting: %s", ssid);

    if (s_sta_timeout_task == NULL) {
        xTaskCreatePinnedToCore(sta_timeout_task, "sta_timeout", 2048, NULL, 3, &s_sta_timeout_task, 0);
    }
}

/* ===================== 公共 API ===================== */

void wifi_manager_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_LOGI(TAG, "init done");
}

void wifi_manager_start(void)
{
    wm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    wm_nvs_open();
    uint8_t configured = 0;
    nvs_get_u8(s_nvs, NVS_KEY_CONF, &configured);

    if (configured) {
        size_t sz = sizeof(cfg.sta_ssid);
        nvs_get_str(s_nvs, NVS_KEY_SSID, cfg.sta_ssid, &sz);
        sz = sizeof(cfg.sta_pass);
        nvs_get_str(s_nvs, NVS_KEY_PASS, cfg.sta_pass, &sz);
        sz = sizeof(cfg.aida64_ip);
        nvs_get_str(s_nvs, NVS_KEY_AIDA, cfg.aida64_ip, &sz);
        wm_nvs_close();

        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);

        if (cfg.aida64_ip[0]) snprintf(s_aida64_ip, sizeof(s_aida64_ip), "%s", cfg.aida64_ip);
        else snprintf(s_aida64_ip, sizeof(s_aida64_ip), "%s", DEFAULT_AIDA64);

        if (cfg.sta_ssid[0]) {
            wifi_sta_start(cfg.sta_ssid, cfg.sta_pass);
            return;
        }
    }
    wm_nvs_close();

    wifi_manager_start_ap_mode();
}

void wifi_manager_start_ap_mode(void)
{
    stop_http_server();
    wifi_ap_start();
    start_http_server();
}

bool wifi_manager_save_and_reboot(const char *sta_ssid, const char *sta_pass,
                                  const char *aida64_ip)
{
    wm_nvs_open();
    esp_err_t err = ESP_OK;
    err |= nvs_set_str(s_nvs, NVS_KEY_SSID, sta_ssid);
    err |= nvs_set_str(s_nvs, NVS_KEY_PASS, sta_pass ? sta_pass : "");
    err |= nvs_set_str(s_nvs, NVS_KEY_AIDA, aida64_ip ? aida64_ip : "");
    err |= nvs_set_u8(s_nvs, NVS_KEY_CONF, 1);
    err |= nvs_commit(s_nvs);
    wm_nvs_close();

    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS save failed"); return false; }
    if (aida64_ip && aida64_ip[0])
        snprintf(s_aida64_ip, sizeof(s_aida64_ip), "%s", aida64_ip);

    ESP_LOGI(TAG, "Config saved, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return true;
}

bool wifi_manager_get_config(wm_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    wm_nvs_open();
    size_t sz = sizeof(cfg->sta_ssid);
    nvs_get_str(s_nvs, NVS_KEY_SSID, cfg->sta_ssid, &sz);
    sz = sizeof(cfg->sta_pass);
    nvs_get_str(s_nvs, NVS_KEY_PASS, cfg->sta_pass, &sz);
    sz = sizeof(cfg->aida64_ip);
    nvs_get_str(s_nvs, NVS_KEY_AIDA, cfg->aida64_ip, &sz);
    wm_nvs_close();
    return cfg->sta_ssid[0] != '\0';
}

const char *wifi_manager_get_aida64_ip(void)
{
    return s_aida64_ip[0] ? s_aida64_ip : DEFAULT_AIDA64;
}

const char *wifi_manager_get_sta_ip(void)
{
    return s_sta_ip[0] ? s_sta_ip : "";
}

wm_mode_t wifi_manager_get_mode(void) { return s_mode; }

int wifi_manager_get_rssi(void)
{
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) return info.rssi;
    return -100;
}

const char *wifi_manager_get_ap_ssid(void)
{
    return s_ap_ssid[0] ? s_ap_ssid : AP_SSID_PREFIX "
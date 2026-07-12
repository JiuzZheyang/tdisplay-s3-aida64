#include "aida64.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "sys/time.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

static const char *TAG = "AIDA64";

extern void set_monitor_connect_state(bool connected);
extern void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                             int gpu_rate, int gpu_temp, int gpu_power);

static TaskHandle_t aida64_task_handle = NULL;
static bool aida64_running = false;
static int  aida64_sock = -1;          // 仅供 aida64_monitor_stop() 强制唤醒 read()
static bool is_connected = false;

#define AIDA64_PORT_NUM 7789

/* ===== AIDA64 RemoteSensor 字段映射 =====
 * AIDA64 把每个 Shared Value 槽位 (SIV<n>) 暴露到 LCD 页面；
 * 你在 AIDA64 → 硬件监视 → LCD → Shared Values 里把每槽绑到哪个传感器，
 * 就决定 SIV4/5/6/7/8/9 哪个是 CPU 占用、哪个是温度、哪个是功率。
 * PC 端配置：SIV4=CPU使用率 / SIV5=GPU使用率 / SIV6=CPU温度 /
 *            SIV7=GPU温度 / SIV8=CPU功率 / SIV9=GPU功率 / SIV10=运行时长
 * （SIV10 解析但不显示，UI 只放 6 项。） */
#define K_CPU_RATE  "SIV4"
#define K_GPU_RATE  "SIV5"
#define K_CPU_TEMP  "SIV6"
#define K_GPU_TEMP  "SIV7"
#define K_CPU_POWER "SIV8"
#define K_GPU_POWER "SIV9"

/* ===== 健壮解析 =====
 * 跳过 key 后的非数字字符，兼容以下任意格式：
 *   SIV4 45        SIV4:45        SIV4=45
 *   SIV4|45        SIV4|45|       SIV4|45{|}
 * 注：用 strstr 匹配会受子串冲突影响（SIV4 会匹配到 SIV40/SIV41...），
 *     因此要求 key 后紧跟非字母数字（'|'、':'、' '、'{' 等边界符）。
 */
static int parse_int_after_key(const char *data, const char *key, int *out)
{
    const char *p = data;
    size_t klen = strlen(key);
    while ((p = strstr(p, key)) != NULL) {
        /* 边界检查：避免 SIV4 误匹配 SIV40/SIV41… */
        char next = *(p + klen);
        if (next == '|' || next == ':' || next == '=' || next == ' ' ||
            next == '\t' || next == '\r' || next == '\n' || next == '{' || next == '\0') {
            break;
        }
        p += klen;
    }
    if (!p) return 0;
    p += klen;
    while (*p && (*p < '0' || *p > '9') && *p != '-') p++;
    if (*p == '\0') return 0;
    *out = atoi(p);
    return 1;
}

/* SSE 行缓冲：跨多次 read() 累积，遇到事件边界或主字段集齐才解析发射 */
#define SSE_BUF_LEN 512
static char sse_buf[SSE_BUF_LEN];
static size_t sse_len = 0;

static void sse_reset(void) { sse_len = 0; }

static void sse_emit(void)
{
    /* 主字段：必须看到 CPU 使用率才发射（其它键缺失则补 0） */
    if (!strstr(sse_buf, K_CPU_RATE)) return;
    aida64_data_t d;
    memset(&d, 0, sizeof(d));
    parse_int_after_key(sse_buf, K_CPU_RATE,  &d.cpu_rate);
    parse_int_after_key(sse_buf, K_GPU_RATE,  &d.gpu_rate);
    parse_int_after_key(sse_buf, K_CPU_TEMP,  &d.cpu_temp);
    parse_int_after_key(sse_buf, K_GPU_TEMP,  &d.gpu_temp);
    parse_int_after_key(sse_buf, K_CPU_POWER, &d.cpu_power);
    parse_int_after_key(sse_buf, K_GPU_POWER, &d.gpu_power);
    set_monitor_param(d.cpu_rate, d.cpu_temp, d.cpu_power,
                      d.gpu_rate, d.gpu_temp, d.gpu_power);
    ESP_LOGI(TAG, "emit cpu=%d/%d/%d gpu=%d/%d/%d",
             d.cpu_rate, d.cpu_temp, d.cpu_power,
             d.gpu_rate, d.gpu_temp, d.gpu_power);
}

static void sse_feed(const char *chunk, int len)
{
    for (int i = 0; i < len && sse_len < SSE_BUF_LEN - 1; i++) {
        sse_buf[sse_len++] = chunk[i];
    }
    sse_buf[sse_len] = '\0';

    bool has_cpu  = strstr(sse_buf, K_CPU_RATE) != NULL;
    bool has_temp = strstr(sse_buf, K_CPU_TEMP) != NULL;
    bool has_gpu  = strstr(sse_buf, K_GPU_RATE) != NULL;

    if (strstr(sse_buf, "\n\n") || strstr(sse_buf, "\r\n\r\n")) {
        /* 事件边界（SSE 以空行分隔） */
        sse_emit();
        sse_reset();
    } else if (has_cpu && (has_temp || has_gpu)) {
        /* 兼容「一次推送全部字段、无空行」的格式 */
        sse_emit();
        sse_reset();
    } else if (sse_len >= SSE_BUF_LEN - 2) {
        /* 溢出保护（也会清掉 HTTP 响应头之类的前缀） */
        sse_emit();
        sse_reset();
    }
}

/* ===== 纯 BSD socket：绕开 esp_http_client / esp-tls =====
 * AIDA64 自带的 mini HTTP 服务器会主动 RST 经过 esp-tls 封装的连接，
 * 而 curl / 浏览器那种"裸" TCP + 普通 GET 则被正常接受。
 * 这里直接用 lwIP 的 BSD socket API，不经过 esp-tls。 */
static int aida64_socket_connect(const char *ip)
{
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_len    = sizeof(dest);
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(AIDA64_PORT_NUM);
    if (inet_pton(AF_INET, ip, &dest.sin_addr) != 1) {
        ESP_LOGE(TAG, "invalid IP: %s", ip);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        return -1;
    }

    /* 接收超时：让 read() 周期性返回，以便检查 aida64_running 实现优雅退出 */
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    /* 启用 TCP keepalive，让对端崩溃/断网时能被探活 */
    int keepalive = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

    ESP_LOGI(TAG, "connecting %s:%d ...", ip, AIDA64_PORT_NUM);
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "connect() failed: %d (%s)", errno, strerror(errno));
        close(sock);
        return -1;
    }
    ESP_LOGI(TAG, "connected to AIDA64");
    return sock;
}

static bool aida64_socket_send_get(int sock, const char *ip)
{
    char req[256];
    int len = snprintf(req, sizeof(req),
        "GET /sse HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: ESP32S3-AIDA64\r\n"
        "Accept: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        ip, AIDA64_PORT_NUM);

    int sent = 0;
    while (sent < len) {
        int n = write(sock, req + sent, len - sent);
        if (n < 0) {
            ESP_LOGE(TAG, "write() failed: %d (%s)", errno, strerror(errno));
            return false;
        }
        sent += n;
    }
    return true;
}

static void aida64_monitor_task(void *param)
{
    const char *ip = (const char *)param;
    uint8_t rxbuf[256];

    while (aida64_running) {
        int sock = aida64_socket_connect(ip);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        aida64_sock = sock;

        if (!aida64_socket_send_get(sock, ip)) {
            close(sock);
            aida64_sock = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        is_connected = true;
        set_monitor_connect_state(true);
        sse_reset();

        bool alive = true;
        while (aida64_running && alive) {
            int n = read(sock, rxbuf, sizeof(rxbuf));
            if (n > 0) {
                ESP_LOGD(TAG, "recv (%dB)", n);
                sse_feed((const char *)rxbuf, n);
            } else if (n == 0) {
                ESP_LOGW(TAG, "server closed connection (EOF), reconnecting...");
                alive = false;
            } else {
                if (errno == EINTR || errno == EAGAIN || errno == ETIMEDOUT) {
                    /* 2s 接收超时或被信号打断：仅重新检查 running 标志 */
                    continue;
                }
                ESP_LOGE(TAG, "read() error: %d (%s), reconnecting...",
                         errno, strerror(errno));
                alive = false;
            }
        }

        if (aida64_sock == sock) aida64_sock = -1;
        close(sock);
        is_connected = false;
        set_monitor_connect_state(false);

        if (aida64_running) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    aida64_task_handle = NULL;
    vTaskDelete(NULL);
}

void aida64_monitor_start(const char *ip)
{
    if (aida64_running) return;
    aida64_running = true;
    xTaskCreatePinnedToCore(aida64_monitor_task, "AIDA64", 8192,
                            (void *)ip, 3, &aida64_task_handle, 1);
}

void aida64_monitor_stop(void)
{
    aida64_running = false;
    /* 强制唤醒阻塞在 read() 上的任务，使其尽快退出循环 */
    if (aida64_sock >= 0) {
        close(aida64_sock);
        aida64_sock = -1;
    }
    if (aida64_task_handle) {
        vTaskDelete(aida64_task_handle);
        aida64_task_handle = NULL;
    }
    is_connected = false;
    set_monitor_connect_state(false);
}

int aida64_monitor_isconnect(void)
{
    return is_connected;
}

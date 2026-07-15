#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * WiFi 管理器 — 负责存储、配网、连接
 *
 * 存储结构（NVS）：
 *   namespace "wifi" {
 *     "sta_ssid"  : string   — 要连接的 WiFi SSID
 *     "sta_pass"  : string   — WiFi 密码（可为空）
 *     "aida64_ip" : string   — AIDA64 服务器 IP
 *     "configured" : u8       — 0=未配置(AP模式) / 1=已配置(尝试STA)
 *   }
 *
 * 使用流程：
 *   1. wifi_manager_init()      — 初始化 NVS + 注册事件处理程序
 *   2. 注册回调（on_sta_connected 等）
 *   3. wifi_manager_start()     — 根据 NVS 配置启动 STA 或 AP
 *
 * 配网流程：
 *   configured==0 → 启动 AP(192.168.4.1) + HTTP 服务器
 *   configured==1 → 尝试 STA 连接
 *      · 连上 → on_sta_connected(IP)
 *      · 失败（10s）→ 自动切换 AP 配网模式
 */

/* 配置结构 */
typedef struct {
    char sta_ssid[32];
    char sta_pass[64];
    char aida64_ip[16];
} wm_config_t;

/* 回调类型 */
typedef void (*wm_sta_cb_t)(const char *ip_str);
typedef void (*wm_sta_disc_cb_t)(void);
typedef void (*wm_ap_cb_t)(void);

/* 模式 */
typedef enum { WM_MODE_AP, WM_MODE_STA } wm_mode_t;

/* ========== 核心 API ========== */

/* 初始化（仅设置事件处理，不启动 WiFi） */
void wifi_manager_init(void);

/* 开始 WiFi（启动 STA 或 AP）。必须在回调注册完之后调用。 */
void wifi_manager_start(void);

/* ========== 配网 ========== */

/* 启动 AP 配网模式（由 main.c 在 STA 失败超时时调用） */
void wifi_manager_start_ap_mode(void);

/* 保存配置并重启（由 web server 调用） */
bool wifi_manager_save_and_reboot(const char *sta_ssid, const char *sta_pass,
                                  const char *aida64_ip);

/* 清除所有配置，恢复出厂 */
void wifi_manager_erase_all(void);

/* ========== 查询 ========== */

/* 读取当前配置（返回 false 表示未配置） */
bool wifi_manager_get_config(wm_config_t *cfg);

/* 读取 AIDA64 服务器 IP（供 aida64.c 使用） */
const char *wifi_manager_get_aida64_ip(void);

/* 读取 STA IP（供 web server 使用） */
const char *wifi_manager_get_sta_ip(void);

/* 当前模式 */
wm_mode_t wifi_manager_get_mode(void);

/* 获取 RSSI（STA 模式有效，失败返回 -100） */
int wifi_manager_get_rssi(void);

/* 获取设备 AP SSID */
const char *wifi_manager_get_ap_ssid(void);

/* ========== 回调注册 ========== */

void wifi_manager_on_sta_connected(wm_sta_cb_t cb);
void wifi_manager_on_sta_disconnected(wm_sta_disc_cb_t cb);
void wifi_manager_on_ap_started(wm_ap_cb_t cb);

#endif /* WIFI_MANAGER_H */

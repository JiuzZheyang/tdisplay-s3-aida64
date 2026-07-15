#ifndef AIDA64_H
#define AIDA64_H

#include <stdint.h>

/* SSE 推送的数据结构：6 项（CPU/GPU 各 占用率/温度/功率） */
typedef struct {
    int cpu_rate;    // CPU 占用率 0-100
    int cpu_temp;   // CPU 温度 °C
    int cpu_power;  // CPU 功率 W
    int gpu_rate;    // GPU 占用率 0-100
    int gpu_temp;   // GPU 温度 °C
    int gpu_power;  // GPU 功率 W
} aida64_data_t;

// 启动 AIDA64 监控任务，连接指定 IP 的 AIDA64 RemoteSensor SSE
void aida64_monitor_start(const char *ip);

// 停止监控
void aida64_monitor_stop(void);

// 查询连接状态
int aida64_monitor_isconnect(void);

// 设置 UI 回调（实现于 ui.c）
void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power);

#endif

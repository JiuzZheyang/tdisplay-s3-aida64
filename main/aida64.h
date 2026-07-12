#ifndef AIDA64_H
#define AIDA64_H

#include <stdint.h>

#define TAG "AIDA64"

typedef struct {
    int cpu_rate;    // CPU占用率 0-100
    int cpu_temp;   // CPU温度
    int mem_rate;   // 内存占用率 0-100
    int mem_use;    // 已用内存 MB
} aida64_data_t;

// 启动AIDA64监控任务，连接指定IP的AIDA64 RemoteSensor SSE
void aida64_monitor_start(const char *ip);

// 停止监控
void aida64_monitor_stop(void);

// 查询连接状态
int aida64_monitor_isconnect(void);

// 设置UI回调
void set_monitor_param(int cpu_rate, int cpu_temp, int mem_rate, int mem_use);

#endif

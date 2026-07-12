#ifndef UI_H
#define UI_H

void ui_init(void);
void set_monitor_connect_state(bool connected);
void set_monitor_param(int cpu_rate, int cpu_temp, int mem_rate, int mem_use);

#endif

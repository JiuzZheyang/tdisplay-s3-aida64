#ifndef UI_H
#define UI_H

#include <stdbool.h>

void ui_init(void);
void set_monitor_connect_state(bool connected);
void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power);

#endif

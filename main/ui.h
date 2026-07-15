#pragma once

#include <stdbool.h>
#include "lvgl.h"

void ui_init(void);
lv_obj_t *ui_get_main_screen(void);
void ui_show_brightness_toast(int percent);

void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power);
void set_monitor_connect_state(bool connected);

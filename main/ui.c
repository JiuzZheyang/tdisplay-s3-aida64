#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui";

static lv_obj_t *cpu_arc;
static lv_obj_t *cpu_label;
static lv_obj_t *temp_label;
static lv_obj_t *mem_bar;
static lv_obj_t *mem_use_label;
static lv_obj_t *status_dot;
static lv_obj_t *status_label;
static lv_timer_t *blink_timer = NULL;
static bool blink_active = false;
static bool blink_on = false;

static void blink_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!blink_active) return;
    blink_on = !blink_on;
    lv_obj_set_style_bg_color(status_dot, blink_on ? lv_color_hex(0x00FF00) : lv_color_hex(0x003300), 0);
}

void set_monitor_connect_state(bool connected)
{
    if (connected) {
        blink_active = false;
        if (blink_timer) {
            lv_timer_pause(blink_timer);
        }
        lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x00FF00), 0);
        lv_label_set_text(status_label, "ON");
    } else {
        blink_active = true;
        blink_on = true;
        if (!blink_timer) {
            blink_timer = lv_timer_create(blink_cb, 500, NULL);
        } else {
            lv_timer_reset(blink_timer);
            lv_timer_resume(blink_timer);
        }
        lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x330000), 0);
        lv_label_set_text(status_label, "...");
    }
}

void set_monitor_param(int cpu_rate, int cpu_temp, int mem_rate, int mem_use)
{
    static char buf[24];

    // CPU Arc + big center label
    lv_arc_set_value(cpu_arc, cpu_rate);
    snprintf(buf, sizeof(buf), "%d%%", cpu_rate);
    lv_label_set_text(cpu_label, buf);

    // CPU Temp
    snprintf(buf, sizeof(buf), "%d°C", cpu_temp);
    lv_label_set_text(temp_label, buf);

    // Memory bar
    lv_bar_set_value(mem_bar, mem_rate, LV_ANIM_OFF);

    // Memory use in MB
    snprintf(buf, sizeof(buf), "%d MB", mem_use);
    lv_label_set_text(mem_use_label, buf);
}

void ui_init(void)
{
    // Dark background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0A0A0F), 0);

    // ===== Title Bar =====
    lv_obj_t *title_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(title_bar, 170, 28);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x141420), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_scrollbar_mode(title_bar, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, "AIDA64 Monitor");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFA500), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 8, 0);

    // Status dot
    status_dot = lv_obj_create(title_bar);
    lv_obj_set_size(status_dot, 10, 10);
    lv_obj_set_pos(status_dot, 135, 9);
    lv_obj_set_style_radius(status_dot, 5, 0);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x330000), 0);
    lv_obj_set_style_border_width(status_dot, 0, 0);

    status_label = lv_label_create(title_bar);
    lv_label_set_text(status_label, "...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 122, 0);

    // ===== CPU Rate Arc =====
    cpu_arc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(cpu_arc, 140, 140);
    lv_obj_set_pos(cpu_arc, 15, 36);
    lv_arc_set_range(cpu_arc, 0, 100);
    lv_arc_set_value(cpu_arc, 0);
    lv_arc_set_rotation(cpu_arc, 135);
    lv_arc_set_bg_angles(cpu_arc, 0, 270);
    lv_obj_set_style_arc_width(cpu_arc, 10, 0);
    lv_obj_set_style_arc_color(cpu_arc, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
    lv_obj_set_style_arc_color(cpu_arc, lv_color_hex(0x00BFFF), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(cpu_arc, 0, 0);
    lv_obj_set_style_bg_color(cpu_arc, lv_color_hex(0x0A0A0F), 0);

    // CPU rate big label
    cpu_label = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_label, "0%");
    lv_obj_set_style_text_color(cpu_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(cpu_label, &lv_font_montserrat_32, 0);
    lv_obj_align(cpu_label, LV_ALIGN_CENTER, 0, 15);

    lv_obj_t *cpu_sub = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_sub, "CPU");
    lv_obj_set_style_text_color(cpu_sub, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(cpu_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(cpu_sub, LV_ALIGN_CENTER, 0, 45);

    // ===== Temperature =====
    lv_obj_t *temp_title = lv_label_create(lv_scr_act());
    lv_label_set_text(temp_title, "TEMP");
    lv_obj_set_style_text_color(temp_title, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(temp_title, &lv_font_montserrat_10, 0);
    lv_obj_align(temp_title, LV_ALIGN_LEFT_MID, 12, 190);

    temp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(temp_label, "--°C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFF6B6B), 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_22, 0);
    lv_obj_align(temp_label, LV_ALIGN_LEFT_MID, 10, 208);

    // ===== Memory =====
    lv_obj_t *mem_title = lv_label_create(lv_scr_act());
    lv_label_set_text(mem_title, "MEM");
    lv_obj_set_style_text_color(mem_title, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(mem_title, &lv_font_montserrat_10, 0);
    lv_obj_align(mem_title, LV_ALIGN_RIGHT_MID, -10, 190);

    mem_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(mem_bar, 70, 10);
    lv_obj_set_pos(mem_bar, 90, 184);
    lv_bar_set_range(mem_bar, 0, 100);
    lv_bar_set_value(mem_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0x9B59B6), LV_PART_INDICATOR);
    lv_obj_set_style_radius(mem_bar, 5, 0);
    lv_obj_set_style_border_width(mem_bar, 0, 0);

    mem_use_label = lv_label_create(lv_scr_act());
    lv_label_set_text(mem_use_label, "-- MB");
    lv_obj_set_style_text_color(mem_use_label, lv_color_hex(0x9B59B6), 0);
    lv_obj_set_style_text_font(mem_use_label, &lv_font_montserrat_12, 0);
    lv_obj_align(mem_use_label, LV_ALIGN_RIGHT_MID, -8, 208);

    // ===== Divider =====
    lv_obj_t *line = lv_line_create(lv_scr_act());
    static lv_point_t pts[] = {{10, 235}, {160, 235}};
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0x282840), 0);
    lv_obj_set_style_line_width(line, 1, 0);

    // ===== Server IP =====
    lv_obj_t *server_label = lv_label_create(lv_scr_act());
    lv_label_set_text(server_label, "192.168.1.121:7789");
    lv_obj_set_style_text_color(server_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(server_label, &lv_font_montserrat_10, 0);
    lv_obj_align(server_label, LV_ALIGN_BOTTOM_MID, 0, -6);

    ESP_LOGI(TAG, "UI initialized: 170x320 portrait");
}

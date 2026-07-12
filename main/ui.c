#include "ui.h"
#include "lv_port.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui";

/* ===== 色彩（深色主题）===== */
#define C_BG       0x0F1117   /* 深色背景 */
#define C_CPU      0x3B82F6   /* CPU 蓝 */
#define C_GPU      0xF59E0B   /* GPU 橙 */
#define C_TEXT     0xE8EAF0   /* 主文字白 */
#define C_TEXT2    0x6B7280   /* 副文字灰 */
#define C_DIVIDER  0x2A2D35   /* 分隔线 */
#define C_OK       0x22C55E   /* 连接绿 */
#define C_WARN     0xEF4444   /* 断开红 */

/* ===== 对象句柄 ===== */
static lv_obj_t *status_label;
static lv_obj_t *footer_label;

static lv_obj_t *cpu_pct_lbl;   /* CPU 百分比大字 */
static lv_obj_t *gpu_pct_lbl;   /* GPU 百分比大字 */
static lv_obj_t *cpu_temp_lbl;  /* CPU 温度 */
static lv_obj_t *gpu_temp_lbl;  /* GPU 温度 */
static lv_obj_t *cpu_power_lbl; /* CPU 功率 */
static lv_obj_t *gpu_power_lbl; /* GPU 功率 */

/* ===== 辅助：右对齐 label ===== */
static void right_align(lv_obj_t *obj, int x_right)
{
    lv_obj_update_layout(obj);
    int w = lv_obj_get_self_width(obj);
    lv_obj_set_x(obj, x_right - w);
}

/* ===== 辅助：在 (x, y) 处创建一行：标签 + 数值 ===== */
static void make_row(int x, int y, const char *lab, const char *val,
                     lv_obj_t **val_out, uint32_t lab_col, uint32_t val_col,
                     const lv_font_t *val_font)
{
    lv_obj_t *l = lv_label_create(lv_scr_act());
    lv_label_set_text(l, lab);
    lv_obj_set_style_text_color(l, lv_color_hex(lab_col), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(l, x, y);

    lv_obj_t *v = lv_label_create(lv_scr_act());
    lv_label_set_text(v, val);
    lv_obj_set_style_text_color(v, lv_color_hex(val_col), 0);
    lv_obj_set_style_text_font(v, val_font, 0);
    lv_obj_set_pos(v, x + 44, y);
    *val_out = v;
}

/* ===== ui_init ===== */
void ui_init(void)
{
    /* ui_init 在 app_main（核心0）执行，LVGL 任务（核心1）已运行。
     * 所有 LVGL 对象操作必须持锁。 */
    lvgl_port_lock(0);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(C_BG), 0);

    /* ============================================================
     * 整屏 170×320 竖屏布局：
     *   y=0..24   : 顶栏（PC Monitor · 状态）
     *   y=30      : 列标题（CPU / GPU）
     *   y=62      : 分隔线
     *   y=68      : CPU 占用率  ---大字 48px---  y=68
     *   y=130     : GPU 占用率  ---大字 48px---  y=130
     *   y=192     : CPU 温度
     *   y=218     : GPU 温度
     *   y=244     : CPU 功率
     *   y=270     : GPU 功率
     *   y=305     : 底部状态
     * ============================================================ */

    /* ----- 顶部状态栏 ----- */
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "PC Monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(title, 6, 4);

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "连接中...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(status_label, 100, 6);

    /* 顶部分隔线 */
    lv_obj_t *l0 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts0[] = {{0, 26}, {170, 26}};
    lv_line_set_points(l0, pts0, 2);
    lv_obj_set_style_line_color(l0, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(l0, 1, 0);

    /* ----- CPU / GPU 列标题 ----- */
    lv_obj_t *cpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_hdr, "CPU");
    lv_obj_set_style_text_color(cpu_hdr, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_hdr, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_hdr, 6, 32);

    lv_obj_t *gpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_hdr, "GPU");
    lv_obj_set_style_text_color(gpu_hdr, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_hdr, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_hdr, 100, 32);

    /* 列标题分隔线 */
    lv_obj_t *l1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts1[] = {{0, 58}, {170, 58}};
    lv_line_set_points(l1, pts1, 2);
    lv_obj_set_style_line_color(l1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(l1, 1, 0);

    /* ----- 中央垂直分隔线 ----- */
    lv_obj_t *lv = lv_line_create(lv_scr_act());
    static lv_point_precise_t ptsv[] = {{85, 62}, {85, 296}};
    lv_line_set_points(lv, ptsv, 2);
    lv_obj_set_style_line_color(lv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lv, 1, 0);

    /* ----- CPU 列（x=0..84，左对齐） ----- */
    /* 占用率 */
    lv_obj_t *cpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_l, "占用率");
    lv_obj_set_style_text_color(cpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(cpu_pct_l, 6, 66);

    cpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(cpu_pct_lbl, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_pct_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(cpu_pct_lbl, 6, 84);

    /* 温度 */
    lv_obj_t *ct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(ct_l, "温度");
    lv_obj_set_style_text_color(ct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(ct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ct_l, 6, 138);

    cpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(cpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_temp_lbl, 6, 154);

    /* 分隔线 */
    lv_obj_t *lc1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_c1[] = {{6, 186}, {80, 186}};
    lv_line_set_points(lc1, pts_c1, 2);
    lv_obj_set_style_line_color(lc1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lc1, 1, 0);

    /* 功率 */
    lv_obj_t *cp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cp_l, "功率");
    lv_obj_set_style_text_color(cp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cp_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(cp_l, 6, 194);

    cpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_power_lbl, "--W");
    lv_obj_set_style_text_color(cpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_power_lbl, 6, 210);

    /* 分隔线 */
    lv_obj_t *lc2 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_c2[] = {{6, 242}, {80, 242}};
    lv_line_set_points(lc2, pts_c2, 2);
    lv_obj_set_style_line_color(lc2, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lc2, 1, 0);

    /* ----- GPU 列（x=88..170，左对齐） ----- */
    /* 占用率 */
    lv_obj_t *gpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_l, "占用率");
    lv_obj_set_style_text_color(gpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gpu_pct_l, 94, 66);

    gpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(gpu_pct_lbl, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_pct_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(gpu_pct_lbl, 94, 84);

    /* 温度 */
    lv_obj_t *gt_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gt_l, "温度");
    lv_obj_set_style_text_color(gt_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gt_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gt_l, 94, 138);

    gpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(gpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_temp_lbl, 94, 154);

    /* 分隔线 */
    lv_obj_t *lg1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_g1[] = {{94, 186}, {164, 186}};
    lv_line_set_points(lg1, pts_g1, 2);
    lv_obj_set_style_line_color(lg1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lg1, 1, 0);

    /* 功率 */
    lv_obj_t *gp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gp_l, "功率");
    lv_obj_set_style_text_color(gp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gp_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gp_l, 94, 194);

    gpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_power_lbl, "--W");
    lv_obj_set_style_text_color(gpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_power_lbl, 94, 210);

    /* 分隔线 */
    lv_obj_t *lg2 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_g2[] = {{94, 242}, {164, 242}};
    lv_line_set_points(lg2, pts_g2, 2);
    lv_obj_set_style_line_color(lg2, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lg2, 1, 0);

    /* ----- 底部状态栏 ----- */
    lv_obj_t *l3 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts3[] = {{0, 294}, {170, 294}};
    lv_line_set_points(l3, pts3, 2);
    lv_obj_set_style_line_color(l3, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(l3, 1, 0);

    footer_label = lv_label_create(lv_scr_act());
    lv_label_set_text(footer_label, "等待 AIDA64 数据...");
    lv_obj_set_style_text_color(footer_label, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(footer_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(footer_label, 6, 300);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "UI initialized: 170x320 2-col CPU/GPU (rate+temp+power)");
}

/* ===== SSE 数据绑定 ===== */
void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power)
{
    lvgl_port_lock(0);

    lv_label_set_text_fmt(cpu_pct_lbl,  "%d%%", cpu_rate);
    lv_label_set_text_fmt(cpu_temp_lbl, "%d°C", cpu_temp);
    lv_label_set_text_fmt(cpu_power_lbl, "%dW",  cpu_power);

    lv_label_set_text_fmt(gpu_pct_lbl,  "%d%%", gpu_rate);
    lv_label_set_text_fmt(gpu_temp_lbl, "%d°C", gpu_temp);
    lv_label_set_text_fmt(gpu_power_lbl, "%dW",  gpu_power);

    lvgl_port_unlock();
}

/* ===== 连接状态 ===== */
void set_monitor_connect_state(bool connected)
{
    lvgl_port_lock(0);
    if (connected) {
        lv_label_set_text(status_label, "已连接");
        lv_obj_set_style_text_color(status_label, lv_color_hex(C_OK), 0);
        lv_label_set_text(footer_label, "AIDA64 · 实时数据");
        lv_obj_set_style_text_color(footer_label, lv_color_hex(C_TEXT2), 0);
    } else {
        lv_label_set_text(status_label, "连接中...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(C_TEXT2), 0);
        lv_label_set_text(footer_label, "等待 AIDA64 数据...");
        lv_obj_set_style_text_color(footer_label, lv_color_hex(C_TEXT2), 0);
    }
    lvgl_port_unlock();
}

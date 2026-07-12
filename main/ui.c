#include "ui.h"
#include "lv_port.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui";

/* ===== 色彩（深色主题）===== */
#define C_BG       0x0F1117   /* 深色背景 */
#define C_CPU      0x3B82F6   /* CPU 蓝 */
#define C_GPU      0xF59E0B   /* GPU 橙 */
#define C_TEXT     0xE8AF0    /* 主文字白 */
#define C_TEXT2    0x6B7280   /* 副文字灰 */
#define C_DIVIDER  0x1E2330   /* 分隔线 */

/* ===== 对象句柄 ===== */
static lv_obj_t *cpu_pct_lbl;
static lv_obj_t *gpu_pct_lbl;
static lv_obj_t *cpu_temp_lbl;
static lv_obj_t *gpu_temp_lbl;
static lv_obj_t *cpu_power_lbl;
static lv_obj_t *gpu_power_lbl;

/* ===== ui_init ===== */
void ui_init(void)
{
    lvgl_port_lock(0);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(C_BG), 0);

    /* ============================================================
     * 整屏 170×320 竖屏，表格布局：
     *   y=0..30   : 表头 CPU | GPU
     *   y=30..66  : 占用率
     *   y=66..102 : 温度
     *   y=102..138: 功率
     * ============================================================ */

    /* ----- 表头分隔线 ----- */
    lv_obj_t *hdiv = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_h[] = {{0, 30}, {170, 30}};
    lv_line_set_points(hdiv, pts_h, 2);
    lv_obj_set_style_line_color(hdiv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(hdiv, 1, 0);

    /* ----- 列标题：CPU / GPU ----- */
    lv_obj_t *cpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_hdr, "CPU");
    lv_obj_set_style_text_color(cpu_hdr, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(cpu_hdr, LV_ALIGN_TOP_LEFT, 6, 6);

    lv_obj_t *gpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_hdr, "GPU");
    lv_obj_set_style_text_color(gpu_hdr, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(gpu_hdr, LV_ALIGN_TOP_RIGHT, -6, 6);

    /* ----- 中央垂直分隔线 ----- */
    lv_obj_t *cv = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_cv[] = {{85, 32}, {85, 320}};
    lv_line_set_points(cv, pts_cv, 2);
    lv_obj_set_style_line_color(cv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(cv, 1, 0);

    /* ============================================================
     * 第1行：占用率
     * ============================================================ */
    lv_obj_t *cpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_l, "占用率");
    lv_obj_set_style_text_color(cpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_align(cpu_pct_l, LV_ALIGN_TOP_LEFT, 6, 36);

    cpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(cpu_pct_lbl, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_pct_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(cpu_pct_lbl, LV_ALIGN_TOP_LEFT, 6, 52);

    lv_obj_t *gpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_l, "占用率");
    lv_obj_set_style_text_color(gpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_align(gpu_pct_l, LV_ALIGN_TOP_RIGHT, -6, 36);

    gpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(gpu_pct_lbl, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_pct_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(gpu_pct_lbl, LV_ALIGN_TOP_RIGHT, -6, 52);

    /* 行分隔线 */
    lv_obj_t *r1div = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_r1[] = {{0, 74}, {170, 74}};
    lv_line_set_points(r1div, pts_r1, 2);
    lv_obj_set_style_line_color(r1div, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(r1div, 1, 0);

    /* ============================================================
     * 第2行：温度
     * ============================================================ */
    lv_obj_t *cpu_temp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_temp_l, "温度");
    lv_obj_set_style_text_color(cpu_temp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cpu_temp_l, &lv_font_montserrat_12, 0);
    lv_obj_align(cpu_temp_l, LV_ALIGN_TOP_LEFT, 6, 80);

    cpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(cpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(cpu_temp_lbl, LV_ALIGN_TOP_LEFT, 6, 96);

    lv_obj_t *gpu_temp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_temp_l, "温度");
    lv_obj_set_style_text_color(gpu_temp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gpu_temp_l, &lv_font_montserrat_12, 0);
    lv_obj_align(gpu_temp_l, LV_ALIGN_TOP_RIGHT, -6, 80);

    gpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(gpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(gpu_temp_lbl, LV_ALIGN_TOP_RIGHT, -6, 96);

    /* 行分隔线 */
    lv_obj_t *r2div = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_r2[] = {{0, 118}, {170, 118}};
    lv_line_set_points(r2div, pts_r2, 2);
    lv_obj_set_style_line_color(r2div, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(r2div, 1, 0);

    /* ============================================================
     * 第3行：功率
     * ============================================================ */
    lv_obj_t *cpu_pwr_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pwr_l, "功率");
    lv_obj_set_style_text_color(cpu_pwr_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cpu_pwr_l, &lv_font_montserrat_12, 0);
    lv_obj_align(cpu_pwr_l, LV_ALIGN_TOP_LEFT, 6, 124);

    cpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_power_lbl, "--W");
    lv_obj_set_style_text_color(cpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(cpu_power_lbl, LV_ALIGN_TOP_LEFT, 6, 140);

    lv_obj_t *gpu_pwr_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pwr_l, "功率");
    lv_obj_set_style_text_color(gpu_pwr_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gpu_pwr_l, &lv_font_montserrat_12, 0);
    lv_obj_align(gpu_pwr_l, LV_ALIGN_TOP_RIGHT, -6, 124);

    gpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_power_lbl, "--W");
    lv_obj_set_style_text_color(gpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(gpu_power_lbl, LV_ALIGN_TOP_RIGHT, -6, 140);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "UI initialized: table layout CPU+GPU (占用率/温度/功率)");
}

/* ===== SSE 数据绑定 ===== */
void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power)
{
    lvgl_port_lock(0);

    lv_label_set_text_fmt(cpu_pct_lbl,   "%d%%",  cpu_rate);
    lv_label_set_text_fmt(cpu_temp_lbl,   "%d°C", cpu_temp);
    lv_label_set_text_fmt(cpu_power_lbl,  "%dW",   cpu_power);

    lv_label_set_text_fmt(gpu_pct_lbl,   "%d%%",  gpu_rate);
    lv_label_set_text_fmt(gpu_temp_lbl,   "%d°C", gpu_temp);
    lv_label_set_text_fmt(gpu_power_lbl,  "%dW",   gpu_power);

    lvgl_port_unlock();
}

/* ===== 连接状态（无操作，保留接口兼容性）===== */
void set_monitor_connect_state(bool connected)
{
    /* 已移除顶栏/底栏，此接口不再输出可视化内容 */
}

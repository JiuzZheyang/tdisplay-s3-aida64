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
#define C_DIVIDER  0x2A2D35   /* 分隔线 */

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
     * 整屏 170×320 竖屏，无顶栏/底栏：
     *   y=0..10   : 列标题 CPU / GPU
     *   y=44      : 列标题底部分隔线（通栏）
     *   y=50      : "占用率" 标签
     *   y=68      : 百分比大字 32px
     *   y=126     : "温度" 标签
     *   y=142     : 温度值 18px
     *   y=178     : 分隔线
     *   y=184     : "功率" 标签
     *   y=200     : 功率值 18px
     *   底部留空 y=240..320
     * ============================================================ */

    /* ----- 列标题 ----- */
    lv_obj_t *cpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_hdr, "CPU");
    lv_obj_set_style_text_color(cpu_hdr, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_hdr, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_hdr, 6, 6);

    lv_obj_t *gpu_hdr = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_hdr, "GPU");
    lv_obj_set_style_text_color(gpu_hdr, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_hdr, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_hdr, 110, 6);

    /* 列标题底部分隔线 */
    lv_obj_t *l1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts1[] = {{0, 44}, {170, 44}};
    lv_line_set_points(l1, pts1, 2);
    lv_obj_set_style_line_color(l1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(l1, 1, 0);

    /* ----- 中央垂直分隔线 ----- */
    lv_obj_t *lv = lv_line_create(lv_scr_act());
    static lv_point_precise_t ptsv[] = {{85, 50}, {85, 270}};
    lv_line_set_points(lv, ptsv, 2);
    lv_obj_set_style_line_color(lv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lv, 1, 0);

    /* ============================================================
     * CPU 列（x=4..81）
     * ============================================================ */
    /* 占用率标签 */
    lv_obj_t *cpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_l, "占用率");
    lv_obj_set_style_text_color(cpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(cpu_pct_l, 6, 50);

    /* 占用率数值 */
    cpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(cpu_pct_lbl, lv_color_hex(C_CPU), 0);
    lv_obj_set_style_text_font(cpu_pct_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(cpu_pct_lbl, 6, 68);

    /* 温度标签 */
    lv_obj_t *ct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(ct_l, "温度");
    lv_obj_set_style_text_color(ct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(ct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ct_l, 6, 126);

    /* 温度数值 */
    cpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(cpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_temp_lbl, 6, 142);

    /* 分隔线 */
    lv_obj_t *lc1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_c1[] = {{6, 176}, {80, 176}};
    lv_line_set_points(lc1, pts_c1, 2);
    lv_obj_set_style_line_color(lc1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lc1, 1, 0);

    /* 功率标签 */
    lv_obj_t *cp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(cp_l, "功率");
    lv_obj_set_style_text_color(cp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(cp_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(cp_l, 6, 182);

    /* 功率数值 */
    cpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(cpu_power_lbl, "--W");
    lv_obj_set_style_text_color(cpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(cpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(cpu_power_lbl, 6, 198);

    /* ============================================================
     * GPU 列（x=89..170）
     * ============================================================ */
    /* 占用率标签 */
    lv_obj_t *gpu_pct_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_l, "占用率");
    lv_obj_set_style_text_color(gpu_pct_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gpu_pct_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gpu_pct_l, 94, 50);

    /* 占用率数值 */
    gpu_pct_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_pct_lbl, "--%");
    lv_obj_set_style_text_color(gpu_pct_lbl, lv_color_hex(C_GPU), 0);
    lv_obj_set_style_text_font(gpu_pct_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(gpu_pct_lbl, 94, 68);

    /* 温度标签 */
    lv_obj_t *gt_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gt_l, "温度");
    lv_obj_set_style_text_color(gt_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gt_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gt_l, 94, 126);

    /* 温度数值 */
    gpu_temp_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_temp_lbl, "--°C");
    lv_obj_set_style_text_color(gpu_temp_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_temp_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_temp_lbl, 94, 142);

    /* 分隔线 */
    lv_obj_t *lg1 = lv_line_create(lv_scr_act());
    static lv_point_precise_t pts_g1[] = {{94, 176}, {164, 176}};
    lv_line_set_points(lg1, pts_g1, 2);
    lv_obj_set_style_line_color(lg1, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_line_width(lg1, 1, 0);

    /* 功率标签 */
    lv_obj_t *gp_l = lv_label_create(lv_scr_act());
    lv_label_set_text(gp_l, "功率");
    lv_obj_set_style_text_color(gp_l, lv_color_hex(C_TEXT2), 0);
    lv_obj_set_style_text_font(gp_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gp_l, 94, 182);

    /* 功率数值 */
    gpu_power_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(gpu_power_lbl, "--W");
    lv_obj_set_style_text_color(gpu_power_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(gpu_power_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(gpu_power_lbl, 94, 198);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "UI initialized: no top/bottom bar, CPU+GPU rate+temp+power");
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

/* ===== 连接状态（无操作，保留接口兼容性）===== */
void set_monitor_connect_state(bool connected)
{
    /* 已移除顶栏/底栏，此接口不再输出可视化内容 */
}

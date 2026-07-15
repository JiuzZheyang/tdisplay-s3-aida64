#include "ui.h"
#include "lv_port.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui";

#define C_BG      0xF4F5F7
#define C_TEXT    0x1A1D21
#define C_TEXT2   0x6B7280
#define C_CPU     0x2563EB
#define C_GPU     0xF59E0B

static lv_obj_t *main_scr   = NULL;
static lv_obj_t *cpu_val[3];
static lv_obj_t *gpu_val[3];

/* 亮度提示（动态创建/销毁） */
static lv_obj_t *toast_obj  = NULL;
static lv_timer_t *toast_timer = NULL;

extern const lv_font_t font_cn_18;
#define F_TITLE  &lv_font_montserrat_32
#define F_CNLBL  &font_cn_18
#define F_VALUE  &lv_font_montserrat_32

/* toast 计时器回调：淡出并删除 */
static void toast_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (toast_obj) {
        lv_obj_fade_out(toast_obj, 300, 0);
        toast_timer = NULL;
    }
}

static void toast_delayed_del(lv_timer_t *t)
{
    (void)t;
    if (toast_obj) {
        lv_obj_del(toast_obj);
        toast_obj = NULL;
    }
}

/* 显示亮度提示：亮条 + 文字，2.5s 后自动消失 */
void ui_show_brightness_toast(int percent)
{
    lvgl_port_lock(0);

    /* 删除旧 toast */
    if (toast_obj) {
        lv_obj_del(toast_obj);
        toast_obj = NULL;
    }
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }

    /* 在主屏上创建 toast（顶部居中） */
    toast_obj = lv_obj_create(main_scr);
    lv_obj_set_size(toast_obj, 160, 50);
    lv_obj_set_align(toast_obj, LV_ALIGN_TOP_MID);
    lv_obj_set_y(toast_obj, 10);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(toast_obj, 8, 0);
    lv_obj_set_style_shadow_width(toast_obj, 0, 0);
    lv_obj_clear_flag(toast_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(toast_obj, 6, 0);

    /* 亮度条 */
    lv_obj_t *bar = lv_bar_create(toast_obj);
    lv_obj_set_size(bar, 120, 8);
    lv_obj_set_align(bar, LV_ALIGN_TOP_MID);
    lv_obj_set_y(bar, 6);
    lv_bar_set_range(bar, 5, 100);
    lv_bar_set_value(bar, percent, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x374151), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

    /* 文字标签 */
    lv_obj_t *lbl = lv_label_create(toast_obj);
    lv_label_set_text_fmt(lbl, "亮度: %d%%", percent);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF9FAFB), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_align(lbl, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(lbl, -6);

    lv_obj_fade_in(toast_obj, 200, 0);

    /* 2.5s 后淡出 */
    toast_timer = lv_timer_create(toast_timer_cb, 2500, NULL);

    lvgl_port_unlock();
}

static lv_obj_t *lbl_l(lv_obj_t *parent, int x, int y, const char *text,
                       const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *lbl_c(lv_obj_t *parent, int x, int y, int w,
                       const char *text, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x, y);
    return l;
}

lv_obj_t *ui_get_main_screen(void)
{
    return main_scr;
}

void ui_init(void)
{
    lvgl_port_lock(0);

    /* 创建一个独立 screen，所有 UI 都建在上面 */
    main_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_scr, lv_color_hex(C_BG), 0);
    lv_obj_clear_flag(main_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(main_scr, 0, 0);

    /* 标题 */
    lbl_c(main_scr, 70,  5, 100, "CPU", F_TITLE, lv_color_hex(C_CPU));
    lbl_c(main_scr, 190, 5, 100, "GPU", F_TITLE, lv_color_hex(C_GPU));

    /* 占用率 */
    lbl_l(main_scr, 8,  56, "占用率", F_CNLBL, lv_color_hex(C_TEXT2));
    cpu_val[0] = lbl_c(main_scr, 70,  50, 100, "--%", F_VALUE, lv_color_hex(C_TEXT));
    gpu_val[0] = lbl_c(main_scr, 190, 50, 100, "--%", F_VALUE, lv_color_hex(C_TEXT));

    /* 温度 */
    lbl_l(main_scr, 8,  99, "温度", F_CNLBL, lv_color_hex(C_TEXT2));
    cpu_val[1] = lbl_c(main_scr, 70,  93, 100, "--°C", F_VALUE, lv_color_hex(C_TEXT));
    gpu_val[1] = lbl_c(main_scr, 190, 93, 100, "--°C", F_VALUE, lv_color_hex(C_TEXT));

    /* 功率 */
    lbl_l(main_scr, 8,  141, "功率", F_CNLBL, lv_color_hex(C_TEXT2));
    cpu_val[2] = lbl_c(main_scr, 70,  135, 100, "--W", F_VALUE, lv_color_hex(C_TEXT));
    gpu_val[2] = lbl_c(main_scr, 190, 135, 100, "--W", F_VALUE, lv_color_hex(C_TEXT));

    lvgl_port_unlock();
    ESP_LOGI(TAG, "UI initialized (screen mode)");
}

void set_monitor_param(int cpu_rate, int cpu_temp, int cpu_power,
                       int gpu_rate, int gpu_temp, int gpu_power)
{
    lvgl_port_lock(0);
    lv_label_set_text_fmt(cpu_val[0], "%d%%",  cpu_rate);
    lv_label_set_text_fmt(cpu_val[1], "%d°C",  cpu_temp);
    lv_label_set_text_fmt(cpu_val[2], "%dW",   cpu_power);
    lv_label_set_text_fmt(gpu_val[0], "%d%%",  gpu_rate);
    lv_label_set_text_fmt(gpu_val[1], "%d°C",  gpu_temp);
    lv_label_set_text_fmt(gpu_val[2], "%dW",   gpu_power);
    lvgl_port_unlock();
}

void set_monitor_connect_state(bool connected)
{
    (void)connected;
}

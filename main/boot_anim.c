#include "boot_anim.h"
#include "lv_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "boot_anim";

/* 配色 */
#define C_BG      0x0A0E1A   /* 深蓝黑背景 */
#define C_ACCENT  0x2563EB   /* 科技蓝 */
#define C_ACCENT2 0xF59E0B   /* 琥珀强调色 */
#define C_TEXT    0xF9FAFB   /* 主文字白 */
#define C_TEXT2   0x9CA3AF   /* 次级灰 */
#define C_BAR_BG  0x1F2937   /* 进度条底色 */

/* 动画参数 */
#define FADE_IN_MS      600
#define LOAD_DURATION   3000   /* 进度条走完整的时间 ms */
#define LOAD_STEP_MS    30     /* 进度更新周期 */
#define SWITCH_FADE_MS  600    /* 切屏淡入时间 */

/* 状态 */
static volatile bool   s_ready        = false;
static lv_obj_t        *s_boot_scr     = NULL;
static lv_obj_t        *s_target_scr   = NULL;
static lv_timer_t      *s_load_timer   = NULL;
static lv_timer_t      *s_switch_timer = NULL;
static uint32_t         s_start_tick    = 0;
static uint32_t         s_progress      = 0;   /* 0~100 */
static uint32_t         s_min_ms        = 2000;
static uint32_t         s_max_ms        = 6000;

/* 进度条 widget */
static lv_obj_t *s_bar_bg   = NULL;
static lv_obj_t *s_bar_fill = NULL;
static lv_obj_t *s_pct_lbl  = NULL;

/* WiFi 状态文字 */
static lv_obj_t *s_status_lbl = NULL;
static const char *s_status_text = "正在连接 WiFi...";

/* ===================== 进度条动画 ===================== */

/* 驱动进度条走完的计时器：每 LOAD_STEP_MS ms 加一次 */
static void load_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t elapsed = lv_tick_elaps(s_start_tick);
    uint32_t p = (elapsed * 100) / LOAD_DURATION;
    if (p > 100) p = 100;

    s_progress = p;

    /* 更新填充条 */
    if (s_bar_fill) {
        lv_bar_set_value(s_bar_fill, p, LV_ANIM_OFF);
    }

    /* 更新百分比文字 */
    if (s_pct_lbl) {
        lv_label_set_text_fmt(s_pct_lbl, "%lu%%", (unsigned long)p);
    }

    if (p >= 100) {
        lv_timer_reset(s_load_timer);
        lv_timer_pause(s_load_timer);
    }
}

/* ===================== 切屏逻辑 ===================== */

static void switch_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t elapsed = lv_tick_elaps(s_start_tick);

    bool do_switch = false;
    if (s_ready && elapsed >= s_min_ms) {
        ESP_LOGI(TAG, "ready + min elapsed (%lu ms), switching", (unsigned long)elapsed);
        do_switch = true;
    } else if (elapsed >= s_max_ms) {
        ESP_LOGW(TAG, "max timeout (%lu ms), forcing switch", (unsigned long)elapsed);
        do_switch = true;
    }

    if (do_switch) {
        /* 删除加载计时器 */
        if (s_load_timer) {
            lv_timer_del(s_load_timer);
            s_load_timer = NULL;
        }
        if (s_switch_timer) {
            lv_timer_del(s_switch_timer);
            s_switch_timer = NULL;
        }
        /* 淡出切换到主屏 */
        lv_scr_load_anim(s_target_scr, LV_SCR_LOAD_ANIM_FADE_IN,
                         SWITCH_FADE_MS, SWITCH_FADE_MS, true);
        s_boot_scr = NULL;
        ESP_LOGI(TAG, "switched to main screen");
    }
}

/* ===================== 开机画面构建 ===================== */

void boot_anim_init(void)
{
    lvgl_port_lock(0);

    s_boot_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_boot_scr, lv_color_hex(C_BG), 0);
    lv_obj_clear_flag(s_boot_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_boot_scr, 0, 0);
    lv_obj_set_style_radius(s_boot_scr, 0, 0);

    /* ---- 顶部装饰线 ---- */
    lv_obj_t *top_line = lv_obj_create(s_boot_scr);
    lv_obj_set_size(top_line, 170, 3);
    lv_obj_set_pos(top_line, 0, 0);
    lv_obj_set_style_bg_color(top_line, lv_color_hex(C_ACCENT), 0);
    lv_obj_clear_flag(top_line, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 主标题 AIDA64 ---- */
    lv_obj_t *title = lv_label_create(s_boot_scr);
    lv_label_set_text(title, "AIDA64");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(C_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 45);

    /* ---- 副标题 PC Monitor ---- */
    lv_obj_t *sub = lv_label_create(s_boot_scr);
    lv_label_set_text(sub, "PC Monitor");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(C_ACCENT), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 90);

    /* ---- 装饰分隔线（两条细线）---- */
    lv_obj_t *div = lv_obj_create(s_boot_scr);
    lv_obj_set_size(div, 100, 1);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 115);
    lv_obj_set_style_bg_color(div, lv_color_hex(C_TEXT2), 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 进度条背景 ---- */
    s_bar_bg = lv_obj_create(s_boot_scr);
    lv_obj_set_size(s_bar_bg, 140, 10);
    lv_obj_align(s_bar_bg, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_radius(s_bar_bg, 5, 0);
    lv_obj_set_style_bg_color(s_bar_bg, lv_color_hex(C_BAR_BG), 0);
    lv_obj_clear_flag(s_bar_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 进度条填充 ---- */
    s_bar_fill = lv_bar_create(s_bar_bg);
    lv_bar_set_range(s_bar_fill, 0, 100);
    lv_bar_set_value(s_bar_fill, 0, LV_ANIM_OFF);
    lv_obj_set_size(s_bar_fill, 140, 10);
    lv_obj_set_pos(s_bar_fill, 0, 0);
    lv_obj_set_style_radius(s_bar_fill, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bar_fill, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);

    /* ---- 百分比文字 ---- */
    s_pct_lbl = lv_label_create(s_boot_scr);
    lv_label_set_text_fmt(s_pct_lbl, "0%%");
    lv_obj_set_style_text_font(s_pct_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_pct_lbl, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(s_pct_lbl, LV_ALIGN_TOP_MID, 0, 158);

    /* ---- 旋转加载圈（右下角装饰）---- */
    lv_obj_t *spinner = lv_spinner_create(s_boot_scr);
    lv_obj_set_size(spinner, 24, 24);
    lv_obj_set_style_arc_length(spinner, 60, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(C_BG), 0);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 50, 150);

    /* ---- 状态文字 ---- */
    s_status_lbl = lv_label_create(s_boot_scr);
    lv_label_set_text(s_status_lbl, s_status_text);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_TOP_MID, 0, 185);

    /* ---- 底部装饰线 ---- */
    lv_obj_t *bottom_line = lv_obj_create(s_boot_scr);
    lv_obj_set_size(bottom_line, 170, 3);
    lv_obj_set_pos(bottom_line, 0, 317);
    lv_obj_set_style_bg_color(bottom_line, lv_color_hex(C_ACCENT2), 0);
    lv_obj_clear_flag(bottom_line, LV_OBJ_FLAG_SCROLLABLE);

    /* 淡入效果 */
    lv_obj_fade_in(s_boot_scr, FADE_IN_MS, 0);

    lv_scr_load_anim(s_boot_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "boot screen created");
}

/* ===================== 启动动画播放 ===================== */

void boot_anim_play(void *target, uint32_t min_ms, uint32_t max_ms)
{
    s_target_scr  = (lv_obj_t *)target;
    s_start_tick  = lv_tick_get();
    s_ready       = false;
    s_progress    = 0;
    s_min_ms      = (min_ms > LOAD_DURATION) ? min_ms : LOAD_DURATION + 500;
    s_max_ms      = max_ms;

    /* 进度条更新计时器 */
    s_load_timer = lv_timer_create(load_timer_cb, LOAD_STEP_MS, NULL);

    /* 切屏检查计时器（每 100ms 检查一次） */
    s_switch_timer = lv_timer_create(switch_timer_cb, 100, NULL);

    ESP_LOGI(TAG, "boot anim started (min=%lu ms, max=%lu ms)",
             (unsigned long)s_min_ms, (unsigned long)s_max_ms);
}

/* WiFi 连接成功后由 main.c 调用，通知可提前结束 */
void boot_anim_signal_ready(void)
{
    s_ready = true;
    /* 更新状态文字 */
    if (s_status_lbl) {
        lvgl_port_lock(0);
        lv_label_set_text(s_status_lbl, "WiFi 已连接");
        lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x16A34A), 0);
        lvgl_port_unlock();
    }
    ESP_LOGI(TAG, "ready signal received");
}

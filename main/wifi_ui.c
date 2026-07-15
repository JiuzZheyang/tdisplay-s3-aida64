#include "wifi_ui.h"
#include "lv_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wifi_ui";

#define C_BG      0x0A0E1A
#define C_ACCENT  0x2563EB
#define C_ACCENT2 0xF59E0B
#define C_TEXT    0xF9FAFB
#define C_TEXT2   0x9CA3AF
#define C_BAR_BG  0x1F2937

static lv_obj_t *s_ap_scr  = NULL;
static lv_obj_t *s_hint_lbl = NULL;

void wifi_ui_show_ap_mode(const char *ap_ssid)
{
    lvgl_port_lock(0);

    if (s_ap_scr) {
        /* 屏幕已存在，直接显示 */
        lv_scr_load_anim(s_ap_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
        lvgl_port_unlock();
        return;
    }

    s_ap_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_ap_scr, lv_color_hex(C_BG), 0);
    lv_obj_clear_flag(s_ap_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_ap_scr, 0, 0);

    /* 顶部蓝色装饰线 */
    lv_obj_t *top_line = lv_obj_create(s_ap_scr);
    lv_obj_set_size(top_line, 170, 3);
    lv_obj_set_pos(top_line, 0, 0);
    lv_obj_set_style_bg_color(top_line, lv_color_hex(C_ACCENT), 0);
    lv_obj_clear_flag(top_line, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(s_ap_scr);
    lv_label_set_text(title, "待配网");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(C_ACCENT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    /* AP SSID 名称 */
    lv_obj_t *ssid_lbl = lv_label_create(s_ap_scr);
    lv_label_set_text(ssid_lbl, ap_ssid ? ap_ssid : "AIDA64-????");
    lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_align(ssid_lbl, LV_ALIGN_TOP_MID, 0, 55);

    /* 密码 */
    lv_obj_t *pass_lbl = lv_label_create(s_ap_scr);
    lv_label_set_text(pass_lbl, "密码: 12345678");
    lv_obj_set_style_text_font(pass_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pass_lbl, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(pass_lbl, LV_ALIGN_TOP_MID, 0, 78);

    /* 分隔线 */
    lv_obj_t *div = lv_obj_create(s_ap_scr);
    lv_obj_set_size(div, 120, 1);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(div, lv_color_hex(C_TEXT2), 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    /* 操作提示 - 第一行 */
    lv_obj_t *step1 = lv_label_create(s_ap_scr);
    lv_label_set_text(step1, "1. 连接以上WiFi热点");
    lv_obj_set_style_text_font(step1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(step1, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(step1, LV_ALIGN_TOP_MID, 0, 112);

    /* 操作提示 - 第二行 */
    lv_obj_t *step2 = lv_label_create(s_ap_scr);
    lv_label_set_text(step2, "2. 打开浏览器访问");
    lv_obj_set_style_text_font(step2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(step2, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(step2, LV_ALIGN_TOP_MID, 0, 132);

    /* IP 地址 */
    lv_obj_t *ip_lbl = lv_label_create(s_ap_scr);
    lv_label_set_text(ip_lbl, "192.168.4.1");
    lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ip_lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_align(ip_lbl, LV_ALIGN_TOP_MID, 0, 155);

    /* 旋转加载圈（LVGL v9: lv_spinner_create 只接收父对象参数） */
    lv_obj_t *spinner = lv_spinner_create(s_ap_scr);
    lv_obj_set_size(spinner, 24, 24);
    // arc length: default is fine for spinner
    lv_obj_set_style_arc_color(spinner, lv_color_hex(C_BG), 0);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 190);

    /* 状态文字 */
    s_hint_lbl = lv_label_create(s_ap_scr);
    lv_label_set_text(s_hint_lbl, "等待配网...");
    lv_obj_set_style_text_font(s_hint_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_hint_lbl, lv_color_hex(C_TEXT2), 0);
    lv_obj_align(s_hint_lbl, LV_ALIGN_TOP_MID, 0, 222);

    /* 底部琥珀色装饰线 */
    lv_obj_t *bottom_line = lv_obj_create(s_ap_scr);
    lv_obj_set_size(bottom_line, 170, 3);
    lv_obj_set_pos(bottom_line, 0, 167);
    lv_obj_set_style_bg_color(bottom_line, lv_color_hex(C_ACCENT2), 0);
    lv_obj_clear_flag(bottom_line, LV_OBJ_FLAG_SCROLLABLE);

    lv_scr_load_anim(s_ap_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "AP mode UI shown (SSID=%s)", ap_ssid);
}

void wifi_ui_hide(void)
{
    if (s_ap_scr) {
        lvgl_port_lock(0);
        lv_obj_del(s_ap_scr);
        s_ap_scr = NULL;
        s_hint_lbl = NULL;
        lvgl_port_unlock();
        ESP_LOGI(TAG, "AP mode UI hidden");
    }
}

void wifi_ui_set_status(const char *status)
{
    if (s_hint_lbl) {
        lvgl_port_lock(0);
        lv_label_set_text(s_hint_lbl, status);
        lvgl_port_unlock();
    }
}

lv_obj_t *wifi_ui_get_screen(void)
{
    return s_ap_scr;
}

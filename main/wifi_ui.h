#ifndef WIFI_UI_H
#define WIFI_UI_H

/*
 * WiFi 配网状态 UI
 *
 * 当设备处于 AP 配网模式时，显示在 LCD 上的提示画面，
 * 告知用户连接热点并访问配网页面。
 *
 * 布局（深色主题，与开机动画风格一致）：
 *   - 顶部蓝线
 *   - AP SSID 名称（大字）
 *   - 配网提示文字
 *   - 旋转加载圈（表示等待中）
 *   - 底部琥珀线
 */

/* 初始化并显示配网 UI（在 wifi_manager 启动 AP 后由 main.c 调用） */
void wifi_ui_show_ap_mode(const char *ap_ssid);

/* 切换回主 UI 屏幕（在 STA 连接成功后 main.c 调用） */
void wifi_ui_hide(void);

/* 更新提示文字（如"正在连接 XXX..."） */
void wifi_ui_set_status(const char *status);

/* 获取配网 UI 的 screen 对象（用于 lv_scr_load_anim 切换） */
lv_obj_t *wifi_ui_get_screen(void);

#endif /* WIFI_UI_H */

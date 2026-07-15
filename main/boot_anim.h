#ifndef BOOT_ANIM_H
#define BOOT_ANIM_H

#include <stdint.h>

/*
 * 开机动画 — 进度条 + 百分比 + WiFi 状态
 *
 * 视觉效果（深蓝黑主题）：
 *   - 顶部蓝色装饰线 + AIDA64 主标题 + PC Monitor 副标题
 *   - 带百分比数字的进度条（3 秒走完）
 *   - 旋转加载圈 + 状态文字
 *   - 底部琥珀色装饰线
 *   - WiFi 连接成功后文字变绿色
 *
 * 使用方式：
 *   1. boot_anim_init()         — 创建开机画面（boot 屏），立即显示
 *   2. boot_anim_play(target, min_ms, max_ms)
 *                              — 启动进度条 + 切屏计时器
 *                                · 进度条走完（3000ms）后等待
 *                                · 收到 ready 信号（WiFi 连上）+ 超过 min_ms → 立即切换
 *                                · 超过 max_ms → 强制切换
 *   3. boot_anim_signal_ready() — WiFi 连上后由 main.c 调用，允许提前结束
 *
 * 切屏效果：LV_SCR_LOAD_ANIM_FADE_IN（600ms 淡入）
 */
void boot_anim_init(void);

/* target: 主 UI 的 screen 对象（由 ui_get_main_screen() 取得） */
void boot_anim_play(void *target, uint32_t min_ms, uint32_t max_ms);

/* WiFi 连接成功后调用，通知可提前结束动画 */
void boot_anim_signal_ready(void);

#endif /* BOOT_ANIM_H */

/****************************************************************************
 * display.h
 * 
 * 显示模块：管理LCD屏幕、U8g2绘制操作
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

extern "C" {
#include "u8g2.h"
}

// LCD初始化和基本操作
void lcd_init();
void lcd_clear();
void lcd_set_contrast(uint8_t contrast);
void lcd_backlight(bool on);
void lcd_set_backlight_brightness(uint8_t level); // 设置背光亮度级别(0-5)
void lcd_set_backlight_timeout(uint16_t seconds); // 设置背光自动熄灭时间(秒)
uint8_t lcd_get_backlight_brightness(); // 获取当前背光亮度
uint16_t lcd_get_backlight_timeout(); // 获取当前背光超时时间
void lcd_update_backlight(); // 更新背光状态，检查超时

// 绘制工具函数
void draw_battery_icon(int x, int y, int percent, bool charging);
void draw_progress_bar(int x, int y, int width, int height, uint32_t pos, uint32_t total);
void draw_volume_indicator(int x, int y, int width, int height, int volume);

// 界面组件
void draw_header(const char* title, int battery_percent, bool charging);
void draw_footer(const char* text);

// 获取U8g2实例(其他模块可以直接绘制)
u8g2_t* get_display();

// 中文字体支持
void lcd_set_chinese_font(); // 设置中文字体
void lcd_set_english_font(); // 恢复英文字体
void lcd_draw_utf8(int x, int y, const char* utf8_text); // 绘制UTF-8文本

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

// 绘制工具函数
void draw_battery_icon(int x, int y, int percent, bool charging);
void draw_progress_bar(int x, int y, int width, int height, uint32_t pos, uint32_t total);
void draw_volume_indicator(int x, int y, int width, int height, int volume);

// 界面组件
void draw_header(const char* title, int battery_percent, bool charging);
void draw_footer(const char* text);

// 获取U8g2实例(其他模块可以直接绘制)
u8g2_t* get_display();

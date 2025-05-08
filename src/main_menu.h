/****************************************************************************
 * main_menu.h
 * 
 * 主菜单系统：整合MP3播放器和GNSS码表功能
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

// 主应用模式
enum AppMode {
  APP_MODE_MENU,      // 主菜单界面
  APP_MODE_MP3,       // MP3播放器模式
  APP_MODE_GNSS,      // GNSS码表模式
  APP_MODE_SYSTEM     // 系统设置
};

// 初始化主菜单
void main_menu_init();

// 绘制主菜单界面
void main_menu_draw(int selected_index);

// 处理主菜单按键输入
void main_menu_handle_key(uint8_t key);

// 获取当前应用模式
AppMode main_menu_get_mode();

// 设置当前应用模式
void main_menu_set_mode(AppMode mode);

// 锁屏控制
void main_menu_lock_screen(bool lock);
bool main_menu_is_locked();

// 更新电池状态
void main_menu_update_battery();
int main_menu_get_battery_percent();
bool main_menu_is_battery_charging();

/****************************************************************************
 * main_menu.cpp
 * 
 * 主菜单系统实现：整合MP3播放器和GNSS码表功能
 * ***************************************************************************/
#include "main_menu.h"
#include "display.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/boardctl.h>

// 全局变量
static AppMode g_current_mode = APP_MODE_MENU;
static int g_menu_index = 0;              // 当前菜单项
static bool g_screen_locked = false;      // 是否锁屏
static bool g_in_backlight_settings = false; // 是否在背光设置界面
static time_t g_last_activity = 0;        // 最后活动时间
static int g_battery_percent = 80;        // 电池电量
static bool g_battery_charging = false;   // 是否在充电
static time_t g_last_battery_check = 0;   // 最后电池检查时间

// 背光设置
static uint8_t g_backlight_brightness = 5;   // 背光亮度级别(0-5)
static uint16_t g_backlight_timeout = 30;    // 背光自动熄灭时间(秒)
static int g_backlight_menu_index = 0;       // 背光设置菜单索引

/**
 * 初始化主菜单
 */
void main_menu_init()
{
  // 初始化显示
  lcd_init();
  g_current_mode = APP_MODE_MENU;
  g_last_activity = time(NULL);
  
  // 初始化背光设置
  lcd_set_backlight_brightness(g_backlight_brightness);
  lcd_set_backlight_timeout(g_backlight_timeout);
  lcd_backlight(true);
  
  // 初始电池状态检查
  main_menu_update_battery();
}

/**
 * 更新电池状态
 */
void main_menu_update_battery()
{
  time_t now = time(NULL);
  if (now - g_last_battery_check < 60) return;  // 每分钟检查一次
  
  g_last_battery_check = now;
  
#ifdef BOARDIOC_BATTERY
  struct board_battery_level_s level;
  if (boardctl(BOARDIOC_BATTERY, (uintptr_t)&level) == 0) {
    g_battery_percent = level.percentage;
    g_battery_charging = level.charging;
  }
#else
  // 如果没有电池API，使用模拟数据
  static int dir = 1;
  g_battery_percent += dir;
  if (g_battery_percent > 95) dir = -1;
  if (g_battery_percent < 30) dir = 1;
#endif
}

/**
 * 获取电池电量百分比
 */
int main_menu_get_battery_percent()
{
  return g_battery_percent;
}

/**
 * 电池是否正在充电
 */
bool main_menu_is_battery_charging()
{
  return g_battery_charging;
}

/**
 * 绘制主菜单界面
 */
void main_menu_draw(int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_8x13B_tr);
  u8g2_DrawStr(u8g2, 10, 15, "Spresense多功能系统");
  
  // 显示时间
  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  
  char time_buf[16];
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
           tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
  
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, 80, 8, time_buf);
  
  // 绘制电池图标
  draw_battery_icon(110, 3, g_battery_percent, g_battery_charging);
  
  // 菜单项
  const char* menu_items[] = {
    "MP3 播放器",
    "GNSS 码表",
    "系统设置",
    "关于"
  };
  const int items_count = sizeof(menu_items) / sizeof(menu_items[0]);
  
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  
  for (int i = 0; i < items_count; i++) {
    int y_pos = 30 + i * 12;
    
    // 选中项反色显示
    if (i == selected_index) {
      u8g2_DrawBox(u8g2, 10, y_pos-10, 108, 12);
      u8g2_SetDrawColor(u8g2, 0);
    }
    
    u8g2_DrawStr(u8g2, 15, y_pos, menu_items[i]);
    
    // 恢复颜色
    if (i == selected_index) {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 处理主菜单按键输入
 */
void main_menu_handle_key(uint8_t key)
{
  const int menu_items_count = 4; // MP3、GNSS、系统设置、关于
  
  // 如果处于锁屏状态，任意键解锁
  if (g_screen_locked) {
    g_screen_locked = false;
    g_last_activity = time(NULL);
    return;
  }
  
  // 更新最后活动时间
  g_last_activity = time(NULL);
  
  // 处理背光设置界面
  if (g_in_backlight_settings) {
    switch (key) {
      case KEY_PREV:
        // 选择上一个选项
        g_backlight_menu_index = (g_backlight_menu_index > 0) ? g_backlight_menu_index - 1 : 0;
        break;
        
      case KEY_NEXT:
        // 选择下一个选项
        g_backlight_menu_index = (g_backlight_menu_index < 1) ? g_backlight_menu_index + 1 : 1;
        break;
        
      case KEY_SELECT:
        // 修改选中项的值
        if (g_backlight_menu_index == 0) { // 亮度
          g_backlight_brightness = (g_backlight_brightness + 1) % 6; // 0-5循环
          main_menu_backlight_set_brightness(g_backlight_brightness);
        } else if (g_backlight_menu_index == 1) { // 超时
          // 切换超时选项：0(始终开启), 10秒, 30秒, 60秒, 120秒
          if (g_backlight_timeout == 0) g_backlight_timeout = 10;
          else if (g_backlight_timeout == 10) g_backlight_timeout = 30;
          else if (g_backlight_timeout == 30) g_backlight_timeout = 60;
          else if (g_backlight_timeout == 60) g_backlight_timeout = 120;
          else g_backlight_timeout = 0;
          main_menu_backlight_set_timeout(g_backlight_timeout);
        }
        break;
        
      case KEY_BACK:
        // 返回设置菜单
        g_in_backlight_settings = false;
        break;
    }
    
    // 重绘背光设置界面
    ui_draw_backlight_settings(g_backlight_menu_index, g_backlight_brightness, g_backlight_timeout);
    return;
  }
  
  // 如果不在主菜单模式，先处理返回主菜单的按键
  if (g_current_mode != APP_MODE_MENU) {
    if (key == KEY_BACK) {
      g_current_mode = APP_MODE_MENU;
    }
    return;
  }
  
  // 主菜单按键处理
  switch (key) {
    case KEY_PREV:
      // 上一项
      g_menu_index = (g_menu_index + menu_items_count - 1) % menu_items_count;
      break;
      
    case KEY_NEXT:
      // 下一项
      g_menu_index = (g_menu_index + 1) % menu_items_count;
      break;
      
    case KEY_SELECT:
      // 选择项目
      switch (g_menu_index) {
        case 0: // MP3播放器
          g_current_mode = APP_MODE_MP3;
          break;
          
        case 1: // GNSS码表
          g_current_mode = APP_MODE_GNSS;
          break;
          
        case 2: // 系统设置
          g_current_mode = APP_MODE_SYSTEM;
          break;
          
        case 3: // 关于
          // 保持在菜单模式，在主程序中显示关于界面
          break;
      }
      break;
      
    case KEY_BACK:
      // 后退键在主菜单中没有作用
      break;
  }
}

/**
 * 获取当前应用模式
 */
AppMode main_menu_get_mode()
{
  return g_current_mode;
}

/**
 * 设置当前应用模式
 */
void main_menu_set_mode(AppMode mode)
{
  g_current_mode = mode;
  g_last_activity = time(NULL); // 重置活动时间
}

/**
 * 锁屏控制
 */
void main_menu_lock_screen(bool lock)
{
  g_screen_locked = lock;
}

/**
 * 是否处于锁屏状态
 */
bool main_menu_is_locked()
{
  return g_screen_locked;
}

/**
 * 设置背光亮度
 */
void main_menu_backlight_set_brightness(uint8_t level)
{
  g_backlight_brightness = level;
  lcd_set_backlight_brightness(level);
}

/**
 * 设置背光超时时间
 */
void main_menu_backlight_set_timeout(uint16_t seconds)
{
  g_backlight_timeout = seconds;
  lcd_set_backlight_timeout(seconds);
}

/**
 * 获取背光亮度
 */
uint8_t main_menu_backlight_get_brightness()
{
  return g_backlight_brightness;
}

/**
 * 获取背光超时时间
 */
uint16_t main_menu_backlight_get_timeout()
{
  return g_backlight_timeout;
}

/**
 * 更新背光状态，检查超时
 */
void main_menu_update_backlight()
{
  // 当有按键时重置超时计算并打开背光
  time_t now = time(NULL);
  if (now - g_last_activity < 1) {
    lcd_backlight(true);
  }
  
  // 检查背光超时
  lcd_update_backlight();
}

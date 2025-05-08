/****************************************************************************
 * main.cxx
 *
 * 主程序入口 - Spresense多功能系统
 * 整合功能:
 *   • MP3播放器
 *   • GNSS码表
 *   • 系统管理
 * 
 * 依赖模块:
 *   • display.h/cpp - 显示控制
 *   • player.h/cpp - 音频播放
 *   • file_system.h/cpp - 文件管理
 *   • ui_screens.h/cpp - MP3播放器界面
 *   • gnss_data.h/cpp - GNSS数据处理
 *   • gnss_screens.h/cpp - GNSS界面
 *   • main_menu.h/cpp - 主菜单系统
 * ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <nuttx/config.h>
#include <sys/boardctl.h>
#include <fcntl.h>
#include <vector>
#include <sys/stat.h>

// 各模块头文件
#include "display.h"
#include "player.h"
#include "file_system.h"
#include "ui_screens.h"
#include "gnss_data.h"
#include "gnss_screens.h"
#include "main_menu.h"
#include "common.h"

// 应用状态定义
static bool g_running = true;             // 应用是否运行
static AppScreen g_mp3_screen = SCREEN_PLAYER;  // MP3当前界面
static GnssScreen g_gnss_screen = GNSS_SCREEN_ODOMETER;  // GNSS当前界面
static uint32_t g_screen_timeout_sec = 30;  // 屏幕自动锁定时间(秒)
static char g_track_filename[64];           // 当前轨迹文件名
static bool g_show_about = false;           // 是否显示关于界面

// 线程相关
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// 前向声明
static void* gnss_thread(void*);
static void handle_mp3_keys(KeyCode key);
static void handle_gnss_keys(KeyCode key);
static void handle_system_settings(KeyCode key);
static void draw_about_screen();
static void draw_system_settings(int selected_item);

// 系统设置选项
enum SystemSetting {
  SYS_SCREEN_TIMEOUT,
  SYS_SLEEP_TIMER,
  SYS_FORMAT_SD,
  SYS_BACK
};
static int g_system_setting_index = 0;  // 当前系统设置选项

/**
 * 自动锁屏检查
 */
static bool check_auto_lock() 
{
  if (main_menu_is_locked()) return true;
  
  time_t now = time(nullptr);
  if (now - g_last_activity > g_screen_timeout_sec) {
    main_menu_lock_screen(true);
    return true;
  }
  return false;
}

/**
 * 保存轨迹文件，自动生成文件名
 */
static void save_current_track() 
{
  time_t now = time(nullptr);
  struct tm* tm = localtime(&now);
  
  sprintf(g_track_filename, "/sd/tracks/track_%04d%02d%02d_%02d%02d%02d.gpx",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          tm->tm_hour, tm->tm_min, tm->tm_sec);
  
  // 确保目录存在
  mkdir("/sd/tracks", 0777);
  
  // 保存轨迹
  gnss_save_track(g_track_filename);
}

/**
 * 绘制系统设置界面
 */
static void draw_system_settings(int selected_item) 
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 30, 10, "系统设置");
  
  // 设置项
  const char* items[] = {
    "屏幕超时",
    "睡眠定时",
    "格式化SD卡",
    "返回"
  };
  const int items_count = sizeof(items) / sizeof(items[0]);
  
  for (int i = 0; i < items_count; i++) {
    int y_pos = 25 + i * 12;
    
    // 选中项反色显示
    if (i == selected_item) {
      u8g2_DrawBox(u8g2, 0, y_pos-10, 128, 12);
      u8g2_SetDrawColor(u8g2, 0);
    }
    
    u8g2_DrawStr(u8g2, 5, y_pos, items[i]);
    
    // 显示当前值
    char value_buf[32] = {0};
    if (i == SYS_SCREEN_TIMEOUT) {
      snprintf(value_buf, sizeof(value_buf), "%us", g_screen_timeout_sec);
    } else if (i == SYS_SLEEP_TIMER) {
      if (g_sleep_minutes > 0) {
        snprintf(value_buf, sizeof(value_buf), "%u分钟", g_sleep_minutes);
      } else {
        strcpy(value_buf, "关闭");
      }
    }
    
    if (value_buf[0] != '\0') {
      int x_pos = 128 - u8g2_GetStrWidth(u8g2, value_buf) - 5;
      u8g2_DrawStr(u8g2, x_pos, y_pos, value_buf);
    }
    
    // 恢复颜色
    if (i == selected_item) {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制关于界面
 */
static void draw_about_screen() 
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_8x13B_tr);
  u8g2_DrawStr(u8g2, 30, 12, "关于系统");
  
  // 版本信息
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 5, 25, "Spresense多功能系统");
  u8g2_DrawStr(u8g2, 5, 37, "版本: 1.0.0");
  u8g2_DrawStr(u8g2, 5, 49, "日期: 2025-05-08");
  u8g2_DrawStr(u8g2, 5, 61, "按任意键返回");
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 处理MP3模式的按键
 */
static void handle_mp3_keys(KeyCode key) 
{
  // MP3模式下各界面的按键处理
  switch (ui_get_current_screen()) {
    case SCREEN_PLAYER:
      if (key == KEY_PREV) {
        prev_track();
      } else if (key == KEY_NEXT) {
        next_track();
      } else if (key == KEY_SELECT) {
        if (player_is_playing()) {
          if (player_is_paused()) {
            player_resume();
          } else {
            player_pause();
          }
        } else if (g_cur_index >= 0) {
          player_start();
        }
      } else if (key == KEY_BACK) {
        // 返回主菜单
        main_menu_set_mode(APP_MODE_MENU);
      }
      break;
      
    case SCREEN_BROWSER:
      // 浏览器界面按键处理在ui_screens.cpp中已实现
      break;
      
    case SCREEN_SETTINGS:
      // 设置界面按键处理在ui_screens.cpp中已实现
      break;
      
    case SCREEN_LOCKSCREEN:
      // 锁屏界面，任意键解锁
      if (key != KEY_NONE) {
        ui_set_screen(SCREEN_PLAYER);
      }
      break;
      
    default:
      break;
  }
}

/**
 * 处理GNSS模式的按键
 */
static void handle_gnss_keys(KeyCode key) 
{
  // GNSS模式下各界面的按键处理
  GnssScreen current = gnss_get_current_screen();
  
  switch (current) {
    case GNSS_SCREEN_ODOMETER:
      // 基本导航按键在gnss_screens.cpp中已处理
      break;
      
    case GNSS_SCREEN_TRACKING:
      if (key == KEY_SELECT) {
        // 开始/停止记录
        if (gnss_is_recording()) {
          gnss_stop_recording();
          // 自动保存轨迹
          save_current_track();
        } else {
          gnss_start_recording();
        }
      }
      break;
      
    case GNSS_SCREEN_SETTINGS:
      if (key == KEY_SELECT) {
        // 处理设置选项
        GnssUpdateRate current_rate = GNSS_RATE_1HZ;
        switch (g_settings_index) {
          case 0: // 更新频率
            // 循环切换频率
            if (current_rate == GNSS_RATE_1HZ) {
              current_rate = GNSS_RATE_5HZ;
            } else if (current_rate == GNSS_RATE_5HZ) {
              current_rate = GNSS_RATE_10HZ;
            } else {
              current_rate = GNSS_RATE_1HZ;
            }
            gnss_set_update_rate(current_rate);
            break;
            
          case 1: // 清除数据
            gnss_reset_trip();
            break;
            
          case 2: // 返回
            gnss_set_screen(GNSS_SCREEN_ODOMETER);
            break;
        }
      }
      break;
      
    case GNSS_SCREEN_ACCEL_TEST:
      if (key == KEY_SELECT) {
        // 重置加速测试 - 重置行程数据中的加速记录
        const TripData* trip = gnss_get_trip_data();
        if (trip) {
          // 在gnss_data.cpp中添加重置加速测试的接口
          gnss_reset_trip();
        }
      }
      break;
      
    default:
      break;
  }
  
  // 处理返回主菜单
  if (key == KEY_BACK && current == GNSS_SCREEN_ODOMETER) {
    main_menu_set_mode(APP_MODE_MENU);
  }
}

/**
 * 处理系统设置模式的按键
 */
static void handle_system_settings(KeyCode key) 
{
  const int settings_count = 4; // 屏幕超时、睡眠定时、格式化SD卡、返回
  
  switch (key) {
    case KEY_PREV:
      g_system_setting_index = (g_system_setting_index + settings_count - 1) % settings_count;
      break;
      
    case KEY_NEXT:
      g_system_setting_index = (g_system_setting_index + 1) % settings_count;
      break;
      
    case KEY_SELECT:
      switch (g_system_setting_index) {
        case SYS_SCREEN_TIMEOUT: {
          // 循环切换屏幕超时时间
          static const uint32_t timeouts[] = {10, 30, 60, 120, 300};
          static const int num_timeouts = sizeof(timeouts) / sizeof(timeouts[0]);
          
          // 找到当前值的索引
          int idx = 0;
          for (int i = 0; i < num_timeouts; i++) {
            if (timeouts[i] == g_screen_timeout_sec) {
              idx = i;
              break;
            }
          }
          
          // 切换到下一个值
          idx = (idx + 1) % num_timeouts;
          g_screen_timeout_sec = timeouts[idx];
          break;
        }
        
        case SYS_SLEEP_TIMER: {
          // 循环切换睡眠定时
          static const uint32_t times[] = {0, 15, 30, 45, 60, 90, 120};
          static const int num_times = sizeof(times) / sizeof(times[0]);
          
          // 找到当前值的索引
          int idx = 0;
          for (int i = 0; i < num_times; i++) {
            if (times[i] == g_sleep_minutes) {
              idx = i;
              break;
            }
          }
          
          // 切换到下一个值
          idx = (idx + 1) % num_times;
          set_sleep_timer(times[idx]);
          break;
        }
        
        case SYS_FORMAT_SD:
          // 格式化SD卡需要确认，这里简化处理
          printf("[系统] 格式化SD卡功能需要确认，暂未实现\n");
          break;
        
        case SYS_BACK:
          main_menu_set_mode(APP_MODE_MENU);
          break;
      }
      break;
      
    case KEY_BACK:
      main_menu_set_mode(APP_MODE_MENU);
      break;
  }
}

/**
 * GNSS线程
 */
static void* gnss_thread(void* arg) 
{
  // 初始化GNSS
  if (!gnss_init()) {
    printf("[GNSS] 初始化失败!\n");
    return nullptr;
  }
  
  // 启动GNSS
  gnss_start();
  
  // 设置默认更新率 (1Hz)
  gnss_set_update_rate(GNSS_RATE_1HZ);
  
  GnssPoint point;
  const TripData* trip_data;
  
  // GNSS处理循环
  while (g_running) {
    // 获取最新定位
    bool has_position = gnss_get_position(&point);
    
    // 获取行程数据
    trip_data = gnss_get_trip_data();
    
    // 更新界面（仅在GNSS模式下）
    if (main_menu_get_mode() == APP_MODE_GNSS) {
      // 根据当前界面绘制
      switch (gnss_get_current_screen()) {
        case GNSS_SCREEN_ODOMETER:
          gnss_draw_odometer(has_position ? &point : nullptr, trip_data, gnss_is_recording());
          break;
          
        case GNSS_SCREEN_COMPASS:
          gnss_draw_compass(has_position ? &point : nullptr);
          break;
          
        case GNSS_SCREEN_TRACKING:
          gnss_draw_tracking(gnss_is_recording(), 
                            gnss_is_recording() ? g_track_points.size() : 0, 
                            trip_data);
          break;
          
        case GNSS_SCREEN_TRIP_DATA:
          gnss_draw_trip_data(trip_data);
          break;
          
        case GNSS_SCREEN_SETTINGS:
          gnss_draw_settings(g_rate, g_settings_index);
          break;
          
        case GNSS_SCREEN_ACCEL_TEST:
          gnss_draw_accel_test(trip_data, 
                              has_position ? point.speed : 0.0f);
          break;
      }
    }
    
    // 延时，避免过高CPU使用率
    // 根据更新频率设置休眠时间
    usleep(50 * 1000); // 50ms
  }
  
  // 关闭GNSS
  gnss_stop();
  gnss_deinit();
  
  return nullptr;
}

/**
 * 主程序入口
 */
extern "C" int spresense_main(int argc, char* argv[]) 
{
  printf("[Spresense] 多功能系统启动...\n");
  
  // 初始化显示
  lcd_init();
  
  // 初始化主菜单
  main_menu_init();
  
  // 初始化MP3播放器
  player_init();
  
  // 初始化GNSS界面
  gnss_screens_init();
  
  // 扫描SD卡音乐文件
  std::vector<MusicFile> music_files;
  scan_music_directory("/sd/MUSIC", &music_files);
  
  // 创建GNSS线程
  pthread_t gnss_tid;
  pthread_create(&gnss_tid, nullptr, gnss_thread, nullptr);
  
  // 如果有音乐文件，预加载第一首
  if (!music_files.empty()) {
    player_load_file(music_files[0].filepath.c_str());
  }
  
  // 主循环
  while (g_running) {
    // 读取按键
    KeyCode key = (KeyCode)adc_to_key(analogRead(ANALOG_PIN));
    
    // 更新电池状态
    main_menu_update_battery();
    
    // 如果处于锁屏状态，任意键解锁
    if (main_menu_is_locked()) {
      if (key != KEY_NONE) {
        main_menu_lock_screen(false);
      }
    } else {
      // 根据当前模式处理按键
      AppMode mode = main_menu_get_mode();
      
      if (g_show_about) {
        // 关于界面，任意键返回
        if (key != KEY_NONE) {
          g_show_about = false;
        }
        
        // 绘制关于界面
        draw_about_screen();
      } else {
        switch (mode) {
          case APP_MODE_MENU:
            // 主菜单按键处理
            main_menu_handle_key(key);
            
            // 检查是否需要显示关于界面
            if (mode == APP_MODE_MENU && g_menu_index == 3 && key == KEY_SELECT) {
              g_show_about = true;
            } else {
              // 绘制主菜单
              main_menu_draw(g_menu_index);
            }
            break;
            
          case APP_MODE_MP3:
            // MP3模式按键处理
            handle_mp3_keys(key);
            
            // 更新MP3界面
            if (player_is_playing()) {
              uint32_t pos = player_get_position_ms();
              
              // 绘制播放界面
              ui_draw_player_screen(g_music_files[g_cur_index], pos, 
                                  player_get_volume(), player_get_loop_mode(), 
                                  EQ_CUSTOM, g_battery_percent, g_battery_charging);
            }
            break;
            
          case APP_MODE_GNSS:
            // GNSS模式按键处理
            handle_gnss_keys(key);
            // 注意：GNSS界面在GNSS线程中更新
            break;
            
          case APP_MODE_SYSTEM:
            // 系统设置模式按键处理
            handle_system_settings(key);
            
            // 绘制系统设置界面
            draw_system_settings(g_system_setting_index);
            break;
        }
      }
    }
    
    // 检查自动锁屏
    check_auto_lock();
    
    // 短暂休眠
    usleep(50 * 1000); // 50ms
  }
  
  // 等待GNSS线程结束
  pthread_join(gnss_tid, nullptr);
  
  // 清理资源
  player_deinit();
  
  printf("[Spresense] 多功能系统已退出\n");
  return 0;
}

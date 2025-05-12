/****************************************************************************
 * ui_screens.h
 * 
 * 界面显示模块：管理各种UI界面的绘制
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include "file_system.h"

// 应用界面状态
enum AppScreen {
  SCREEN_PLAYER,     // 播放界面
  SCREEN_BROWSER,    // 文件浏览器
  SCREEN_SETTINGS,   // 设置界面
  SCREEN_LOCKSCREEN, // 锁屏界面
  SCREEN_EQ,         // 均衡器设置
  SCREEN_LYRICS,     // 歌词显示
  SCREEN_BACKLIGHT   // 背光设置
};

// 界面初始化
void ui_init();

// 锁屏界面
void ui_draw_lockscreen(int battery_percent, bool charging, 
                        const char* current_title, bool is_playing);

// 播放界面
void ui_draw_player_screen(const MusicFile& file, uint32_t position_ms, 
                          uint8_t volume, LoopMode loop_mode, EQPreset eq_mode,
                          int battery_percent, bool charging);

// 歌词显示
void ui_draw_lyrics_screen(const std::vector<LrcLine>& lyrics, 
                           uint32_t current_ms, const char* title);

// 文件浏览器
void ui_draw_browser(const std::vector<MusicFile>& files, 
                     size_t current_index, size_t start_index);

// 系统设置界面
void ui_draw_settings(int selected_item, LoopMode loop_mode, 
                     EQPreset eq_mode, uint32_t sleep_minutes, bool include_backlight = true);

// EQ调整界面
void ui_draw_eq_screen(const int8_t* eq_bands, int num_bands, int selected_band);

// SD卡信息界面
void ui_draw_sd_info(const SDCardInfo& sd_info);

// 背光设置界面
void ui_draw_backlight_settings(int selected_item, uint8_t brightness, uint16_t timeout_seconds);

// 屏幕刷新
void ui_update();

// 获取当前界面
AppScreen ui_get_current_screen();

// 设置当前界面
void ui_set_screen(AppScreen screen);

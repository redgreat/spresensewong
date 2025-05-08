/****************************************************************************
 * ui_screens.cpp
 * 
 * 界面显示模块实现
 * ***************************************************************************/
#include "ui_screens.h"
#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 当前界面 */
static AppScreen g_current_screen = SCREEN_PLAYER;

/**
 * 界面初始化
 */
void ui_init()
{
  /* 初始化LCD */
  lcd_init();
  g_current_screen = SCREEN_PLAYER;
}

/**
 * 格式化时间
 */
static void format_time(uint32_t ms, char* buf, size_t buf_size)
{
  uint32_t total_seconds = ms / 1000;
  uint32_t minutes = total_seconds / 60;
  uint32_t seconds = total_seconds % 60;
  
  snprintf(buf, buf_size, "%02u:%02u", minutes, seconds);
}

/**
 * 格式化当前时间
 */
static void format_current_time(char* buf, size_t buf_size, bool include_date = false)
{
  time_t now = time(nullptr);
  struct tm* tm_info = localtime(&now);
  
  if (include_date) {
    snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
  } else {
    snprintf(buf, buf_size, "%02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
  }
}

/**
 * 获取循环模式图标
 */
static const char* get_loop_mode_icon(LoopMode mode)
{
  switch (mode) {
    case LOOP_SEQUENTIAL:  return "→";
    case LOOP_REPEAT_ONE:  return "⟳1";
    case LOOP_SHUFFLE:     return "⤮";
    default:               return "→";
  }
}

/**
 * 获取EQ模式名称
 */
static const char* get_eq_mode_name(EQPreset eq)
{
  switch (eq) {
    case EQ_FLAT:          return "平直";
    case EQ_BASS_BOOST:    return "低音增强";
    case EQ_TREBLE_BOOST:  return "高音增强";
    case EQ_CUSTOM:        return "自定义";
    default:               return "平直";
  }
}

/**
 * 绘制锁屏界面
 */
void ui_draw_lockscreen(int battery_percent, bool charging, 
                       const char* current_title, bool is_playing)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 显示当前时间和日期 */
  char time_buf[32];
  format_current_time(time_buf, sizeof(time_buf), true);
  
  u8g2_SetFont(u8g2, u8g2_font_9x15_tr);
  u8g2_DrawStr(u8g2, 5, 20, time_buf);
  
  /* 绘制电池状态 */
  draw_battery_icon(56, 34, battery_percent, charging);
  
  /* 显示歌曲信息 */
  if (current_title && *current_title) {
    u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(u8g2, 5, 48, current_title);
    
    /* 如果正在播放，显示播放图标 */
    if (is_playing) {
      u8g2_DrawTriangle(u8g2, 5, 54, 5, 60, 10, 57);
    } else {
      /* 暂停图标 */
      u8g2_DrawBox(u8g2, 5, 54, 2, 6);
      u8g2_DrawBox(u8g2, 9, 54, 2, 6);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制播放界面
 */
void ui_draw_player_screen(const MusicFile& file, uint32_t position_ms, 
                         uint8_t volume, LoopMode loop_mode, EQPreset eq_mode,
                         int battery_percent, bool charging)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 顶部信息栏 */
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  
  char header[32];
  snprintf(header, sizeof(header), "%s | %s", 
           get_loop_mode_icon(loop_mode), 
           get_eq_mode_name(eq_mode));
           
  u8g2_DrawStr(u8g2, 0, 8, header);
  
  /* 电池图标 */
  draw_battery_icon(110, 0, battery_percent, charging);
  
  /* 歌曲标题 */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  std::string title = file.metadata.title.empty() ? file.filename : file.metadata.title;
  u8g2_DrawStr(u8g2, 0, 22, title.c_str());
  
  /* 艺术家 */
  if (!file.metadata.artist.empty()) {
    u8g2_DrawStr(u8g2, 0, 34, file.metadata.artist.c_str());
  }
  
  /* 进度条 */
  draw_progress_bar(0, 40, 128, 5, position_ms, file.metadata.duration_ms);
  
  /* 时间信息 */
  char time_buf[16];
  format_time(position_ms, time_buf, sizeof(time_buf));
  u8g2_DrawStr(u8g2, 0, 54, time_buf);
  
  format_time(file.metadata.duration_ms, time_buf, sizeof(time_buf));
  u8g2_DrawStr(u8g2, 100, 54, time_buf);
  
  /* 音量指示 */
  draw_volume_indicator(80, 47, 30, 8, volume);
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制歌词显示
 */
void ui_draw_lyrics_screen(const std::vector<LrcLine>& lyrics, 
                          uint32_t current_ms, const char* title)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 显示标题 */
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, 0, 8, title ? title : "歌词显示");
  
  /* 查找当前播放歌词行 */
  int current_line = find_lyric_line(lyrics, current_ms);
  
  /* 如果没有歌词或还没到第一句 */
  if (lyrics.empty() || current_line < 0) {
    u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(u8g2, 20, 32, "暂无歌词");
    u8g2_SendBuffer(u8g2);
    return;
  }
  
  /* 显示多行歌词：
   * - 当前行在中间加亮显示
   * - 显示前后各1行(如果有)
   */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  
  /* 前一行歌词 */
  if (current_line > 0) {
    u8g2_DrawStr(u8g2, 10, 20, lyrics[current_line-1].text.c_str());
  }
  
  /* 当前行歌词(反色显示) */
  int y_pos = 32;
  int width = u8g2_GetStrWidth(u8g2, lyrics[current_line].text.c_str());
  int x_pos = (128 - width) / 2;
  if (x_pos < 0) x_pos = 0;
  
  u8g2_DrawBox(u8g2, x_pos-2, y_pos-10, width+4, 12);
  u8g2_SetDrawColor(u8g2, 0);
  u8g2_DrawStr(u8g2, x_pos, y_pos, lyrics[current_line].text.c_str());
  u8g2_SetDrawColor(u8g2, 1);
  
  /* 后一行歌词 */
  if (current_line < (int)lyrics.size() - 1) {
    u8g2_DrawStr(u8g2, 10, 44, lyrics[current_line+1].text.c_str());
  }
  
  /* 下一行歌词(如果有) */
  if (current_line < (int)lyrics.size() - 2) {
    u8g2_DrawStr(u8g2, 10, 56, lyrics[current_line+2].text.c_str());
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制文件浏览器
 */
void ui_draw_browser(const std::vector<MusicFile>& files, 
                    size_t current_index, size_t start_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 顶部标题 */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  char header[32];
  snprintf(header, sizeof(header), "音乐文件 (%zu)", files.size());
  u8g2_DrawStr(u8g2, 0, 10, header);
  
  /* 列表条目 */
  if (files.empty()) {
    u8g2_DrawStr(u8g2, 10, 30, "没有音乐文件");
  } else {
    /* 每页显示4个文件 */
    const size_t items_per_page = 4;
    
    /* 确保不超出范围 */
    if (start_index >= files.size()) {
      start_index = 0;
    }
    
    /* 显示文件列表 */
    size_t end_index = start_index + items_per_page;
    if (end_index > files.size()) {
      end_index = files.size();
    }
    
    for (size_t i = start_index; i < end_index; i++) {
      size_t y_pos = 22 + (i - start_index) * 12;
      
      /* 当前选中项反色显示 */
      if (i == current_index) {
        u8g2_DrawBox(u8g2, 0, y_pos-10, 128, 12);
        u8g2_SetDrawColor(u8g2, 0);
      }
      
      /* 限制显示长度 */
      std::string display_name = files[i].filename;
      if (display_name.length() > 20) {
        display_name = display_name.substr(0, 17) + "...";
      }
      
      u8g2_DrawStr(u8g2, 5, y_pos, display_name.c_str());
      
      /* 恢复颜色 */
      if (i == current_index) {
        u8g2_SetDrawColor(u8g2, 1);
      }
    }
    
    /* 如果有更多文件，显示滚动指示 */
    if (files.size() > items_per_page) {
      /* 计算滚动条 */
      int scroll_height = 64 / (files.size() / items_per_page + 1);
      int scroll_pos = 12 + (64 - scroll_height) * start_index / (files.size() - items_per_page);
      
      u8g2_DrawBox(u8g2, 126, scroll_pos, 2, scroll_height);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制设置界面
 */
void ui_draw_settings(int selected_item, LoopMode loop_mode, 
                    EQPreset eq_mode, uint32_t sleep_minutes)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 标题 */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 40, 10, "设置");
  
  /* 设置项目 */
  const char* settings[] = {
    "循环模式",
    "均衡器",
    "睡眠定时",
    "SD卡信息",
    "返回"
  };
  
  const int settings_count = sizeof(settings) / sizeof(settings[0]);
  
  for (int i = 0; i < settings_count; i++) {
    int y_pos = 24 + i * 12;
    
    /* 当前选中项反色显示 */
    if (i == selected_item) {
      u8g2_DrawBox(u8g2, 0, y_pos-10, 128, 12);
      u8g2_SetDrawColor(u8g2, 0);
    }
    
    u8g2_DrawStr(u8g2, 5, y_pos, settings[i]);
    
    /* 显示当前设置值 */
    const char* value = "";
    char buf[32] = {0};
    
    switch (i) {
      case 0: /* 循环模式 */
        value = get_loop_mode_icon(loop_mode);
        break;
        
      case 1: /* 均衡器 */
        value = get_eq_mode_name(eq_mode);
        break;
        
      case 2: /* 睡眠定时 */
        if (sleep_minutes > 0) {
          snprintf(buf, sizeof(buf), "%u分钟", sleep_minutes);
          value = buf;
        } else {
          value = "关闭";
        }
        break;
    }
    
    /* 在右侧显示值 */
    if (*value) {
      int x_pos = 128 - u8g2_GetStrWidth(u8g2, value) - 5;
      u8g2_DrawStr(u8g2, x_pos, y_pos, value);
    }
    
    /* 恢复颜色 */
    if (i == selected_item) {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制EQ调整界面
 */
void ui_draw_eq_screen(const int8_t* eq_bands, int num_bands, int selected_band)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 标题 */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 40, 10, "均衡器");
  
  /* 绘制EQ条 */
  const int max_bands = 8;  /* 最多支持8段EQ */
  const int bar_width = 10;
  const int bar_space = 4;
  const int bar_height = 40;
  const int zero_line = 40;  /* 0dB位置 */
  const int x_start = 10;
  
  for (int i = 0; i < num_bands && i < max_bands; i++) {
    int x_pos = x_start + i * (bar_width + bar_space);
    
    /* 绘制0dB线 */
    u8g2_DrawHLine(u8g2, x_pos, zero_line, bar_width);
    
    /* 绘制EQ条 */
    int value = eq_bands[i];
    
    /* 如果是当前选中的频段，用反色显示 */
    if (i == selected_band) {
      u8g2_DrawFrame(u8g2, x_pos-1, 20, bar_width+2, bar_height+2);
    }
    
    if (value >= 0) {
      /* 正增益 */
      int height = value * 2;  /* 每1dB对应2像素高度 */
      u8g2_DrawBox(u8g2, x_pos, zero_line - height, bar_width, height);
    } else {
      /* 负增益 */
      int height = -value * 2;
      u8g2_DrawBox(u8g2, x_pos, zero_line, bar_width, height);
    }
    
    /* 频段标签 */
    char label[8];
    switch (i) {
      case 0: strcpy(label, "63"); break;
      case 1: strcpy(label, "125"); break;
      case 2: strcpy(label, "250"); break;
      case 3: strcpy(label, "500"); break;
      case 4: strcpy(label, "1k"); break;
      case 5: strcpy(label, "2k"); break;
      case 6: strcpy(label, "4k"); break;
      case 7: strcpy(label, "8k"); break;
      default: label[0] = '\0'; break;
    }
    
    u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
    int label_width = u8g2_GetStrWidth(u8g2, label);
    u8g2_DrawStr(u8g2, x_pos + (bar_width - label_width) / 2, 62, label);
  }
  
  /* 显示当前选中频段的dB值 */
  if (selected_band >= 0 && selected_band < num_bands) {
    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%+ddB", eq_bands[selected_band]);
    
    u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
    int value_width = u8g2_GetStrWidth(u8g2, value_str);
    u8g2_DrawStr(u8g2, (128 - value_width) / 2, 15, value_str);
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制SD卡信息界面
 */
void ui_draw_sd_info(const SDCardInfo& sd_info)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  /* 标题 */
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 30, 10, "SD卡信息");
  
  /* 显示SD卡信息 */
  char buf[32];
  
  /* 总容量 */
  snprintf(buf, sizeof(buf), "总容量: %uMB", sd_info.total_mb);
  u8g2_DrawStr(u8g2, 5, 25, buf);
  
  /* 已用空间 */
  snprintf(buf, sizeof(buf), "已用: %uMB (%.1f%%)", 
           sd_info.used_mb, sd_info.used_percent);
  u8g2_DrawStr(u8g2, 5, 37, buf);
  
  /* 可用空间 */
  snprintf(buf, sizeof(buf), "可用: %uMB", sd_info.free_mb);
  u8g2_DrawStr(u8g2, 5, 49, buf);
  
  /* 绘制存储条 */
  draw_progress_bar(5, 54, 118, 8, sd_info.used_mb, sd_info.total_mb);
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 刷新屏幕
 */
void ui_update()
{
  u8g2_UpdateDisplay(get_display());
}

/**
 * 获取当前界面
 */
AppScreen ui_get_current_screen()
{
  return g_current_screen;
}

/**
 * 设置当前界面
 */
void ui_set_screen(AppScreen screen)
{
  g_current_screen = screen;
}

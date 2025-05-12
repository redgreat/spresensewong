/****************************************************************************
 * display.cpp
 * 
 * 显示模块实现
 * ***************************************************************************/
#include "display.h"
#include "common.h"

/* 全局U8g2实例 */
static u8g2_t g_u8g2;

/* LCD引脚配置 - 定义在common.h */
// #define LCD_SPI_CS   10
// #define LCD_SPI_A0    9
// #define LCD_SPI_RST   8

/* 背光控制 */
#define LCD_BACKLIGHT_PIN 6  // 背光控制引脚
#define LCD_BACKLIGHT_PWM 7  // PWM引脚，可能需要配置PWM模块

/* 背光设置 */
static uint8_t g_backlight_brightness = 5;  // 当前背光亮度(0-5)
static uint16_t g_backlight_timeout = 30;  // 背光自动关闭时间(秒)
static time_t g_last_backlight_activity = 0; // 最后背光活动时间
static bool g_backlight_on = true;          // 背光的状态

/**
 * LCD初始化
 */
void lcd_init() 
{
  /* 使用 180° 旋转以更符合视觉习惯 */
  u8g2_Setup_st7565_nhd_c12864_f(&g_u8g2, U8G2_R2, u8x8_byte_spi_hw_spi1, 
                                  u8x8_gpio_and_delay_nuttx);
  u8x8_SetPin_CS(&g_u8g2.u8x8, LCD_SPI_CS);
  u8x8_SetPin_DC(&g_u8g2.u8x8, LCD_SPI_A0);
  u8x8_SetPin_Reset(&g_u8g2.u8x8, LCD_SPI_RST);
  u8g2_InitDisplay(&g_u8g2);
  u8g2_SetPowerSave(&g_u8g2, 0);
  
  /* 默认设置英文字体 */
  u8g2_SetFont(&g_u8g2, u8g2_font_6x12_tr);
  
  u8g2_ClearBuffer(&g_u8g2);
  u8g2_SendBuffer(&g_u8g2);
}

/**
 * 清除屏幕
 */
void lcd_clear() 
{
  u8g2_ClearBuffer(&g_u8g2);
  u8g2_SendBuffer(&g_u8g2);
}

/**
 * 设置屏幕对比度
 */
void lcd_set_contrast(uint8_t contrast) 
{
  u8g2_SetContrast(&g_u8g2, contrast);
}

/**
 * 控制背光 (需要在硬件上支持)
 */
void lcd_backlight(bool on) 
{
  /* 使用GPIO控制背光开关 */
  g_backlight_on = on;
  
  if (on) {
    // 根据亮度调节PWM占空比
    int brightness_percent = (g_backlight_brightness * 100) / 5;
    // 这里这一部分需要使用Spresense的PWM API
    // 简化示例，假设有一个PWM控制函数
    // pwm_set_duty(LCD_BACKLIGHT_PWM, brightness_percent);
    printf("[背光] 开启，亮度: %d%%\n", brightness_percent);
  } else {
    // 关闭背光
    // pwm_set_duty(LCD_BACKLIGHT_PWM, 0);
    printf("[背光] 关闭\n");
  }
  
  // 更新最后活动时间
  g_last_backlight_activity = time(NULL);
}

/**
 * 绘制电池图标
 */
void draw_battery_icon(int x, int y, int percent, bool charging) 
{
  int width = 16;
  int height = 8;
  
  /* 电池外框 */
  u8g2_DrawFrame(&g_u8g2, x, y, width, height);
  u8g2_DrawBox(&g_u8g2, x+width, y+2, 2, height-4);
  
  /* 电量填充 */
  if (percent > 0) {
    int fill_width = (percent * (width-2) / 100);
    if (fill_width < 1) fill_width = 1;
    u8g2_DrawBox(&g_u8g2, x+1, y+1, fill_width, height-2);
  }
  
  /* 充电指示 */
  if (charging) {
    u8g2_DrawLine(&g_u8g2, x+5, y+height/2, x+8, y+1);
    u8g2_DrawLine(&g_u8g2, x+8, y+1, x+11, y+height/2);
    u8g2_DrawLine(&g_u8g2, x+11, y+height/2, x+8, y+height-1);
    u8g2_DrawLine(&g_u8g2, x+8, y+height-1, x+5, y+height/2);
  }
}

/**
 * 绘制进度条
 */
void draw_progress_bar(int x, int y, int width, int height, uint32_t pos, uint32_t total) 
{
  u8g2_DrawFrame(&g_u8g2, x, y, width, height);
  if (total > 0 && pos <= total) {
    int fill = pos * (width-2) / total;
    if (fill > 0)
      u8g2_DrawBox(&g_u8g2, x+1, y+1, fill, height-2);
  }
}

/**
 * 绘制音量指示器
 */
void draw_volume_indicator(int x, int y, int width, int height, int volume) 
{
  /* 音量范围应该是0-255 */
  int volume_percent = (volume * 100) / 255;
  
  /* 绘制外框 */
  u8g2_DrawFrame(&g_u8g2, x, y, width, height);
  
  /* 绘制音量级别 */
  int levels = 5; /* 分成5档 */
  int level_width = (width - 2) / levels;
  int active_levels = (volume_percent * levels) / 100;
  
  for (int i = 0; i < active_levels; i++) {
    int bar_height = (i+1) * height / levels;
    u8g2_DrawBox(&g_u8g2, x+1+i*level_width, y+height-bar_height,
                 level_width-1, bar_height-1);
  }
}

/**
 * 绘制页眉
 */
void draw_header(const char* title, int battery_percent, bool charging) 
{
  u8g2_SetFont(&g_u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(&g_u8g2, 0, 8, title);
  
  /* 绘制电池图标在右上角 */
  draw_battery_icon(108, 0, battery_percent, charging);
}

/**
 * 绘制页脚
 */
void draw_footer(const char* text) 
{
  u8g2_SetFont(&g_u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(&g_u8g2, 0, 63, text);
}

/**
 * 获取U8g2实例
 */
u8g2_t* get_display() 
{
  return &g_u8g2;
}

/**
 * 设置中文字体
 * 使用unifont字库的中文字体支持
 */
void lcd_set_chinese_font()
{
  u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);
}

/**
 * 恢复英文字体
 * 英文字体较小，显示更多内容
 */
void lcd_set_english_font()
{
  u8g2_SetFont(&g_u8g2, u8g2_font_6x12_tr);
}

/**
 * 绘制UTF-8编码的文本
 * 用于显示中文等多字节字符
 */
void lcd_draw_utf8(int x, int y, const char* utf8_text)
{
  u8g2_DrawUTF8(&g_u8g2, x, y, utf8_text);
}

/**
 * 设置背光亮度级别(0-5)
 */
void lcd_set_backlight_brightness(uint8_t level)
{
  // 确保在有效范围内
  if (level > 5) level = 5;
  
  g_backlight_brightness = level;
  
  // 如果背光当前是打开的，立即应用新亮度
  if (g_backlight_on) {
    lcd_backlight(true); // 重新调用会应用新的亮度设置
  }
  
  printf("[背光] 亮度级别已设置为: %d\n", level);
}

/**
 * 设置背光自动熄灭时间(秒)
 */
void lcd_set_backlight_timeout(uint16_t seconds)
{
  g_backlight_timeout = seconds;
  printf("[背光] 自动熄灭时间已设置为: %d秒\n", seconds);
}

/**
 * 获取当前背光亮度
 */
uint8_t lcd_get_backlight_brightness()
{
  return g_backlight_brightness;
}

/**
 * 获取当前背光超时时间
 */
uint16_t lcd_get_backlight_timeout()
{
  return g_backlight_timeout;
}

/**
 * 更新背光状态，检查超时
 */
void lcd_update_backlight()
{
  // 如果已配置超时时间且当前背光是打开的
  if (g_backlight_timeout > 0 && g_backlight_on) {
    time_t now = time(NULL);
    time_t idle_time = now - g_last_backlight_activity;
    
    // 超时自动关闭背光
    if (idle_time >= g_backlight_timeout) {
      lcd_backlight(false);
    }
  }
}

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
  /* 如果有GPIO控制背光，可在此实现 */
  /* 使用板级GPIO控制或PWM模块 */
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

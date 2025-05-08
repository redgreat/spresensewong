/****************************************************************************
 * gnss_screens.cpp
 * 
 * GNSS 界面模块实现：显示码表、指南针、轨迹分析等界面
 * ***************************************************************************/
#include "gnss_screens.h"
#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// 全局变量
static GnssScreen g_current_screen = GNSS_SCREEN_ODOMETER;
static ScreenOrientation g_orientation = ORIENTATION_0;
static int g_settings_index = 0;  // 设置菜单当前选项

/**
 * 初始化GNSS界面
 */
void gnss_screens_init()
{
  // 初始化LCD
  // 注意：从主程序调用过display.h中的lcd_init()后就不需要再次初始化了
  g_current_screen = GNSS_SCREEN_ODOMETER;
}

/**
 * 设置屏幕方向
 */
void gnss_screens_set_orientation(ScreenOrientation orientation)
{
  g_orientation = orientation;
  
  // 设置LCD旋转方向
  u8g2_t* u8g2 = get_display();
  
  switch (orientation) {
    case ORIENTATION_0:
      u8g2_SetDisplayRotation(u8g2, U8G2_R0);
      break;
    case ORIENTATION_90:
      u8g2_SetDisplayRotation(u8g2, U8G2_R1);
      break;
    case ORIENTATION_180:
      u8g2_SetDisplayRotation(u8g2, U8G2_R2);
      break;
    case ORIENTATION_270:
      u8g2_SetDisplayRotation(u8g2, U8G2_R3);
      break;
  }
}

/**
 * 绘制状态栏
 */
static void draw_status_bar(const GnssPoint* point, bool recording)
{
  u8g2_t* u8g2 = get_display();
  
  // 清空顶部状态栏区域
  u8g2_SetDrawColor(u8g2, 0);
  u8g2_DrawBox(u8g2, 0, 0, 128, 10);
  u8g2_SetDrawColor(u8g2, 1);
  
  // 绘制底部边框线
  u8g2_DrawHLine(u8g2, 0, 10, 128);
  
  // 显示卫星数
  char sat_info[16];
  snprintf(sat_info, sizeof(sat_info), "SAT:%d", 
           point ? point->num_satellites : 0);
  
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, 0, 8, sat_info);
  
  // 显示定位状态
  const char* fix_type = "无定位";
  if (point) {
    switch (point->fix_type) {
      case FIX_2D: fix_type = "2D定位"; break;
      case FIX_3D: fix_type = "3D定位"; break;
      default: fix_type = "无定位"; break;
    }
  }
  u8g2_DrawStr(u8g2, 40, 8, fix_type);
  
  // 显示记录状态
  if (recording) {
    u8g2_DrawDisc(u8g2, 120, 5, 3);  // 小圆点表示记录中
  }
}

/**
 * 绘制方向箭头
 */
static void draw_direction_arrow(int x, int y, float course, int size)
{
  u8g2_t* u8g2 = get_display();
  
  // 计算箭头的三个点坐标
  float angle_rad = course * M_PI / 180.0;
  
  int x1 = x + size * sin(angle_rad);
  int y1 = y - size * cos(angle_rad);
  
  int x2 = x + 0.5 * size * sin(angle_rad + 2.5);
  int y2 = y - 0.5 * size * cos(angle_rad + 2.5);
  
  int x3 = x + 0.5 * size * sin(angle_rad - 2.5);
  int y3 = y - 0.5 * size * cos(angle_rad - 2.5);
  
  // 绘制三角形箭头
  u8g2_DrawTriangle(u8g2, x1, y1, x2, y2, x3, y3);
  
  // 绘制中心点
  u8g2_DrawDisc(u8g2, x, y, 2);
}

/**
 * 格式化时间为字符串
 */
static void format_time(uint32_t seconds, char* buf, size_t buf_size)
{
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  
  snprintf(buf, buf_size, "%02u:%02u:%02u", hours, minutes, secs);
}

/**
 * 格式化距离为字符串，自动选择km或m单位
 */
static void format_distance(double distance_m, char* buf, size_t buf_size)
{
  if (distance_m >= 1000.0) {
    snprintf(buf, buf_size, "%.2f km", distance_m / 1000.0);
  } else {
    snprintf(buf, buf_size, "%.0f m", distance_m);
  }
}

/**
 * 绘制码表界面
 */
void gnss_draw_odometer(const GnssPoint* point, const TripData* trip, bool recording)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 绘制状态栏
  draw_status_bar(point, recording);
  
  // 显示速度(大号)
  char speed_buf[32];
  float speed_kmh = point && point->fix_type != FIX_NONE ? point->speed * 3.6f : 0.0f;
  snprintf(speed_buf, sizeof(speed_buf), "%.1f", speed_kmh);
  
  u8g2_SetFont(u8g2, u8g2_font_inb19_mr);  // 大字体
  u8g2_DrawStr(u8g2, 10, 40, speed_buf);
  
  // 显示公里每小时单位
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 100, 40, "km/h");
  
  // 显示距离
  char dist_buf[32];
  format_distance(trip ? trip->total_distance : 0.0, dist_buf, sizeof(dist_buf));
  
  u8g2_DrawStr(u8g2, 5, 54, "距离:");
  u8g2_DrawStr(u8g2, 45, 54, dist_buf);
  
  // 显示行驶时间
  char time_buf[32];
  format_time(trip ? trip->duration : 0, time_buf, sizeof(time_buf));
  
  u8g2_DrawStr(u8g2, 5, 64, "时间:");
  u8g2_DrawStr(u8g2, 45, 64, time_buf);
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制指南针界面
 */
void gnss_draw_compass(const GnssPoint* point)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 绘制状态栏
  draw_status_bar(point, false);
  
  // 显示经纬度
  char lat_buf[32], lon_buf[32];
  if (point && point->fix_type != FIX_NONE) {
    snprintf(lat_buf, sizeof(lat_buf), "纬度: %.6f°", point->latitude);
    snprintf(lon_buf, sizeof(lon_buf), "经度: %.6f°", point->longitude);
  } else {
    snprintf(lat_buf, sizeof(lat_buf), "纬度: ---.------°");
    snprintf(lon_buf, sizeof(lon_buf), "经度: ---.------°");
  }
  
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, 0, 20, lat_buf);
  u8g2_DrawStr(u8g2, 0, 30, lon_buf);
  
  // 显示海拔
  char alt_buf[32];
  if (point && point->fix_type == FIX_3D) {
    snprintf(alt_buf, sizeof(alt_buf), "海拔: %.1f m", point->altitude);
  } else {
    snprintf(alt_buf, sizeof(alt_buf), "海拔: ---.-- m");
  }
  u8g2_DrawStr(u8g2, 0, 40, alt_buf);
  
  // 绘制指南针
  u8g2_DrawCircle(u8g2, 96, 32, 20, U8G2_DRAW_ALL);
  
  // 方向标记
  u8g2_DrawStr(u8g2, 96, 10, "N");
  u8g2_DrawStr(u8g2, 96, 58, "S");
  u8g2_DrawStr(u8g2, 78, 32, "W");
  u8g2_DrawStr(u8g2, 115, 32, "E");
  
  // 绘制方向箭头
  if (point && point->fix_type != FIX_NONE) {
    draw_direction_arrow(96, 32, point->course, 16);
    
    // 显示方向角度
    char course_buf[16];
    snprintf(course_buf, sizeof(course_buf), "%d°", (int)point->course);
    u8g2_DrawStr(u8g2, 88, 50, course_buf);
  }
  
  // 显示时间(GNSS授时)
  struct tm tm_data;
  gnss_get_date_time(&tm_data);
  
  char time_buf[32];
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
           tm_data.tm_hour, tm_data.tm_min, tm_data.tm_sec);
  
  u8g2_DrawStr(u8g2, 80, 64, time_buf);
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制轨迹记录控制界面
 */
void gnss_draw_tracking(bool recording, uint32_t points_count, const TripData* trip)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 35, 10, "轨迹记录");
  
  // 当前状态
  u8g2_DrawStr(u8g2, 5, 25, "状态:");
  u8g2_DrawStr(u8g2, 50, 25, recording ? "记录中" : "已停止");
  
  // 轨迹点数
  char points_buf[32];
  snprintf(points_buf, sizeof(points_buf), "%u 个点", points_count);
  
  u8g2_DrawStr(u8g2, 5, 35, "数据点:");
  u8g2_DrawStr(u8g2, 50, 35, points_buf);
  
  // 总距离
  char dist_buf[32];
  format_distance(trip ? trip->total_distance : 0.0, dist_buf, sizeof(dist_buf));
  
  u8g2_DrawStr(u8g2, 5, 45, "总距离:");
  u8g2_DrawStr(u8g2, 50, 45, dist_buf);
  
  // 操作提示
  if (recording) {
    u8g2_DrawStr(u8g2, 15, 60, "按确认键停止记录");
  } else {
    u8g2_DrawStr(u8g2, 15, 55, "按确认键开始记录");
    u8g2_DrawStr(u8g2, 15, 64, "按后退键保存轨迹");
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制行程数据分析界面
 */
void gnss_draw_trip_data(const TripData* trip)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 35, 10, "行程数据");
  
  if (!trip) {
    u8g2_DrawStr(u8g2, 25, 35, "无行程数据");
    u8g2_SendBuffer(u8g2);
    return;
  }
  
  // 行程距离
  char dist_buf[32];
  format_distance(trip->total_distance, dist_buf, sizeof(dist_buf));
  
  u8g2_DrawStr(u8g2, 0, 22, "距离:");
  u8g2_DrawStr(u8g2, 40, 22, dist_buf);
  
  // 最大速度
  char max_speed_buf[32];
  snprintf(max_speed_buf, sizeof(max_speed_buf), "%.1f km/h", trip->max_speed * 3.6f);
  
  u8g2_DrawStr(u8g2, 0, 32, "最高:");
  u8g2_DrawStr(u8g2, 40, 32, max_speed_buf);
  
  // 平均速度
  char avg_speed_buf[32];
  snprintf(avg_speed_buf, sizeof(avg_speed_buf), "%.1f km/h", trip->avg_speed * 3.6f);
  
  u8g2_DrawStr(u8g2, 0, 42, "平均:");
  u8g2_DrawStr(u8g2, 40, 42, avg_speed_buf);
  
  // 行程时间
  char time_buf[32];
  format_time(trip->duration, time_buf, sizeof(time_buf));
  
  u8g2_DrawStr(u8g2, 0, 52, "用时:");
  u8g2_DrawStr(u8g2, 40, 52, time_buf);
  
  // 百米加速
  char accel_buf[32];
  if (trip->has_0_100_time) {
    snprintf(accel_buf, sizeof(accel_buf), "%.1f 秒", trip->time_0_100);
  } else {
    strcpy(accel_buf, "无数据");
  }
  
  u8g2_DrawStr(u8g2, 0, 62, "0-100:");
  u8g2_DrawStr(u8g2, 40, 62, accel_buf);
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制GNSS设置界面
 */
void gnss_draw_settings(GnssUpdateRate rate, int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 35, 10, "GNSS设置");
  
  // 设置项
  const char* items[] = {
    "更新频率",
    "清除数据",
    "返回"
  };
  const int items_count = sizeof(items) / sizeof(items[0]);
  
  for (int i = 0; i < items_count; i++) {
    int y_pos = 25 + i * 12;
    
    // 如果是当前选中项，反色显示
    if (i == selected_index) {
      u8g2_DrawBox(u8g2, 0, y_pos-10, 128, 12);
      u8g2_SetDrawColor(u8g2, 0);
    }
    
    u8g2_DrawStr(u8g2, 5, y_pos, items[i]);
    
    // 显示当前设置值
    if (i == 0) {
      // 更新频率
      const char* rate_str = "1Hz";
      if (rate == GNSS_RATE_5HZ) rate_str = "5Hz";
      else if (rate == GNSS_RATE_10HZ) rate_str = "10Hz";
      
      u8g2_DrawStr(u8g2, 90, y_pos, rate_str);
    }
    
    // 恢复颜色
    if (i == selected_index) {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制百米加速测试界面
 */
void gnss_draw_accel_test(const TripData* trip, float current_speed)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 20, 10, "百米加速测试");
  
  // 当前速度
  char speed_buf[32];
  snprintf(speed_buf, sizeof(speed_buf), "%.1f km/h", current_speed * 3.6f);
  
  u8g2_SetFont(u8g2, u8g2_font_inb19_mr);  // 大字体
  int width = u8g2_GetStrWidth(u8g2, speed_buf);
  u8g2_DrawStr(u8g2, (128 - width) / 2, 38, speed_buf);
  
  // 0-100 km/h 时间
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  if (trip && trip->has_0_100_time) {
    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "0-100 km/h: %.1f秒", trip->time_0_100);
    u8g2_DrawStr(u8g2, 15, 55, time_buf);
  } else {
    u8g2_DrawStr(u8g2, 15, 55, "等待开始加速测试...");
  }
  
  // 操作提示
  u8g2_DrawStr(u8g2, 5, 64, "从停止状态加速至100km/h");
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 获取当前界面
 */
GnssScreen gnss_get_current_screen()
{
  return g_current_screen;
}

/**
 * 设置当前界面
 */
void gnss_set_screen(GnssScreen screen)
{
  g_current_screen = screen;
}

/**
 * 处理按键输入
 */
void gnss_handle_key(uint8_t key)
{
  switch (g_current_screen) {
    case GNSS_SCREEN_ODOMETER:
      // 码表界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_COMPASS;
      } else if (key == KEY_PREV) {
        // 上一界面
        g_current_screen = GNSS_SCREEN_ACCEL_TEST;
      } else if (key == KEY_SELECT) {
        // 进入轨迹记录控制
        g_current_screen = GNSS_SCREEN_TRACKING;
      } else if (key == KEY_BACK) {
        // 进入设置
        g_current_screen = GNSS_SCREEN_SETTINGS;
        g_settings_index = 0;
      }
      break;
      
    case GNSS_SCREEN_COMPASS:
      // 指南针界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_TRACKING;
      } else if (key == KEY_PREV) {
        // 上一界面
        g_current_screen = GNSS_SCREEN_ODOMETER;
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
      
    case GNSS_SCREEN_TRACKING:
      // 轨迹记录控制界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_TRIP_DATA;
      } else if (key == KEY_PREV) {
        // 上一界面
        g_current_screen = GNSS_SCREEN_COMPASS;
      } else if (key == KEY_SELECT) {
        // 开始/停止记录 - 在主程序中处理
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
      
    case GNSS_SCREEN_TRIP_DATA:
      // 行程数据分析界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_ACCEL_TEST;
      } else if (key == KEY_PREV) {
        // 上一界面
        g_current_screen = GNSS_SCREEN_TRACKING;
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
      
    case GNSS_SCREEN_SETTINGS:
      // GNSS设置界面按键处理
      if (key == KEY_NEXT) {
        // 下一选项
        g_settings_index = (g_settings_index + 1) % 3;
      } else if (key == KEY_PREV) {
        // 上一选项
        g_settings_index = (g_settings_index + 2) % 3;
      } else if (key == KEY_SELECT) {
        // 选择选项 - 在主程序中处理
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
      
    case GNSS_SCREEN_ACCEL_TEST:
      // 百米加速测试界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_ODOMETER;
      } else if (key == KEY_PREV) {
        // 上一界面
        g_current_screen = GNSS_SCREEN_TRIP_DATA;
      } else if (key == KEY_SELECT) {
        // 重置加速测试 - 在主程序中处理
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
  }
}

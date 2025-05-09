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
static int g_segment_index = 0;   // 分段设置菜单当前选项
static int g_history_index = 0;   // 历史分段列表当前选项
static int g_detail_index = 0;    // 分段详情当前选项
static int g_accel_history_index = 0; // 加速度历史记录索引
static std::vector<std::string> g_history_files; // 历史分段文件列表
static std::vector<SegmentData> g_loaded_segments; // 已加载的分段数据

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
  
  // 显示当前分段信息
  if (recording && trip && trip->segment_count > 0) {
    char seg_text[16];
    sprintf(seg_text, "分段:%d", trip->segment_count);
    u8g2_DrawStr(u8g2, 85, 54, seg_text);
    
    // 截取分段提示
    u8g2_DrawStr(u8g2, 85, 64, "长按S:分段");
  }
  
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
    "分段设置",
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
  
  // 显示操作提示
  u8g2_DrawStr(u8g2, 5, 62, "上下键:选择  确认:进入");
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制分段设置界面
 */
void gnss_draw_segment_settings(bool enabled, SegmentTimeOption option, uint32_t custom_time, int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 35, 10, "分段设置");
  u8g2_DrawHLine(u8g2, 0, 12, 128);
  
  // 分段启用选项
  if (selected_index == 0) {
    u8g2_DrawBox(u8g2, 0, 17, 128, 13);
    u8g2_SetDrawColor(u8g2, 0);
  }
  u8g2_DrawStr(u8g2, 5, 27, "分段功能:");
  u8g2_DrawStr(u8g2, 65, 27, enabled ? "已启用" : "已禁用");
  u8g2_SetDrawColor(u8g2, 1);
  
  // 分段时间选项
  const char* option_text = "";
  switch (option) {
    case SEGMENT_TIME_1MIN:
      option_text = "1分钟";
      break;
    case SEGMENT_TIME_5MIN:
      option_text = "5分钟";
      break;
    case SEGMENT_TIME_10MIN:
      option_text = "10分钟";
      break;
    case SEGMENT_TIME_30MIN:
      option_text = "30分钟";
      break;
    case SEGMENT_TIME_CUSTOM:
      option_text = "自定义";
      break;
  }
  
  if (enabled) {
    if (selected_index == 1) {
      u8g2_DrawBox(u8g2, 0, 31, 128, 13);
      u8g2_SetDrawColor(u8g2, 0);
    }
    u8g2_DrawStr(u8g2, 5, 41, "分段时间:");
    u8g2_DrawStr(u8g2, 65, 41, option_text);
    u8g2_SetDrawColor(u8g2, 1);
    
    // 如果是自定义选项，显示自定义时间
    if (option == SEGMENT_TIME_CUSTOM) {
      if (selected_index == 2) {
        u8g2_DrawBox(u8g2, 0, 45, 128, 13);
        u8g2_SetDrawColor(u8g2, 0);
      }
      
      char custom_text[32];
      int minutes = custom_time / 60;
      sprintf(custom_text, "%d分钟", minutes);
      
      u8g2_DrawStr(u8g2, 5, 55, "自定义时间:");
      u8g2_DrawStr(u8g2, 75, 55, custom_text);
      u8g2_SetDrawColor(u8g2, 1);
    } else {
      // 显示说明文本
      u8g2_DrawStr(u8g2, 5, 55, "无GPS定位超时后将分段");
    }
  } else {
    // 如果分段被禁用，显示提示信息
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawStr(u8g2, 5, 41, "当前分段功能已关闭");
    u8g2_SetDrawColor(u8g2, 1);
  }
  
  // 显示当前分段数量
  char segment_count_text[32];
  sprintf(segment_count_text, "%d", gnss_get_segment_count());
  u8g2_DrawStr(u8g2, 55, 15, segment_count_text);

  // 显示操作提示
  u8g2_DrawStr(u8g2, 10, 64, "上下键:选择 左右键:调整");
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制百米加速测试界面
 */
void gnss_draw_accel_test(const TripData* trip, float current_speed)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
{{ ... }}
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
 * 绘制历史分段列表界面
 */
void gnss_draw_history_list(const std::vector<std::string>& files, int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 5, 10, "历史分段记录");
  
  // 绘制水平线
  u8g2_DrawHLine(u8g2, 0, 12, 128);
  
  // 显示历史文件列表
  int max_display = 4; // 最多显示行数
  int start_index = 0;
  
  // 计算显示起始位置，确保选中项可见
  if (selected_index >= max_display) {
    start_index = selected_index - max_display + 1;
  }
  
  // 每个文件显示一行
  for (int i = 0; i < max_display && start_index + i < files.size(); i++) {
    int y_pos = 25 + i * 12;
    int index = start_index + i;
    
    // 从文件名中提取日期时间
    std::string filename = files[index];
    size_t start_pos = filename.rfind("/") + 1;
    size_t end_pos = filename.rfind(".json");
    std::string display_name = filename.substr(start_pos, end_pos - start_pos);
    
    // 简化显示时间格式 (去掉segment_前缀)
    if (display_name.find("segment_") == 0) {
      display_name = display_name.substr(8);
    }
    
    // 集合年月日和时分秒一起显示
    std::string date_part = display_name.substr(0, 8);
    std::string time_part = display_name.substr(9);
    
    char display_buf[32];
    snprintf(display_buf, sizeof(display_buf), "%s %s", date_part.c_str(), time_part.c_str());
    
    // 选中项再黑显示
    if (index == selected_index) {
      u8g2_DrawBox(u8g2, 0, y_pos - 10, 128, 12);
      u8g2_SetDrawColor(u8g2, 0);
    }
    
    u8g2_DrawStr(u8g2, 5, y_pos, display_buf);
    
    // 恢复默认颜色
    if (index == selected_index) {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }
  
  // 显示导航提示
  u8g2_DrawStr(u8g2, 5, 64, "新前启：查看详情");
  
  u8g2_SendBuffer(u8g2);
}

/**
 * 绘制分段详情界面
 */
void gnss_draw_segment_detail(const std::vector<SegmentData>& segments, int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  if (segments.empty() || selected_index >= segments.size()) {
    u8g2_DrawStr(u8g2, 5, 32, "没有分段数据");
    u8g2_SendBuffer(u8g2);
    return;
  }
  
  // 获取选中的分段
  const SegmentData& segment = segments[selected_index];
  
  // 标题
  char title[32];
  snprintf(title, sizeof(title), "分段 #%d 详情", selected_index + 1);
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 5, 10, title);
  
  // 绘制水平线
  u8g2_DrawHLine(u8g2, 0, 12, 128);
  
  // 时间信息
  u8g2_DrawStr(u8g2, 5, 24, segment.start_time_str);
  
  // 分段各项数据
  char buf[64];
  
  // 总时长
  snprintf(buf, sizeof(buf), "总时长: %02u:%02u:%02u", 
           segment.duration / 3600, (segment.duration % 3600) / 60, segment.duration % 60);
  u8g2_DrawStr(u8g2, 5, 35, buf);
  
  // 运动时长
  snprintf(buf, sizeof(buf), "运动: %02u:%02u:%02u", 
           segment.moving_time / 3600, (segment.moving_time % 3600) / 60, segment.moving_time % 60);
  u8g2_DrawStr(u8g2, 5, 46, buf);
  
  // 停留时长
  snprintf(buf, sizeof(buf), "停留: %02u:%02u:%02u", 
           segment.idle_time / 3600, (segment.idle_time % 3600) / 60, segment.idle_time % 60);
  u8g2_DrawStr(u8g2, 5, 57, buf);
  
  // 距离和速度
  double distance_km = segment.distance / 1000.0;
  float avg_speed_kmh = segment.avg_speed * 3.6f;
  float moving_avg_speed_kmh = segment.moving_avg_speed * 3.6f;
  
  snprintf(buf, sizeof(buf), "距离: %.2f km", distance_km);
  u8g2_DrawStr(u8g2, 70, 35, buf);
  
  snprintf(buf, sizeof(buf), "平均: %.1f km/h", moving_avg_speed_kmh);
  u8g2_DrawStr(u8g2, 70, 46, buf);
  
  // 导航提示
  char nav_hint[32];
  snprintf(nav_hint, sizeof(nav_hint), "< %d/%d >", selected_index + 1, (int)segments.size());
  u8g2_DrawStr(u8g2, 85, 57, nav_hint);
  
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
  static time_t press_time = 0;
  
  switch (g_current_screen) {
    case GNSS_SCREEN_ODOMETER:
      // 码表界面按键处理
      if (key == KEY_NEXT) {
        // 下一界面
        g_current_screen = GNSS_SCREEN_COMPASS;
      } else if (key == KEY_PREV) {
        // 切换到加速度历史记录界面
        g_current_screen = GNSS_SCREEN_ACCELERATION;
        g_accel_history_index = 0; // 从第一条记录开始显示
      } else if (key == KEY_SELECT) {
        time_t now = time(NULL);
        
        if (press_time == 0) {
          // 开始计时
          press_time = now;
        } else if (now - press_time >= 1) { // 1秒以上为长按
          // 长按SELECT键手动结束当前分段
          if (gnss_is_recording() && gnss_is_segment_enabled()) {
            gnss_end_current_segment();
          }
          press_time = 0; // 重置计时
        } else {
          // 短按进入轨迹记录控制
          g_current_screen = GNSS_SCREEN_TRACKING;
          press_time = 0; // 重置计时
        }
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
      // 设置界面按键处理
      if (key == KEY_NEXT) {
        // 下一选项
        g_settings_index = (g_settings_index + 1) % 5;
      } else if (key == KEY_PREV) {
        // 上一选项
        g_settings_index = (g_settings_index + 4) % 5;
      } else if (key == KEY_SELECT) {
        // 选中当前项
        if (g_settings_index == 0) {
          // 导航到更新率设置
        } else if (g_settings_index == 1) {
          // 导航到分段设置
          g_current_screen = GNSS_SCREEN_SEGMENT;
          g_segment_index = 0;
        } else if (g_settings_index == 2) {
          // 查看历史分段数据
          g_current_screen = GNSS_SCREEN_HISTORY;
          g_history_index = 0; // 新增全局变量
        } else if (g_settings_index == 3) {
          // 清除数据
          gnss_reset_trip();
        } else {
          // 返回码表
          g_current_screen = GNSS_SCREEN_ODOMETER;
        }
      } else if (key == KEY_BACK) {
        // 返回码表
        g_current_screen = GNSS_SCREEN_ODOMETER;
      }
      break;
      
    case GNSS_SCREEN_SEGMENT:
      // 分段设置界面按键处理
      {
        bool enabled = gnss_is_segment_enabled();
        SegmentTimeOption option = gnss_get_segment_option();
        uint32_t custom_time = gnss_get_segment_custom_time();
        int max_index = (option == SEGMENT_TIME_CUSTOM && enabled) ? 3 : 2;
        
        if (key == KEY_NEXT) {
          // 下一选项
          g_segment_index = (g_segment_index + 1) % max_index;
        } else if (key == KEY_PREV) {
          // 上一选项
          g_segment_index = (g_segment_index + max_index - 1) % max_index;
        } else if (key == KEY_BACK) {
          // 返回设置
          g_current_screen = GNSS_SCREEN_SETTINGS;
        } else if (key == KEY_SELECT || key == KEY_LEFT || key == KEY_RIGHT) {
          // 切换或调整选项
          if (g_segment_index == 0) {
            // 切换分段启用/禁用
            if (key == KEY_SELECT || key == KEY_LEFT || key == KEY_RIGHT) {
              gnss_enable_segment(!enabled);
            }
          } else if (g_segment_index == 1 && enabled) {
            // 切换时间选项
            if (key == KEY_LEFT) {
              // 上一个选项
              int opt = (int)option;
              opt = (opt == 0) ? 4 : (opt - 1);
              gnss_set_segment_option((SegmentTimeOption)opt);
            } else if (key == KEY_RIGHT || key == KEY_SELECT) {
              // 下一个选项
              int opt = (int)option;
              opt = (opt + 1) % 5;
              gnss_set_segment_option((SegmentTimeOption)opt);
            }
          } else if (g_segment_index == 2 && enabled && option == SEGMENT_TIME_CUSTOM) {
            // 调整自定义时间
            uint32_t minutes = custom_time / 60;
            if (key == KEY_LEFT && minutes > 1) {
              minutes--;
              gnss_set_segment_custom_time(minutes * 60);
            } else if (key == KEY_RIGHT) {
              minutes++;
              gnss_set_segment_custom_time(minutes * 60);
            }
          }
        }
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
      
    case GNSS_SCREEN_HISTORY:
      // 历史分段列表界面按键处理
      {
        if (g_history_files.empty()) {
          // 第一次进入此界面，加载历史文件列表
          g_history_files = gnss_get_segment_history_files();
        }
        
        if (g_history_files.empty()) {
          // 没有历史文件，返回设置界面
          if (key != KEY_NONE) {
            g_current_screen = GNSS_SCREEN_SETTINGS;
          }
        } else {
          // 有历史文件，处理导航
          if (key == KEY_NEXT) {
            // 下一项
            g_history_index = (g_history_index + 1) % g_history_files.size();
          } else if (key == KEY_PREV) {
            // 上一项
            g_history_index = (g_history_index + g_history_files.size() - 1) % g_history_files.size();
          } else if (key == KEY_SELECT) {
            // 选中当前文件查看详情
            if (!g_history_files.empty() && g_history_index < g_history_files.size()) {
              // 加载分段数据
              g_loaded_segments.clear();
              if (gnss_load_segment_data_from_file(g_history_files[g_history_index].c_str(), g_loaded_segments)) {
                // 加载成功，跳转到详情界面
                g_current_screen = GNSS_SCREEN_SEGMENT_DETAIL;
                g_detail_index = 0;
              }
            }
          } else if (key == KEY_BACK) {
            // 返回设置界面
            g_current_screen = GNSS_SCREEN_SETTINGS;
          }
        }
      }
      break;
      
    case GNSS_SCREEN_SEGMENT_DETAIL:
      // 分段详情界面按键处理
      {
        if (g_loaded_segments.empty()) {
          // 没有数据，返回历史列表
          if (key != KEY_NONE) {
            g_current_screen = GNSS_SCREEN_HISTORY;
          }
        } else {
          // 有分段数据，处理导航
          if (key == KEY_NEXT) {
            // 下一段
            g_detail_index = (g_detail_index + 1) % g_loaded_segments.size();
          } else if (key == KEY_PREV) {
            // 上一段
            g_detail_index = (g_detail_index + g_loaded_segments.size() - 1) % g_loaded_segments.size();
          } else if (key == KEY_BACK) {
            // 返回历史列表
            g_current_screen = GNSS_SCREEN_HISTORY;
          }
        }
      }
      break;
      
    case GNSS_SCREEN_ACCELERATION:
      // 加速度历史记录界面按键处理
      {
        int accel_count = gnss_get_acceleration_history_count();
        if (accel_count == 0) {
          // 没有数据，任意键返回
          if (key != KEY_NONE) {
            g_current_screen = GNSS_SCREEN_ODOMETER;
          }
        } else {
          // 有历史数据，处理导航
          if (key == KEY_NEXT) {
            // 下一条记录
            g_accel_history_index = (g_accel_history_index + 1) % accel_count;
          } else if (key == KEY_PREV) {
            // 上一条记录
            g_accel_history_index = (g_accel_history_index + accel_count - 1) % accel_count;
          } else if (key == KEY_BACK) {
            // 返回码表
            g_current_screen = GNSS_SCREEN_ODOMETER;
          }
        }
      }
      break;
  }
}

/**
 * 绘制加速度历史记录界面
 */
void gnss_draw_acceleration_history(int selected_index)
{
  u8g2_t* u8g2 = get_display();
  
  u8g2_ClearBuffer(u8g2);
  
  // 标题
  u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(u8g2, 15, 8, "加速度历史记录");
  u8g2_DrawHLine(u8g2, 0, 10, 128);
  
  // 获取加速度历史数据
  int count = gnss_get_acceleration_history_count();
  
  if (count == 0) {
    // 没有历史数据
    u8g2_DrawStr(u8g2, 10, 35, "无加速度历史记录");
    u8g2_DrawStr(u8g2, 10, 50, "在行驶时自动记录");
    u8g2_SendBuffer(u8g2);
    return;
  }
  
  // 确保选择索引合法
  if (selected_index >= count) {
    selected_index = count - 1;
  }
  
  // 获取当前选中的加速度数据
  const AccelerationData* data = gnss_get_acceleration_data(selected_index);
  if (!data) {
    u8g2_DrawStr(u8g2, 10, 35, "读取数据失败");
    u8g2_SendBuffer(u8g2);
    return;
  }
  
  // 索引和日期
  char title[32];
  snprintf(title, sizeof(title), "记录 %d/%d - %s", 
           selected_index + 1, count, data->date_time_str);
  u8g2_DrawStr(u8g2, 0, 22, title);
  
  // 主要数据
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  
  // 0-30km/h 加速时间
  char time_0_30[20];
  if (data->time_0_30 >= 0) {
    snprintf(time_0_30, sizeof(time_0_30), "%.2f秒", data->time_0_30);
  } else {
    strcpy(time_0_30, "未达到");
  }
  u8g2_DrawStr(u8g2, 0, 34, "0-30km/h:");
  u8g2_DrawStr(u8g2, 70, 34, time_0_30);
  
  // 0-50km/h 加速时间
  char time_0_50[20];
  if (data->time_0_50 >= 0) {
    snprintf(time_0_50, sizeof(time_0_50), "%.2f秒", data->time_0_50);
  } else {
    strcpy(time_0_50, "未达到");
  }
  u8g2_DrawStr(u8g2, 0, 44, "0-50km/h:");
  u8g2_DrawStr(u8g2, 70, 44, time_0_50);
  
  // 最大速度
  char max_speed[16];
  snprintf(max_speed, sizeof(max_speed), "%.1f km/h", data->max_speed_reached);
  u8g2_DrawStr(u8g2, 0, 54, "最大速度:");
  u8g2_DrawStr(u8g2, 70, 54, max_speed);
  
  u8g2_SendBuffer(u8g2);
}

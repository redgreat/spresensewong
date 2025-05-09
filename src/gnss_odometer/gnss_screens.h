/****************************************************************************
 * gnss_screens.h
 * 
 * GNSS 界面模块：显示码表、指南针、轨迹分析等界面
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gnss_data.h"

// GNSS 界面状态
enum GnssScreen {
  GNSS_SCREEN_ODOMETER,    // 码表界面 (速度/里程/时间)
  GNSS_SCREEN_COMPASS,     // 指南针界面 (方向/经纬度)
  GNSS_SCREEN_TRACKING,    // 轨迹记录控制
  GNSS_SCREEN_TRIP_DATA,   // 行程数据分析
  GNSS_SCREEN_SETTINGS,    // GNSS 设置
  GNSS_SCREEN_SEGMENT,     // 分段设置
  GNSS_SCREEN_ACCEL_TEST,  // 百米加速测试
  GNSS_SCREEN_HISTORY,     // 历史分段列表
  GNSS_SCREEN_SEGMENT_DETAIL, // 分段详情
  GNSS_SCREEN_ACCELERATION  // 加速度记录历史
};

// 屏幕旋转方向
enum ScreenOrientation {
  ORIENTATION_0,
  ORIENTATION_90,
  ORIENTATION_180,
  ORIENTATION_270
};

// 初始化GNSS界面
void gnss_screens_init();

// 设置屏幕方向
void gnss_screens_set_orientation(ScreenOrientation orientation);

// 绘制码表界面
void gnss_draw_odometer(const GnssPoint* point, const TripData* trip, bool recording);

// 绘制指南针界面
void gnss_draw_compass(const GnssPoint* point);

// 绘制轨迹记录控制界面
void gnss_draw_tracking(bool recording, uint32_t points_count, const TripData* trip);

// 绘制行程数据分析界面
void gnss_draw_trip_data(const TripData* trip);

// 绘制GNSS设置界面
void gnss_draw_settings(GnssUpdateRate rate, int selected_index);

// 绘制分段设置界面
void gnss_draw_segment_settings(bool enabled, SegmentTimeOption option, uint32_t custom_time, int selected_index);

// 绘制百米加速测试界面
void gnss_draw_accel_test(const TripData* trip, float current_speed);

// 绘制历史分段列表界面
void gnss_draw_history_list(const std::vector<std::string>& files, int selected_index);

// 绘制分段详情界面
void gnss_draw_segment_detail(const std::vector<SegmentData>& segments, int selected_index);

// 绘制加速度历史记录界面
void gnss_draw_acceleration_history(int selected_index);

// 获取当前界面
GnssScreen gnss_get_current_screen();

// 设置当前界面
void gnss_set_screen(GnssScreen screen);

// 处理按键输入
void gnss_handle_key(uint8_t key);

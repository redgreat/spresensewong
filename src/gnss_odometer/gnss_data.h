/****************************************************************************
 * gnss_data.h
 * 
 * GNSS 数据管理模块：处理GNSS数据、计算距离、速度、存储轨迹等
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string>
#include <vector>

// GNSS 更新频率选项
enum GnssUpdateRate {
  GNSS_RATE_1HZ = 1000,  // 1Hz (默认)
  GNSS_RATE_5HZ = 200,   // 5Hz
  GNSS_RATE_10HZ = 100   // 10Hz
};

// 定位状态
enum GnssFixType {
  FIX_NONE = 0,          // 无定位
  FIX_2D,                // 2D定位
  FIX_3D                 // 3D定位
};

// 定位数据点
struct GnssPoint {
  double latitude;       // 纬度 (度)
  double longitude;      // 经度 (度)
  double altitude;       // 海拔 (米)
  float speed;           // 速度 (m/s)
  float course;          // 航向 (度，相对北)
  uint8_t num_satellites; // 使用的卫星数
  int32_t timestamp;     // 时间戳 (秒)
  GnssFixType fix_type;  // 定位类型
};

// 行程数据
struct TripData {
  double total_distance;  // 总距离 (米)
  double max_speed;       // 最大速度 (m/s)
  double avg_speed;       // 平均速度 (m/s)
  int32_t duration;       // 持续时间 (秒)
  time_t start_time;      // 开始时间
  time_t end_time;        // 结束时间
  
  // 百米加速
  bool has_0_100_time;    // 是否有 0-100km/h 加速数据
  float time_0_100;       // 0-100km/h 加速时间 (秒)
  
  // 其他统计
  double max_altitude;    // 最大海拔 (米)
  double min_altitude;    // 最小海拔 (米)
};

// 初始化GNSS
bool gnss_init();

// 关闭GNSS
void gnss_deinit();

// 设置更新频率
bool gnss_set_update_rate(GnssUpdateRate rate);

// 启动定位
bool gnss_start();

// 停止定位
void gnss_stop();

// 获取最新的定位数据
bool gnss_get_position(GnssPoint* point);

// 检查是否定位成功
bool gnss_has_fix();

// 当前星历的卫星数
int gnss_satellite_count();

// 行程记录控制
void gnss_start_recording();
void gnss_stop_recording();
bool gnss_is_recording();

// 获取当前行程数据
const TripData* gnss_get_trip_data();

// 重置行程数据
void gnss_reset_trip();

// 获取最后点和当前点的距离
double gnss_get_last_point_distance();

// 保存轨迹到文件
bool gnss_save_track(const char* filename);

// 检测百米加速
void gnss_detect_acceleration();

// 获取当前日期时间(GNSS授时)
void gnss_get_date_time(struct tm* tm_data);

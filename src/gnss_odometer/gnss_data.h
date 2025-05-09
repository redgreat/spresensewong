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
#include <map>
#include <fstream>

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
  float acceleration;    // 加速度 (m/s²)，由相邻点计算
};

// 分段数据
struct SegmentData {
  double distance;         // 分段距离 (米)
  double avg_speed;        // 分段平均速度 (m/s)
  int32_t duration;        // 分段持续时间 (秒)
  time_t start_time;       // 分段开始时间
  time_t end_time;         // 分段结束时间
  uint32_t point_count;    // 分段内点数
  
  // 分析数据
  uint32_t moving_time;    // 运动时长(秒)
  uint32_t idle_time;      // 停留时长(秒)
  float moving_avg_speed;  // 运动时的平均速度
  double start_lat;        // 起点纬度
  double start_lon;        // 起点经度
  double end_lat;          // 终点纬度
  double end_lon;          // 终点经度
  char start_time_str[20]; // 开始时间字符串 (yyyy-mm-dd HH:MM)
};

// 分段预设时间选项
enum SegmentTimeOption {
  SEGMENT_TIME_1MIN = 60,    // 1分钟
  SEGMENT_TIME_5MIN = 300,   // 5分钟
  SEGMENT_TIME_10MIN = 600,  // 10分钟
  SEGMENT_TIME_30MIN = 1800, // 30分钟
  SEGMENT_TIME_CUSTOM       // 自定义时间
};

// 当前分段设置
struct SegmentSettings {
  bool enabled;                // 是否启用分段
  SegmentTimeOption option;   // 时间选项
  uint32_t custom_time_sec;    // 自定义时间（秒）
};

// 加速度测量数据结构
struct AccelerationData {
  time_t timestamp;         // 测量时间戳
  float time_0_30;          // 0-30km/h 加速时间 (秒)，-1表示未达到目标速度
  float time_0_50;          // 0-50km/h 加速时间 (秒)，-1表示未达到目标速度
  float max_speed_reached;  // 测量中达到的最大速度 (km/h)
  char date_time_str[20];   // 测量日期时间字符串 (yyyy-mm-dd HH:MM)
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
  
  // 新增加速度测量
  bool measuring_acceleration; // 是否正在测量加速度
  time_t acceleration_start_time; // 加速开始测量时间
  float time_0_30;        // 0-30km/h 加速时间 (秒)
  float time_0_50;        // 0-50km/h 加速时间 (秒)
  bool reached_30kmh;     // 是否达到30km/h
  bool reached_50kmh;     // 是否达到50km/h
  
  // 其他统计
  double max_altitude;    // 最大海拔 (米)
  double min_altitude;    // 最小海拔 (米)
  
  // 分段数据
  uint32_t segment_count; // 分段数量
  std::vector<SegmentData> segments; // 分段数据列表
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

// 开始测量加速度
void gnss_start_acceleration_measurement();

// 停止测量加速度
void gnss_stop_acceleration_measurement(bool save_result = true);

// 检查加速度测量
void gnss_check_acceleration_measurement(const GnssPoint& point);

// 保存加速度测量结果
bool gnss_save_acceleration_data(const AccelerationData& data);

// 获取加速度历史数据
std::vector<AccelerationData> gnss_get_acceleration_history();

// 加速度记录界面显示相关
int gnss_get_acceleration_history_count();
const AccelerationData* gnss_get_acceleration_data(uint32_t index);

// 获取当前日期时间(GNSS授时)
void gnss_get_date_time(struct tm* tm_data);

// 设置分段选项
void gnss_set_segment_option(SegmentTimeOption option);

// 获取当前分段选项
SegmentTimeOption gnss_get_segment_option();

// 设置分段自定义时间（单位：秒）
void gnss_set_segment_custom_time(uint32_t seconds);

// 获取当前分段自定义时间
uint32_t gnss_get_segment_custom_time();

// 启用/禁用分段功能
void gnss_enable_segment(bool enable);

// 获取分段是否启用
bool gnss_is_segment_enabled();

// 获取当前分段数量
uint32_t gnss_get_segment_count();

// 获取指定分段数据
const SegmentData* gnss_get_segment_data(uint32_t index);

// 创建新分段
void gnss_create_new_segment();

// 手动结束当前分段并创建新分段
void gnss_end_current_segment();

// 保存分段数据到JSON文件
bool gnss_save_segment_data_to_json();

// 获取可用的历史分段文件列表
std::vector<std::string> gnss_get_segment_history_files();

// 从文件加载分段数据
bool gnss_load_segment_data_from_file(const char* filename, std::vector<SegmentData>& segments);

// 判断是否为停留状态 (两点间距离小于指定阈值)
bool gnss_is_idle_state(const GnssPoint& p1, const GnssPoint& p2, double threshold_meters = 5.0);

// 根据经纬度计算两点间的距离(米)
double gnss_calculate_distance(double lat1, double lon1, double lat2, double lon2);

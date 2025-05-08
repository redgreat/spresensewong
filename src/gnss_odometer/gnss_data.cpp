/****************************************************************************
 * gnss_data.cpp
 * 
 * GNSS 数据管理模块实现 - 处理GNSS数据、计算距离、速度、存储轨迹等
 * ***************************************************************************/
#include "gnss_data.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <arch/chip/gnss.h>

// Spresense GNSS操作全局变量
static int g_fd = -1;             // 文件描述符
static GnssUpdateRate g_rate = GNSS_RATE_1HZ;  // 默认1Hz
static bool g_running = false;    // 是否已启动
static bool g_recording = false;  // 是否记录轨迹
static GnssPoint g_last_point;    // 上一个定位点
static GnssPoint g_current_point; // 当前定位点
static bool g_has_last_point = false;  // 是否有上一个点

// 用于百米加速计算
static bool g_accel_start = false;  // 已开始加速测试
static float g_accel_start_time = 0;  // 加速开始时间
static float g_speed_threshold_kmh = 5.0f; // 开始加速的速度阈值 (km/h)

// 行程数据
static TripData g_trip_data;
static std::vector<GnssPoint> g_track_points;  // 轨迹点记录

// 计算两点的距离(米)
static double calculate_distance(double lat1, double lon1, double lat2, double lon2)
{
  // 将经纬度转换为弧度
  double lat1_rad = lat1 * M_PI / 180.0;
  double lon1_rad = lon1 * M_PI / 180.0;
  double lat2_rad = lat2 * M_PI / 180.0;
  double lon2_rad = lon2 * M_PI / 180.0;
  
  // Haversine公式
  double dlon = lon2_rad - lon1_rad;
  double dlat = lat2_rad - lat1_rad;
  double a = sin(dlat/2) * sin(dlat/2) + cos(lat1_rad) * cos(lat2_rad) * sin(dlon/2) * sin(dlon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  double distance = 6371000.0 * c;  // 地球半径约为6371km
  
  return distance;
}

/**
 * 初始化GNSS
 */
bool gnss_init()
{
  // 检查是否已初始化
  if (g_fd >= 0) return true;
  
  // 打开GNSS设备
  g_fd = open("/dev/gps", O_RDONLY);
  if (g_fd < 0) {
    printf("[GNSS] 打开设备失败: %d\n", errno);
    return false;
  }
  
  // 重置行程数据
  gnss_reset_trip();
  
  // 配置默认设置
  struct cxd56_gnss_ope_mode_param_s mode_param;
  memset(&mode_param, 0, sizeof(mode_param));
  mode_param.mode = 0;   // 正常模式
  mode_param.cycle = g_rate;  // 默认更新频率
  
  if (ioctl(g_fd, CXD56_GNSS_IOCTL_SET_OPE_MODE, (unsigned long)&mode_param) < 0) {
    printf("[GNSS] 设置操作模式失败\n");
    close(g_fd);
    g_fd = -1;
    return false;
  }
  
  printf("[GNSS] 初始化成功，更新频率: %d ms\n", g_rate);
  return true;
}

/**
 * 关闭GNSS
 */
void gnss_deinit()
{
  if (g_fd >= 0) {
    if (g_running) {
      gnss_stop();
    }
    close(g_fd);
    g_fd = -1;
  }
}

/**
 * 设置更新频率
 */
bool gnss_set_update_rate(GnssUpdateRate rate)
{
  // 必须先停止GNSS再设置
  bool was_running = g_running;
  if (was_running) {
    gnss_stop();
  }
  
  g_rate = rate;
  
  // 重新设置操作模式
  struct cxd56_gnss_ope_mode_param_s mode_param;
  memset(&mode_param, 0, sizeof(mode_param));
  mode_param.mode = 0;   // 正常模式
  mode_param.cycle = g_rate;
  
  if (ioctl(g_fd, CXD56_GNSS_IOCTL_SET_OPE_MODE, (unsigned long)&mode_param) < 0) {
    printf("[GNSS] 设置更新频率失败\n");
    return false;
  }
  
  // 如果之前在运行，则重新启动
  if (was_running) {
    return gnss_start();
  }
  
  return true;
}

/**
 * 启动定位
 */
bool gnss_start()
{
  if (g_fd < 0) {
    if (!gnss_init()) return false;
  }
  
  if (!g_running) {
    if (ioctl(g_fd, CXD56_GNSS_IOCTL_START, 0) < 0) {
      printf("[GNSS] 启动失败\n");
      return false;
    }
    g_running = true;
    printf("[GNSS] 开始定位\n");
  }
  
  return true;
}

/**
 * 停止定位
 */
void gnss_stop()
{
  if (g_fd >= 0 && g_running) {
    ioctl(g_fd, CXD56_GNSS_IOCTL_STOP, 0);
    g_running = false;
    printf("[GNSS] 停止定位\n");
  }
}

/**
 * 获取最新的定位数据
 */
bool gnss_get_position(GnssPoint* point)
{
  if (!g_running || g_fd < 0 || !point) return false;
  
  struct cxd56_gnss_positiondata_s posdat;
  
  if (read(g_fd, &posdat, sizeof(posdat)) < 0) {
    printf("[GNSS] 读取位置数据失败\n");
    return false;
  }
  
  // 检查定位类型
  if (posdat.receiver.pos_fixmode == 0) {
    // 无定位
    point->fix_type = FIX_NONE;
    return false;
  }
  
  // 更新上一个点
  if (g_current_point.fix_type != FIX_NONE) {
    g_last_point = g_current_point;
    g_has_last_point = true;
  }
  
  // 填充数据
  point->latitude = posdat.receiver.latitude;
  point->longitude = posdat.receiver.longitude;
  point->altitude = posdat.receiver.altitude;
  point->speed = posdat.receiver.speed;
  point->course = posdat.receiver.direction;
  point->num_satellites = posdat.receiver.pos_satellite_system;
  point->timestamp = posdat.receiver.time.sec;
  
  if (posdat.receiver.pos_fixmode == 1) {
    point->fix_type = FIX_2D;
  } else if (posdat.receiver.pos_fixmode >= 2) {
    point->fix_type = FIX_3D;
  } else {
    point->fix_type = FIX_NONE;
  }
  
  // 更新当前点
  g_current_point = *point;
  
  // 如果在记录行程，更新行程数据
  if (g_recording && point->fix_type != FIX_NONE) {
    // 添加到轨迹
    g_track_points.push_back(*point);
    
    // 更新行程统计
    g_trip_data.end_time = time(NULL);
    g_trip_data.duration = g_trip_data.end_time - g_trip_data.start_time;
    
    // 更新最大速度
    if (point->speed > g_trip_data.max_speed) {
      g_trip_data.max_speed = point->speed;
    }
    
    // 更新平均速度
    if (g_trip_data.duration > 0) {
      g_trip_data.avg_speed = g_trip_data.total_distance / g_trip_data.duration;
    }
    
    // 更新海拔统计
    if (point->altitude > g_trip_data.max_altitude) {
      g_trip_data.max_altitude = point->altitude;
    }
    if (point->altitude < g_trip_data.min_altitude || g_trip_data.min_altitude == 0) {
      g_trip_data.min_altitude = point->altitude;
    }
    
    // 如果有前一个点，计算距离
    if (g_has_last_point) {
      double dist = calculate_distance(
        g_last_point.latitude, g_last_point.longitude,
        point->latitude, point->longitude
      );
      
      // 防止异常值，位移小于500米才算(跳点过滤)
      if (dist < 500.0) {
        g_trip_data.total_distance += dist;
      }
    }
    
    // 检测百米加速
    gnss_detect_acceleration();
  }
  
  return true;
}

/**
 * 检查是否定位成功
 */
bool gnss_has_fix()
{
  return g_current_point.fix_type != FIX_NONE;
}

/**
 * 当前星历的卫星数
 */
int gnss_satellite_count()
{
  return g_current_point.num_satellites;
}

/**
 * 行程记录控制：开始
 */
void gnss_start_recording()
{
  if (!g_recording) {
    g_recording = true;
    g_track_points.clear();
    gnss_reset_trip();
    g_trip_data.start_time = time(NULL);
    g_trip_data.end_time = g_trip_data.start_time;
    printf("[GNSS] 开始记录轨迹\n");
  }
}

/**
 * 行程记录控制：停止
 */
void gnss_stop_recording()
{
  if (g_recording) {
    g_recording = false;
    printf("[GNSS] 停止记录轨迹，总点数: %zu\n", g_track_points.size());
  }
}

/**
 * 是否正在记录行程
 */
bool gnss_is_recording()
{
  return g_recording;
}

/**
 * 获取当前行程数据
 */
const TripData* gnss_get_trip_data()
{
  return &g_trip_data;
}

/**
 * 重置行程数据
 */
void gnss_reset_trip()
{
  g_trip_data.total_distance = 0.0;
  g_trip_data.max_speed = 0.0;
  g_trip_data.avg_speed = 0.0;
  g_trip_data.duration = 0;
  g_trip_data.start_time = 0;
  g_trip_data.end_time = 0;
  g_trip_data.has_0_100_time = false;
  g_trip_data.time_0_100 = 0.0f;
  g_trip_data.max_altitude = 0.0;
  g_trip_data.min_altitude = 0.0;
  
  g_accel_start = false;
  g_accel_start_time = 0.0f;
}

/**
 * 获取最后点和当前点的距离
 */
double gnss_get_last_point_distance()
{
  if (!g_has_last_point) return 0.0;
  
  return calculate_distance(
    g_last_point.latitude, g_last_point.longitude,
    g_current_point.latitude, g_current_point.longitude
  );
}

/**
 * 保存轨迹到文件
 */
bool gnss_save_track(const char* filename)
{
  if (g_track_points.empty()) {
    printf("[GNSS] 没有轨迹点可保存\n");
    return false;
  }
  
  FILE* f = fopen(filename, "w");
  if (!f) {
    printf("[GNSS] 创建文件失败: %s\n", filename);
    return false;
  }
  
  // 写入GPX格式的轨迹文件
  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(f, "<gpx version=\"1.1\" creator=\"Spresense GNSS Odometer\">\n");
  fprintf(f, "<trk><name>Track %ld</name><trkseg>\n", (long)time(NULL));
  
  // 写入轨迹点
  for (const auto& pt : g_track_points) {
    fprintf(f, "<trkpt lat=\"%.9f\" lon=\"%.9f\"><ele>%.2f</ele>", 
            pt.latitude, pt.longitude, pt.altitude);
    fprintf(f, "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>",
            2000, 1, 1, 0, 0, 0); // 简化版时间
    fprintf(f, "<speed>%.2f</speed></trkpt>\n", pt.speed);
  }
  
  fprintf(f, "</trkseg></trk></gpx>\n");
  fclose(f);
  
  printf("[GNSS] 保存轨迹到: %s, %zu 个点\n", filename, g_track_points.size());
  return true;
}

/**
 * 检测百米加速
 */
void gnss_detect_acceleration()
{
  // 如果已经有记录，或无定位，则返回
  if (g_trip_data.has_0_100_time || g_current_point.fix_type == FIX_NONE) {
    return;
  }
  
  // 速度转换为km/h
  float speed_kmh = g_current_point.speed * 3.6f;
  
  // 开始加速检测
  if (!g_accel_start && speed_kmh < g_speed_threshold_kmh) {
    // 等待速度低于阈值
    return;
  } else if (!g_accel_start) {
    // 开始记录加速
    g_accel_start = true;
    g_accel_start_time = g_trip_data.duration;
    printf("[GNSS] 开始加速检测: %.1f s\n", g_accel_start_time);
  }
  
  // 检查是否达到100km/h
  if (g_accel_start && speed_kmh >= 100.0f) {
    float elapsed = g_trip_data.duration - g_accel_start_time;
    g_trip_data.has_0_100_time = true;
    g_trip_data.time_0_100 = elapsed;
    printf("[GNSS] 0-100 km/h 加速时间: %.1f 秒\n", elapsed);
  }
}

/**
 * 获取当前日期时间(GNSS授时)
 */
void gnss_get_date_time(struct tm* tm_data)
{
  if (!tm_data) return;
  
  if (g_fd >= 0) {
    struct cxd56_gnss_datetime_s datetime;
    
    if (ioctl(g_fd, CXD56_GNSS_IOCTL_GET_DATETIME, (unsigned long)&datetime) >= 0) {
      tm_data->tm_year = datetime.year - 1900;
      tm_data->tm_mon = datetime.month - 1;
      tm_data->tm_mday = datetime.day;
      tm_data->tm_hour = datetime.hour;
      tm_data->tm_min = datetime.minute;
      tm_data->tm_sec = datetime.sec;
      return;
    }
  }
  
  // 如果失败，使用系统时间
  time_t now = time(NULL);
  *tm_data = *localtime(&now);
}

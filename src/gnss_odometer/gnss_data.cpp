/****************************************************************************
 * gnss_data.cpp
 * 
 * GNSS 数据管理模块实现 - 处理GNSS数据、计算距离、速度、存储轨迹等
 * ***************************************************************************/
#include "gnss_data.h"
#include "cxd56_gnss.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
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

// 加速度测量历史记录
static std::vector<AccelerationData> g_acceleration_history;

// 行程数据
static TripData g_trip_data;
static std::vector<GnssPoint> g_track_points;  // 轨迹点记录

// 分段相关变量
static SegmentSettings g_segment_settings = {
  true,                   // 默认启用分段
  SEGMENT_TIME_5MIN,      // 默认时间销9为5分钟
  300                     // 默认自定义时间5分钟
};
static time_t g_last_gps_time = 0;       // 最后一次有效GPS定位的时间
static time_t g_last_segment_time = 0;   // 上一次分段的时间
static bool g_has_lost_fix = false;      // 是否已失去定位

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
  if (g_fd < 0 || !g_running) return false;
  
  struct cxd56_gnss_positiondata_s posdat;
  int ret = read(g_fd, &posdat, sizeof(posdat));
  
  if (ret < 0) {
    printf("[GNSS] 读取位置数据失败: %d\n", errno);
    return false;
  } else if (ret != sizeof(posdat)) {
    printf("[GNSS] 读取数据大小不匹配, 期望 %zu, 实际 %d\n", sizeof(posdat), ret);
    return false;
  }
  
  // 获取当前时间
  time_t current_time = time(NULL);
  
  // 如果当前在记录模式且启用了分段功能
  if (g_recording && g_segment_settings.enabled && g_last_gps_time > 0) {
    // 检查是否超过分段时间
    uint32_t segment_time = g_segment_settings.option == SEGMENT_TIME_CUSTOM ? 
                     g_segment_settings.custom_time_sec : 
                     (uint32_t)g_segment_settings.option;
                     
    if (current_time - g_last_gps_time >= segment_time) {
      if (!g_has_lost_fix) {
        printf("[GNSS] 检测到GPS失去定位超过设定时间 %u秒\n", segment_time);
        g_has_lost_fix = true;
      }
    }
  }
  
  // 检查是否有有效定位
  if (posdat.receiver.pos_fixmode != 0) {
    // 更新数据点
    GnssPoint new_point;
    new_point.latitude = posdat.receiver.latitude;
    new_point.longitude = posdat.receiver.longitude;
    new_point.altitude = posdat.receiver.altitude;
    new_point.speed = posdat.receiver.velocity;
    new_point.course = posdat.receiver.direction;
    new_point.num_satellites = posdat.receiver.pos_svs;
    new_point.timestamp = time(NULL);
    
    // 设置定位类型
    switch (posdat.receiver.pos_fixmode) {
      case 1:
        new_point.fix_type = FIX_NONE;
        break;
      case 2:
        new_point.fix_type = FIX_2D;
        break;
      case 3:
      case 4:
        new_point.fix_type = FIX_3D;
        break;
      default:
        new_point.fix_type = FIX_NONE;
        break;
    }
    
    // 保存位置数据
    if (g_has_last_point) {
      // 保存上一个点
      g_last_point = g_current_point;
      
      // 如果正在记录，计算距离和保存轨迹点
      if (g_recording && new_point.fix_type != FIX_NONE) {
        double distance = calculate_distance(
          g_last_point.latitude, g_last_point.longitude,
          new_point.latitude, new_point.longitude
        );
        
        // 更新行程累计距离
        if (distance > 0.5) {  // 忽略小于0.5米的变动（减少噪声）
          g_trip_data.total_distance += distance;
          
          // 更新当前分段的累计距离
          if (!g_trip_data.segments.empty()) {
            g_trip_data.segments.back().distance += distance;
            g_trip_data.segments.back().end_time = new_point.timestamp;
            g_trip_data.segments.back().duration = 
              g_trip_data.segments.back().end_time - g_trip_data.segments.back().start_time;
            g_trip_data.segments.back().point_count++;
            
            // 计算分段平均速度
            if (g_trip_data.segments.back().duration > 0) {
              g_trip_data.segments.back().avg_speed = 
                g_trip_data.segments.back().distance / g_trip_data.segments.back().duration;
            }
          }
          
          // 重置无GPS定位计时
          g_last_gps_time = new_point.timestamp;
          
          // 如果之前失去过定位，并已经满足分段时间，创建新分段
          if (g_has_lost_fix && g_segment_settings.enabled) {
            uint32_t segment_time = g_segment_settings.option == SEGMENT_TIME_CUSTOM ? 
                             g_segment_settings.custom_time_sec : 
                             (uint32_t)g_segment_settings.option;
            gnss_create_new_segment();
            g_last_segment_time = new_point.timestamp;
            g_has_lost_fix = false;
            printf("[GNSS] 检测到GPS重新获得定位，创建新分段\n");
          }
        }
        
        // 记录轨迹点
        g_track_points.push_back(new_point);
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
    
    // 创建第一个分段
    gnss_create_new_segment();
    
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
  
  // 重置分段数据
  g_trip_data.segment_count = 0;
  g_trip_data.segments.clear();
  g_last_segment_time = 0;
  g_last_gps_time = 0;
  g_has_lost_fix = false;
  
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
 * 开始测量加速度
 */
void gnss_start_acceleration_measurement()
{
  // 如果无定位或已在测量中，则返回
  if (g_current_point.fix_type == FIX_NONE || g_trip_data.measuring_acceleration) {
    return;
  }
  
  // 重置测量数据
  g_trip_data.measuring_acceleration = true;
  g_trip_data.acceleration_start_time = time(NULL);
  g_trip_data.time_0_30 = -1;
  g_trip_data.time_0_50 = -1;
  g_trip_data.reached_30kmh = false;
  g_trip_data.reached_50kmh = false;
  
  printf("[GNSS] 开始测量加速度\n");
}

/**
 * 停止测量加速度
 */
void gnss_stop_acceleration_measurement(bool save_result)
{
  if (!g_trip_data.measuring_acceleration) {
    return;
  }
  
  // 如果指定保存结果，则创建加速度数据记录
  if (save_result && (g_trip_data.reached_30kmh || g_trip_data.reached_50kmh)) {
    AccelerationData data;
    data.timestamp = time(NULL);
    data.time_0_30 = g_trip_data.time_0_30;
    data.time_0_50 = g_trip_data.time_0_50;
    
    // 计算测量期间达到的最大速度 (km/h)
    data.max_speed_reached = g_trip_data.max_speed * 3.6f;
    
    // 生成测量时间字符串
    struct tm* tm_info = localtime(&data.timestamp);
    strftime(data.date_time_str, sizeof(data.date_time_str), "%Y-%m-%d %H:%M", tm_info);
    
    // 保存数据
    gnss_save_acceleration_data(data);
  }
  
  // 重置测量状态
  g_trip_data.measuring_acceleration = false;
  printf("[GNSS] 停止测量加速度\n");
}

/**
 * 检查加速度测量
 */
void gnss_check_acceleration_measurement(const GnssPoint& point)
{
  if (!g_trip_data.measuring_acceleration || point.fix_type == FIX_NONE) {
    return;
  }
  
  // 速度转换为km/h
  float speed_kmh = point.speed * 3.6f;
  
  // 查看是否达到30km/h
  if (!g_trip_data.reached_30kmh && speed_kmh >= 30.0f) {
    time_t now = time(NULL);
    float elapsed = difftime(now, g_trip_data.acceleration_start_time);
    g_trip_data.time_0_30 = elapsed;
    g_trip_data.reached_30kmh = true;
    printf("[GNSS] 0-30 km/h 加速时间: %.1f 秒\n", elapsed);
  }
  
  // 查看是否达到50km/h
  if (!g_trip_data.reached_50kmh && speed_kmh >= 50.0f) {
    time_t now = time(NULL);
    float elapsed = difftime(now, g_trip_data.acceleration_start_time);
    g_trip_data.time_0_50 = elapsed;
    g_trip_data.reached_50kmh = true;
    printf("[GNSS] 0-50 km/h 加速时间: %.1f 秒\n", elapsed);
    
    // 如果已达到50km/h，则算测量完成
    gnss_stop_acceleration_measurement(true);
  }
  
  // 如果测量超过60秒还没有达到50km/h，则放弃此次测量
  time_t now = time(NULL);
  if (difftime(now, g_trip_data.acceleration_start_time) > 60.0) {
    printf("[GNSS] 测量超时，放弃\n");
    gnss_stop_acceleration_measurement(false);
  }
}

/**
 * 保存加速度测量结果
 */
bool gnss_save_acceleration_data(const AccelerationData& data)
{
  // 新增到列表开头，使得最新的在前面
  g_acceleration_history.insert(g_acceleration_history.begin(), data);
  
  // 仅保留最近50条记录
  if (g_acceleration_history.size() > 50) {
    g_acceleration_history.resize(50);
  }
  
  // 确保目录存在
  struct stat st = {0};
  if (stat("/sd/acceleration", &st) == -1) {
    mkdir("/sd/acceleration", 0777);
  }
  
  // 保存到JSON文件
  char filename[64];
  snprintf(filename, sizeof(filename), "/sd/acceleration/accel_history.json");
  
  FILE* f = fopen(filename, "w");
  if (!f) {
    printf("[GNSS] 创建加速度记录文件失败\n");
    return false;
  }
  
  // 写入JSON格式的数据
  fprintf(f, "[\n");
  for (size_t i = 0; i < g_acceleration_history.size(); i++) {
    const AccelerationData& item = g_acceleration_history[i];
    
    fprintf(f, "  {\n");
    fprintf(f, "    \"timestamp\": %ld,\n", (long)item.timestamp);
    fprintf(f, "    \"time_0_30\": %.2f,\n", item.time_0_30);
    fprintf(f, "    \"time_0_50\": %.2f,\n", item.time_0_50);
    fprintf(f, "    \"max_speed\": %.2f,\n", item.max_speed_reached);
    fprintf(f, "    \"date_time\": \"%s\"\n", item.date_time_str);
    
    // 最后一项不加逗号
    fprintf(f, "  }%s\n", (i < g_acceleration_history.size() - 1) ? "," : "");
  }
  fprintf(f, "]\n");
  
  fclose(f);
  printf("[GNSS] 保存加速度记录到: %s\n", filename);
  return true;
}

/**
 * 获取加速度历史数据
 */
std::vector<AccelerationData> gnss_get_acceleration_history()
{
  // 如果内存中有数据，直接返回
  if (!g_acceleration_history.empty()) {
    return g_acceleration_history;
  }
  
  // 检查文件是否存在
  char filename[64];
  snprintf(filename, sizeof(filename), "/sd/acceleration/accel_history.json");
  
  FILE* f = fopen(filename, "r");
  if (!f) {
    printf("[GNSS] 加速度记录文件不存在\n");
    return g_acceleration_history; // 返回空列表
  }
  
  // 读取文件内容
  fseek(f, 0, SEEK_END);
  long fileSize = ftell(f);
  rewind(f);
  
  char* buffer = (char*)malloc(fileSize + 1);
  if (!buffer) {
    printf("[GNSS] 内存分配失败\n");
    fclose(f);
    return g_acceleration_history;
  }
  
  size_t bytesRead = fread(buffer, 1, fileSize, f);
  fclose(f);
  
  if (bytesRead < fileSize) {
    printf("[GNSS] 读取文件不完整\n");
    free(buffer);
    return g_acceleration_history;
  }
  
  buffer[bytesRead] = '\0';
  
  // 简单解析JSON数组
  try {
    // 查找数组开始和结束
    char* arr_start = strchr(buffer, '[');
    char* arr_end = strrchr(buffer, ']');
    
    if (!arr_start || !arr_end || arr_end <= arr_start) {
      throw std::runtime_error("Invalid JSON array");
    }
    
    // 找到每个对象并解析
    char* obj_start = arr_start + 1;
    while (obj_start < arr_end) {
      // 查找对象开始和结束
      char* obj_begin = strchr(obj_start, '{');
      if (!obj_begin || obj_begin > arr_end) break;
      
      char* obj_end = strchr(obj_begin, '}');
      if (!obj_end || obj_end > arr_end) break;
      
      // 解析对象
      AccelerationData data;
      data.timestamp = 0;
      data.time_0_30 = -1;
      data.time_0_50 = -1;
      data.max_speed_reached = 0;
      strcpy(data.date_time_str, "");
      
      // 查找JSON字段
      char* timestamp_str = strstr(obj_begin, "\"timestamp\":");
      char* time_0_30_str = strstr(obj_begin, "\"time_0_30\":");
      char* time_0_50_str = strstr(obj_begin, "\"time_0_50\":");
      char* max_speed_str = strstr(obj_begin, "\"max_speed\":");
      char* date_time_str = strstr(obj_begin, "\"date_time\":");
      
      // 解析时间戳
      if (timestamp_str && timestamp_str < obj_end) {
        data.timestamp = atol(timestamp_str + 12);
      }
      
      // 解析加速时间
      if (time_0_30_str && time_0_30_str < obj_end) {
        data.time_0_30 = atof(time_0_30_str + 12);
      }
      
      if (time_0_50_str && time_0_50_str < obj_end) {
        data.time_0_50 = atof(time_0_50_str + 12);
      }
      
      // 解析最大速度
      if (max_speed_str && max_speed_str < obj_end) {
        data.max_speed_reached = atof(max_speed_str + 13);
      }
      
      // 解析日期时间字符串
      if (date_time_str && date_time_str < obj_end) {
        char* quote_start = strchr(date_time_str + 13, '"');
        char* quote_end = quote_start ? strchr(quote_start + 1, '"') : NULL;
        
        if (quote_start && quote_end && quote_end > quote_start) {
          int len = (quote_end - quote_start - 1) < 19 ? (quote_end - quote_start - 1) : 19;
          strncpy(data.date_time_str, quote_start + 1, len);
          data.date_time_str[len] = '\0';
        }
      }
      
      // 添加到列表
      g_acceleration_history.push_back(data);
      
      // 移动到下一个对象
      obj_start = obj_end + 1;
    }
    
    printf("[GNSS] 从%s加载了%zu个加速度记录\n", filename, g_acceleration_history.size());
  } catch (...) {
    printf("[GNSS] 解析加速度记录JSON文件异常\n");
  }
  
  free(buffer);
  return g_acceleration_history;
}

/**
 * 获取加速度记录数量
 */
int gnss_get_acceleration_history_count()
{
  // 确保已加载历史数据
  if (g_acceleration_history.empty()) {
    gnss_get_acceleration_history();
  }
  
  return g_acceleration_history.size();
}

/**
 * 获取指定索引的加速度数据
 */
const AccelerationData* gnss_get_acceleration_data(uint32_t index)
{
  // 确保已加载历史数据
  if (g_acceleration_history.empty()) {
    gnss_get_acceleration_history();
  }
  
  if (index >= g_acceleration_history.size()) {
    return nullptr;
  }
  
  return &g_acceleration_history[index];
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

/**
 * 设置分段选项
 */
void gnss_set_segment_option(SegmentTimeOption option)
{
  g_segment_settings.option = option;
  printf("[GNSS] 设置分段时间选项: %d\n", (int)option);
}

/**
 * 获取当前分段选项
 */
SegmentTimeOption gnss_get_segment_option()
{
  return g_segment_settings.option;
}

/**
 * 设置分段自定义时间
 */
void gnss_set_segment_custom_time(uint32_t seconds)
{
  g_segment_settings.custom_time_sec = seconds;
  printf("[GNSS] 设置分段自定义时间: %u秒\n", seconds);
}

/**
 * 获取当前分段自定义时间
 */
uint32_t gnss_get_segment_custom_time()
{
  return g_segment_settings.custom_time_sec;
}

/**
 * 启用/禁用分段功能
 */
void gnss_enable_segment(bool enable)
{
  g_segment_settings.enabled = enable;
  printf("[GNSS] %s分段功能\n", enable ? "启用" : "禁用");
}

/**
 * 获取分段是否启用
 */
bool gnss_is_segment_enabled()
{
  return g_segment_settings.enabled;
}

/**
 * 获取当前分段数量
 */
uint32_t gnss_get_segment_count()
{
  return g_trip_data.segment_count;
}

/**
 * 获取指定分段数据
 */
const SegmentData* gnss_get_segment_data(uint32_t index)
{
  if (index < g_trip_data.segments.size()) {
    return &g_trip_data.segments[index];
  }
  return nullptr;
}

/**
 * 创建新分段
 */
void gnss_create_new_segment()
{
  SegmentData segment;
  segment.distance = 0.0;
  segment.avg_speed = 0.0;
  segment.duration = 0;
  segment.start_time = time(NULL);
  segment.end_time = segment.start_time;
  segment.point_count = 0;
  
  // 初始化新增字段
  segment.moving_time = 0;
  segment.idle_time = 0;
  segment.moving_avg_speed = 0.0f;
  segment.start_lat = 0.0;
  segment.start_lon = 0.0;
  segment.end_lat = 0.0;
  segment.end_lon = 0.0;
  
  // 设置起点位置
  if (g_current_point.fix_type != FIX_NONE) {
    segment.start_lat = g_current_point.latitude;
    segment.start_lon = g_current_point.longitude;
  }
  
  // 将开始时间格式化为字符串
  struct tm *tm_info = localtime(&segment.start_time);
  strftime(segment.start_time_str, sizeof(segment.start_time_str), "%Y-%m-%d %H:%M", tm_info);
  
  g_trip_data.segments.push_back(segment);
  g_trip_data.segment_count = g_trip_data.segments.size();
  
  printf("[GNSS] 创建新分段 #%u\n", g_trip_data.segment_count);
}

/**
 * 计算两点间距离(米)
 * 使用Haversine公式计算球面距离
 */
double gnss_calculate_distance(double lat1, double lon1, double lat2, double lon2)
{
  // 地球半径（米）
  const double earth_radius = 6371000.0;
  
  // 转化为弧度
  double lat1_rad = lat1 * M_PI / 180.0;
  double lon1_rad = lon1 * M_PI / 180.0;
  double lat2_rad = lat2 * M_PI / 180.0;
  double lon2_rad = lon2 * M_PI / 180.0;
  
  // 经纬度差值
  double dlat = lat2_rad - lat1_rad;
  double dlon = lon2_rad - lon1_rad;
  
  // Haversine公式
  double a = sin(dlat/2) * sin(dlat/2) + 
             cos(lat1_rad) * cos(lat2_rad) * 
             sin(dlon/2) * sin(dlon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  double distance = earth_radius * c;
  
  return distance;
}

/**
 * 判断是否为停留状态
 * @param p1 第一个点
 * @param p2 第二个点
 * @param threshold_meters 距离阈值（米）
 * @return 如果两点间距离小于阈值，则认为处于停留状态
 */
bool gnss_is_idle_state(const GnssPoint& p1, const GnssPoint& p2, double threshold_meters)
{
  // 如果两点间距离小于阈值，则认为处于停留状态
  double distance = gnss_calculate_distance(
      p1.latitude, p1.longitude,
      p2.latitude, p2.longitude);
  
  return distance < threshold_meters;
}

/**
 * 手动结束当前分段并创建新分段
 */
void gnss_end_current_segment()
{
  if (g_recording && !g_trip_data.segments.empty()) {
    // 更新当前分段的结束时间
    if (!g_trip_data.segments.empty()) {
      SegmentData& current_segment = g_trip_data.segments.back();
      current_segment.end_time = time(NULL);
      current_segment.duration = current_segment.end_time - current_segment.start_time;
      
      // 记录终点位置
      if (g_current_point.fix_type != FIX_NONE) {
        current_segment.end_lat = g_current_point.latitude;
        current_segment.end_lon = g_current_point.longitude;
      }
    }
    
    // 保存当前分段数据到JSON文件
    gnss_save_segment_data_to_json();
    
    // 创建新分段
    gnss_create_new_segment();
    g_last_segment_time = time(NULL);
    printf("[GNSS] 手动结束当前分段\n");
  }
}

/**
 * 保存分段数据到JSON文件
 */
/**
 * 获取可用的历史分段文件列表
 */
std::vector<std::string> gnss_get_segment_history_files()
{
  std::vector<std::string> files;
  
  // 确保目录存在
  struct stat st = {0};
  if (stat("/sd/segments", &st) == -1) {
    mkdir("/sd/segments", 0777);
    return files; // 目录刚创建，肯定是空的
  }
  
  // 打开目录
  DIR *dir = opendir("/sd/segments");
  if (!dir) {
    printf("[GNSS] 无法打开分段数据目录\n");
    return files;
  }
  
  // 读取所有json文件
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) { // 是常规文件
      std::string filename(entry->d_name);
      if (filename.find(".json") != std::string::npos) {
        // 是JSON文件，添加到列表
        files.push_back(std::string("/sd/segments/") + filename);
      }
    }
  }
  
  closedir(dir);
  
  // 按文件名降序排序（最新的文件在前面）
  std::sort(files.begin(), files.end(), std::greater<std::string>());
  
  return files;
}

/**
 * 从文件加载分段数据
 */
bool gnss_load_segment_data_from_file(const char* filename, std::vector<SegmentData>& segments)
{
  // 清空原有数据
  segments.clear();
  
  // 打开文件
  FILE* f = fopen(filename, "r");
  if (!f) {
    printf("[GNSS] 打开文件失败: %s\n", filename);
    return false;
  }
  
  // 读取文件内容到内存
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  if (file_size <= 0) {
    fclose(f);
    printf("[GNSS] 文件内容为空: %s\n", filename);
    return false;
  }
  
  char* buffer = (char*)malloc(file_size + 1);
  if (!buffer) {
    fclose(f);
    printf("[GNSS] 内存分配失败\n");
    return false;
  }
  
  size_t read_size = fread(buffer, 1, file_size, f);
  buffer[read_size] = '\0';
  fclose(f);
  
  if (read_size != file_size) {
    free(buffer);
    printf("[GNSS] 文件读取不完整: %s\n", filename);
    return false;
  }
  
  // 简易JSON解析
  // 这里的实现非常简单，只支持我们自己生成的JSON格式
  try {
    // 查找"segments"字段
    const char* segments_start = strstr(buffer, "\"segments\":");
    if (!segments_start) {
      free(buffer);
      printf("[GNSS] JSON格式错误: 找不到segments字段\n");
      return false;
    }
    
    // 查找segments数组的开始和结束
    const char* array_start = strchr(segments_start, '[');
    if (!array_start) {
      free(buffer);
      printf("[GNSS] JSON格式错误: segments不是数组\n");
      return false;
    }
    
    const char* array_end = strrchr(buffer, ']');
    if (!array_end || array_end < array_start) {
      free(buffer);
      printf("[GNSS] JSON格式错误: 找不到segments数组的结束\n");
      return false;
    }
    
    // 解析每个分段数据
    const char* obj_start = array_start;
    while ((obj_start = strchr(obj_start, '{')) != NULL && obj_start < array_end) {
      const char* obj_end = strchr(obj_start, '}');
      if (!obj_end || obj_end > array_end) break;
      
      // 创建新分段
      SegmentData segment;
      segment.distance = 0.0;
      segment.avg_speed = 0.0;
      segment.duration = 0;
      segment.moving_time = 0;
      segment.idle_time = 0;
      segment.moving_avg_speed = 0.0f;
      segment.point_count = 0;
      
      // 从对象中提取数据
      std::string obj_str(obj_start, obj_end - obj_start + 1);
      
      // 提取各个字段 (简化实现，只提取我们需要的字段)
      const char* dist_pos = strstr(obj_str.c_str(), "\"distance\":");
      if (dist_pos) {
        segment.distance = atof(dist_pos + 12); // 12 = length of ""distance":"
      }
      
      const char* avg_speed_pos = strstr(obj_str.c_str(), "\"avg_speed\":");
      if (avg_speed_pos) {
        segment.avg_speed = atof(avg_speed_pos + 13); // 13 = length of ""avg_speed":"
      }
      
      const char* duration_pos = strstr(obj_str.c_str(), "\"duration\":");
      if (duration_pos) {
        segment.duration = atoi(duration_pos + 12); // 12 = length of ""duration":"
      }
      
      const char* moving_time_pos = strstr(obj_str.c_str(), "\"moving_time\":");
      if (moving_time_pos) {
        segment.moving_time = atoi(moving_time_pos + 14); // 14 = length of ""moving_time":"
      }
      
      const char* idle_time_pos = strstr(obj_str.c_str(), "\"idle_time\":");
      if (idle_time_pos) {
        segment.idle_time = atoi(idle_time_pos + 12); // 12 = length of ""idle_time":"
      }
      
      const char* moving_avg_pos = strstr(obj_str.c_str(), "\"moving_avg_speed\":");
      if (moving_avg_pos) {
        segment.moving_avg_speed = atof(moving_avg_pos + 19); // 19 = length of ""moving_avg_speed":"
      }
      
      const char* start_time_pos = strstr(obj_str.c_str(), "\"start_time\":");
      if (start_time_pos) {
        const char* quote_start = strchr(start_time_pos + 13, '\"');
        const char* quote_end = strchr(quote_start + 1, '\"');
        if (quote_start && quote_end && quote_end > quote_start) {
          int len = (quote_end - quote_start - 1) < 19 ? (quote_end - quote_start - 1) : 19;
          strncpy(segment.start_time_str, quote_start + 1, len);
          segment.start_time_str[len] = '\0';
          
          // 将字符串时间转换为time_t(简化实现)
          segment.start_time = time(NULL); // 用当前时间替代
        }
      }
      
      // 添加到列表
      segments.push_back(segment);
      
      // 移动到下一个对象
      obj_start = obj_end + 1;
    }
    
    printf("[GNSS] 从%s加载了%zu个分段数据\n", filename, segments.size());
    free(buffer);
    return !segments.empty();
  }
  catch (...) {
    free(buffer);
    printf("[GNSS] 解析JSON文件异常: %s\n", filename);
    return false;
  }
}

/**
 * 保存分段数据到JSON文件
 */
bool gnss_save_segment_data_to_json()
{
  if (g_trip_data.segments.empty()) {
    printf("[GNSS] 没有分段数据可保存\n");
    return false;
  }
  
  // 确保存储目录存在
  struct stat st = {0};
  if (stat("/sd/segments", &st) == -1) {
    mkdir("/sd/segments", 0777);
  }
  
  // 生成文件名（使用当前时间）
  char filename[64];
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  snprintf(filename, sizeof(filename), "/sd/segments/segment_%04d%02d%02d_%02d%02d%02d.json",
           tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
           tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
  
  // 打开文件
  FILE* f = fopen(filename, "w");
  if (!f) {
    printf("[GNSS] 创建文件失败: %s\n", filename);
    return false;
  }
  
  // 写入JSON形式的数据
  fprintf(f, "{\n");
  fprintf(f, "  \"trip\": {\n");
  fprintf(f, "    \"total_distance\": %.2f,\n", g_trip_data.total_distance);
  fprintf(f, "    \"avg_speed\": %.2f,\n", g_trip_data.avg_speed);
  fprintf(f, "    \"max_speed\": %.2f,\n", g_trip_data.max_speed);
  fprintf(f, "    \"duration\": %d,\n", g_trip_data.duration);
  
  // 转换开始时间为可读字符串
  char start_time_str[64];
  struct tm *trip_start_tm = localtime(&g_trip_data.start_time);
  strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", trip_start_tm);
  fprintf(f, "    \"start_time\": \"%s\",\n", start_time_str);
  
  // 写入分段数据
  fprintf(f, "    \"segments\": [\n");
  
  for (size_t i = 0; i < g_trip_data.segments.size(); i++) {
    const SegmentData& seg = g_trip_data.segments[i];
    
    fprintf(f, "      {\n");
    fprintf(f, "        \"id\": %zu,\n", i + 1);
    fprintf(f, "        \"distance\": %.2f,\n", seg.distance);
    fprintf(f, "        \"avg_speed\": %.2f,\n", seg.avg_speed);
    fprintf(f, "        \"moving_avg_speed\": %.2f,\n", seg.moving_avg_speed);
    fprintf(f, "        \"duration\": %d,\n", seg.duration);
    fprintf(f, "        \"moving_time\": %u,\n", seg.moving_time);
    fprintf(f, "        \"idle_time\": %u,\n", seg.idle_time);
    
    // 开始时间
    char seg_time_str[64];
    struct tm *seg_start_tm = localtime(&seg.start_time);
    strftime(seg_time_str, sizeof(seg_time_str), "%Y-%m-%d %H:%M:%S", seg_start_tm);
    fprintf(f, "        \"start_time\": \"%s\",\n", seg_time_str);
    
    // 结束时间
    struct tm *seg_end_tm = localtime(&seg.end_time);
    strftime(seg_time_str, sizeof(seg_time_str), "%Y-%m-%d %H:%M:%S", seg_end_tm);
    fprintf(f, "        \"end_time\": \"%s\",\n", seg_time_str);
    
    // 起终点位置
    fprintf(f, "        \"start_lat\": %.6f,\n", seg.start_lat);
    fprintf(f, "        \"start_lon\": %.6f,\n", seg.start_lon);
    fprintf(f, "        \"end_lat\": %.6f,\n", seg.end_lat);
    fprintf(f, "        \"end_lon\": %.6f\n", seg.end_lon);
    
    // 如果不是最后一个分段，添加逗号
    fprintf(f, "      }%s\n", (i < g_trip_data.segments.size() - 1) ? "," : "");
  }
  
  fprintf(f, "    ]\n");
  fprintf(f, "  }\n");
  fprintf(f, "}\n");
  
  fclose(f);
  
  printf("[GNSS] 保存分段数据到: %s\n", filename);
  return true;
}

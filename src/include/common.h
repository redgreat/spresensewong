// =============================================================================
// common.h - 公共类型与宏定义 (Spresense SDK / NuttX)
// =============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// LCD SPI 引脚 (Arduino 逻辑编号)
#define LCD_SPI_CS   10
#define LCD_SPI_A0    9
#define LCD_SPI_RST   8

// Joystick ADC -> KeyCode
static inline uint8_t adc_to_key(int val)
{
  if (val < 100)  return 4; /* BACK */
  if (val < 300)  return 3; /* SELECT */
  if (val < 500)  return 2; /* NEXT */
  if (val < 900)  return 1; /* PREV */
  return 0;                 /* NONE */
}

enum KeyCode {
  KEY_NONE=0, KEY_PREV, KEY_NEXT, KEY_SELECT, KEY_BACK
};

enum LoopMode {
  LOOP_SEQUENTIAL,  // 顺序播放
  LOOP_REPEAT_ONE,  // 单曲循环
  LOOP_SHUFFLE      // 随机播放
};

enum EQPreset {
  EQ_FLAT,
  EQ_BASS_BOOST,
  EQ_TREBLE_BOOST,
  EQ_CUSTOM
};

static inline double deg2rad(double deg){return deg*M_PI/180.0;}
static inline double haversine(double lat1,double lon1,double lat2,double lon2){
  double dlat=deg2rad(lat2-lat1);
  double dlon=deg2rad(lat2-lon1);
  lat1=deg2rad(lat1);lat2=deg2rad(lat2);
  double a=sin(dlat/2)*sin(dlat/2)+cos(lat1)*cos(lat2)*sin(dlon/2)*sin(dlon/2);
  return 6371000*2*atan2(sqrt(a),sqrt(1-a));
}

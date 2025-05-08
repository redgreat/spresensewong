/****************************************************************************
 * player.cpp
 * 
 * 音频播放模块实现
 * ***************************************************************************/
#include "player.h"
#include <stdio.h>
#include <string.h>
#include <audio/audio_high_level_api.h>

/* 播放器状态变量 */
static bool g_initialized = false;
static bool g_playing = false;
static bool g_paused = false;
static uint8_t g_volume = 128;  /* 默认音量 */
static LoopMode g_loop_mode = LOOP_SEQUENTIAL;
static EQPreset g_current_eq = EQ_FLAT;
static int8_t g_custom_eq[8] = {0};  /* 8段EQ自定义参数 */
static uint32_t g_current_duration = 0;  /* 当前文件时长(毫秒) */

/**
 * 初始化播放器
 * 返回: 成功返回true，失败返回false
 */
bool player_init()
{
  if (g_initialized) return true;

  /* 初始化音频子系统 */
  AS_InitMicFrontend();
  
  /* 初始化高层API */
  if (AS_audio_init(NULL) < 0) {
    printf("[播放器] 初始化失败!\n");
    return false;
  }
  
  /* 注册MP3解码器 */
  if (AS_RegisterPlayer(AS_CODECTYPE_MP3, MP3_DECODER) < 0) {
    printf("[播放器] 注册解码器失败!\n");
    return false;
  }
  
  /* 设置默认音量 */
  if (AS_SetPlayerVolume(g_volume) < 0) {
    printf("[播放器] 设置音量失败!\n");
    /* 非致命错误，继续 */
  }
  
  g_initialized = true;
  return true;
}

/**
 * 关闭播放器
 */
void player_deinit()
{
  if (!g_initialized) return;
  
  AS_StopPlayer();
  AS_audio_finalize();
  g_initialized = false;
  g_playing = false;
  g_paused = false;
}

/**
 * 加载音频文件
 */
bool player_load_file(const char* filepath)
{
  if (!g_initialized) {
    if (!player_init()) return false;
  }
  
  /* 停止当前播放 */
  if (g_playing) {
    AS_StopPlayer();
  }
  
  g_playing = false;
  g_paused = false;
  
  /* 激活播放器 */
  if (AS_ActivatePlayer(AS_SETPLAYER_SAMPLE_RATE_AUTO) < 0) {
    printf("[播放器] 激活失败!\n");
    return false;
  }
  
  /* 应用当前EQ设置 */
  player_set_equalizer(g_current_eq);
  
  /* 设置音量 */
  AS_SetPlayerVolume(g_volume);
  
  /* 加载文件 */
  printf("[播放器] 加载文件: %s\n", filepath);
  if (AS_AddPlayerFile(filepath) < 0) {
    printf("[播放器] 加载文件失败!\n");
    return false;
  }
  
  return true;
}

/**
 * 开始播放
 */
void player_start()
{
  if (!g_initialized || g_playing) return;
  
  if (AS_StartPlayer() < 0) {
    printf("[播放器] 开始播放失败!\n");
    return;
  }
  
  g_playing = true;
  g_paused = false;
}

/**
 * 停止播放
 */
void player_stop()
{
  if (!g_initialized || !g_playing) return;
  
  AS_StopPlayer();
  g_playing = false;
  g_paused = false;
}

/**
 * 暂停播放
 */
void player_pause()
{
  if (!g_initialized || !g_playing || g_paused) return;
  
  if (AS_PausePlayer() < 0) {
    printf("[播放器] 暂停失败!\n");
    return;
  }
  
  g_paused = true;
}

/**
 * 恢复播放
 */
void player_resume()
{
  if (!g_initialized || !g_playing || !g_paused) return;
  
  if (AS_ResumePlayer() < 0) {
    printf("[播放器] 恢复失败!\n");
    return;
  }
  
  g_paused = false;
}

/**
 * 设置音量
 */
void player_set_volume(uint8_t volume)
{
  g_volume = volume;
  
  if (g_initialized) {
    AS_SetPlayerVolume(g_volume);
  }
}

/**
 * 获取当前音量
 */
uint8_t player_get_volume()
{
  return g_volume;
}

/**
 * 设置EQ预设
 */
void player_set_equalizer(EQPreset preset)
{
  if (!g_initialized) return;
  g_current_eq = preset;
  
  AS_EqualizerBandParam eq_params;
  memset(&eq_params, 0, sizeof(eq_params));
  
  switch (preset) {
    case EQ_BASS_BOOST:
      /* 低音增强预设 */
      eq_params.band_gain[0] = 8;  /* 增强低频 +8dB */
      eq_params.band_gain[1] = 4;
      eq_params.band_gain[2] = 0;
      eq_params.band_gain[3] = 0;
      eq_params.band_gain[4] = 0;
      eq_params.band_gain[5] = 0;
      eq_params.band_gain[6] = 0;
      eq_params.band_gain[7] = 0;
      break;
      
    case EQ_TREBLE_BOOST:
      /* 高音增强预设 */
      eq_params.band_gain[0] = 0;
      eq_params.band_gain[1] = 0;
      eq_params.band_gain[2] = 0;
      eq_params.band_gain[3] = 0;
      eq_params.band_gain[4] = 0;
      eq_params.band_gain[5] = 2;
      eq_params.band_gain[6] = 4;
      eq_params.band_gain[7] = 6;  /* 增强高频 +6dB */
      break;
    
    case EQ_CUSTOM:
      /* 使用自定义设置 */
      for (int i = 0; i < 8; i++) {
        eq_params.band_gain[i] = g_custom_eq[i];
      }
      break;
      
    case EQ_FLAT:
    default:
      /* 平坦EQ，不做任何处理 */
      break;
  }
  
  if (AS_SetEqualizerParam(&eq_params) < 0) {
    printf("[播放器] 设置EQ失败!\n");
  }
}

/**
 * 设置自定义EQ
 */
void player_set_custom_eq(const int8_t* bands, int num_bands)
{
  /* 最多支持8段EQ */
  int max_bands = (num_bands > 8) ? 8 : num_bands;
  
  for (int i = 0; i < max_bands; i++) {
    g_custom_eq[i] = bands[i];
  }
  
  /* 如果当前是自定义EQ模式，立即应用更改 */
  if (g_current_eq == EQ_CUSTOM && g_initialized) {
    player_set_equalizer(EQ_CUSTOM);
  }
}

/**
 * 判断是否正在播放
 */
bool player_is_playing()
{
  return g_playing;
}

/**
 * 判断是否暂停
 */
bool player_is_paused()
{
  return g_paused;
}

/**
 * 获取当前播放位置(毫秒)
 */
uint32_t player_get_position_ms()
{
  if (!g_initialized || !g_playing) return 0;
  
  return AS_getPlayerPosition();
}

/**
 * 获取当前文件总时长(毫秒)
 * 注意：这个函数在Spresense SDK中可能不稳定
 */
uint32_t player_get_duration_ms()
{
  /* 目前硬件API不直接支持获取时长，需要将文件完整解析一遍
   * 此处可能需要依赖外部计算或缓存的值
   */
  return g_current_duration;
}

/**
 * 设置循环模式
 */
void player_set_loop_mode(LoopMode mode)
{
  g_loop_mode = mode;
}

/**
 * 获取当前循环模式
 */
LoopMode player_get_loop_mode()
{
  return g_loop_mode;
}

/**
 * 解析音频元数据
 * 简单实现，仅支持ID3v1
 */
bool player_get_metadata(const char* filepath, AudioMetadata* metadata)
{
  if (!filepath || !metadata) return false;
  
  /* 设置默认值 */
  metadata->title = "";
  metadata->artist = "";
  metadata->album = "";
  metadata->duration_ms = 0;
  metadata->has_cover = false;
  
  /* 提取文件名作为默认标题 */
  const char* filename = strrchr(filepath, '/');
  if (filename) {
    filename++; /* 跳过'/' */
    metadata->title = filename;
    
    /* 去掉扩展名 */
    size_t dot_pos = metadata->title.rfind('.');
    if (dot_pos != std::string::npos) {
      metadata->title = metadata->title.substr(0, dot_pos);
    }
  } else {
    metadata->title = filepath;
  }
  
  /* 尝试读取ID3v1标签 */
  FILE* f = fopen(filepath, "rb");
  if (!f) return false;
  
  /* ID3v1位于文件末尾128字节 */
  if (fseek(f, -128, SEEK_END) == 0) {
    char tag[4] = {0};
    if (fread(tag, 1, 3, f) == 3 && memcmp(tag, "TAG", 3) == 0) {
      char buffer[31];
      
      /* 读取标题 */
      memset(buffer, 0, sizeof(buffer));
      fread(buffer, 1, 30, f);
      if (buffer[0] != 0) metadata->title = buffer;
      
      /* 读取艺术家 */
      memset(buffer, 0, sizeof(buffer));
      fread(buffer, 1, 30, f);
      metadata->artist = buffer;
      
      /* 读取专辑 */
      memset(buffer, 0, sizeof(buffer));
      fread(buffer, 1, 30, f);
      metadata->album = buffer;
    }
  }
  
  fclose(f);
  
  /* 简单按照比特率估算时长 
   * 注意：这只是粗略估计，实际情况可能有误差
   */
  struct stat st;
  if (stat(filepath, &st) == 0) {
    size_t file_size = st.st_size;
    /* 假设MP3的平均比特率为128kbps */
    metadata->duration_ms = (uint32_t)((file_size * 8) / 128000);
    
    /* 缓存时长供其他函数使用 */
    g_current_duration = metadata->duration_ms;
  }
  
  return true;
}

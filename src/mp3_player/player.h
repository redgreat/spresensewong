/****************************************************************************
 * player.h
 * 
 * 音频播放模块：管理Spresense音频子系统、播放控制、音量和EQ设置
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include "common.h"

// 播放器初始化/关闭
bool player_init();
void player_deinit();

// 播放控制
bool player_load_file(const char* filepath);
void player_start();
void player_stop();
void player_pause();
void player_resume();

// 音量控制 (0-255)
void player_set_volume(uint8_t volume);
uint8_t player_get_volume();

// EQ控制
void player_set_equalizer(EQPreset preset);
void player_set_custom_eq(const int8_t* bands, int num_bands);

// 获取播放状态
bool player_is_playing();
bool player_is_paused();
uint32_t player_get_position_ms();
uint32_t player_get_duration_ms();

// 设置循环模式
void player_set_loop_mode(LoopMode mode);
LoopMode player_get_loop_mode();

// 元数据
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    uint32_t duration_ms;
    bool has_cover;
};

bool player_get_metadata(const char* filepath, AudioMetadata* metadata);

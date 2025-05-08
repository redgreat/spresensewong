/****************************************************************************
 * file_system.h
 * 
 * 文件系统模块：管理SD卡、文件扫描、歌词解析等
 * ***************************************************************************/
#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "player.h"

// SD卡信息
struct SDCardInfo {
    uint32_t total_mb;     // 总容量(MB)
    uint32_t free_mb;      // 可用空间(MB)
    uint32_t used_mb;      // 已用空间(MB)
    float used_percent;    // 使用百分比
};

// 获取SD卡信息
bool sd_get_info(SDCardInfo* info);

// 音乐文件管理
struct MusicFile {
    std::string filepath;  // 完整路径
    std::string filename;  // 文件名(不含路径)
    AudioMetadata metadata;// 音频元数据
    bool has_lrc;          // 是否有对应歌词
};

// 歌词管理
struct LrcLine {
    uint32_t time_ms;      // 时间点(毫秒)
    std::string text;      // 歌词文本
};

// 扫描音乐目录
bool scan_music_directory(const char* dir_path, std::vector<MusicFile>* files);

// 解析LRC歌词文件
bool parse_lrc_file(const char* lrc_path, std::vector<LrcLine>* lyrics);

// 根据当前播放时间查找对应歌词行
int find_lyric_line(const std::vector<LrcLine>& lyrics, uint32_t current_ms);

// 路径操作工具
std::string get_filename_from_path(const std::string& path);
std::string get_directory_from_path(const std::string& path);
std::string get_extension(const std::string& path);
std::string combine_path(const std::string& dir, const std::string& filename);

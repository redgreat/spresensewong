/****************************************************************************
 * file_system.cpp
 * 
 * 文件系统模块实现
 * ***************************************************************************/
#include "file_system.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <algorithm>

/**
 * 获取SD卡信息
 */
bool sd_get_info(SDCardInfo* info) 
{
  if (!info) return false;
  
  struct statfs st;
  if (statfs("/sd", &st) < 0) {
    printf("[文件系统] 获取SD卡信息失败!\n");
    return false;
  }
  
  uint64_t block_size = st.f_bsize;
  uint64_t total_blocks = st.f_blocks;
  uint64_t free_blocks = st.f_bfree;
  
  info->total_mb = (uint32_t)((total_blocks * block_size) / (1024 * 1024));
  info->free_mb = (uint32_t)((free_blocks * block_size) / (1024 * 1024));
  info->used_mb = info->total_mb - info->free_mb;
  
  if (info->total_mb > 0) {
    info->used_percent = ((float)info->used_mb / info->total_mb) * 100.0f;
  } else {
    info->used_percent = 0.0f;
  }
  
  return true;
}

/**
 * 扫描音乐目录
 */
bool scan_music_directory(const char* dir_path, std::vector<MusicFile>* files) 
{
  if (!dir_path || !files) return false;
  
  files->clear();
  
  DIR* dir = opendir(dir_path);
  if (!dir) {
    printf("[文件系统] 无法打开目录: %s\n", dir_path);
    return false;
  }
  
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    
    /* 检查是否为MP3文件 */
    std::string ext = get_extension(name);
    if (ext == "mp3" || ext == "MP3") {
      MusicFile file;
      file.filename = name;
      file.filepath = combine_path(dir_path, name);
      
      /* 检查是否有对应的LRC文件 */
      std::string lrc_path = file.filepath.substr(0, file.filepath.size()-4) + ".lrc";
      file.has_lrc = (access(lrc_path.c_str(), F_OK) == 0);
      
      /* 解析MP3元数据 */
      player_get_metadata(file.filepath.c_str(), &file.metadata);
      
      files->push_back(file);
    }
  }
  
  closedir(dir);
  
  /* 按文件名排序 */
  std::sort(files->begin(), files->end(), 
    [](const MusicFile& a, const MusicFile& b) {
      return a.filename < b.filename;
    });
  
  printf("[文件系统] 扫描到 %zu 个MP3文件\n", files->size());
  return true;
}

/**
 * 解析LRC歌词文件
 */
bool parse_lrc_file(const char* lrc_path, std::vector<LrcLine>* lyrics) 
{
  if (!lrc_path || !lyrics) return false;
  
  lyrics->clear();
  
  FILE* f = fopen(lrc_path, "r");
  if (!f) {
    printf("[文件系统] 无法打开歌词文件: %s\n", lrc_path);
    return false;
  }
  
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* p = line;
    while (*p) {
      /* 寻找时间标签 [mm:ss.xx] */
      if (*p == '[') {
        char* end = strchr(p, ']');
        if (end) {
          /* 尝试解析时间 */
          int min = 0, sec = 0, msec = 0;
          if (sscanf(p+1, "%d:%d.%d", &min, &sec, &msec) >= 2) {
            uint32_t time_ms = min * 60000 + sec * 1000 + msec * 10;
            
            /* 提取歌词文本 */
            char* text = end + 1;
            /* 去除行尾换行符 */
            char* nl = strchr(text, '\n');
            if (nl) *nl = 0;
            
            LrcLine lrc_line;
            lrc_line.time_ms = time_ms;
            lrc_line.text = text;
            
            /* 如果歌词文本部分非空，则添加 */
            if (!lrc_line.text.empty()) {
              lyrics->push_back(lrc_line);
            }
          }
          p = end + 1;
        } else {
          p++;
        }
      } else {
        p++;
      }
    }
  }
  
  fclose(f);
  
  /* 按时间排序歌词 */
  std::sort(lyrics->begin(), lyrics->end(), 
    [](const LrcLine& a, const LrcLine& b) {
      return a.time_ms < b.time_ms;
    });
  
  printf("[文件系统] 解析到 %zu 行歌词\n", lyrics->size());
  return !lyrics->empty();
}

/**
 * 根据当前播放时间查找对应歌词行
 */
int find_lyric_line(const std::vector<LrcLine>& lyrics, uint32_t current_ms) 
{
  if (lyrics.empty()) return -1;
  
  /* 二分查找最接近但不超过当前时间的歌词 */
  int low = 0;
  int high = lyrics.size() - 1;
  
  /* 如果当前时间比第一条歌词还早，返回-1 */
  if (current_ms < lyrics[0].time_ms) {
    return -1;
  }
  
  /* 如果当前时间比最后一条歌词还晚，返回最后一条 */
  if (current_ms >= lyrics[high].time_ms) {
    return high;
  }
  
  /* 二分查找 */
  while (low <= high) {
    int mid = (low + high) / 2;
    
    if (lyrics[mid].time_ms <= current_ms && 
        (mid == lyrics.size()-1 || lyrics[mid+1].time_ms > current_ms)) {
      return mid;
    } else if (lyrics[mid].time_ms > current_ms) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  
  return -1;
}

/**
 * 获取文件名(不含路径)
 */
std::string get_filename_from_path(const std::string& path) 
{
  size_t pos = path.find_last_of('/');
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

/**
 * 获取目录(不含文件名)
 */
std::string get_directory_from_path(const std::string& path) 
{
  size_t pos = path.find_last_of('/');
  if (pos != std::string::npos) {
    return path.substr(0, pos);
  }
  return "";
}

/**
 * 获取文件扩展名(不含点)
 */
std::string get_extension(const std::string& path) 
{
  size_t pos = path.find_last_of('.');
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return "";
}

/**
 * 连接路径
 */
std::string combine_path(const std::string& dir, const std::string& filename) 
{
  if (dir.empty()) return filename;
  
  if (dir[dir.size()-1] == '/') {
    return dir + filename;
  } else {
    return dir + "/" + filename;
  }
}

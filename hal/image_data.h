#pragma once

#include <chrono>

#include <memory>
#include <string>
#include <vector>

#include "frame_data.h"
#include <opencv2/opencv.hpp>

struct ImageData {
  AX_U32 width;
  AX_U32 height;
  AX_IMG_FORMAT_E img_format;
  AX_BOOL end_of_stream;
  std::shared_ptr<FrameData> data;

  std::chrono::steady_clock::time_point time_point;
};

// 平面布局：planar 数量与每个 plane 的字节数。
// 这是映射/拷贝/释放三处尺寸公式的唯一来源，改动需同步 FrameData::Destroy()。
struct PlaneLayout {
  AX_U8 planar_num = 0;
  AX_U32 size[3] = {0, 0, 0};
};

bool ComputePlaneLayout(const AX_VIDEO_FRAME_T& vframe, PlaneLayout& layout);

// 确保 img 的 u64VirAddr 可用；幂等，已映射则直接返回 0。
//
// 约定：映射地址缓存在 FrameInfo 内，由 FrameData::Destroy() 统一 munmap，
// 调用方无需配对释放。需要提前归还映射时才显式调用 Unmap()。
// 新增读取帧像素的函数请一律走这里，不要自行 AX_SYS_Mmap 到局部变量，
// 否则地址无人记录、帧析构时不会被 munmap。
int EnsureMapped(ImageData& img);

// 提前归还 EnsureMapped 建立的映射并把 u64VirAddr 置零。
int Unmap(ImageData& img);

// 深拷贝到新的 CMM 缓冲（kMemIdSys）。会按需映射 src。
int Clone(ImageData& dest, ImageData& src);

int JpegEncode(std::vector<uint8_t>& dest, const ImageData& src);
int JpegDecode(ImageData& dest, const std::string& jpegFile);

// 按 plane 顺序拼接到 host 内存。会按需映射 src。
int Copy2Host(std::vector<uint8_t>& dest, ImageData& src);

// 仅支持 AX_FORMAT_RGB888，逐行去 stride 拷进 cv::Mat。会按需映射 src。
int Copy2Mat(cv::Mat& dest, ImageData& src);

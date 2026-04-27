#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

enum StreamType {
  STREAM_VIDEO = 0,
  STREAM_RTSP,
};

enum DecodeStatus {
  DECODE_ERROR = -1,
  DECODE_UNINIT = 0,
  DECODE_READY = 1,
  DECODE_START = 2,
  DECODE_FFMPEG_FINISHED = 3,
  DECODE_DVPP_FINISHED = 4,
  DECODE_FINISHED = 5
};

#define INVALID_CHANNEL_ID (-1)
#define INVALID_STREAM_FORMAT (-1)
#define VIDEO_CHANNEL_MAX (256)
#define RTSP_TRANSPORT_UDP "udp"
#define RTSP_TRANSPORT_TCP "tcp"

typedef int (*FrameProcessCallBack)(void* callback_param, void* frame_data,
                                    int frame_size);

class FFmpegDecoder {
 public:
  explicit FFmpegDecoder(const std::string& name);
  ~FFmpegDecoder() = default;

  void Decode(FrameProcessCallBack callback_func, void* callback_param);
  int GetVideoInfo();

  int GetFrameWidth() { return frame_width_; }
  int GetFrameHeight() { return frame_height_; }
  int GetVideoType() { return video_type_; }
  int GetFps() { return fps_; }
  bool IsFinished() { return is_finished_; }
  int GetProfile() { return profile_; }

  void SetTransport(const std::string& transport_type);
  void StopDecode() { is_stop_ = true; }

 private:
  int GetVideoIndex(AVFormatContext* av_format_context);
  void InitVideoStreamFilter(const AVBitStreamFilter*& video_filter);
  bool OpenVideo(AVFormatContext*& av_format_context);
  void SetDictForRtsp(AVDictionary*& avdic);
  bool InitVideoParams(int video_index, AVFormatContext* av_format_context,
                       AVBSFContext*& bsf_ctx);

  bool is_finished_ = false;
  bool is_stop_ = false;
  int frame_width_ = 0;
  int frame_height_ = 0;
  int video_type_ = 0;
  int profile_ = 0;
  int fps_ = 0;
  std::string stream_name_;
  std::string rtsp_transport_;
};
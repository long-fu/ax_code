#include "ffmpeg_decoder.hpp"

#include <string>

#include "logger.h"

#define DEVICE_MAX 4

namespace {
const int64_t kUsec = 1000000;
const uint32_t kDecodeFrameQueueSize = 256;
const int kDecodeQueueOpWait = 10000;
const int kFrameEnQueueRetryTimes = 1000;
const int kQueueOpRetryTimes = 1000;
const int kOutputJamWait = 10000;
const int kInvalidTpye = -1;
const int kWaitDecodeFinishInterval = 1000;
const int kDefaultFps = 1;
const int kReadSlow = 5;
const uint32_t kVideoChannelMax310 = 32;
const uint32_t kVideoChannelMax310B = 128;
const uint32_t kVideoChannelMax310P = 256;

const int kNoFlag = 0;
const int kInvalidVideoIndex = -1;
const std::string kRtspTransport = "rtsp_transport";
const std::string kUdp = "udp";
const std::string kTcp = "tcp";
const std::string kBufferSize = "buffer_size";
const std::string kMaxBufferSize = "10485760";
const std::string kMaxDelayStr = "max_delay";
const std::string kMaxDelayValue = "100000000";
const std::string kTimeoutStr = "stimeout";
const std::string kTimeoutValue = "5000000";
const std::string kPktSize = "pkt_size";
const std::string kPktSizeValue = "10485760";
const std::string kReorderQueueSize = "reorder_queue_size";
const std::string kReorderQueueSizeValue = "0";
const int kErrorBufferSize = 1024;
const uint32_t kDefaultStreamFps = 5;
const uint32_t kOneSecUs = 1000 * 1000;
}  // namespace

FFmpegDecoder::FFmpegDecoder(const std::string& name)
    : stream_name_(name) {
  rtsp_transport_.assign(kTcp.c_str());
  is_finished_ = false;
  is_stop_ = false;
}

void FFmpegDecoder::SetTransport(const std::string& transport_type) {
  rtsp_transport_.assign(transport_type.c_str());
}

int FFmpegDecoder::GetVideoIndex(AVFormatContext* av_format_context) {
  if (av_format_context == nullptr) {
    return kInvalidVideoIndex;
  }

  for (uint32_t i = 0; i < av_format_context->nb_streams; i++) {
    if (av_format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      return i;
    }
  }

  return kInvalidVideoIndex;
}

void FFmpegDecoder::InitVideoStreamFilter(
    const AVBitStreamFilter*& video_filter) {
  if (video_type_ == AV_CODEC_ID_H264) {
    video_filter = av_bsf_get_by_name("h264_mp4toannexb");
  } else {
    video_filter = av_bsf_get_by_name("hevc_mp4toannexb");
  }
}

void FFmpegDecoder::SetDictForRtsp(AVDictionary*& avdic) {
  LOG_INFO("Set parameters for {}", stream_name_);

  av_dict_set(&avdic, kRtspTransport.c_str(), rtsp_transport_.c_str(), kNoFlag);
  av_dict_set(&avdic, kBufferSize.c_str(), kMaxBufferSize.c_str(), kNoFlag);
  av_dict_set(&avdic, kMaxDelayStr.c_str(), kMaxDelayValue.c_str(), kNoFlag);
  av_dict_set(&avdic, kTimeoutStr.c_str(), kTimeoutValue.c_str(), kNoFlag);
  av_dict_set(&avdic, kReorderQueueSize.c_str(),
              kReorderQueueSizeValue.c_str(), kNoFlag);
  av_dict_set(&avdic, kPktSize.c_str(), kPktSizeValue.c_str(), kNoFlag);
  LOG_INFO("Set parameters for {} end", stream_name_);
}

bool FFmpegDecoder::OpenVideo(AVFormatContext*& av_format_context) {
  bool ret = true;
  AVDictionary* avdic = nullptr;

  LOG_INFO("Open video {} ...", stream_name_);
  SetDictForRtsp(avdic);
  int open_ret = avformat_open_input(&av_format_context, stream_name_.c_str(),
                                     nullptr, &avdic);
  if (open_ret < 0) {
    char buf_error[kErrorBufferSize];
    av_strerror(open_ret, buf_error, kErrorBufferSize);

    LOG_ERROR("Could not open video:{}, return:{}, error info:{}",
                  stream_name_, open_ret, buf_error);
    ret = false;
  }

  if (avdic != nullptr) {
    av_dict_free(&avdic);
  }

  return ret;
}

bool FFmpegDecoder::InitVideoParams(int video_index,
                                    AVFormatContext* av_format_context,
                                    AVBSFContext*& bsf_ctx) {
  const AVBitStreamFilter* video_filter = nullptr;
  InitVideoStreamFilter(video_filter);
  if (video_filter == nullptr) {
    LOG_ERROR("Unkonw bitstream filter, videoFilter is nullptr!");
    return false;
  }

  if (av_bsf_alloc(video_filter, &bsf_ctx) < 0) {
    LOG_ERROR("Fail to call av_bsf_alloc!");
    return false;
  }

  if (avcodec_parameters_copy(
          bsf_ctx->par_in,
          av_format_context->streams[video_index]->codecpar) < 0) {
    LOG_ERROR("Fail to call avcodec_parameters_copy!");
    return false;
  }

  bsf_ctx->time_base_in = av_format_context->streams[video_index]->time_base;

  if (av_bsf_init(bsf_ctx) < 0) {
    LOG_ERROR("Fail to call av_bsf_init!");
    return false;
  }

  return true;
}

void FFmpegDecoder::Decode(FrameProcessCallback callback,
                           void* callback_param) {
  LOG_INFO("Start ffmpeg decode video {} ...", stream_name_);
  avformat_network_init();

  AVFormatContext* av_format_context = avformat_alloc_context();

  if (!OpenVideo(av_format_context)) {
    return;
  }

  int video_index = GetVideoIndex(av_format_context);
  if (video_index == kInvalidVideoIndex) {
    LOG_ERROR("Rtsp {} index is -1", stream_name_);
    return;
  }

  AVBSFContext* bsf_ctx = nullptr;
  if (!InitVideoParams(video_index, av_format_context, bsf_ctx)) {
    return;
  }

  LOG_INFO("Start decode frame of video {} ...", stream_name_);

  AVPacket av_packet;
  bool process_ok = true;
  while ((av_read_frame(av_format_context, &av_packet) == 0) && process_ok &&
         !is_stop_) {
    if (av_packet.stream_index == video_index) {
      if (av_bsf_send_packet(bsf_ctx, &av_packet)) {
        LOG_ERROR("Fail to call av_bsf_send_packet, channel id:{}",
                      stream_name_);
      }

      while ((av_bsf_receive_packet(bsf_ctx, &av_packet) == 0) && !is_stop_) {
        int ret = callback(callback_param, av_packet.data, av_packet.size);
        if (ret != 0) {
          process_ok = false;
          break;
        }
      }
    }
    av_packet_unref(&av_packet);
  }

  av_bsf_free(&bsf_ctx);
  avformat_close_input(&av_format_context);

  is_finished_ = true;
  LOG_INFO("Ffmpeg decoder {} finished", stream_name_);
}

int FFmpegDecoder::GetVideoInfo() {
  avformat_network_init();
  AVFormatContext* av_format_context = avformat_alloc_context();
  bool ret = OpenVideo(av_format_context);
  if (!ret) {
    LOG_ERROR("Open {} failed", stream_name_);
    return -1;
  }

  if (avformat_find_stream_info(av_format_context, NULL) < 0) {
    LOG_ERROR("Get stream info of {} failed", stream_name_);
    return -1;
  }

  int video_index = GetVideoIndex(av_format_context);
  if (video_index == kInvalidVideoIndex) {
    LOG_ERROR(
        "Video index is {}, current media stream has no video info:{}",
        kInvalidVideoIndex, stream_name_);
    avformat_close_input(&av_format_context);
    return -1;
  }

  AVStream* in_stream = av_format_context->streams[video_index];

  frame_width_ = in_stream->codecpar->width;
  frame_height_ = in_stream->codecpar->height;
  if (in_stream->avg_frame_rate.den) {
    fps_ = in_stream->avg_frame_rate.num / in_stream->avg_frame_rate.den;
  } else {
    fps_ = kDefaultStreamFps;
  }

  video_type_ = in_stream->codecpar->codec_id;
  profile_ = in_stream->codecpar->profile;

  avformat_close_input(&av_format_context);

  LOG_INFO("Video {}, type {}, profile {}, width:{}, height:{}, fps:{}",
           stream_name_, video_type_, profile_, frame_width_, frame_height_,
           fps_);
  return 0;
}
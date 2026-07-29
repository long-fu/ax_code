#pragma once

#include <memory>
#include <string>

#include "ffmpeg_decoder.h"
#include "ffmpeg_encoder.h"
#include "task_scheduler.h"
#include "task_node.h"
#include "process_msg.h"
#include "venc_helper.h"

class EncProcess : public pipeline::TaskNode {
 public:
  EncProcess(const std::string& rtmp, FFmpegDecoder* ff_decoder) {
    venc_ = new VencHelper(0, ff_decoder->GetFrameWidth(),
                              ff_decoder->GetFrameHeight(), 25, 25);
    ff_encoder_ = new FFmpegEncoder(
        rtmp, 25, ff_decoder->GetFrameWidth(), ff_decoder->GetFrameHeight(),
        AV_PIX_FMT_NV12, 25, "main");
  }

  ~EncProcess() {
    venc_->StopEncode();
    ff_encoder_->Release();

    delete ff_encoder_;
    delete venc_;
  }

  static int VencProcessCallbackFunc(AX_VENC_STREAM_T stream_data, int chn,
                                     void* user_data) {
    auto self = static_cast<EncProcess*>(user_data);
    TIME_START(WritePacket);
    self->ff_encoder_->WritePacket(stream_data.stPack.pu8Addr,
                                      stream_data.stPack.u32Len);
    TIME_END(WritePacket);
    // TIME_USEC_SHOW(WritePacket);
    return 0;
  }

  int Init() override {
    if (0 != ff_encoder_->Init()) {
      LOG_ERROR("FFmpeg Encoder Init failled!");
      return -1;
    }
    if (0 != venc_->Init()) {
      LOG_ERROR("VENC Init failed!");
      return -2;
    }
    return 0;
  }

  int Start() {
    int ret = venc_->Encode(VencProcessCallbackFunc, this);
    return ret;
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    switch (msg_id) {
      case kMsgAppStart:
        Start();
        break;
      case kMsgBusprocData: {
        auto in_data = std::static_pointer_cast<BusData>(msg_data);
        venc_->Write(&in_data->image, nullptr);
        break;
      }
      case kMsgAppExit:
        // venc_->StopEncode();
        // delete venc_;
        break;
      default:
        break;
    }
    return 0;
  }

 private:
  FFmpegEncoder* ff_encoder_ = nullptr;
  VencHelper* venc_ = nullptr;
};
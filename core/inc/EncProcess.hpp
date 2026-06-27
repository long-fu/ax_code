#pragma once

#include <memory>
#include <string>

#include "FFmpegDecoder.hpp"
#include "FFmpegEncoder.hpp"
#include "Pipeline.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "VencHelper.hpp"

class EncProcess : public PipelineThread {
 public:
  EncProcess(const std::string& rtmp, FFmpegDecoder* ff_decoder) {
    m_p_venc = new VencHelper(0, ff_decoder->GetFrameWidth(),
                              ff_decoder->GetFrameHeight(), 25, 25);
    m_p_ff_encoder = new FFmpegEncoder(
        rtmp, 25, ff_decoder->GetFrameWidth(), ff_decoder->GetFrameHeight(),
        AV_PIX_FMT_NV12, 25, "main");
  }

  ~EncProcess() {
    m_p_venc->StopEncode();
    m_p_ff_encoder->Release();

    delete m_p_ff_encoder;
    delete m_p_venc;
  }

  static int VencProcessCallBackFunc(AX_VENC_STREAM_T stream_data, int chn,
                                     void* user_data) {
    auto self = static_cast<EncProcess*>(user_data);
    TIME_START(WritePacket);
    self->m_p_ff_encoder->WritePacket(stream_data.stPack.pu8Addr,
                                      stream_data.stPack.u32Len);
    TIME_END(WritePacket);
    // TIME_USEC_SHOW(WritePacket);
    return 0;
  }

  int Init() override {
    if (0 != m_p_ff_encoder->Init()) {
      LOG_ERROR("FFmpeg Encoder Init failled!");
      return -1;
    }
    if (0 != m_p_venc->Init()) {
      LOG_ERROR("VENC Init failed!");
      return -2;
    }
    return 0;
  }

  int Start() {
    int ret = m_p_venc->Encode(VencProcessCallBackFunc, this);
    return ret;
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        ret = Start();
        break;
      case kMsgBusprocData: {
        auto in_data = std::static_pointer_cast<BusData>(msg_data);
        ret = m_p_venc->Write(&in_data->image, nullptr);
        break;
      }
      case kMsgAppExit:
        // m_p_venc->StopEncode();
        // delete m_p_venc;
        break;
      default:
        break;
    }
    return 0;
  }

 private:
  FFmpegEncoder* m_p_ff_encoder = nullptr;
  VencHelper* m_p_venc = nullptr;
};
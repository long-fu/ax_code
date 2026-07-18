#pragma once

#include <memory>

#include "FFmpegDecoder.hpp"
#include "IvpsHelper.hpp"
#include "Pipeline.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "VdecHelper.hpp"

class PreProcess : public PipelineThread {
 public:
  explicit PreProcess(FFmpegDecoder* ff_decoder)
      : ff_decoder_(ff_decoder) {
    vdec_ = new VdecHelper(0, PT_H264, ff_decoder->GetFrameWidth(),
                              ff_decoder->GetFrameHeight(),
                              ff_decoder->GetFps());
    ivps_ = new IvpsHelper(0,
                              ff_decoder->GetFrameWidth() *
                                      ff_decoder->GetFrameHeight() * 3,
                              32);
  }

  ~PreProcess() {
    LOG_INFO("~PreProcess");

    ff_decoder_->StopDecode();
    vdec_->StopDecode();

    pthread_join(ffmpeg_thread_, nullptr);

    delete vdec_;
    delete ivps_;
  }

  int Init() override {
    if (0 != vdec_->Init()) {
      LOG_ERROR("VDEC Init failled!");
      return -1;
    }

    if (0 != ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640)) {
      LOG_ERROR("IVPS Init failed!");
      return -2;
    }

    next_thread_id_ = GetPipelineThreadIdByName("InfProccess");
    return 0;
  }

  static int FrameProcessCallBackFunc(void* user_data, void* frame_data,
                                      int frame_size) {
    auto self = static_cast<PreProcess*>(user_data);
    self->vdec_->Write(frame_data, frame_size, nullptr);
    return 0;
  }

  static int VdecProcessCallBackFunc(ImageData image, int grp, int chn,
                                     void* user_data) {
    auto data = std::make_shared<ImageData>(image);
    data->time_point = std::chrono::steady_clock::now();

    auto self = static_cast<PreProcess*>(user_data);
    int ret = SendMessage(self->SelfInstanceId(), kMsgVdecData, data);

    return 0;
  }

  static void* FFmpegDecodeCallBackFunc(void* argv) {
    pthread_setname_np(pthread_self(), "FFDec");
    auto self = static_cast<PreProcess*>(argv);
    self->ff_decoder_->Decode(FrameProcessCallBackFunc, argv);
    return nullptr;
  }

  int Start() {
    int ret = vdec_->Decode(VdecProcessCallBackFunc, this);
    pthread_create(&ffmpeg_thread_, nullptr, FFmpegDecodeCallBackFunc, this);
    return ret;
  }

  int Proprocess(std::shared_ptr<ImageData> img_data) {
    ImageData dest;
    ImageData src = *img_data.get();
    static uint64 index = 0;
    if (index >= 250 * 6) {
      SendMessage(g_main_thread_id, kMsgAppExit, nullptr);
    }
    index++;
    int ret = ivps_->Process(dest, src);
    if (ret != 0) {
      LOG_ERROR("CSC failed, ret={}", ret);
      return ret;
    }

    auto data = std::make_shared<PreData>();
    data->image = src;
    Copy2Host(data->data, dest);

    int send_ret = SendMessage(next_thread_id_, kMsgPreprocData, data);

    return 0;
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        ret = Start();
        break;
      case kMsgVdecData: {
        auto in_data = std::static_pointer_cast<ImageData>(msg_data);
        ret = Proprocess(in_data);
        break;
      }
      case kMsgAppExit:
        // ff_decoder_->StopDecode();
        // vdec_->StopDecode();
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  FFmpegDecoder* ff_decoder_ = nullptr;
  VdecHelper* vdec_ = nullptr;
  IvpsHelper* ivps_ = nullptr;
  pthread_t ffmpeg_thread_ = -1;
  int next_thread_id_ = -1;
};
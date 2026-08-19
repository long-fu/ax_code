#pragma once

#include <cstdlib>
#include <memory>

#include "ffmpeg_decoder.h"
#include "ivps_helper.h"
#include "logger.h"
#include "task_scheduler.h"
#include "task_node.h"
#include "process_msg.h"
#include "vdec_helper.h"
#include "file.h"

using namespace pipeline;

class PreProcess : public TaskNode {
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

    next_thread_id_ = TaskNodeIdByName("InfProcess");
    
    LOG_INFO("PreProcess Init done, next_thread_id={}", next_thread_id_);
    
    return 0;
  }

  static int FrameProcessCallbackFunc(void* user_data, void* frame_data,
                                      int frame_size) {
    auto self = static_cast<PreProcess*>(user_data);
    // LOG_INFO("FrameProcessCallbackFunc: frame_size={}", frame_size);
    self->vdec_->Write(frame_data, frame_size, nullptr);
    return 0;
  }

  static int VdecProcessCallbackFunc(ImageData image, int grp, int chn,
                                     void* user_data) {
    auto data = std::make_shared<ImageData>(image);
    data->time_point = std::chrono::steady_clock::now();

    // LOG_INFO("VdecProcessCallbackFunc: grp={} chn={} width={} height={} format={} size={}",
    //          grp, chn, image.width, image.height, image.img_format, image.data->FrameInfo()->stVFrame.u32PicStride[0] * image.height);
    auto self = static_cast<PreProcess*>(user_data);
    pipeline::SendMessage(self->InstanceId(), kMsgVdecData, data);

    return 0;
  }

  static void* FFmpegDecodeCallbackFunc(void* argv) {
    pthread_setname_np(pthread_self(), "FFDec");
    auto self = static_cast<PreProcess*>(argv);
    self->ff_decoder_->Decode(FrameProcessCallbackFunc, argv);
    return nullptr;
  }

  int Start() {
    int ret = vdec_->Decode(VdecProcessCallbackFunc, this);
    pthread_create(&ffmpeg_thread_, nullptr, FFmpegDecodeCallbackFunc, this);
    return ret;
  }

  int Preprocess(std::shared_ptr<ImageData> img_data) {
    ImageData dest;
    ImageData src = *img_data.get();
    
    int ret = ivps_->Process(dest, src);
    if (ret != 0) {
      LOG_ERROR("CSC failed, ret={}", ret);
      return ret;
    }

    auto data = std::make_shared<PreData>();
    data->image = src;
    Copy2Host(data->data, dest);
    
    // yuv 数据正确
    // std::vector<uint8_t> jpeg;
    // JpegEncode(jpeg, dest);
    // utilities::DumpFile("out.jpg", jpeg);
    // exit(1);
    
    // LOG_INFO("Preprocess done, width={} height={} format={} size={}",
            //  dest.width, dest.height, (int)dest.img_format, data->data.size());
    pipeline::SendMessage(next_thread_id_, kMsgPreprocData, data);
    
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
        ret = Preprocess(in_data);
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
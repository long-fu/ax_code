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
      : m_p_ff_decoder(ff_decoder) {
    m_p_vdec = new VdecHelper(0, PT_H264, ff_decoder->GetFrameWidth(),
                              ff_decoder->GetFrameHeight(),
                              ff_decoder->GetFps());
    m_p_ivps = new IvpsHelper(0,
                              ff_decoder->GetFrameWidth() *
                                      ff_decoder->GetFrameHeight() * 3,
                              32);
  }

  ~PreProcess() {
    LOG_INFO("~PreProcess");
    
    m_p_ff_decoder->StopDecode();
    m_p_vdec->StopDecode();

    pthread_join(m_t_ffmpeg_thread,nullptr);

    delete m_p_vdec;
    delete m_p_ivps;
  }

  int Init() override {
    if (0 != m_p_vdec->Init()) {
      LOG_ERROR("VDEC Init failled!");
      return -1;
    }

    if (0 != m_p_ivps->Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640)) {
      LOG_ERROR("IVPS Init failed!");
      return -2;
    }

    m_next_thread_id_ = GetPipelineThreadIdByName("InfProccess");
    return 0;
  }

  static int FrameProcessCallBackFunc(void* user_data, void* frame_data,
                                      int frame_size) {
    auto self = static_cast<PreProcess*>(user_data);
    self->m_p_vdec->Write(frame_data, frame_size, nullptr);
    return 0;
  }

  static int VdecProcessCallBackFunc(ImageData image, int grp, int chn,
                                     void* user_data) {
    auto data = std::make_shared<ImageData>(image);
    data->timePoint = std::chrono::steady_clock::now();

    auto self = static_cast<PreProcess*>(user_data);
    int ret = SendMessage(self->SelfInstanceId(), kMsgVdecData, data);

    return 0;
  }

  static void* FFmpegDecodeCallBackFunc(void* argv) {
    pthread_setname_np(pthread_self(), "FFDec");
    auto self = static_cast<PreProcess*>(argv);
    self->m_p_ff_decoder->Decode(FrameProcessCallBackFunc, argv);
    return nullptr;
  }

  int Start() {
    int ret = m_p_vdec->Decode(VdecProcessCallBackFunc, this);
    pthread_create(&m_t_ffmpeg_thread, nullptr, FFmpegDecodeCallBackFunc, this);
    return ret;
  }

  int Proprocess(std::shared_ptr<ImageData> img_data) {
    ImageData dest;
    ImageData src = *img_data.get();
    static uint64 index = 0;
    if(index >= 250 * 6) {
      SendMessage(g_main_thread_id,kMsgAppExit, nullptr);
    } 
    index++;
    int ret = m_p_ivps->Process(dest, src);
    if (ret != 0) {
      LOG_ERROR("CSC failed, ret={}", ret);
      return ret;
    }

    auto data = std::make_shared<PreData>();
    data->image = src;
    Copy2Host(data->data, dest);

    int send_ret = SendMessage(m_next_thread_id_, kMsgPreprocData, data);

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
        // m_p_ff_decoder->StopDecode();
        // m_p_vdec->StopDecode();
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  FFmpegDecoder* m_p_ff_decoder = nullptr;
  VdecHelper* m_p_vdec = nullptr;
  IvpsHelper* m_p_ivps = nullptr;
  pthread_t m_t_ffmpeg_thread = -1;
  int m_next_thread_id_ = -1;
};
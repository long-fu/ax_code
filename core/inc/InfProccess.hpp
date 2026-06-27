#pragma once

#include <memory>
#include <string>

#include "FFmpegDecoder.hpp"
#include "Pipeline.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "Yolov5.hpp"

class InfProccess : public PipelineThread {
 public:
  InfProccess(const std::string& model_config, FFmpegDecoder* ff_decoder)
      : m_yolov5(model_config), m_p_ff_decoder(ff_decoder) {}

  ~InfProccess() {
    LOG_INFO("~InfProccess");
  };

  int Init() override {
    m_next_thread_id_ = GetPipelineThreadIdByName("BusProcess");
    // LOG_INFO("BusProcess: {}",m_next_thread_id_);
    return m_yolov5.Init();
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        break;
      case kMsgPreprocData: {
        // if(isExit_) {
        //   return 0;
        // }
        auto in_data = std::static_pointer_cast<PreData>(msg_data);
        m_yolov5.Process(in_data->data);
        in_data->data.clear();

        auto out_data = std::make_shared<InfData>();
        out_data->image = in_data->image;
        int pp_ret = m_yolov5.Postprocess(m_p_ff_decoder->GetFrameWidth(),
                                          m_p_ff_decoder->GetFrameHeight(),
                                          out_data->objects);
        if (pp_ret != 0) {
            LOG_ERROR("Yolov5 Postprocess failed, ret={}", pp_ret);
            return pp_ret;
        }

        ret = SendMessage(m_next_thread_id_, kMsgInfprocData, out_data);
        break;
      }
      case kMsgAppExit:
      // delete
        // m_yolov5.Destroy();
        // isExit_ = true;
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  bool isExit_ = false;
  Yolov5 m_yolov5;
  FFmpegDecoder* m_p_ff_decoder = nullptr;
  int m_next_thread_id_ = -1;
};
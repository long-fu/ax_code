#pragma once

#include <memory>
#include <string>

#include "ffmpeg_decoder.h"
#include "task_scheduler.h"
#include "task_node.h"
#include "process_msg.h"
#include "yolov5.h"

class InfProccess : public pipeline::TaskNode {
 public:
  InfProccess(const std::string& model_config, FFmpegDecoder* ff_decoder)
      : yolov5_(Yolov5Config(model_config)), ff_decoder_(ff_decoder) {}

  ~InfProccess() {
    LOG_INFO("~InfProccess");
  };

  int Init() override {
    next_thread_id_ = pipeline::TaskNodeIdByName("BusProcess");
    // LOG_INFO("BusProcess: {}", next_thread_id_);
    return yolov5_.Init();
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        break;
      case kMsgPreprocData: {
        auto in_data = std::static_pointer_cast<PreData>(msg_data);
        int infer_ret = yolov5_.Process(in_data->data);
        in_data->data.clear();
        if (infer_ret != 0) {
            LOG_ERROR("Yolov5 Process failed, ret={}", infer_ret);
            return 0;  // drop frame; do not kill node
        }

        auto out_data = std::make_shared<InfData>();
        out_data->image = in_data->image;
        int pp_ret = yolov5_.Postprocess(ff_decoder_->GetFrameWidth(),
                                          ff_decoder_->GetFrameHeight(),
                                          out_data->objects);
        if (pp_ret != 0) {
            LOG_ERROR("Yolov5 Postprocess failed, ret={}", pp_ret);
            return 0;  // drop frame; do not kill node
        }

        pipeline::SendMessage(next_thread_id_, kMsgInfprocData, out_data);
        break;
      }
      case kMsgAppExit:
      // delete
        // yolov5_.Destroy();
        // is_exit_ = true;
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  bool is_exit_ = false;
  Yolov5 yolov5_;
  FFmpegDecoder* ff_decoder_ = nullptr;
  int next_thread_id_ = -1;
};
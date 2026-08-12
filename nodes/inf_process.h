#pragma once

#include <memory>
#include <string>

#include "engine.h"
#include "engine_factory.h"
#include "ffmpeg_decoder.h"
#include "logger.h"
#include "process_msg.h"
#include "task_node.h"
#include "task_scheduler.h"

class InfProccess : public pipeline::TaskNode {
 public:
  InfProccess(const std::string& model_config, FFmpegDecoder* ff_decoder)
      : model_config_path_(model_config), ff_decoder_(ff_decoder) {}

  ~InfProccess() { LOG_INFO("~InfProccess"); }

  int Init() override {
    next_thread_id_ = pipeline::TaskNodeIdByName("BusProcess");
    LOG_INFO("BusProcess Next Proccess: {}", next_thread_id_);

    engine_ = engine_factory::CreateEngine(model_config_path_);
    if (engine_ == nullptr) {
      LOG_ERROR("InfProcess: CreateEngine failed for {}", model_config_path_);
      return -1;
    }
    return engine_->Init();
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        break;
      case kMsgPreprocData: {
        if (engine_ == nullptr) {
          LOG_ERROR("InfProcess: engine is null");
          return 0;
        }
        auto in_data = std::static_pointer_cast<PreData>(msg_data);
        int infer_ret = engine_->Process(in_data->data);
        in_data->data.clear();
        if (infer_ret != 0) {
          LOG_ERROR("Engine Process failed, ret={}", infer_ret);
          return 0;
        }

        auto out_data = std::make_shared<InfData>();
        out_data->image = in_data->image;
        int pp_ret = engine_->Postprocess(ff_decoder_->GetFrameWidth(),
                                          ff_decoder_->GetFrameHeight(),
                                          out_data->objects);
        if (pp_ret != 0) {
          LOG_ERROR("Engine Postprocess failed, ret={}", pp_ret);
          return 0;
        }

        pipeline::SendMessage(next_thread_id_, kMsgInfprocData, out_data);
        break;
      }
      case kMsgAppExit:
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  bool is_exit_ = false;
  std::string model_config_path_;
  std::unique_ptr<Engine> engine_;
  FFmpegDecoder* ff_decoder_ = nullptr;
  int next_thread_id_ = -1;
};

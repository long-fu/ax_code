#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "engine.h"
#include "engine_factory.h"
#include "ffmpeg_decoder.h"
#include "logger.h"
#include "process_msg.h"
#include "task_node.h"
#include "task_scheduler.h"

class InfProcess : public pipeline::TaskNode {
 public:
  InfProcess(const std::string& model_config, FFmpegDecoder* ff_decoder)
      : model_config_path_(model_config), ff_decoder_(ff_decoder) {}

  ~InfProcess() { LOG_INFO("~InfProcess"); }

  int Init() override {
    // -1 会让后续 SendMessage 被 scheduler 静默拒绝，整条链路不工作却无根因日志
    next_thread_id_ = pipeline::TaskNodeIdByName("BusProcess");
    if (next_thread_id_ < 0) {
      LOG_ERROR("InfProcess: 找不到下游节点 BusProcess");
      return -1;
    }
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
        // 不可达的防御分支：Init() 返回非 0 时 TaskNodeMgr 会置 kError 并退出
        // 线程，app.Start() 随之失败、main 直接 ExitPipeline，走不到这里。
        if (engine_ == nullptr) {
          return ReportFailure(kErrEngineNotReady, "engine 未就绪");
        }
        
        // LOG_INFO("InfProcess: Processing preprocessed data");
        // TIME_START(InfProcess);

        auto in_data = std::static_pointer_cast<PreData>(msg_data);
        int infer_ret = engine_->Process(in_data->data);
        
        // TIME_END(InfProcess);
        // TIME_USEC_SHOW(InfProcess);
        // Func InfProcess cost : 10306 us

        in_data->data.clear();
        if (infer_ret != 0) {
          return ReportFailure(kErrInfer, "Engine Process");
        }

        auto out_data = std::make_shared<InfData>();
        out_data->image = in_data->image;

        // TIME_START(Postprocess);
        int pp_ret = engine_->Postprocess(ff_decoder_->GetFrameWidth(),
                                          ff_decoder_->GetFrameHeight(),
                                          out_data->objects);
        // TIME_END(Postprocess);
        // TIME_USEC_SHOW(Postprocess);
        // Func Postprocess cost : 339 us                                          
        if (pp_ret != 0) {
          return ReportFailure(kErrPostprocess, "Engine Postprocess");
        }

        ReportRecovered();

        // 此处不检查 SendMessage 返回值：队列满属背压而非本节点故障，
        // 且已由 TaskNodeMgr::PushMessage 统一记账（见 P1-3）。
        // 若在此再返回非 0，会把背压误报成推理失败，反而污染信号。
        // LOG_INFO("InfProcess: Sending inference results {}", out_data->objects.size());
        pipeline::SendMessage(next_thread_id_, kMsgInfprocData, out_data);
        break;
      }
      case kMsgAppExit:
        LOG_INFO("InfProcess: Received exit message");
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  // Process() 的返回码。非 0 会被 TaskNodeMgr 记录为 process error 并丢弃该帧
  // （不会杀掉节点），上层据此可做统计与健康判断。
  static constexpr int kErrEngineNotReady = -1;
  static constexpr int kErrInfer = -2;
  static constexpr int kErrPostprocess = -3;

  // 持续故障时每帧都会命中（25fps 即每秒 25 次），加上 TaskNodeMgr 对非 0
  // 返回值的日志就是 50 行/秒，会把日志刷爆。故按窗口汇总，并额外记录恢复
  // 时刻——排查时真正想知道的是"从何时开始失败、丢了多少帧、何时恢复"。
  // 仅在 worker 线程内调用，无需同步。
  int ReportFailure(int code, const char* what) {
    const auto now = std::chrono::steady_clock::now();
    ++fail_total_;
    if (fail_streak_ == 0) {
      LOG_ERROR("InfProcess: {} 失败 ret={}，开始丢弃帧", what, code);
      last_fail_log_ = now;
      streak_start_ = now;
    } else if (now - last_fail_log_ >= kFailLogInterval) {
      const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
          now - streak_start_).count();
      LOG_ERROR("InfProcess: {} 持续失败 {} 秒，已连续丢弃 {} 帧(累计 {})",
                what, secs, fail_streak_, fail_total_);
      last_fail_log_ = now;
    }
    ++fail_streak_;
    return code;
  }

  // 从连续失败中恢复时打一条，给出这段故障的持续时长与丢帧数。
  void ReportRecovered() {
    if (fail_streak_ == 0) {
      return;
    }
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - streak_start_).count();
    LOG_WARN("InfProcess: 已恢复，本次故障持续 {} 秒、丢弃 {} 帧(累计 {})",
             secs, fail_streak_, fail_total_);
    fail_streak_ = 0;
  }

  static constexpr auto kFailLogInterval = std::chrono::seconds(5);

  bool is_exit_ = false;
  std::string model_config_path_;
  std::unique_ptr<Engine> engine_;
  FFmpegDecoder* ff_decoder_ = nullptr;
  int next_thread_id_ = -1;

  uint64_t fail_streak_ = 0;
  uint64_t fail_total_ = 0;
  std::chrono::steady_clock::time_point streak_start_;
  std::chrono::steady_clock::time_point last_fail_log_;
};

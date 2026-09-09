#pragma once

#include <atomic>
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

    vdec_ = new VdecHelper(PT_H264, ff_decoder->GetFrameWidth(),
                              ff_decoder->GetFrameHeight(),
                              ff_decoder->GetFps());

    ivps_ = new IvpsHelper(ff_decoder->GetFrameWidth() *
                                      ff_decoder->GetFrameHeight() * 3,
                              32);
  }

  ~PreProcess() {
    LOG_INFO("~PreProcess");

    // 正常路径下 ExitPipeline 已调过；此处兜底覆盖 Start 失败等异常路径。
    StopSources();

    delete vdec_;
    delete ivps_;
  }

  // 停止 FFmpeg 拉流线程与 VDEC 接收线程。ExitPipeline 在 app.Exit() 之前调用，
  // 析构再兜底调用一次，故必须幂等。返回后两个线程都已 join，不会再触发
  // SendMessage，也不会再访问本对象。
  void StopSources() override {
    if (sources_stopped_.exchange(true)) {
      return;
    }
    LOG_INFO("PreProcess::StopSources");

    // 先停上游供流，再停下游取帧，避免 VDEC 停了之后 Write 还在灌数据。
    ff_decoder_->StopDecode();
    vdec_->StopDecode();

    if (ffmpeg_started_.exchange(false)) {
      pthread_join(ffmpeg_thread_, nullptr);
    }
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

    // 节点名写错时 TaskNodeIdByName 返回 -1，后续 SendMessage(-1, ...) 会被
    // scheduler 的边界检查拒掉，表现为"整条链路静默不工作"且无根因日志。
    // 在此拦下，让启动直接失败。
    next_thread_id_ = TaskNodeIdByName("InfProcess");
    if (next_thread_id_ < 0) {
      LOG_ERROR("PreProcess: 找不到下游节点 InfProcess");
      return -3;
    }

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
    // 记录创建成功与否：Start 未跑到/失败时不能去 join 一个不存在的线程。
    if (0 == pthread_create(&ffmpeg_thread_, nullptr, FFmpegDecodeCallbackFunc,
                            this)) {
      ffmpeg_started_.store(true);
    } else {
      LOG_ERROR("PreProcess: pthread_create for FFmpeg decode failed");
    }
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
    // 映射失败时 data->data 会是空的/不完整的。此前不检查返回值就往下游发，
    // 下游 PushInput 的尺寸校验虽能拦住，但报的是"推理失败"，根因日志缺失，
    // 排查要绕一大圈（P1-4 之后还会被计入推理故障统计，更易误判）。
    ret = Copy2Host(data->data, dest);
    if (ret != 0) {
      LOG_ERROR("Copy2Host failed, ret={}", ret);
      return ret;
    }
    
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
  pthread_t ffmpeg_thread_{};
  std::atomic<bool> ffmpeg_started_{false};
  std::atomic<bool> sources_stopped_{false};
  int next_thread_id_ = -1;
};
#pragma once

#include <atomic>
#include <string>

#include "image_data.h"
#include "logger.h"
#include "ax_global_type.h"
#include "ax_venc_api.h"

typedef int (*VencProcessCallback)(AX_VENC_STREAM_T streamData, int chn,
                                   void* user_data);

class VencHelper {
 public:
  VencHelper(VENC_CHN ve_chn, int picture_width, int picture_height,
             float src_frame_rate, float dst_frame_rate)
      : chn_(ve_chn),
        picture_width_(picture_width),
        picture_height_(picture_height),
        src_frame_rate_(src_frame_rate),
        dst_frame_rate_(dst_frame_rate) {}

  int Init();
  int Encode(VencProcessCallback callback, void* user_data);
  int StopEncode();

  int WriteEOF() {
    AX_VIDEO_FRAME_INFO_T pst_frame = {0};
    memset(&pst_frame, 0x0, sizeof(AX_VIDEO_FRAME_INFO_T));
    pst_frame.bEndOfStream = AX_TRUE;
    int s32_ret = AX_VENC_SendFrame(chn_, &pst_frame, 0);
    return s32_ret;
  }

  int Write(ImageData* image_data, void* user_data);
  int Destroy();
  ~VencHelper() { Destroy(); }

  VencHelper(const VencHelper& src) = delete;
  VencHelper& operator=(const VencHelper& rhs) = delete;

  bool IsExit() { return is_stop_.load(); }

 private:
  static void* VencRecvThreadFunc(void* argv);

  VENC_CHN chn_;
  int picture_width_;
  int picture_height_;
  int src_frame_rate_;
  int dst_frame_rate_;
  std::atomic<bool> is_stop_{false};
  pthread_t recv_thd_;
  void* user_data_ = nullptr;
  VencProcessCallback callback_ = nullptr;
};

#pragma once

#include <atomic>
#include <mutex>

#include "image_data.h"
#include "ax_global_type.h"
#include "ax_vdec_type.h"

typedef int (*VdecProcessCallback)(ImageData imageData, int grp, int chn,
                                   void* user_data);

class VdecHelper {
 public:
  VdecHelper(AX_VDEC_GRP vd_grp, AX_PAYLOAD_TYPE_E codec_type,
             AX_U32 frame_width, AX_U32 frame_height, int fps = 25)
      : vd_grp_(vd_grp),
        codec_type_(codec_type),
        frame_width_(frame_width),
        frame_height_(frame_height),
        fps_(fps) {}

  VdecHelper() = delete;
  VdecHelper(const VdecHelper& src) = delete;
  VdecHelper& operator=(const VdecHelper& rhs) = delete;
  ~VdecHelper();

  int Init();
  int Destory();

  static void* RecvStreamFunc(void* argv);
  int Decode(VdecProcessCallback callback, void* user_data);
  int StopDecode();
  int WriteEOF();
  int Write(void* data, size_t data_size, void* user_data);
  AX_VDEC_GRP VdGrp() { return vd_grp_; }

 private:
  void* user_data_ = nullptr;
  std::atomic<bool> is_stop_{false};
  bool is_finished_ = false;

  AX_VDEC_GRP vd_grp_ = -1;
  AX_PAYLOAD_TYPE_E codec_type_ = PT_BUTT;
  AX_U32 frame_width_ = 0;
  AX_U32 frame_height_ = 0;
  int fps_ = 25;
  VdecProcessCallback callback_ = nullptr;
  // std::mutex callback_mutex_;
  AX_IMG_FORMAT_E img_format_{AX_FORMAT_YUV420_SEMIPLANAR};
  AX_U32 buf_size_ = 3 * 1024 * 1024;
  pthread_t recv_tid_{0};
  AX_MEMORY_ADDR_T buf_addr_;
};

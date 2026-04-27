#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "ax_buffer_tool.h"
#include "ax_global_type.h"
#include "ax_ivps_api.h"
#include "ax_ivps_type.h"
#include "ax_pool_type.h"
#include "ax_sys_api.h"
#include "ax_vdec_api.h"
#include "ax_vdec_type.h"

enum MemId {
  MEM_ID_MIN = 0x00,
  MEM_ID_VDEC = 0x01,
  MEM_ID_VENC = 0x02,
  MEM_ID_IVPS = 0x03,
  MEM_ID_IVES = 0x04,
  MEM_ID_JENC = 0x05,
  MEM_ID_JDEC = 0x06,
  MEM_ID_NPU = 0x07,
  MEM_ID_SYS = 0x08,
  MEM_ID_MAX = 0xFF
};

class FrameData {
 public:
  ~FrameData();

  FrameData& operator=(const FrameData& rhs) = delete;
  FrameData(const FrameData& src) = delete;

  static std::shared_ptr<FrameData> Create(AX_VIDEO_FRAME_INFO_T* frame_data,
                                           AX_S32 grp, AX_S32 chn,
                                           MemId mem_id) {
    return std::shared_ptr<FrameData>(
        new FrameData(frame_data, grp, chn, mem_id));
  }

  static std::shared_ptr<FrameData> Create(AX_VIDEO_FRAME_INFO_T* frame_data,
                                           MemId mem_id) {
    return std::shared_ptr<FrameData>(new FrameData(frame_data, mem_id));
  }

  AX_VIDEO_FRAME_INFO_T* FrameInfo() { return frame_data_; }

 private:
  explicit FrameData(AX_VIDEO_FRAME_INFO_T* frame_data, AX_S32 grp, AX_S32 chn,
                     MemId mem_id)
      : frame_data_(frame_data), grp_(grp), chn_(chn), mem_id_(mem_id) {}

  explicit FrameData(AX_VIDEO_FRAME_INFO_T* frame_data, MemId mem_id)
      : frame_data_(frame_data), grp_(-1), chn_(-1), mem_id_(mem_id) {}

  int Init() { return 0; }
  int Destroy();

  MemId mem_id_ = MEM_ID_MIN;
  AX_S32 grp_ = -1;
  AX_S32 chn_ = -1;
  AX_VIDEO_FRAME_INFO_T* frame_data_ = nullptr;
};
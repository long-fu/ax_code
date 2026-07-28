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
  kMemIdMin = 0x00,
  kMemIdVdec = 0x01,
  kMemIdVenc = 0x02,
  kMemIdIvps = 0x03,
  kMemIdIves = 0x04,
  kMemIdJenc = 0x05,
  kMemIdJdec = 0x06,
  kMemIdNpu = 0x07,
  kMemIdSys = 0x08,
  kMemIdMax = 0xFF
};

class FrameData {
 public:
  ~FrameData(){
    Destroy();
    delete frame_data_;
  };

  FrameData& operator=(const FrameData& rhs) = delete;
  FrameData(const FrameData& src) = delete;

  FrameData& operator=(const FrameData&& rhs) = delete;
  FrameData(FrameData&& src) = delete;  

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

  MemId mem_id_ = kMemIdMin;
  AX_S32 grp_ = -1;
  AX_S32 chn_ = -1;
  AX_VIDEO_FRAME_INFO_T* frame_data_ = nullptr;
};
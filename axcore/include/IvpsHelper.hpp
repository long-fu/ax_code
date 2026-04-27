#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "ImageData.hpp"
#include "ax_ivps_api.h"
#include "ax_ivps_type.h"

class IvpsHelper {
 public:
  IvpsHelper(IVPS_GRP ivps_grp, AX_U64 blk_size, AX_U32 blk_cnt);
  ~IvpsHelper();

  AX_S32 Resize(AX_IVPS_ASPECT_RATIO_E e_mode, AX_U32 dest_width,
                AX_U32 dest_height);
  AX_S32 Process(ImageData& dest_frame, const ImageData& src_frame);
  AX_S32 CropAndCSC(AX_IMG_FORMAT_E e_dst_pic_format, AX_U16 n_crop_x,
                    AX_U16 n_crop_y, AX_U16 n_crop_w, AX_U16 n_crop_h);
  AX_S32 ResizeAndCSC(AX_IMG_FORMAT_E e_dst_pic_format, AX_U32 n_dst_pic_width,
                      AX_U32 n_dst_pic_height);
  AX_S32 CSC(AX_IMG_FORMAT_E e_dst_pic_format);

  IVPS_GRP GetGrpID() const { return ivps_grp_; }

 private:
  AX_S32 Init();
  AX_S32 DestroyResource();
  AX_S32 CreatePool();
  AX_S32 CreateGrp();

  IVPS_GRP ivps_grp_{0};
  AX_U64 blk_size_;
  AX_U32 blk_cnt_;
  AX_IVPS_GRP_ATTR_T grp_attr_;
  AX_IVPS_PIPELINE_ATTR_T pipeline_attr_;
  AX_IVPS_POOL_ATTR_T pool_attr_;
  AX_POOL pool_id_{AX_INVALID_POOLID};
  bool is_initialized_{false};
  bool is_released_{false};
};

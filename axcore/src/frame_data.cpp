#include "frame_data.hpp"

#include "logger.h"

int FrameData::Destroy() {
  if (mem_id_ == kMemIdSys) {

    // LOG_INFO("Destory SYS Info");

    AX_S32 s_ret = AX_SUCCESS;
    if (frame_data_->stVFrame.u64PhyAddr[0] != 0) {
      s_ret = AX_SYS_MemFree(
          frame_data_->stVFrame.u64PhyAddr[0],
          reinterpret_cast<AX_VOID*>(
              static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[0])));
      frame_data_->stVFrame.u64PhyAddr[0] = 0;
      frame_data_->stVFrame.u64VirAddr[0] = 0;
    } else if (frame_data_->stVFrame.u64VirAddr[0] != 0) {
      free(reinterpret_cast<AX_VOID*>(
          static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[0])));
      frame_data_->stVFrame.u64VirAddr[0] = 0;
    }

    if (frame_data_->stVFrame.u64PhyAddr[1] != 0) {
      s_ret = AX_SYS_MemFree(
          frame_data_->stVFrame.u64PhyAddr[1],
          reinterpret_cast<AX_VOID*>(
              static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[1])));
      frame_data_->stVFrame.u64PhyAddr[1] = 0;
      frame_data_->stVFrame.u64VirAddr[1] = 0;
    } else if (frame_data_->stVFrame.u64VirAddr[1] != 0) {
      free(reinterpret_cast<AX_VOID*>(
          static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[1])));
      frame_data_->stVFrame.u64VirAddr[1] = 0;
    }

    if (frame_data_->stVFrame.u64PhyAddr[2] != 0) {
      s_ret = AX_SYS_MemFree(
          frame_data_->stVFrame.u64PhyAddr[2],
          reinterpret_cast<AX_VOID*>(
              static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[2])));
      frame_data_->stVFrame.u64PhyAddr[2] = 0;
      frame_data_->stVFrame.u64VirAddr[2] = 0;
    } else if (frame_data_->stVFrame.u64VirAddr[2] != 0) {
      free(reinterpret_cast<AX_VOID*>(
          static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[2])));
      frame_data_->stVFrame.u64VirAddr[2] = 0;
    }

    return s_ret;
  }

  AX_U32 n_pixel_size;
  AX_S32 bit_num = 0;
  AX_U8 n_storage_planar_num = 0;

  n_pixel_size = static_cast<AX_U32>(
      frame_data_->stVFrame.u32PicStride[0] * frame_data_->stVFrame.u32Height);

  switch (frame_data_->stVFrame.enImgFormat) {
    case AX_FORMAT_YUV420_PLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR_VU:
    case AX_FORMAT_YUV422_SEMIPLANAR:
    case AX_FORMAT_YUV422_SEMIPLANAR_VU:
      bit_num = 8;
      n_storage_planar_num = 2;
      break;
    case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
    case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
      bit_num = 10;
      n_storage_planar_num = 2;
      break;
    case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
    case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
      bit_num = 16;
      n_storage_planar_num = 2;
      break;
    case AX_FORMAT_YUV444_PACKED:
    case AX_FORMAT_RGB888:
    case AX_FORMAT_BGR888:
    case AX_FORMAT_RGB565:
    case AX_FORMAT_BGR565:
    case AX_FORMAT_RGBA8888:
    case AX_FORMAT_ARGB8888:
    case AX_FORMAT_ARGB4444:
    case AX_FORMAT_ARGB1555:
    case AX_FORMAT_ARGB8565:
    case AX_FORMAT_RGBA5551:
    case AX_FORMAT_RGBA4444:
    case AX_FORMAT_RGBA5658:
    case AX_FORMAT_ABGR4444:
    case AX_FORMAT_ABGR1555:
    case AX_FORMAT_ABGR8888:
    case AX_FORMAT_ABGR8565:
    case AX_FORMAT_BGRA8888:
    case AX_FORMAT_BGRA5551:
    case AX_FORMAT_BGRA4444:
    case AX_FORMAT_BGRA5658:
    case AX_FORMAT_YUV400:
      n_storage_planar_num = 1;
      break;
    default:
      return -1;
  }

  switch (n_storage_planar_num) {
    case 2:
      if (!frame_data_->stVFrame.u64PhyAddr[1]) {
        frame_data_->stVFrame.u64PhyAddr[1] =
            frame_data_->stVFrame.u64PhyAddr[0] +
            frame_data_->stVFrame.u32PicStride[0] *
                frame_data_->stVFrame.u32Height;
      }
      n_pixel_size = n_pixel_size * bit_num / 8;
      if (AX_FORMAT_YUV422_SEMIPLANAR ==
              frame_data_->stVFrame.enImgFormat ||
          AX_FORMAT_YUV422_SEMIPLANAR_VU ==
              frame_data_->stVFrame.enImgFormat ||
          AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 ==
              frame_data_->stVFrame.enImgFormat ||
          AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 ==
              frame_data_->stVFrame.enImgFormat) {
        if ((frame_data_->stVFrame.u64VirAddr[0] != 0) &&
            (frame_data_->stVFrame.u64VirAddr[1] != 0)) {
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(
                  static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[0])),
              n_pixel_size);
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(
                  static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[1])),
              n_pixel_size);
          frame_data_->stVFrame.u64VirAddr[0] = 0;
          frame_data_->stVFrame.u64VirAddr[1] = 0;
        }
      } else {
        if ((frame_data_->stVFrame.u64VirAddr[0] != 0) &&
            (frame_data_->stVFrame.u64VirAddr[1] != 0)) {
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(
                  static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[0])),
              n_pixel_size);
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(
                  static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[1])),
              n_pixel_size / 2);
          frame_data_->stVFrame.u64VirAddr[0] = 0;
          frame_data_->stVFrame.u64VirAddr[1] = 0;
        }
      }
      break;
    case 3: {
      if ((frame_data_->stVFrame.u64VirAddr[0] != 0) &&
          (frame_data_->stVFrame.u64VirAddr[1] != 0) &&
          (frame_data_->stVFrame.u64VirAddr[2] != 0)) {
    (void)AX_SYS_Munmap(
            reinterpret_cast<AX_VOID*>(
                static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[0])),
            n_pixel_size);
    (void)AX_SYS_Munmap(
            reinterpret_cast<AX_VOID*>(
                static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[1])),
            n_pixel_size / 2);
    (void)AX_SYS_Munmap(
            reinterpret_cast<AX_VOID*>(
                static_cast<AX_ULONG>(frame_data_->stVFrame.u64VirAddr[2])),
            n_pixel_size / 2);
        frame_data_->stVFrame.u64VirAddr[0] = 0;
        frame_data_->stVFrame.u64VirAddr[1] = 0;
        frame_data_->stVFrame.u64VirAddr[2] = 0;
      }
      break;
    }
    default:
      if (frame_data_->stVFrame.u32FrameSize) {
        if (frame_data_->stVFrame.u64VirAddr[0] != 0) {
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(static_cast<AX_ULONG>(
                  frame_data_->stVFrame.u64VirAddr[0])),
              frame_data_->stVFrame.u32FrameSize);
          frame_data_->stVFrame.u64VirAddr[0] = 0;
        }
      } else {
        if (frame_data_->stVFrame.u64VirAddr[0] != 0) {
    (void)AX_SYS_Munmap(
              reinterpret_cast<AX_VOID*>(static_cast<AX_ULONG>(
                  frame_data_->stVFrame.u64VirAddr[0])),
              n_pixel_size * 3);
          frame_data_->stVFrame.u64VirAddr[0] = 0;
        }
      }
      break;
  }

  if (mem_id_ == kMemIdIvps) {
    // LOG_INFO("Destory IVPS Info");
    (void)AX_IVPS_ReleaseChnFrame(grp_, chn_, &frame_data_->stVFrame);
  } else if (mem_id_ == kMemIdVdec) {
    (void)AX_VDEC_ReleaseChnFrame(grp_, chn_, frame_data_);
  }
  
  return 0;
}
#include "image_data.h"

#include <ax_sys_api.h>
#include <memory.h>

#include <opencv2/opencv.hpp>
#include <vector>

#include "jpeg_help.h"
#include "logger.h"

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#endif

namespace {

// 格式 -> planar 数量与每像素位宽。位宽仅对 2-planar 的 YUV 生效。
// 注意：AX_FORMAT_YUV420_PLANAR 在本工程里一直按 2-planar 处理，
// 与 FrameData::Destroy() 保持一致，此处沿用。
bool PlanarNumAndBits(AX_IMG_FORMAT_E fmt, AX_U8& planar_num, AX_S32& bit_num)
{
	switch (fmt)
	{
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
		bit_num = 8;
		planar_num = 2;
		return true;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		bit_num = 10;
		planar_num = 2;
		return true;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		bit_num = 16;
		planar_num = 2;
		return true;
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
		bit_num = 0;
		planar_num = 1;
		return true;
	default:
		return false;
	}
}

// NV16 系列的 UV 平面与 luma 等大，其余 semiplanar 为一半。
bool IsYuv422SemiPlanar(AX_IMG_FORMAT_E fmt)
{
	return fmt == AX_FORMAT_YUV422_SEMIPLANAR ||
		   fmt == AX_FORMAT_YUV422_SEMIPLANAR_VU ||
		   fmt == AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 ||
		   fmt == AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010;
}

// 2-planar 帧若未给出 UV 物理地址，按 luma 平面推导。
void FillDerivedUvPhyAddr(AX_VIDEO_FRAME_T& vframe)
{
	if (vframe.u64PhyAddr[1] == 0)
	{
		vframe.u64PhyAddr[1] =
			vframe.u64PhyAddr[0] +
			(AX_U64)vframe.u32PicStride[0] * vframe.u32Height;
	}
}

bool FrameValid(const ImageData& img)
{
	return img.data != nullptr && img.data->FrameInfo() != nullptr;
}

}  // namespace

bool ComputePlaneLayout(const AX_VIDEO_FRAME_T& vframe, PlaneLayout& layout)
{
	layout = PlaneLayout{};

	AX_S32 bit_num = 0;
	if (!PlanarNumAndBits(vframe.enImgFormat, layout.planar_num, bit_num))
	{
		return false;
	}

	const AX_U32 base = (AX_U32)vframe.u32PicStride[0] * vframe.u32Height;

	if (layout.planar_num == 2)
	{
		const AX_U32 luma = base * bit_num / 8;
		layout.size[0] = luma;
		layout.size[1] = IsYuv422SemiPlanar(vframe.enImgFormat) ? luma : luma / 2;
	}
	else
	{
		layout.size[0] = vframe.u32FrameSize ? vframe.u32FrameSize : base * 3;
	}

	return true;
}

int EnsureMapped(ImageData& img)
{
	if (!FrameValid(img))
	{
		LOG_ERROR("EnsureMapped: invalid frame");
		return -1;
	}

	AX_VIDEO_FRAME_T& vframe = img.data->FrameInfo()->stVFrame;

	PlaneLayout layout;
	if (!ComputePlaneLayout(vframe, layout))
	{
		LOG_ERROR("EnsureMapped: unsupported format {}",
				  (int)vframe.enImgFormat);
		return -2;
	}

	if (vframe.u64VirAddr[0] != 0)
	{
		return 0;
	}

	if (vframe.u64PhyAddr[0] == 0)
	{
		LOG_ERROR("EnsureMapped: frame has no phy addr");
		return -3;
	}

	// kMemIdSys 帧的缓冲来自 AX_SYS_MemAlloc，虚拟地址随分配一并给出，
	// 且 Destroy() 走 MemFree 而非 munmap。此时若还去 mmap，
	// 地址会被当成 MemFree 的入参，属于构造帧时就漏填 VirAddr。
	if (img.data->MemIdOf() == kMemIdSys)
	{
		LOG_ERROR("EnsureMapped: kMemIdSys frame missing vir addr");
		return -5;
	}

	if (layout.planar_num == 2)
	{
		FillDerivedUvPhyAddr(vframe);
	}

	for (AX_U8 i = 0; i < layout.planar_num; ++i)
	{
		AX_VOID* vir = AX_SYS_Mmap(vframe.u64PhyAddr[i], layout.size[i]);
		if (vir == nullptr)
		{
			LOG_ERROR("EnsureMapped: mmap plane {} size {} failed", i,
					  layout.size[i]);
			// 回滚，避免留下半映射状态导致 Destroy() 漏 munmap。
			for (AX_U8 k = 0; k < i; ++k)
			{
				(void)AX_SYS_Munmap(
					(AX_VOID*)(AX_ULONG)vframe.u64VirAddr[k], layout.size[k]);
				vframe.u64VirAddr[k] = 0;
			}
			return -4;
		}
		vframe.u64VirAddr[i] = (AX_ULONG)vir;
	}

	return 0;
}

int Unmap(ImageData& img)
{
	if (!FrameValid(img))
	{
		return -1;
	}

	AX_VIDEO_FRAME_T& vframe = img.data->FrameInfo()->stVFrame;

	PlaneLayout layout;
	if (!ComputePlaneLayout(vframe, layout))
	{
		LOG_ERROR("Unmap: unsupported format {}", (int)vframe.enImgFormat);
		return -1;
	}

	if (layout.planar_num == 2)
	{
		FillDerivedUvPhyAddr(vframe);
	}

	AX_S32 first_err = 0;
	for (AX_U8 i = 0; i < layout.planar_num; ++i)
	{
		if (vframe.u64VirAddr[i] == 0)
		{
			continue;
		}
		const AX_S32 ret = AX_SYS_Munmap(
			(AX_VOID*)(AX_ULONG)vframe.u64VirAddr[i], layout.size[i]);
		if (ret != 0)
		{
			LOG_ERROR("Unmap: munmap plane {} failed, code {:#x}", i, ret);
			if (first_err == 0)
			{
				first_err = ret;
			}
		}
		vframe.u64VirAddr[i] = 0;
	}

	return first_err;
}

int Clone(ImageData& dest, ImageData& src)
{
	if (!FrameValid(src))
	{
		LOG_ERROR("Clone: invalid source frame");
		return -1;
	}

	AX_VIDEO_FRAME_T& src_vframe = src.data->FrameInfo()->stVFrame;

	PlaneLayout layout;
	if (!ComputePlaneLayout(src_vframe, layout))
	{
		LOG_ERROR("Clone: unsupported format {}", (int)src_vframe.enImgFormat);
		return -2;
	}

	if (EnsureMapped(src) != 0)
	{
		return -4;
	}

	dest.width = src.width;
	dest.height = src.height;
	dest.img_format = src.img_format;
	dest.end_of_stream = src.end_of_stream;

	static const char* kMemToken = "Clone";

	AX_VIDEO_FRAME_INFO_T* frame_info = new AX_VIDEO_FRAME_INFO_T();
	memcpy(frame_info, src.data->FrameInfo(), sizeof(AX_VIDEO_FRAME_INFO_T));

	// 立刻清掉继承来的地址：dest 用自己的 CMM 缓冲。
	// 否则中途分配失败时 FrameData::Destroy() 会去 free 源帧的物理地址。
	for (int i = 0; i < 3; ++i)
	{
		frame_info->stVFrame.u64PhyAddr[i] = 0;
		frame_info->stVFrame.u64VirAddr[i] = 0;
	}

	dest.data = FrameData::Create(frame_info, kMemIdSys);

	for (AX_U8 i = 0; i < layout.planar_num; ++i)
	{
		AX_MEMORY_ADDR_T buf;
		const AX_S32 ret =
			AX_SYS_MemAlloc(&buf.u64PhyAddr, (AX_VOID**)&buf.pVirAddr,
							layout.size[i], 0x0, (AX_S8*)kMemToken);
		if (ret != 0)
		{
			LOG_ERROR("Clone: MemAlloc plane {} size {} failed, code {:#x}", i,
					  layout.size[i], ret);
			// 已分配的平面随 dest.data 析构释放。
			return -3;
		}

		frame_info->stVFrame.u64PhyAddr[i] = buf.u64PhyAddr;
		frame_info->stVFrame.u64VirAddr[i] = (AX_ULONG)buf.pVirAddr;

		memcpy(buf.pVirAddr,
			   (const AX_VOID*)(AX_ULONG)src_vframe.u64VirAddr[i],
			   layout.size[i]);
	}

	return 0;
}

/// @brief 把Image编码成Jpeg格式的图片
int JpegEncode(std::vector<uint8_t>& dest, ImageData const& src)
{
	if (!FrameValid(src))
	{
		LOG_ERROR("JpegEncode: invalid frame");
		return -1;
	}
	return JpegHelp::JpegEncode(dest, src.data->FrameInfo());
}

int JpegDecode(ImageData& dest, std::string const& jpegFile)
{
	AX_VIDEO_FRAME_INFO_T* frame_info = nullptr;
	int ret = JpegHelp::JpegDecode(&frame_info, jpegFile);
	if (ret != 0 || frame_info == nullptr)
	{
		LOG_ERROR("JpegDecode {}", ret);
		if (frame_info != nullptr)
		{
			delete frame_info;
		}
		return ret != 0 ? ret : -1;
	}
	dest.width = frame_info->stVFrame.u32Width;
	dest.height = frame_info->stVFrame.u32Height;
	dest.img_format = frame_info->stVFrame.enImgFormat;
	dest.data = FrameData::Create(frame_info, kMemIdSys);

	return ret;
}

/// @brief 按 plane 顺序拼接到 host 内存（已按 stride 计算每平面大小）
int Copy2Host(std::vector<uint8_t>& dest, ImageData& src)
{
	if (!FrameValid(src))
	{
		LOG_ERROR("Copy2Host: invalid frame");
		return -1;
	}

	AX_VIDEO_FRAME_T& vframe = src.data->FrameInfo()->stVFrame;

	PlaneLayout layout;
	if (!ComputePlaneLayout(vframe, layout))
	{
		LOG_ERROR("Copy2Host: unsupported format {}", (int)vframe.enImgFormat);
		return -2;
	}

	const int map_ret = EnsureMapped(src);
	if (map_ret != 0)
	{
		return map_ret;
	}

	size_t total = 0;
	for (AX_U8 i = 0; i < layout.planar_num; ++i)
	{
		total += layout.size[i];
	}
	dest.resize(total);

	size_t offset = 0;
	for (AX_U8 i = 0; i < layout.planar_num; ++i)
	{
		memcpy(dest.data() + offset,
			   (const AX_VOID*)(AX_ULONG)vframe.u64VirAddr[i],
			   layout.size[i]);
		offset += layout.size[i];
	}

	return 0;
}

/// @brief 拷进 cv::Mat。仅接受 AX_FORMAT_RGB888，
/// 通道顺序即 IVPS 输出的字节序，不做 CSC。
int Copy2Mat(cv::Mat& dest, ImageData& src)
{
	if (!FrameValid(src))
	{
		LOG_ERROR("Copy2Mat: invalid frame");
		return -1;
	}

	AX_VIDEO_FRAME_T& vframe = src.data->FrameInfo()->stVFrame;

	if (vframe.enImgFormat != AX_FORMAT_RGB888)
	{
		LOG_ERROR("Copy2Mat: need AX_FORMAT_RGB888, got {}",
				  (int)vframe.enImgFormat);
		return -1;
	}

	if (vframe.u64PhyAddr[0] == 0 || vframe.u32PicStride[0] == 0)
	{
		LOG_ERROR("Copy2Mat: frame has no phy addr / stride");
		return -1;
	}

	if (EnsureMapped(src) != 0)
	{
		return -1;
	}

	const AX_U32 width = vframe.u32Width;
	const AX_U32 height = vframe.u32Height;
	const AX_U32 stride = vframe.u32PicStride[0];

	const uint8_t* p_lu = (const uint8_t*)(AX_ULONG)vframe.u64VirAddr[0];

	dest = cv::Mat(height, width, CV_8UC3);
	uint8_t* mat_data = dest.data;
	for (AX_U32 i = 0; i < height; ++i)
	{
		memcpy(mat_data, p_lu, (size_t)width * 3);
		p_lu += (size_t)stride * 3;
		mat_data += (size_t)width * 3;
	}

	return 0;
}

#include "ImageData.hpp"

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#endif

static AX_U32 CalcImgSize(AX_U32 nStride, AX_U32 nW, AX_U32 nH, AX_IMG_FORMAT_E eType, AX_U32 nAlign)
{
	AX_U32 nBpp = 0;
	if (nW == 0 || nH == 0)
	{
		// LOG_ERROR("Invalid width %d or height %d!", nW, nH);
		// LOG(ERROR) << "Invalid width or height " << nW << "x" << nH;
		return 0;
	}

	if (0 == nStride)
	{
		nStride = (0 == nAlign) ? nW : ALIGN_UP(nW, nAlign);
	}
	else
	{
		if (nAlign > 0)
		{
			if (nStride % nAlign)
			{
				// LOG_ERROR("stride: %u not %u aligned.!", nStride, nAlign);
				// LOG(ERROR) << "stride: not aligned.!" << nStride << " " << nAlign;
				return 0;
			}
		}
	}

	switch (eType)
	{
	case AX_FORMAT_YUV400:
		nBpp = 8;
		break;
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
		nBpp = 12;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
		nBpp = 15;
		break;
	case AX_FORMAT_YUV422_INTERLEAVED_YUYV:
	case AX_FORMAT_YUV422_INTERLEAVED_UYVY:
	case AX_FORMAT_YUV422_SEMIPLANAR:
	case AX_FORMAT_RGB565:
	case AX_FORMAT_BGR565:
	case AX_FORMAT_ARGB4444:
	case AX_FORMAT_RGBA4444:
	case AX_FORMAT_ABGR4444:
	case AX_FORMAT_BGRA4444:
	case AX_FORMAT_RGBA5551:
	case AX_FORMAT_ARGB1555:
	case AX_FORMAT_ABGR1555:
	case AX_FORMAT_BGRA5551:
		nBpp = 16;
		break;
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		nBpp = 20;
		break;
	case AX_FORMAT_YUV444_PACKED:
	case AX_FORMAT_RGB888:
	case AX_FORMAT_BGR888:
	case AX_FORMAT_ARGB8565:
	case AX_FORMAT_RGBA5658:
	case AX_FORMAT_ABGR8565:
	case AX_FORMAT_BGRA5658:
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
		nBpp = 24;
		break;
	case AX_FORMAT_RGBA8888:
	case AX_FORMAT_ARGB8888:
	case AX_FORMAT_BGRA8888:
	case AX_FORMAT_ABGR8888:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		nBpp = 32;
		break;
	default:
		nBpp = 0;
		break;
	}

	return nStride * nH * nBpp / 8;
}

#include <memory.h>

int Clone(ImageData &dest, ImageData const &src)
{
	dest.u32Width = src.u32Width;
	dest.u32Height = src.u32Height;
	dest.enImgFormat = src.enImgFormat;

	const static char *MEM_TOKEN = "Clone";
	AX_VIDEO_FRAME_INFO_T *frameInfo = new AX_VIDEO_FRAME_INFO_T();
	dest.data = FrameData::Create(frameInfo, MEM_ID_SYS);

	memcpy(frameInfo, src.data->FrameInfo(), sizeof(AX_VIDEO_FRAME_INFO_T));

	AX_U32 nPixelSize = (AX_U32)src.data->FrameInfo()->stVFrame.u32PicStride[0] * src.data->FrameInfo()->stVFrame.u32Height;

	memset(&frameInfo->stVFrame, 0x0, sizeof(AX_VIDEO_FRAME_T));
	memcpy(&frameInfo->stVFrame, &src.data->FrameInfo()->stVFrame, sizeof(AX_VIDEO_FRAME_T));

	AX_S32 sRet = 0;
	AX_S32 bit_num = 0;
	AX_U8 nStoragePlanarNum = 0;

	switch (src.data->FrameInfo()->stVFrame.enImgFormat)
	{
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
		bit_num = 8;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		bit_num = 10;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		bit_num = 16;
		nStoragePlanarNum = 2;
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
		nStoragePlanarNum = 1;
		break;
	default:
		return -2;
	}
	AX_MEMORY_ADDR_T tBufAddr;
	switch (nStoragePlanarNum)
	{
	case 2:
		if (!src.data->FrameInfo()->stVFrame.u64PhyAddr[1])
		{
			src.data->FrameInfo()->stVFrame.u64PhyAddr[1] = src.data->FrameInfo()->stVFrame.u64PhyAddr[0] + src.data->FrameInfo()->stVFrame.u32PicStride[0] * src.data->FrameInfo()->stVFrame.u32Height;
		}
		nPixelSize = nPixelSize * bit_num / 8;
		if (AX_FORMAT_YUV422_SEMIPLANAR == src.data->FrameInfo()->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == src.data->FrameInfo()->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == src.data->FrameInfo()->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == src.data->FrameInfo()->stVFrame.enImgFormat)
		{

			if (src.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
			{
				src.data->FrameInfo()->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[0], nPixelSize);
				src.data->FrameInfo()->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[1], nPixelSize);
			}

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);

			if (sRet)
			{
				return -3;
			}

			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[0]), nPixelSize);

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
			if (sRet)
			{
				return -3;
			}

			frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[1]), nPixelSize);
		}
		else
		{
			if (src.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
			{
				src.data->FrameInfo()->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[0], nPixelSize);
				src.data->FrameInfo()->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[1], nPixelSize / 2);
			}

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
			if (sRet)
			{
				return -3;
			}
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;

			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[0]), nPixelSize);

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
			if (sRet)
			{
				return -3;
			}
			frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[1]), nPixelSize / 2);
		}
		break;
	case 3:
		if (src.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
		{
			src.data->FrameInfo()->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[0], nPixelSize);
			src.data->FrameInfo()->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[1], nPixelSize / 2);
			src.data->FrameInfo()->stVFrame.u64VirAddr[2] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[2], nPixelSize / 2);
		}

		sRet = AX_SYS_MemAlloc(
			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
			nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
		if (sRet)
		{
			return -3;
		}
		frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
		frameInfo->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
		memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[0]), nPixelSize);

		sRet = AX_SYS_MemAlloc(
			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
			nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
		if (sRet)
		{
			return -3;
		}

		frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
		frameInfo->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
		memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[1]), nPixelSize / 2);

		sRet = AX_SYS_MemAlloc(
			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
			nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
		if (sRet)
		{
			return -3;
		}

		frameInfo->stVFrame.u64VirAddr[2] = (AX_ULONG)tBufAddr.pVirAddr;
		frameInfo->stVFrame.u64PhyAddr[2] = tBufAddr.u64PhyAddr;
		memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[2], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[2]), nPixelSize / 2);

		break;
	default:
		if (src.data->FrameInfo()->stVFrame.u32FrameSize)
		{
			if (src.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
			{
				src.data->FrameInfo()->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[0], src.data->FrameInfo()->stVFrame.u32FrameSize);
			}

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				src.data->FrameInfo()->stVFrame.u32FrameSize, 0x0, (AX_S8 *)MEM_TOKEN);
			if (sRet)
			{
				return -3;
			}

			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[0]), src.data->FrameInfo()->stVFrame.u32FrameSize);
		}
		else
		{
			if (src.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
			{
				src.data->FrameInfo()->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(src.data->FrameInfo()->stVFrame.u64PhyAddr[0], nPixelSize * 3);
			}

			sRet = AX_SYS_MemAlloc(
				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
				nPixelSize * 3, 0x0, (AX_S8 *)MEM_TOKEN);
			if (sRet)
			{
				return -3;
			}

			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
			frameInfo->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
			memcpy((AX_VOID *)frameInfo->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)src.data->FrameInfo()->stVFrame.u64VirAddr[0]), nPixelSize * 3);
		}
		break;
	}

	// FrameData data;
	// src.data->Clone(data);
	// auto value1 = std::shared_ptr<FrameData>(data); 直接把指针对象转换成功 ,值对象无法转换
	// dest.data = std::make_shared<FrameData>(data);

	return 0;
}

#include <vector>
#include "ax_venc_api.h"
#include "ax_ivps_api.h"
#include "JpegHelp.hpp"

/// @brief 把Image编码成Jpeg格式的图片
/// @param dest
/// @param src
/// @return
int JpegEncode(std::vector<uint8_t> &dest, ImageData const &src)
{
	return JpegHelp::JpegEncode(dest, src.data->FrameInfo());
}

int JpegDecode(ImageData &dest, std::string const &jpegFile)
{
	AX_VIDEO_FRAME_INFO_T *frame_info = nullptr;
	int ret = JpegHelp::JpegDecode(frame_info, jpegFile);
	dest.u32Width = frame_info->stVFrame.u32Width;
	dest.u32Height = frame_info->stVFrame.u32Height;
	dest.enImgFormat = frame_info->stVFrame.enImgFormat;
	dest.data = FrameData::Create(frame_info, MEM_ID_SYS);

	return ret;
}

/// @brief 要考虑数据 stride
/// @param dest
/// @param src
/// @return
int Copy2Host(std::vector<uint8_t> &dest, ImageData const &src)
{
	// const static char *MEM_TOKEN = "Clone";

	AX_VIDEO_FRAME_INFO_T *frameInfo = src.data->FrameInfo();
	AX_U32 nPixelSize = (AX_U32)frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;

	// AX_S32 sRet = 0;
	AX_S32 bit_num = 0;
	AX_U8 nStoragePlanarNum = 0;

	switch (frameInfo->stVFrame.enImgFormat)
	{
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
		bit_num = 8;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		bit_num = 10;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		bit_num = 16;
		nStoragePlanarNum = 2;
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
		nStoragePlanarNum = 1;
		break;
	default:
		return -2;
	}
	// AX_MEMORY_ADDR_T tBufAddr;
	switch (nStoragePlanarNum)
	{
	case 2:
		if (!frameInfo->stVFrame.u64PhyAddr[1])
		{
			frameInfo->stVFrame.u64PhyAddr[1] = frameInfo->stVFrame.u64PhyAddr[0] + frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;
		}
		nPixelSize = nPixelSize * bit_num / 8;
		if (AX_FORMAT_YUV422_SEMIPLANAR == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == frameInfo->stVFrame.enImgFormat)
		{

			if (frameInfo->stVFrame.u64VirAddr[0] == 0)
			{
				frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
				frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize);
			}
			dest.resize(nPixelSize + nPixelSize);

			memcpy(dest.data(), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[0]), nPixelSize);

			memcpy(dest.data() + nPixelSize, (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[1]), nPixelSize);
		}
		else
		{
			if (frameInfo->stVFrame.u64VirAddr[0] == 0)
			{
				frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
				frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize / 2);
			}
			dest.resize(nPixelSize + (nPixelSize / 2));

			memcpy(dest.data(), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[0]), nPixelSize);

			memcpy(dest.data() + nPixelSize, (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[1]), nPixelSize / 2);
		}
		break;
	case 3:
		if (frameInfo->stVFrame.u64VirAddr[0] == 0)
		{
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
			frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize / 2);
			frameInfo->stVFrame.u64VirAddr[2] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[2], nPixelSize / 2);
		}
		dest.resize(nPixelSize + (nPixelSize / 2) + (nPixelSize / 2));

		memcpy(dest.data(), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[0]), nPixelSize);

		memcpy(dest.data() + nPixelSize, (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[1]), nPixelSize / 2);

		memcpy(dest.data() + (nPixelSize + (nPixelSize / 2)), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[2]), nPixelSize / 2);
		break;
	default:
		if (frameInfo->stVFrame.u32FrameSize)
		{
			if (frameInfo->stVFrame.u64VirAddr[0] == 0)
			{
				frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], frameInfo->stVFrame.u32FrameSize);
			}
			dest.resize(frameInfo->stVFrame.u32FrameSize);

			memcpy(dest.data(), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[0]), frameInfo->stVFrame.u32FrameSize);
		}
		else
		{
			if (frameInfo->stVFrame.u64VirAddr[0] == 0)
			{
				frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize * 3);
			}
			dest.resize(nPixelSize * 3);

			memcpy(dest.data(), (AX_VOID *)((AX_ULONG)frameInfo->stVFrame.u64VirAddr[0]), nPixelSize * 3);
		}
		break;
	}

	return 0;
};

#include <opencv2/opencv.hpp>

/// @brief 调用此函数之前请保证数据是BGR格式, 不是请调用IVPS进行CSC进行格式变换
/// @param dest
/// @param src
/// @return
int Copy2Mat(cv::Mat &dest, ImageData const &src)
{
	// int ret = 0;
	AX_S32 sRet = 0;
	AX_U32 i;
	uint8_t *p_lu = NULL;

	AX_S32 s32Ret = 0;
	AX_VOID *pLumaVirAddr = NULL;

	AX_U32 lumaMapSize = 0;

	AX_VIDEO_FRAME_INFO_T *frameInfo = src.data->FrameInfo();
	AX_U32 coded_width = frameInfo->stVFrame.u32Width;
	AX_U32 coded_height = frameInfo->stVFrame.u32Height;
	AX_U32 pic_stride = frameInfo->stVFrame.u32PicStride[0];
	// AX_U32 coded_width_ch = frameInfo->stVFrame.u32Width;
	// AX_U32 coded_h_ch = frameInfo->stVFrame.u32Height / 2;
	// AX_U32 pic_stride_ch = frameInfo->stVFrame.u32PicStride[1];
	// AX_U32 pic_format = frameInfo->stVFrame.enImgFormat;

	if (frameInfo->stVFrame.enImgFormat != AX_FORMAT_RGB888)
	{
		return -1;
	}

	if ((frameInfo->stVFrame.u64PhyAddr[0] == 0) ||
		(frameInfo->stVFrame.u32PicStride[0] == 0))
	{
		return -1;
	}

	if (frameInfo->stVFrame.u32FrameSize)
	{
		lumaMapSize = frameInfo->stVFrame.u32FrameSize;
	}
	else
	{
		lumaMapSize = frameInfo->stVFrame.u32PicStride[0] * coded_height * 3;
	}

	if (frameInfo->stVFrame.u64VirAddr[0] == 0)
	{
		pLumaVirAddr = AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], lumaMapSize);
	}
	else
	{
		pLumaVirAddr = (AX_VOID *)frameInfo->stVFrame.u64VirAddr[0];
	}

	if (NULL == pLumaVirAddr)
	{
		return -1;
	}

	p_lu = (uint8_t *)pLumaVirAddr;

	dest = cv::Mat(coded_height, coded_width, CV_8UC3);
	uint8_t *mat_data = dest.data;
	for (i = 0; i < coded_height; i++)
	{
		memcpy(mat_data, p_lu, coded_width * 3);
		p_lu += pic_stride * 3;
		mat_data += coded_width * 3;
	}

	return s32Ret || sRet;
};

/// @brief 只对解码通道出来视频帧数据进行Map
/// @param frameInfo
/// @return
int Map(ImageData &img)
{
	AX_VIDEO_FRAME_INFO_T *frameInfo = img.data->FrameInfo();
	AX_U32 nPixelSize;
	AX_S32 bit_num = 0;
	AX_U8 nStoragePlanarNum = 0;

	nPixelSize = (AX_U32)frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;

	switch (frameInfo->stVFrame.enImgFormat)
	{
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
		bit_num = 8;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		bit_num = 10;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		bit_num = 16;
		nStoragePlanarNum = 2;
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
		nStoragePlanarNum = 1;
		break;
	default:
		// LOG_ERROR("FrameInfoMap not support fromat %d", frameInfo->stVFrame.enImgFormat);
		// LOG(ERROR) << "FrameInfoMap not support froma: " << frameInfo->stVFrame.enImgFormat;
		return -1;
		break;
	}

	if (frameInfo->stVFrame.u64VirAddr[0] != 0)
	{
		// LOG_ERROR("FrameInfoMap memery is maped ");
		// LOG(INFO) << "FrameInfoMap memery is maped";
		return 0;
	}

	switch (nStoragePlanarNum)
	{
	case 2:
		if (!frameInfo->stVFrame.u64PhyAddr[1])
		{
			frameInfo->stVFrame.u64PhyAddr[1] = frameInfo->stVFrame.u64PhyAddr[0] + frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;
		}
		nPixelSize = nPixelSize * bit_num / 8;
		if (AX_FORMAT_YUV422_SEMIPLANAR == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == frameInfo->stVFrame.enImgFormat)
		{
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
			frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize);
		}
		else
		{
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
			frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize / 2);
		}
		break;
	case 3:
		frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize);
		frameInfo->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[1], nPixelSize / 2);
		frameInfo->stVFrame.u64VirAddr[2] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[2], nPixelSize / 2);
		break;
	default:
		if (frameInfo->stVFrame.u32FrameSize)
		{
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], frameInfo->stVFrame.u32FrameSize);
		}
		else
		{
			frameInfo->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(frameInfo->stVFrame.u64PhyAddr[0], nPixelSize * 3);
		}
		break;
	}

	return 0;
}

/// @brief 只对解码通道出来视频帧数据进行unmap
/// @param frameInfo
/// @return
int Unmap(ImageData &img)
{
	AX_VIDEO_FRAME_INFO_T *frameInfo = img.data->FrameInfo();
	AX_U32 nPixelSize;
	AX_S32 s32Ret1 = 0;
	AX_S32 s32Ret2 = 0;
	AX_S32 s32Ret3 = 0;
	AX_S32 bit_num = 0;
	AX_S32 sRet = 0;
	AX_U8 nStoragePlanarNum = 0;
	AX_MEMORY_ADDR_T tBufAddr;

	nPixelSize = (AX_U32)frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;

	switch (frameInfo->stVFrame.enImgFormat)
	{
	case AX_FORMAT_YUV420_PLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR:
	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
		bit_num = 8;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
		bit_num = 10;
		nStoragePlanarNum = 2;
		break;
	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
		bit_num = 16;
		nStoragePlanarNum = 2;
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
		nStoragePlanarNum = 1;
		break;
	default:
		// LOG_ERROR("FrameInfoUnmap not support fromat %d", frameInfo->stVFrame.enImgFormat);
		// LOG(ERROR) << "FrameInfoUnmap not support fromat " << frameInfo->stVFrame.enImgFormat;
		return -1;
	}

	switch (nStoragePlanarNum)
	{
	case 2:
		if (!frameInfo->stVFrame.u64PhyAddr[1])
		{
			frameInfo->stVFrame.u64PhyAddr[1] = frameInfo->stVFrame.u64PhyAddr[0] + frameInfo->stVFrame.u32PicStride[0] * frameInfo->stVFrame.u32Height;
		}
		nPixelSize = nPixelSize * bit_num / 8;
		if (AX_FORMAT_YUV422_SEMIPLANAR == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == frameInfo->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == frameInfo->stVFrame.enImgFormat)
		{

			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[0], nPixelSize);
			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[1], nPixelSize);
			frameInfo->stVFrame.u64VirAddr[0] = 0;
			frameInfo->stVFrame.u64VirAddr[1] = 0;
		}
		else
		{
			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[0], nPixelSize);
			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[1], nPixelSize / 2);
			frameInfo->stVFrame.u64VirAddr[0] = 0;
			frameInfo->stVFrame.u64VirAddr[1] = 0;
		}
		break;
	case 3:
	{
		s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[0], nPixelSize);
		s32Ret2 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[1], nPixelSize / 2);
		s32Ret3 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[2], nPixelSize / 2);
		frameInfo->stVFrame.u64VirAddr[0] = 0;
		frameInfo->stVFrame.u64VirAddr[1] = 0;
		frameInfo->stVFrame.u64VirAddr[2] = 0;
	}

	break;
	default:
		if (frameInfo->stVFrame.u32FrameSize)
		{

			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[0], frameInfo->stVFrame.u32FrameSize);
			frameInfo->stVFrame.u64VirAddr[0] = 0;
		}
		else
		{

			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)frameInfo->stVFrame.u64VirAddr[0], nPixelSize * 3);
			frameInfo->stVFrame.u64VirAddr[0] = 0;
		}
		break;
	}

	if (s32Ret1 || s32Ret2 || s32Ret3)
	{
		// LOG_ERROR("FrameInfoUnmap AX_SYS_Munmap s32Ret1=0x%x ,s32Ret2=0x%x ,s32Ret2=0x%x", s32Ret1, s32Ret2, s32Ret3);
		// LOG(ERROR) << "FrameInfoUnmap AX_SYS_Munmap " << s32Ret1 << " " << s32Ret2 << " " << s32Ret3;
	}
	return s32Ret1 || s32Ret2 || s32Ret3;
}

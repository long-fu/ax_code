#include "FrameData.hpp"
#include "Logger.h"
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

// FrameData::FrameData()
// {
// }

// FrameData::FrameData( const FrameData & src )
// {
// }

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

/// @brief Clone数据到sys_mem中
/// @param dest
/// @return
// int FrameData::Clone(FrameData &dest)
// {

// 	const static char *MEM_TOKEN = "Clone";

// 	dest.m_enMemID = MEM_ID_SYS;

// 	dest.m_pFrameData = new AX_VIDEO_FRAME_INFO_T();
// 	memcpy(dest.m_pFrameData, m_pFrameData, sizeof(AX_VIDEO_FRAME_INFO_T));

// 	AX_U32 nPixelSize = (AX_U32)m_pFrameData->stVFrame.u32PicStride[0] * m_pFrameData->stVFrame.u32Height;

// 	memset(&dest.m_pFrameData->stVFrame, 0x0, sizeof(AX_VIDEO_FRAME_T));
// 	memcpy(&dest.m_pFrameData->stVFrame, &m_pFrameData->stVFrame, sizeof(AX_VIDEO_FRAME_T));

// 	AX_S32 sRet = 0;
// 	AX_S32 bit_num = 0;
// 	AX_U8 nStoragePlanarNum = 0;

// 	switch (m_pFrameData->stVFrame.enImgFormat)
// 	{
// 	case AX_FORMAT_YUV420_PLANAR:
// 	case AX_FORMAT_YUV420_SEMIPLANAR:
// 	case AX_FORMAT_YUV420_SEMIPLANAR_VU:
// 	case AX_FORMAT_YUV422_SEMIPLANAR: /* NV16 */
// 	case AX_FORMAT_YUV422_SEMIPLANAR_VU:
// 		bit_num = 8;
// 		nStoragePlanarNum = 2;
// 		break;
// 	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P101010:
// 	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010:
// 		bit_num = 10;
// 		nStoragePlanarNum = 2;
// 		break;
// 	case AX_FORMAT_YUV420_SEMIPLANAR_10BIT_P010:
// 	case AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010:
// 		bit_num = 16;
// 		nStoragePlanarNum = 2;
// 		break;
// 	case AX_FORMAT_YUV444_PACKED:
// 	case AX_FORMAT_RGB888:
// 	case AX_FORMAT_BGR888:
// 	case AX_FORMAT_RGB565:
// 	case AX_FORMAT_BGR565:
// 	case AX_FORMAT_RGBA8888:
// 	case AX_FORMAT_ARGB8888:
// 	case AX_FORMAT_ARGB4444:
// 	case AX_FORMAT_ARGB1555:
// 	case AX_FORMAT_ARGB8565:
// 	case AX_FORMAT_RGBA5551:
// 	case AX_FORMAT_RGBA4444:
// 	case AX_FORMAT_RGBA5658:
// 	case AX_FORMAT_ABGR4444:
// 	case AX_FORMAT_ABGR1555:
// 	case AX_FORMAT_ABGR8888:
// 	case AX_FORMAT_ABGR8565:
// 	case AX_FORMAT_BGRA8888:
// 	case AX_FORMAT_BGRA5551:
// 	case AX_FORMAT_BGRA4444:
// 	case AX_FORMAT_BGRA5658:
// 	case AX_FORMAT_YUV400:
// 		nStoragePlanarNum = 1;
// 		break;
// 	default:
// 		return -2;
// 	}
// 	AX_MEMORY_ADDR_T tBufAddr;
// 	switch (nStoragePlanarNum)
// 	{
// 	case 2:
// 		if (!m_pFrameData->stVFrame.u64PhyAddr[1])
// 		{
// 			m_pFrameData->stVFrame.u64PhyAddr[1] = m_pFrameData->stVFrame.u64PhyAddr[0] + m_pFrameData->stVFrame.u32PicStride[0] * m_pFrameData->stVFrame.u32Height;
// 		}
// 		nPixelSize = nPixelSize * bit_num / 8;
// 		if (AX_FORMAT_YUV422_SEMIPLANAR == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == m_pFrameData->stVFrame.enImgFormat)
// 		{

// 			if (m_pFrameData->stVFrame.u64VirAddr[0] == 0)
// 			{
// 				m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[0], nPixelSize);
// 				m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[1], nPixelSize);
// 			}

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);

// 			if (sRet)
// 			{
// 				return -3;
// 			}

// 			dest.m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]), nPixelSize);

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
// 			if (sRet)
// 			{
// 				return -3;
// 			}

// 			dest.m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1]), nPixelSize);
// 		}
// 		else
// 		{
// 			if (m_pFrameData->stVFrame.u64VirAddr[0] == 0)
// 			{
// 				m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[0], nPixelSize);
// 				m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[1], nPixelSize / 2);
// 			}

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
// 			if (sRet)
// 			{
// 				return -3;
// 			}
// 			dest.m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;

// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]), nPixelSize);

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
// 			if (sRet)
// 			{
// 				return -3;
// 			}
// 			dest.m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1]), nPixelSize / 2);
// 		}
// 		break;
// 	case 3:
// 		if (m_pFrameData->stVFrame.u64VirAddr[0] == 0)
// 		{
// 			m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[0], nPixelSize);
// 			m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[1], nPixelSize / 2);
// 			m_pFrameData->stVFrame.u64VirAddr[2] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[2], nPixelSize / 2);
// 		}

// 		sRet = AX_SYS_MemAlloc(
// 			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 			nPixelSize, 0x0, (AX_S8 *)MEM_TOKEN);
// 		if (sRet)
// 		{
// 			return -3;
// 		}
// 		dest.m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
// 		dest.m_pFrameData->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
// 		memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]), nPixelSize);

// 		sRet = AX_SYS_MemAlloc(
// 			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 			nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
// 		if (sRet)
// 		{
// 			return -3;
// 		}

// 		dest.m_pFrameData->stVFrame.u64VirAddr[1] = (AX_ULONG)tBufAddr.pVirAddr;
// 		dest.m_pFrameData->stVFrame.u64PhyAddr[1] = tBufAddr.u64PhyAddr;
// 		memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[1], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1]), nPixelSize / 2);

// 		sRet = AX_SYS_MemAlloc(
// 			&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 			nPixelSize / 2, 0x0, (AX_S8 *)MEM_TOKEN);
// 		if (sRet)
// 		{
// 			return -3;
// 		}

// 		dest.m_pFrameData->stVFrame.u64VirAddr[2] = (AX_ULONG)tBufAddr.pVirAddr;
// 		dest.m_pFrameData->stVFrame.u64PhyAddr[2] = tBufAddr.u64PhyAddr;
// 		memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[2], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[2]), nPixelSize / 2);

// 		break;
// 	default:
// 		if (m_pFrameData->stVFrame.u32FrameSize)
// 		{
// 			if (m_pFrameData->stVFrame.u64VirAddr[0] == 0)
// 			{
// 				m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[0], m_pFrameData->stVFrame.u32FrameSize);
// 			}

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				m_pFrameData->stVFrame.u32FrameSize, 0x0, (AX_S8 *)MEM_TOKEN);
// 			if (sRet)
// 			{
// 				return -3;
// 			}

// 			dest.m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]), m_pFrameData->stVFrame.u32FrameSize);
// 		}
// 		else
// 		{
// 			if (m_pFrameData->stVFrame.u64VirAddr[0] == 0)
// 			{
// 				m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)AX_SYS_Mmap(m_pFrameData->stVFrame.u64PhyAddr[0], nPixelSize * 3);
// 			}

// 			sRet = AX_SYS_MemAlloc(
// 				&tBufAddr.u64PhyAddr, (AX_VOID **)&tBufAddr.pVirAddr,
// 				nPixelSize * 3, 0x0, (AX_S8 *)MEM_TOKEN);
// 			if (sRet)
// 			{
// 				return -3;
// 			}

// 			dest.m_pFrameData->stVFrame.u64VirAddr[0] = (AX_ULONG)tBufAddr.pVirAddr;
// 			dest.m_pFrameData->stVFrame.u64PhyAddr[0] = tBufAddr.u64PhyAddr;
// 			memcpy((AX_VOID *)dest.m_pFrameData->stVFrame.u64VirAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]), nPixelSize * 3);
// 		}
// 		break;
// 	}

// 	return 0;
// };

int FrameData::Destroy()
{

	if (m_enMemID == MEM_ID_SYS)
	{
		LOG_INFO("Destory SYS Info");

		AX_S32 sRet = AX_SUCCESS;
		if (m_pFrameData->stVFrame.u64PhyAddr[0] != 0)
		{
			sRet = AX_SYS_MemFree(m_pFrameData->stVFrame.u64PhyAddr[0], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]));
			if (sRet != AX_SUCCESS)
			{
			}
			m_pFrameData->stVFrame.u64PhyAddr[0] = 0;
			m_pFrameData->stVFrame.u64VirAddr[0] = 0;
		}
		else if (m_pFrameData->stVFrame.u64VirAddr[0] != 0)
		{
			free((AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0]));
			m_pFrameData->stVFrame.u64VirAddr[0] = 0;
			perror("");
		}
		else
		{
			perror("");
		}

		if (m_pFrameData->stVFrame.u64PhyAddr[1] != 0)
		{
			sRet = AX_SYS_MemFree(m_pFrameData->stVFrame.u64PhyAddr[1], (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1]));
			if (sRet != AX_SUCCESS)
			{
			}
			m_pFrameData->stVFrame.u64PhyAddr[1] = 0;
			m_pFrameData->stVFrame.u64VirAddr[1] = 0;
		}
		else if (m_pFrameData->stVFrame.u64VirAddr[1] != 0)
		{
			free((AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1]));
			m_pFrameData->stVFrame.u64VirAddr[1] = 0;
			perror("");
		}
		else
		{
			perror("");
		}

		if (m_pFrameData->stVFrame.u64PhyAddr[2] != 0)
		{
			sRet = AX_SYS_MemFree(m_pFrameData->stVFrame.u64PhyAddr[2],
								  (AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[2]));
			if (sRet != AX_SUCCESS)
			{
			}
			m_pFrameData->stVFrame.u64PhyAddr[2] = 0;
			m_pFrameData->stVFrame.u64VirAddr[2] = 0;
		}
		else if (m_pFrameData->stVFrame.u64VirAddr[2] != 0)
		{
			free((AX_VOID *)((AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[2]));
			m_pFrameData->stVFrame.u64VirAddr[2] = 0;
			perror("");
		}
		else
		{
			perror("");
		}

		return sRet;
	}

	AX_U32 nPixelSize;
	AX_S32 s32Ret1 = 0;
	AX_S32 s32Ret2 = 0;
	AX_S32 s32Ret3 = 0;
	AX_S32 bit_num = 0;
	AX_S32 sRet = 0;
	AX_U8 nStoragePlanarNum = 0;

	nPixelSize = (AX_U32)m_pFrameData->stVFrame.u32PicStride[0] * m_pFrameData->stVFrame.u32Height;

	switch (m_pFrameData->stVFrame.enImgFormat)
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
		return -1;
	}

	switch (nStoragePlanarNum)
	{
	case 2:
		if (!m_pFrameData->stVFrame.u64PhyAddr[1])
		{
			m_pFrameData->stVFrame.u64PhyAddr[1] = m_pFrameData->stVFrame.u64PhyAddr[0] + m_pFrameData->stVFrame.u32PicStride[0] * m_pFrameData->stVFrame.u32Height;
		}
		nPixelSize = nPixelSize * bit_num / 8;
		if (AX_FORMAT_YUV422_SEMIPLANAR == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_VU == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P101010 == m_pFrameData->stVFrame.enImgFormat || AX_FORMAT_YUV422_SEMIPLANAR_10BIT_P010 == m_pFrameData->stVFrame.enImgFormat)
		{
			if ((m_pFrameData->stVFrame.u64VirAddr[0] != 0) &&
				(m_pFrameData->stVFrame.u64VirAddr[1] != 0))
			{
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0], nPixelSize);
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1], nPixelSize);

				m_pFrameData->stVFrame.u64VirAddr[0] = 0;
				m_pFrameData->stVFrame.u64VirAddr[1] = 0;
			}
		}
		else
		{
			if ((m_pFrameData->stVFrame.u64VirAddr[0] != 0) &&
				(m_pFrameData->stVFrame.u64VirAddr[1] != 0))
			{
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0], nPixelSize);
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1], nPixelSize / 2);
				m_pFrameData->stVFrame.u64VirAddr[0] = 0;
				m_pFrameData->stVFrame.u64VirAddr[1] = 0;
			}
		}
		break;
	case 3:
	{
		if ((m_pFrameData->stVFrame.u64VirAddr[0] != 0) &&
			(m_pFrameData->stVFrame.u64VirAddr[1] != 0) &&
			(m_pFrameData->stVFrame.u64VirAddr[2] != 0))
		{

			s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0], nPixelSize);
			s32Ret2 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[1], nPixelSize / 2);
			s32Ret3 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[2], nPixelSize / 2);
			m_pFrameData->stVFrame.u64VirAddr[0] = 0;
			m_pFrameData->stVFrame.u64VirAddr[1] = 0;
			m_pFrameData->stVFrame.u64VirAddr[2] = 0;
		}
	}

	break;
	default:
		if (m_pFrameData->stVFrame.u32FrameSize)
		{
			if (m_pFrameData->stVFrame.u64VirAddr[0] != 0)
			{
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0], m_pFrameData->stVFrame.u32FrameSize);
				m_pFrameData->stVFrame.u64VirAddr[0] = 0;
			}
		}
		else
		{
			if (m_pFrameData->stVFrame.u64VirAddr[0] != 0)
			{
				s32Ret1 = AX_SYS_Munmap((AX_VOID *)(AX_ULONG)m_pFrameData->stVFrame.u64VirAddr[0], nPixelSize * 3);
				m_pFrameData->stVFrame.u64VirAddr[0] = 0;
			}
		}
		break;
	}

	if (s32Ret1 || s32Ret2 || s32Ret3)
	{
	}

	// AX_S32 ret;
	if (!m_pFrameData->stVFrame.u64PhyAddr[0])
	{
	}

	if (m_enMemID == MEM_ID_IVPS)
	{
		LOG_INFO("Destory IVPS Info");
		sRet = AX_IVPS_ReleaseChnFrame(m_nGrp, m_nChn, &m_pFrameData->stVFrame);
		if (sRet)
		{
			if (AX_ERR_VDEC_FLOW_END != sRet)
			{
				if (sRet == AX_ERR_VDEC_UNEXIST || sRet == AX_ERR_VDEC_NOT_PERM)
				{
					// LOG_WARNING("VdGrp=%d, VdChn=%d, AX_VDEC_GetChnFrame AX_ERR_VDEC_UNEXIST \n",
					// 			VdGrp, VdChn);
					// sRet = ret;
					// goto ERR_RET;
				}
				// sRet = ret;
				// LOG_ERROR("VdGrp=%d, VdChn:%d, AX_VDEC_ReleaseChnFrame FAILED! res:0x%x %s \n"
				// 		  "u64PhyAddr[0]:0x%llX, BlkId[0]:0x%x, BlkId[1]:0x%x\n",
				// 		  VdGrp, VdChn, ret, AX_VdecRetStr(ret), pstFrameInfo->stVFrame.u64PhyAddr[0],
				// 		  pstFrameInfo->stVFrame.u32BlkId[0], pstFrameInfo->stVFrame.u32BlkId[1]);
				// goto ERR_RET;
			}
		}
	}
	else if (m_enMemID == MEM_ID_VDEC)
	{
		// LOG_INFO("Destory Vdec Info\n");
		sRet = AX_VDEC_ReleaseChnFrame(m_nGrp, m_nChn, m_pFrameData);
		if (sRet)
		{
			if (AX_ERR_VDEC_FLOW_END != sRet)
			{
				if (sRet == AX_ERR_VDEC_UNEXIST || sRet == AX_ERR_VDEC_NOT_PERM)
				{
					// LOG_WARNING("VdGrp=%d, VdChn=%d, AX_VDEC_GetChnFrame AX_ERR_VDEC_UNEXIST \n",
					// 			VdGrp, VdChn);
					// sRet = ret;
					// goto ERR_RET;
				}
				// sRet = ret;
				// LOG_ERROR("VdGrp=%d, VdChn:%d, AX_VDEC_ReleaseChnFrame FAILED! res:0x%x %s \n"
				// 		  "u64PhyAddr[0]:0x%llX, BlkId[0]:0x%x, BlkId[1]:0x%x\n",
				// 		  VdGrp, VdChn, ret, AX_VdecRetStr(ret), pstFrameInfo->stVFrame.u64PhyAddr[0],
				// 		  pstFrameInfo->stVFrame.u32BlkId[0], pstFrameInfo->stVFrame.u32BlkId[1]);
				// goto ERR_RET;
			}
		}
	}
	return 0;
};

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

// FrameData &				FrameData::operator=( FrameData const & rhs )
// {
// 	//if ( this != &rhs )
// 	//{
// 		//this->_value = rhs.getValue();
// 	//}
// 	return *this;
// }

// std::ostream &			operator<<( std::ostream & o, FrameData const & i )
// {
// 	//o << "Value = " << i.getValue();
// 	return o;
// }

/*
** --------------------------------- METHODS ----------------------------------
*/

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
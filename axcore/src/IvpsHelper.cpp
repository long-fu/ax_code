#include "IvpsHelper.hpp"
// #include "AppUtils.h"
#include "ax_base_type.h"
#include "ax_ivps_api.h"
#include "Logger.h"
// #include "glog/logging.h"

// extern int g_channel_id;
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

IvpsHelper::IvpsHelper(IVPS_GRP IvpsGrp, AX_U64 blkSize, AX_U32 blkCnt) : m_nIvpsGrp(IvpsGrp),
																		  m_nBlkSize(blkSize),
																		  m_nBlkCnt(blkCnt)
{
	LOG_INFO("Create IVPS GRP {}", IvpsGrp);
}

AX_S32 IvpsHelper::CreatePool()
{
	int ret;

	if (m_nPoolId != AX_INVALID_POOLID)
	{
		return 0;
	}

	std::vector<AX_BLK> blks;

	AX_POOL_CONFIG_T stPoolConfig;
	memset(&stPoolConfig, 0, sizeof(AX_POOL_CONFIG_T));
	stPoolConfig.MetaSize = 0;
	stPoolConfig.BlkCnt = m_nBlkCnt;
	stPoolConfig.BlkSize = m_nBlkSize;
	stPoolConfig.CacheMode = POOL_CACHE_MODE_NONCACHE;
	memset(stPoolConfig.PartitionName, 0, sizeof(stPoolConfig.PartitionName));

	// 分区名称需要固定写死
	strcpy((AX_CHAR *)stPoolConfig.PartitionName, "anonymous");

	m_nPoolId = AX_POOL_CreatePool(&stPoolConfig);
	if (AX_INVALID_POOLID == m_nPoolId)
	{
		// //("AX_POOL_CreatePool Failed!! %X\n", ret);
		// << "AX_POOL_CreatePool Failed!! " << m_nPoolId;
		return AX_INVALID_POOLID;
	}

	for (size_t i = 0; i < m_nBlkCnt; i++)
	{
		AX_BLK blkId = AX_POOL_GetBlock(m_nPoolId, m_nBlkSize, NULL);
		blks.push_back(blkId);

		if (blkId == AX_INVALID_BLOCKID)
		{

			ret = AX_POOL_DestroyPool(m_nPoolId);
			if (IVPS_SUCC != ret)
			{
				// //("AX_POOL_DestroyPool(Grp: %d) failed(this grp is not started) ret=0x%x.", m_nIvpsGrp, ret);
				// return -1;
				// << "AX_POOL_DestroyPool failed(this grp is not started) " << m_nIvpsGrp << " " << ret;
			}
			m_nPoolId = AX_INVALID_POOLID;
			//("AX_POOL_GetBlock fail!\n");
			return -1;
		}
		else
		{
			void *blockVirAddr = AX_POOL_GetBlockVirAddr(blkId);
			memset(blockVirAddr, 0x0, m_nBlkSize);
		}
	}

	for (size_t i = 0; i < m_nBlkCnt; i++)
	{
		AX_BLK bklId = blks[i];
		AX_POOL_ReleaseBlock(bklId);
	}

	return 0;
}

AX_S32 IvpsHelper::CreateGrp()
{
	AX_S32 ret = 0;

	// TODO：错误一场需要 直接停止

	memset(&m_tGrpAttr, 0x0, sizeof(AX_IVPS_GRP_ATTR_T));
	m_tGrpAttr.ePipeline = AX_IVPS_PIPELINE_DEFAULT;
	m_tGrpAttr.nInFifoDepth = 1;

	memset(&m_tPoolAttr, 0x0, sizeof(AX_IVPS_POOL_ATTR_T));
	m_tPoolAttr.ePoolSrc = POOL_SOURCE_USER;
	m_tPoolAttr.PoolId = m_nPoolId;

	// 1.
	ret = AX_IVPS_CreateGrp(m_nIvpsGrp, &m_tGrpAttr);
	if (IVPS_SUCC != ret)
	{
		//("AX_IVPS_CreateGrp(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_CreateGrp failed  " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	// 2.
	ret = AX_IVPS_SetPipelineAttr(m_nIvpsGrp, &m_tPipelineAttr);
	if (IVPS_SUCC != ret)
	{
		//("AX_IVPS_SetPipelineAttr(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_SetPipelineAttr failed  " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	for (IVPS_CHN chn = 0; chn < m_tPipelineAttr.nOutChnNum; chn++)
	{
		// LOG_INFO("chn id :%d", chn);
		ret = AX_IVPS_SetChnPoolAttr(m_nIvpsGrp, chn, &m_tPoolAttr);
		if (IVPS_SUCC != ret)
		{
			// //("AX_IVPS_SetChnPoolAttr(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
			// << "AX_IVPS_SetChnPoolAttr failed  " << m_nIvpsGrp << " " << ret;
			return -3;
		}
		// 3.
		ret = AX_IVPS_EnableChn(m_nIvpsGrp, chn);
		if (IVPS_SUCC != ret)
		{
			// //("AX_IVPS_EnableChn(Chn: %d) failed, ret=0x%x.", chn, ret);
			// << "AX_IVPS_EnableChn failed  " << m_nIvpsGrp << " " <<  chn << " " << ret;
			return -1;
		}
	}

	// 4.
	ret = AX_IVPS_StartGrp(m_nIvpsGrp);
	if (IVPS_SUCC != ret)
	{
		// //("AX_IVPS_StartGrp(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_StartGrp failed  " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	// << "IVPS Create Success!!!" << " GRP: " << m_nIvpsGrp;

	return 0;
}

AX_S32 IvpsHelper::Init()
{
	int ret = 0;
	if (isInitialized)
	{
		int chn = 0;
		ret = AX_IVPS_SetPipelineAttr(m_nIvpsGrp, &m_tPipelineAttr);
		if (IVPS_SUCC != ret)
		{
			//("AX_IVPS_SetPipelineAttr(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
			// << "AX_IVPS_SetPipelineAttr failed  " << m_nIvpsGrp << " " << ret;
			return -1;
		}

		ret = AX_IVPS_DisableChn(m_nIvpsGrp, chn);
		if (IVPS_SUCC != ret)
		{
			//("AX_IVPS_DisableChn(Chn: %d) failed, ret=0x%x.", chn, ret);
			// << "AX_IVPS_DisableChn failed  " << m_nIvpsGrp << " " << chn << " " << ret;
			return -1;
		}

		ret = AX_IVPS_EnableChn(m_nIvpsGrp, chn);
		if (IVPS_SUCC != ret)
		{
			//("AX_IVPS_EnableChn(Chn: %d) failed, ret=0x%x.", chn, ret);
			// << "AX_IVPS_EnableChn failed  " << m_nIvpsGrp << " " << chn << " " << ret;
			return -1;
		}
		// ret = 0;
	}
	else
	{
		// TODO: 对返回结果需要完善处理
		ret = CreatePool();
		if (AX_SUCCESS != ret)
		{
			// << "Frame IVPS CreatePool Failed";
			return -1;
		}
		ret = CreateGrp();
		if (AX_SUCCESS != ret)
		{
			// << "Frame IVPS CreateGrp Failed";
			return -1;
		}
		isInitialized = true;
	}
	return 0;
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

IvpsHelper::~IvpsHelper()
{
	DestroyResource();
}

AX_S32 IvpsHelper::DestroyResource()
{
	if (isReleased)
	{
		return 0;
	}

	AX_S32 ret = IVPS_SUCC;

	ret = AX_IVPS_StopGrp(m_nIvpsGrp);
	if (IVPS_SUCC != ret)
	{
		// //("AX_IVPS_StopGrp(Grp: %d) failed(this grp is not started) ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_StopGrp(Grp: ) failed(this grp is not started) ret " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	for (IVPS_CHN chn = 0; chn < m_tPipelineAttr.nOutChnNum; ++chn)
	{
		ret = AX_IVPS_DisableChn(m_nIvpsGrp, chn);
		if (IVPS_SUCC != ret)
		{
			// //("AX_IVPS_DestoryChn(Chn: %d) failed, ret=0x%x.", chn, ret);
			// << "AX_IVPS_DestoryChn(Chn: ) failed(this grp is not started) ret " << chn << " " << ret;
			return -1;
		}
	}

	ret = AX_IVPS_DestoryGrp(m_nIvpsGrp);
	if (IVPS_SUCC != ret)
	{
		// //("AX_IVPS_DestoryGrp(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_DestoryGrp(Grp: ) failed ret " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	ret = AX_POOL_DestroyPool(m_nPoolId);
	if (IVPS_SUCC != ret)
	{
		// //("AX_POOL_DestroyPool(Grp: %d) failed(this grp is not started) ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_POOL_DestroyPool(Grp: ) failed(this grp is not started) ret " << m_nIvpsGrp << " " << ret;
		return -1;
	}
	m_nPoolId = AX_INVALID_POOLID;
	isReleased = true;
	return 0;
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

/*
** --------------------------------- METHODS ----------------------------------
*/

// #define SHARED_IVPS_FRAMEINFO(VdGrp, VdChn, frameInfo)                                                                     \
// 	(std::shared_ptr<AX_VIDEO_FRAME_INFO_T>((AX_VIDEO_FRAME_INFO_T *)(frameInfo), [VdGrp, VdChn](AX_VIDEO_FRAME_INFO_T *p) \
// 											{ AX_IVPS_ReleaseChnFrame(VdGrp, VdChn ,&p->stVFrame); delete p; }))

AX_S32 IvpsHelper::Process(ImageData &dest_frame,
						   ImageData const &src_frame)
{
	int outCount = m_tPipelineAttr.nOutChnNum;

	if (outCount != 1)
	{
		//("代码错误");
		exit(-1);
	}

	int grp = m_nIvpsGrp;
	int chn = 0;
	int ret = AX_IVPS_SendFrame(grp, &src_frame.data->FrameInfo()->stVFrame, -1);

	AX_VIDEO_FRAME_INFO_T *tDstFrame = new AX_VIDEO_FRAME_INFO_T();
	if (IVPS_SUCC != ret)
	{
		//("AX_IVPS_SendFrame(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_SendFrame(Grp: ) failed, ret " << m_nIvpsGrp << " " << ret;
		return -1;
	}

	memset(tDstFrame, 0x0, sizeof(AX_VIDEO_FRAME_INFO_T));
	ret = AX_IVPS_GetChnFrame(grp, chn, &tDstFrame->stVFrame, -1);
	if (IVPS_SUCC != ret)
	{
		//("AX_IVPS_GetChnFrame(Grp: %d) failed, ret=0x%x.", m_nIvpsGrp, ret);
		// << "AX_IVPS_GetChnFrame(Grp: ) failed, ret " << m_nIvpsGrp << " " << ret;
		return -1;
	}
	// dest_frame = SHARED_IVPS_FRAMEINFO(grp, chn, tDstFrame);
	dest_frame.bEndOfStream = tDstFrame->bEndOfStream;
	dest_frame.enImgFormat = tDstFrame->stVFrame.enImgFormat;
	dest_frame.u32Width = tDstFrame->stVFrame.u32Width;
	dest_frame.u32Height = tDstFrame->stVFrame.u32Height;
	dest_frame.data = FrameData::Create(tDstFrame, m_nIvpsGrp, 0, MEM_ID_IVPS);
	return ret;
}

AX_S32 IvpsHelper::Resize(AX_IVPS_ASPECT_RATIO_E eMode, AX_U32 dest_width, AX_U32 dest_height)

{
	int ret;
	int ch = 1;

	memset(&m_tPipelineAttr, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	m_tPipelineAttr.nOutChnNum = 1;
	m_tPipelineAttr.tFilter[ch][0].bEngage = AX_TRUE;
	m_tPipelineAttr.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	m_tPipelineAttr.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	AX_S32 frmStride = ALIGN_UP(dest_width, 16);
	AX_S32 wAlign = ALIGN_UP(dest_width, 2);
	AX_S32 hAlign = ALIGN_UP(dest_height, 2);

	m_tPipelineAttr.tFilter[ch][0].nDstPicWidth = wAlign;
	m_tPipelineAttr.tFilter[ch][0].nDstPicHeight = hAlign;
	m_tPipelineAttr.tFilter[ch][0].nDstPicStride = frmStride;
	m_tPipelineAttr.tFilter[ch][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR;

	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eMode = eMode;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	m_tPipelineAttr.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;
	m_tPipelineAttr.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;
	m_tPipelineAttr.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;
	m_tPipelineAttr.nOutFifoDepth[ch - 1] = 4;

	ret = Init();
	return ret;
}

AX_S32 IvpsHelper::CropAndCSC(
	AX_IMG_FORMAT_E eDstPicFormat,
	AX_U16 nCropX,
	AX_U16 nCropY,
	AX_U16 nCropW,
	AX_U16 nCropH)
{
	int ret;
	int ch = 1;

	memset(&m_tPipelineAttr, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	m_tPipelineAttr.nOutChnNum = 1;
	m_tPipelineAttr.tFilter[ch][0].bEngage = AX_TRUE;
	m_tPipelineAttr.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;

	m_tPipelineAttr.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	m_tPipelineAttr.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	// 裁剪
	m_tPipelineAttr.tFilter[ch][0].bCrop = AX_TRUE;

	AX_S32 frmStride = ALIGN_UP(nCropW, 16);
	AX_S32 wAlign = ALIGN_UP(nCropW, 2);
	AX_S32 hAlign = ALIGN_UP(nCropH, 2);
	AX_S32 xAlign = ALIGN_UP(nCropX, 2);
	AX_S32 yAlign = ALIGN_UP(nCropY, 2);

	m_tPipelineAttr.tFilter[ch][0].tCropRect.nX = xAlign;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nY = yAlign;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nW = wAlign;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nH = hAlign;

	m_tPipelineAttr.tFilter[ch][0].nDstPicWidth = wAlign;

	m_tPipelineAttr.tFilter[ch][0].nDstPicHeight = hAlign;

	m_tPipelineAttr.tFilter[ch][0].nDstPicStride = frmStride;

	// 颜色转换
	m_tPipelineAttr.tFilter[ch][0].eDstPicFormat = eDstPicFormat;

	// // 缩放
	// m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
	// m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	// m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	// m_tPipelineAttr.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	// 旋转
	// m_tPipelineAttr.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;

	// 压缩等级
	// m_tPipelineAttr.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;

	m_tPipelineAttr.nOutFifoDepth[ch - 1] = 4;

	ret = Init();
	return ret;
}

AX_S32 IvpsHelper::ResizeAndCSC(
	AX_IMG_FORMAT_E eDstPicFormat,
	AX_U32 nDstPicWidth,
	AX_U32 nDstPicHeight)
{
	int ch = 1;
	memset(&m_tPipelineAttr, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	m_tPipelineAttr.nOutChnNum = 1;
	m_tPipelineAttr.tFilter[ch][0].bEngage = AX_TRUE;
	m_tPipelineAttr.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;

	m_tPipelineAttr.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	m_tPipelineAttr.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	// 裁剪
	m_tPipelineAttr.tFilter[ch][0].bCrop = AX_FALSE;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nX = 0;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nY = 0;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nW = 0;
	m_tPipelineAttr.tFilter[ch][0].tCropRect.nH = 0;

	AX_S32 frmStride = ALIGN_UP(nDstPicWidth, 16);
	AX_S32 wAlign = ALIGN_UP(nDstPicWidth, 2);
	AX_S32 hAlign = ALIGN_UP(nDstPicHeight, 2);

	m_tPipelineAttr.tFilter[ch][0].nDstPicWidth = wAlign;
	m_tPipelineAttr.tFilter[ch][0].nDstPicHeight = hAlign;
	m_tPipelineAttr.tFilter[ch][0].nDstPicStride = frmStride;

	// 颜色转换
	m_tPipelineAttr.tFilter[ch][0].eDstPicFormat = eDstPicFormat;

	// // 缩放
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	m_tPipelineAttr.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	// 旋转
	m_tPipelineAttr.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;

	// 压缩等级
	m_tPipelineAttr.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;

	m_tPipelineAttr.nOutFifoDepth[ch - 1] = 4;

	int ret = Init();
	return ret;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
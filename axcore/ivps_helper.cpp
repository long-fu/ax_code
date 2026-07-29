#include "ivps_helper.h"
#include "ax_base_type.h"
#include "ax_ivps_api.h"
#include "logger.h"
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

IvpsHelper::IvpsHelper(IVPS_GRP IvpsGrp, AX_U64 blkSize, AX_U32 blkCnt) : ivps_grp_(IvpsGrp),
																		  blk_size_(blkSize),
																		  blk_cnt_(blkCnt)
{
	LOG_INFO("Create IVPS GRP {}", IvpsGrp);
}

AX_S32 IvpsHelper::CreatePool()
{
	int ret;

	if (pool_id_ != AX_INVALID_POOLID)
	{
		return 0;
	}

	std::vector<AX_BLK> blks;

	AX_POOL_CONFIG_T stPoolConfig;
	memset(&stPoolConfig, 0, sizeof(AX_POOL_CONFIG_T));
	stPoolConfig.MetaSize = 0;
	stPoolConfig.BlkCnt = blk_cnt_;
	stPoolConfig.BlkSize = blk_size_;
	stPoolConfig.CacheMode = POOL_CACHE_MODE_NONCACHE;
	memset(stPoolConfig.PartitionName, 0, sizeof(stPoolConfig.PartitionName));

	// 分区名称需要固定写死
	strcpy((AX_CHAR *)stPoolConfig.PartitionName, "anonymous");

	pool_id_ = AX_POOL_CreatePool(&stPoolConfig);
	if (AX_INVALID_POOLID == pool_id_)
	{
		LOG_ERROR("AX_POOL_CreatePool Failed!! code:{:#x}", pool_id_);
		return AX_INVALID_POOLID;
	}

	for (size_t i = 0; i < blk_cnt_; i++)
	{
		AX_BLK blkId = AX_POOL_GetBlock(pool_id_, blk_size_, NULL);
		blks.push_back(blkId);

		if (blkId == AX_INVALID_BLOCKID)
		{

			ret = AX_POOL_DestroyPool(pool_id_);
			if (IVPS_SUCC != ret)
			{
				LOG_ERROR("AX_POOL_DestroyPool failed! code:{:#x}", ret);
			}
			pool_id_ = AX_INVALID_POOLID;
			LOG_ERROR("AX_POOL_GetBlock fail! code:{:#x}", blkId);
			return -1;
		}
		else
		{
			void *blockVirAddr = AX_POOL_GetBlockVirAddr(blkId);
			memset(blockVirAddr, 0x0, blk_size_);
		}
	}

	for (size_t i = 0; i < blk_cnt_; i++)
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

	memset(&grp_attr_, 0x0, sizeof(AX_IVPS_GRP_ATTR_T));
	grp_attr_.ePipeline = AX_IVPS_PIPELINE_DEFAULT;
	grp_attr_.nInFifoDepth = 1;

	memset(&pool_attr_, 0x0, sizeof(AX_IVPS_POOL_ATTR_T));
	pool_attr_.ePoolSrc = POOL_SOURCE_USER;
	pool_attr_.PoolId = pool_id_;

	// 1.
	ret = AX_IVPS_CreateGrp(ivps_grp_, &grp_attr_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_CreateGrp failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		return -1;
	}

	// 2.
	ret = AX_IVPS_SetPipelineAttr(ivps_grp_, &pipeline_attr_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_SetPipelineAttr failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		return -1;
	}

	for (IVPS_CHN chn = 0; chn < pipeline_attr_.nOutChnNum; chn++)
	{
		// LOG_INFO("chn id :%d", chn);
		ret = AX_IVPS_SetChnPoolAttr(ivps_grp_, chn, &pool_attr_);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_SetChnPoolAttr failed! Grp:{}, Chn:{}, code:{:#x}", ivps_grp_, chn, ret);
			return -3;
		}
		// 3.
		ret = AX_IVPS_EnableChn(ivps_grp_, chn);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_EnableChn failed! Grp:{}, Chn:{}, code:{:#x}", ivps_grp_, chn, ret);
			return -1;
		}
	}

	// 4.
	ret = AX_IVPS_StartGrp(ivps_grp_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_StartGrp failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		return -1;
	}

	LOG_INFO("IVPS Create Success! GRP: {}", ivps_grp_);

	return 0;
}

AX_S32 IvpsHelper::Init()
{
	int ret = 0;
	if (is_initialized_)
	{
		int chn = 0;
		ret = AX_IVPS_SetPipelineAttr(ivps_grp_, &pipeline_attr_);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_SetPipelineAttr failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
			return -1;
		}

		ret = AX_IVPS_DisableChn(ivps_grp_, chn);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_DisableChn failed! Grp:{}, Chn:{}, code:{:#x}", ivps_grp_, chn, ret);
			return -1;
		}

		ret = AX_IVPS_EnableChn(ivps_grp_, chn);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_EnableChn failed! Grp:{}, Chn:{}, code:{:#x}", ivps_grp_, chn, ret);
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
			LOG_ERROR("Frame IVPS CreatePool Failed! code:{:#x}", ret);
			return -1;
		}
		ret = CreateGrp();
		if (AX_SUCCESS != ret)
		{
			LOG_ERROR("Frame IVPS CreateGrp Failed! code:{:#x}", ret);
			return -1;
		}
		is_initialized_ = true;
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
	if (is_released_)
	{
		return 0;
	}

	AX_S32 ret = IVPS_SUCC;

	ret = AX_IVPS_StopGrp(ivps_grp_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_StopGrp failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		return -1;
	}

	for (IVPS_CHN chn = 0; chn < pipeline_attr_.nOutChnNum; ++chn)
	{
		ret = AX_IVPS_DisableChn(ivps_grp_, chn);
		if (IVPS_SUCC != ret)
		{
			LOG_ERROR("AX_IVPS_DisableChn failed! Grp:{}, Chn:{}, code:{:#x}", ivps_grp_, chn, ret);
			return -1;
		}
	}

	ret = AX_IVPS_DestoryGrp(ivps_grp_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_DestoryGrp failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		return -1;
	}

	ret = AX_POOL_DestroyPool(pool_id_);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_POOL_DestroyPool failed! PoolId:{}, code:{:#x}", pool_id_, ret);
		return -1;
	}
	pool_id_ = AX_INVALID_POOLID;
	is_released_ = true;
	return 0;
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

/*
** --------------------------------- METHODS ----------------------------------
*/

/* (removed unused macro) */

AX_S32 IvpsHelper::Process(ImageData &dest_frame,
						   ImageData const &src_frame)
{
	int outCount = pipeline_attr_.nOutChnNum;

	if (outCount != 1)
	{
		LOG_ERROR("IVPS outCount != 1, outCount:{}", outCount);
		return -1;
	}

	int grp = ivps_grp_;
	int chn = 0;
	int ret = AX_IVPS_SendFrame(grp, &src_frame.data->FrameInfo()->stVFrame, -1);

	AX_VIDEO_FRAME_INFO_T *tDstFrame = new AX_VIDEO_FRAME_INFO_T();
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_SendFrame failed! Grp:{}, code:{:#x}", ivps_grp_, ret);
		delete tDstFrame;
		return -1;
	}

	memset(tDstFrame, 0x0, sizeof(AX_VIDEO_FRAME_INFO_T));
	ret = AX_IVPS_GetChnFrame(grp, chn, &tDstFrame->stVFrame, -1);
	if (IVPS_SUCC != ret)
	{
		LOG_ERROR("AX_IVPS_GetChnFrame failed! Grp:{}, Chn:{}, code:{:#x}", grp, chn, ret);
		delete tDstFrame;
		return -1;
	}
	// dest_frame = SHARED_IVPS_FRAMEINFO(grp, chn, tDstFrame);
	dest_frame.end_of_stream = tDstFrame->bEndOfStream;
	dest_frame.img_format = tDstFrame->stVFrame.enImgFormat;
	dest_frame.width = tDstFrame->stVFrame.u32Width;
	dest_frame.height = tDstFrame->stVFrame.u32Height;
	dest_frame.data = FrameData::Create(tDstFrame, ivps_grp_, 0, kMemIdIvps);
	return ret;
}

AX_S32 IvpsHelper::Resize(AX_IVPS_ASPECT_RATIO_E eMode, AX_U32 dest_width, AX_U32 dest_height)

{
	int ret;
	int ch = 1;

	memset(&pipeline_attr_, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	pipeline_attr_.nOutChnNum = 1;
	pipeline_attr_.tFilter[ch][0].bEngage = AX_TRUE;
	pipeline_attr_.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	pipeline_attr_.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	AX_S32 frmStride = ALIGN_UP(dest_width, 16);
	AX_S32 wAlign = ALIGN_UP(dest_width, 2);
	AX_S32 hAlign = ALIGN_UP(dest_height, 2);

	pipeline_attr_.tFilter[ch][0].nDstPicWidth = wAlign;
	pipeline_attr_.tFilter[ch][0].nDstPicHeight = hAlign;
	pipeline_attr_.tFilter[ch][0].nDstPicStride = frmStride;
	pipeline_attr_.tFilter[ch][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR;

	pipeline_attr_.tFilter[ch][0].tAspectRatio.eMode = eMode;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	pipeline_attr_.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;
	pipeline_attr_.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;
	pipeline_attr_.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;
	pipeline_attr_.nOutFifoDepth[ch - 1] = 4;

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

	memset(&pipeline_attr_, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	pipeline_attr_.nOutChnNum = 1;
	pipeline_attr_.tFilter[ch][0].bEngage = AX_TRUE;
	pipeline_attr_.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;

	pipeline_attr_.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	pipeline_attr_.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	// 裁剪
	pipeline_attr_.tFilter[ch][0].bCrop = AX_TRUE;

	AX_S32 frmStride = ALIGN_UP(nCropW, 16);
	AX_S32 wAlign = ALIGN_UP(nCropW, 2);
	AX_S32 hAlign = ALIGN_UP(nCropH, 2);
	AX_S32 xAlign = ALIGN_UP(nCropX, 2);
	AX_S32 yAlign = ALIGN_UP(nCropY, 2);

	pipeline_attr_.tFilter[ch][0].tCropRect.nX = xAlign;
	pipeline_attr_.tFilter[ch][0].tCropRect.nY = yAlign;
	pipeline_attr_.tFilter[ch][0].tCropRect.nW = wAlign;
	pipeline_attr_.tFilter[ch][0].tCropRect.nH = hAlign;

	pipeline_attr_.tFilter[ch][0].nDstPicWidth = wAlign;

	pipeline_attr_.tFilter[ch][0].nDstPicHeight = hAlign;

	pipeline_attr_.tFilter[ch][0].nDstPicStride = frmStride;

	// 颜色转换
	pipeline_attr_.tFilter[ch][0].eDstPicFormat = eDstPicFormat;

	// // 缩放
	// pipeline_attr_.tFilter[ch][0].tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
	// pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	// pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	// pipeline_attr_.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	// 旋转
	// pipeline_attr_.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;

	// 压缩等级
	// pipeline_attr_.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;

	pipeline_attr_.nOutFifoDepth[ch - 1] = 4;

	ret = Init();
	return ret;
}

AX_S32 IvpsHelper::ResizeAndCSC(
	AX_IMG_FORMAT_E eDstPicFormat,
	AX_U32 nDstPicWidth,
	AX_U32 nDstPicHeight)
{
	int ch = 1;
	memset(&pipeline_attr_, 0x0, sizeof(AX_IVPS_PIPELINE_ATTR_T));

	pipeline_attr_.nOutChnNum = 1;
	pipeline_attr_.tFilter[ch][0].bEngage = AX_TRUE;
	pipeline_attr_.tFilter[ch][0].eEngine = AX_IVPS_ENGINE_VPP;

	pipeline_attr_.tFilter[ch][0].tFRC.fSrcFrameRate = 25;
	pipeline_attr_.tFilter[ch][0].tFRC.fDstFrameRate = 25;

	// 裁剪
	pipeline_attr_.tFilter[ch][0].bCrop = AX_FALSE;
	pipeline_attr_.tFilter[ch][0].tCropRect.nX = 0;
	pipeline_attr_.tFilter[ch][0].tCropRect.nY = 0;
	pipeline_attr_.tFilter[ch][0].tCropRect.nW = 0;
	pipeline_attr_.tFilter[ch][0].tCropRect.nH = 0;

	AX_S32 frmStride = ALIGN_UP(nDstPicWidth, 16);
	AX_S32 wAlign = ALIGN_UP(nDstPicWidth, 2);
	AX_S32 hAlign = ALIGN_UP(nDstPicHeight, 2);

	pipeline_attr_.tFilter[ch][0].nDstPicWidth = wAlign;
	pipeline_attr_.tFilter[ch][0].nDstPicHeight = hAlign;
	pipeline_attr_.tFilter[ch][0].nDstPicStride = frmStride;

	// 颜色转换
	pipeline_attr_.tFilter[ch][0].eDstPicFormat = eDstPicFormat;

	// // 缩放
	pipeline_attr_.tFilter[ch][0].tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
	pipeline_attr_.tFilter[ch][0].tAspectRatio.nBgColor = 0x000000;

	// 旋转
	pipeline_attr_.tFilter[ch][0].tTdpCfg.eRotation = AX_IVPS_ROTATION_0;

	// 压缩等级
	pipeline_attr_.tFilter[ch][0].tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;

	pipeline_attr_.nOutFifoDepth[ch - 1] = 4;

	int ret = Init();
	return ret;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
#include "VdecHelper.hpp"
#include "ax_buffer_tool.h"
#include <unistd.h>
#include <string.h>
#include "Logger.h"
#define AX_COMM_ALIGN(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define AX_SHIFT_LEFT_ALIGN(a) (1 << (a))

#define AX_VDEC_WIDTH_ALIGN AX_SHIFT_LEFT_ALIGN(8)

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#endif

#define AX_DEC_VALUE_2_STR_CASE(s32Ret) \
	case (s32Ret):                      \
		return (#s32Ret)

static char s_str_unknown[16] = ("Unknown code");

const char *AX_VdecRetStr(AX_S32 value)
{
	switch (value)
	{
		AX_DEC_VALUE_2_STR_CASE(AX_SUCCESS);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_INVALID_GRPID);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_INVALID_CHNID);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_ILLEGAL_PARAM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NULL_PTR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_BAD_ADDR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_SYS_NOTREADY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_BUSY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOT_INIT);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOT_CONFIG);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOT_SUPPORT);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOT_PERM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_EXIST);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_UNEXIST);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOMEM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOBUF);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NOT_MATCH);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_BUF_EMPTY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_BUF_FULL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_QUEUE_EMPTY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_QUEUE_FULL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_TIMED_OUT);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_FLOW_END);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_UNKNOWN);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_RUN_ERROR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_STRM_ERROR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_VDEC_NEED_REALLOC_BUF);

		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_NULL_PTR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_NOTREADY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_NOMEM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_MMAP_FAIL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_MUNMAP_FAIL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_FREE_FAIL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_CMM_UNKNOWN);

		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_ILLEGAL_PARAM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_NULL_PTR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_NOTREADY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_NOT_PERM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_UNEXIST);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_NOMEM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_MMAP_FAIL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_MUNMAP_FAIL);

		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_BUSY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_NOT_SUPPORT);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_BLKFREE_FAIL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_POOL_UNKNOWN);

		AX_DEC_VALUE_2_STR_CASE(AX_ERR_PTS_ILLEGAL_PARAM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_PTS_NULL_PTR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_PTS_NOTREADY);

		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_ILLEGAL_PARAM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_NULL_PTR);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_NOTREADY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_NOT_SUPPORT);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_NOT_PERM);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_UNEXIST);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_TABLE_FULL);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_TABLE_EMPTY);
		AX_DEC_VALUE_2_STR_CASE(AX_ERR_LINK_UNKNOWN);

	default:
		// SAMPLE_CRIT_LOG("Unknown return code. 0x%x", value);
		snprintf(s_str_unknown, sizeof(s_str_unknown), "Unknown %d", value);
		return s_str_unknown;
	}
}

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

VdecHelper::~VdecHelper()
{
	Destory();
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

/*
** --------------------------------- METHODS ----------------------------------
*/

int VdecHelper::Init()
{
	AX_S32 sRet = AX_SUCCESS;

	AX_U64 streamPhyAddr = 0;
	AX_VOID *pStreamVirAddr = NULL;
	buf_addr_.pVirAddr = 0;
	buf_addr_.u64PhyAddr = 0;
	sRet = AX_SYS_MemAlloc(&streamPhyAddr, (AX_VOID **)&pStreamVirAddr,
						   buf_size_, 0x100, (AX_S8 *)"vdec_input_stream");
	if (sRet != AX_SUCCESS)
	{
		LOG_ERROR_LOC("AX_SYS_MemAlloc FAILED code:{:#x}",sRet);
		return sRet;
	}

	buf_addr_.u64PhyAddr = streamPhyAddr;
	buf_addr_.pVirAddr = pStreamVirAddr;

	AX_VDEC_GRP_ATTR_T pstVdGrpAttr_;
	memset(&pstVdGrpAttr_, 0x0, sizeof(AX_VDEC_GRP_ATTR_T));
	pstVdGrpAttr_.enCodecType = codec_type_;											  // 96
	pstVdGrpAttr_.u32MaxPicWidth = AX_COMM_ALIGN(frame_width_, 16); /*Max pic width*/	  // 1920
	pstVdGrpAttr_.u32MaxPicHeight = AX_COMM_ALIGN(frame_height_, 16); /*Max pic height*/ // 1080
	// pstVdGrpAttr_.u32MaxPicHeight = frameHeight; /*Max pic height*/ // 1080
	pstVdGrpAttr_.u32StreamBufSize = buf_size_; // 3 * 1024 * 1024
	pstVdGrpAttr_.enInputMode = AX_VDEC_INPUT_MODE_FRAME;
	pstVdGrpAttr_.bSdkAutoFramePool = AX_TRUE;
	pstVdGrpAttr_.bSkipSdkStreamPool = AX_FALSE;

	sRet = AX_VDEC_CreateGrp(vd_grp_, &pstVdGrpAttr_);
	if (sRet != AX_SUCCESS)
	{
		LOG_ERROR_LOC("AX_VDEC_CreateGrp FAILED VdGrp:{},code:{:#x},msg:{}", vd_grp_,sRet, AX_VdecRetStr(sRet));
		return sRet;
	}

	AX_VDEC_CHN VdChn = 0;
	AX_VDEC_CHN_ATTR_T pstVdChnAttr_[AX_DEC_MAX_CHN_NUM] = {0x0};
	for (VdChn = 0; VdChn < AX_DEC_MAX_CHN_NUM; VdChn++)
	{

		pstVdChnAttr_[VdChn].enImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;

		pstVdChnAttr_[VdChn].u32FrameStride = 0;
		pstVdChnAttr_[VdChn].u32FramePadding = 0;
		pstVdChnAttr_[VdChn].u32CropX = 0;
		pstVdChnAttr_[VdChn].u32CropY = 0;

		pstVdChnAttr_[VdChn].u32ScaleRatioX = 0;
		pstVdChnAttr_[VdChn].u32ScaleRatioY = 0;
		pstVdChnAttr_[VdChn].u32OutputFifoDepth = 8;

		if (VdChn == 0)
		{
			pstVdChnAttr_[VdChn].enOutputMode = AX_VDEC_OUTPUT_ORIGINAL;
			pstVdChnAttr_[VdChn].u32PicWidth = frame_width_;
			pstVdChnAttr_[VdChn].u32PicHeight = frame_height_;
			int uPixBits = 8;
			if (pstVdChnAttr_[VdChn].u32FrameStride == 0)
			{
				pstVdChnAttr_[VdChn].u32FrameStride = AX_COMM_ALIGN(frame_width_ * uPixBits, AX_VDEC_WIDTH_ALIGN * 8) / 8;
			}
		}
		// else if (VdChn == 1)
		// {
		//     pstVdChnAttr_[VdChn].enOutputMode = AX_VDEC_OUTPUT_SCALE;
		//     pstVdChnAttr_[VdChn].u32PicWidth = 640;
		//     pstVdChnAttr_[VdChn].u32PicHeight = 640;
		// }

		if (VdChn >= 1)
		{
			continue;
		}

		AX_U32 uWidth = pstVdChnAttr_[VdChn].u32PicWidth;
		AX_U32 uPixBits = 8;

		// int frameStride = AX_COMM_ALIGN(uWidth * uPixBits, AX_VDEC_WIDTH_ALIGN * 8) / 8;
		if (pstVdChnAttr_[VdChn].u32FrameStride == 0)
		{
			pstVdChnAttr_[VdChn].u32FrameStride = AX_COMM_ALIGN(uWidth * uPixBits, AX_VDEC_WIDTH_ALIGN * 8) / 8;
		}

		if (pstVdGrpAttr_.bSdkAutoFramePool == AX_TRUE)
		{
			int heightAlign = ALIGN_UP(pstVdChnAttr_[VdChn].u32FrameStride, 2);
			pstVdChnAttr_[VdChn].u32FrameBufSize = AX_VDEC_GetPicBufferSize(
				pstVdChnAttr_[VdChn].u32FrameStride,
				heightAlign,
				pstVdChnAttr_[VdChn].enImgFormat,
				&pstVdChnAttr_[VdChn].stCompressInfo,
				pstVdGrpAttr_.enCodecType);

			pstVdChnAttr_[VdChn].u32FrameBufCnt = 16;
		}

		pstVdChnAttr_[VdChn].u32FramePadding = 0;

		sRet = AX_VDEC_SetChnAttr(vd_grp_, VdChn, &pstVdChnAttr_[VdChn]);
		if (sRet != AX_SUCCESS)
		{
			LOG_ERROR_LOC("AX_VDEC_SetChnAttr FAILED code:{:#x}, msg:{}", sRet, AX_VdecRetStr(sRet));
			return sRet;
		}

		sRet = AX_VDEC_EnableChn(vd_grp_, VdChn);
		if (sRet != AX_SUCCESS)
		{
			LOG_ERROR_LOC("AX_VDEC_EnableChn FAILED code:{:#x}, msg:{}", sRet, AX_VdecRetStr(sRet));
			return sRet;
		}
	} // for

	AX_VDEC_GRP_PARAM_T stGrpParam_;
	memset(&stGrpParam_, 0, sizeof(stGrpParam_));
	stGrpParam_.stVdecVideoParam.enOutputOrder = AX_VDEC_OUTPUT_ORDER_DISP;
	stGrpParam_.stVdecVideoParam.enVdecMode = VIDEO_DEC_MODE_IPB;
	stGrpParam_.f32SrcFrmRate = fps_;
	sRet = AX_VDEC_SetGrpParam(vd_grp_, &stGrpParam_);
	if (sRet != AX_SUCCESS)
	{
		LOG_ERROR_LOC("AX_VDEC_SetGrpParam FAILED code:{:#x}, msg:{}", sRet, AX_VdecRetStr(sRet));
		return sRet;
	}

	AX_VDEC_DISPLAY_MODE_E enDisplayMode = AX_VDEC_DISPLAY_MODE_PREVIEW;
	sRet = AX_VDEC_SetDisplayMode(vd_grp_, enDisplayMode);
	if (sRet != AX_SUCCESS)
	{
		LOG_ERROR_LOC("AX_VDEC_SetDisplayMode FAILED code:{:#x}, msg:{}", sRet, AX_VdecRetStr(sRet));
		return sRet;
	}

	return 0;
};

void *VdecHelper::RecvStreamFunc(void *argv)
{
	AX_S32 sRet = AX_SUCCESS;
	VdecHelper *self = (VdecHelper *)argv;
	AX_VDEC_CHN VdChn = 0;
	AX_VDEC_GRP VdGrp = self->vd_grp_;
	self->is_stop_ = false;
	pthread_setname_np(pthread_self(), "VDECGet");
	while (!self->is_stop_)
	{

		AX_VIDEO_FRAME_INFO_T *frameInfo = new AX_VIDEO_FRAME_INFO_T();
		memset(frameInfo, 0x0, sizeof(AX_VIDEO_FRAME_INFO_T));

		// 0：非阻塞 -1：阻塞
		sRet = AX_VDEC_GetChnFrame(VdGrp, VdChn, frameInfo, -1);
		// frameInfo->stVFrame.u64UserData
		if (sRet != AX_SUCCESS)
		{
			LOG_ERROR_LOC("AX_VDEC_GetChnFrame FAILED VdGrp:{} VdChn:{} code:{:#x}, msg:{}", VdGrp, VdChn, sRet, AX_VdecRetStr(sRet));
			sRet = AX_VDEC_ReleaseChnFrame(VdGrp, VdChn, frameInfo);
			delete frameInfo;
		}
		else
		{
			// fprintf(stdout, "AX_VDEC_GetChnFrame AX_SUCCESS\n");
		}

		if (sRet == AX_SUCCESS)
		{

			frameInfo->stVFrame.u64VirAddr[0] = 0;
			frameInfo->stVFrame.u64VirAddr[1] = 0;
			frameInfo->stVFrame.u64VirAddr[2] = 0;

			ImageData image;
			image.data = FrameData::Create(frameInfo, VdGrp, VdChn, MEM_ID_VDEC);
			image.bEndOfStream = frameInfo->bEndOfStream;
			image.enImgFormat = frameInfo->stVFrame.enImgFormat;
			image.u32Width = frameInfo->stVFrame.u32Width;
			image.u32Height = frameInfo->stVFrame.u32Height;

			self->callback_(image, VdGrp, VdChn, self->user_data_);

			// std::shared_ptr<FrameData> data = std::make_shared<FrameData>(frameInfo, VdGrp, VdChn, MEM_ID_VDEC);

			// ImageData image;
			// image.bEndOfStream = frameInfo->bEndOfStream;
			// image.enImgFormat = frameInfo->stVFrame.enImgFormat;
			// image.u32Width = frameInfo->stVFrame.u32Width;
			// image.u32Height = frameInfo->stVFrame.u32Height;
			// image.data = data;

			// self->FrameImageEnQueue(std::make_shared<ImageData>(image));
		}
		else if (sRet == AX_ERR_VDEC_QUEUE_EMPTY)
		{
			// 队列中无数据
			/* no data in unblock mode or timeout mode */
			usleep(20 * 1000);
			continue;
		}
		else if (sRet == AX_ERR_VDEC_UNEXIST)
		{
			// 目标对象不存在
			usleep(20 * 1000);
			continue;
		}
		else if (sRet == AX_ERR_VDEC_FLOW_END)
		{
			// 流程终止
			break;
		}
		else if (sRet == AX_ERR_VDEC_STRM_ERROR)
		{
			// 输入码流错误,无法解码
			continue;
		}
		else if (AX_ERR_VDEC_NOT_PERM == sRet)
		{
			// 操作不允许 硬件初始化中
			continue;
		}
		else
		{
			break;
		}
	}
	// fprintf(stdout, "Read Vdec Data Stop %s\n", AX_VdecRetStr(sRet));
	LOG_INFO_LOC("Read Vdec Data Stop code:{}, msg:{}", sRet, AX_VdecRetStr(sRet));
	return nullptr;
}
int VdecHelper::Decode(VdecProcessCallBack callbac, void *user_data)
{

	AX_VDEC_RECV_PIC_PARAM_T tRecvParam;
	memset(&tRecvParam, 0, sizeof(tRecvParam));
	tRecvParam.s32RecvPicNum = -1;
	AX_S32 sRet = AX_VDEC_StartRecvStream(vd_grp_, &tRecvParam);
	if (sRet != AX_SUCCESS)
	{
		LOG_ERROR_LOC("AX_VDEC_StartRecvStream FAILED code:{:#x}, msg:{}", sRet, AX_VdecRetStr(sRet));
		return sRet;
	}

	callback_ = callbac;
	user_data_ = user_data;

	pthread_create(&recv_tid_, nullptr, RecvStreamFunc, this);

	return 0;
}

int VdecHelper::StopDecode()
{
	WriteEOF();

	is_stop_ = true;

	AX_S32 sRet;

	sRet = AX_VDEC_StopRecvStream(vd_grp_);
	if (sRet)
	{
		if (sRet == AX_ERR_VDEC_UNEXIST)
		{
			return 0;
		}
	}

	void *res = nullptr;
	int joinThreadErr = pthread_join(recv_tid_, &res);
	if (joinThreadErr)
	{
		LOG_ERROR_LOC("Join thread failed, threadId = {}, err = {:#x}",
				  recv_tid_, joinThreadErr);
	}
	else
	{
		if ((uint64_t)res != 0)
		{
			LOG_ERROR_LOC("thread run failed. ret is {}", (uint64_t)res);
		}
	}
	// 等待线程退出

	// get all frame
	while (1)
	{
		AX_VIDEO_FRAME_INFO_T pstFrameInfo;
		AX_VDEC_GRP_STATUS_T pstGrpStatus;

		sRet = AX_VDEC_GetChnFrame(vd_grp_, 0, &pstFrameInfo, -1);

		if (sRet == AX_SUCCESS)
		{
			sRet = AX_VDEC_ReleaseChnFrame(vd_grp_, 0, &pstFrameInfo);
		}

		sRet = AX_VDEC_QueryStatus(vd_grp_, &pstGrpStatus);
		if (pstGrpStatus.u32LeftStreamBytes == 0 && pstGrpStatus.u32LeftPics[0] == 0)
		{
			break;
		}
	}
	is_finished_ = true;
	return 0;
}

int VdecHelper::WriteEOF()
{
	AX_VDEC_STREAM_T tStrInfo = {0};
	tStrInfo.bEndOfStream = AX_TRUE;
	AX_S32 sRet = AX_VDEC_SendStream(vd_grp_, &tStrInfo, -1);
	return sRet;
};

#if 0
#include "vdec.h"
int VdecHelper::Write(void *data, size_t data_size) {
	vdec_write_frame(vd_grp_,&buf_addr_,buf_size_, (AX_U8*)data, data_size);
	return 0;
}

#else

int VdecHelper::Write(void *data, size_t data_size, void *user_data)
{
	int sRet;
	AX_VDEC_STREAM_T tStrInfo = {0};

	memset(buf_addr_.pVirAddr, 0x0, data_size);
	memcpy(buf_addr_.pVirAddr, data, data_size);

	tStrInfo.pu8Addr = (AX_U8 *)buf_addr_.pVirAddr;
	tStrInfo.u64PhyAddr = 0;
	tStrInfo.u32StreamPackLen = (AX_U32)data_size; /*stream len*/
	tStrInfo.bEndOfFrame = AX_TRUE;
	tStrInfo.bEndOfStream = AX_FALSE;
	tStrInfo.u64PTS = 0;
	tStrInfo.u64PrivateData = 0xAFAF5A5A;

	tStrInfo.u64UserData = (AX_ULONG)user_data;

	sRet = AX_SYS_GetCurPTS(&tStrInfo.u64PTS);
	if (sRet)
	{
		LOG_ERROR_LOC("AX_SYS_GetCurPTS FAILED code:{:#x},msg:{}", sRet, AX_VdecRetStr(sRet));
		// ret = sRet;
		// goto ERR_RET;
	}
	// tStrInfo.u64PTS = uTmpPts + 1;

	// tStrInfo.u64PrivateData = 0xAFAF5A5A;
	// 0 非阻塞
	sRet = AX_VDEC_SendStream(vd_grp_, &tStrInfo, 0);
	if (sRet == AX_SUCCESS)
	{
		return sRet;
	}
	else
	{
		LOG_ERROR_LOC("AX_VDEC_SendStream FAILED VdGrp:{},code:{:#x},msg:{}", vd_grp_,sRet, AX_VdecRetStr(sRet));
	}

	if (sRet == AX_ERR_VDEC_FLOW_END)
	{

		return AX_ERR_VDEC_FLOW_END;
	}
	else if (sRet == AX_ERR_VDEC_QUEUE_FULL)
	{
		usleep(1000);
		return 0;
	}
	else if ((sRet == AX_ERR_VDEC_NOT_PERM) || (sRet == AX_ERR_VDEC_NOT_MATCH))
	{
		usleep(1000);
		return 0;
	}
	else if ((sRet == AX_ERR_VDEC_NOT_SUPPORT) || (sRet == AX_ERR_VDEC_NOMEM) || (sRet == AX_ERR_VDEC_NOBUF))
	{
		// TODO: 直接可以发送结束
		// ret = __VdecSendEndOfStream(VdGrp);
		// if (ret) {
		//     SAMPLE_CRIT_LOG("VdGrp=%d, __VdecSendEndOfStream FAILED! ret:0x%x %s",
		//                     VdGrp, ret, AX_VdecRetStr(ret));
		// }
		// goto ERR_RET;
		return sRet;
	}
	else
	{
		// fprintf(stdout, "VdGrp=%d, AX_VDEC_SendStream FAILED! ret:0x%x %s\n",
		// 		vd_grp_, sRet, AX_VdecRetStr(sRet));
		// TODO: 直接可以发送结束
		// ret = AX_ERR_VDEC_UNKNOWN;
		// goto ERR_RET;
		// LOG_ERROR_LOC("AX_VDEC_SendStream FAILED VdGrp:{},code:{},msg:{}");
	}
	return sRet;
}
#endif
int VdecHelper::Destory()
{
	AX_S32 sRet;

	while (1)
	{
		sRet = AX_VDEC_DestroyGrp(vd_grp_);
		if (sRet == AX_ERR_VDEC_BUSY)
		{
			usleep(10000);
			continue;
		}
		break;
	}

	if (buf_addr_.u64PhyAddr != 0)
	{
		sRet = AX_SYS_MemFree(buf_addr_.u64PhyAddr, buf_addr_.pVirAddr);
		if (sRet != AX_SUCCESS)
		{
		}
		else
		{
			buf_addr_.u64PhyAddr = 0;
			buf_addr_.pVirAddr = 0;
		}
	}
	else
	{
		free(buf_addr_.pVirAddr);
		buf_addr_.pVirAddr = 0;
	}
	return sRet;
};

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
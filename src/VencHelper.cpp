
#include <string.h>

#include "VencHelper.hpp"
#include "Logger.h"

static const AX_U32 u32MaxPixelWidth = 16384;
static const AX_U32 u32MaxPixelHeight = 16384;
static const AX_U32 u32DefaultSrcPixelWidth = 3840;
static const AX_U32 u32DefaultSrcPixelHeight = 2160;

static const AX_U32 u32StrideAlign = 64;
static const AX_F32 f32SrcVideoFrameRate = 30.0;
static const AX_F32 f32DstVideoFrameRate = 30.0;

/*QP:0-51, QPLevel:0-99, the lower qp/qp level value, the higher quality*/
static const AX_U32 gU32QPMin = 10;
static const AX_U32 gU32QPMax = 22;
static const AX_U32 gU32FixedQP = 22;
static const AX_S32 gS32QPLevel = 90;

static const AX_S32 gS32EnableCrop = 0;
static const AX_S32 gS32QTableEnable = 0;

static const AX_S32 gSyncType = -1;

#define UVC_SNS_OS08A20_MAX_WIDTH 3840
#define UVC_SNS_OS08A20_MAX_HEIGHT 2160

#define JENC_NUM_COMM 64
#define ISP_JENC_NUM 3

#define VENC_NUM_COMM 64

#define MAX_UVC_CAMERAS 2

#define VIDEO_ENABLE_RC_DYNAMIC

#define UVC_ENCODER_FBC_WIDTH_ALIGN_VAL (256)
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define COLOR_COUNT 6

#define AX_ENC_VALUE_2_STR_CASE(s32Ret) \
	case (s32Ret):                      \
		return (#s32Ret)

static char s_str_unknown[16] = ("Unknown code");

const char *AX_VencRetStr(AX_S32 value)
{
	switch (value)
	{
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_CREATE_CHAN_ERR);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_SET_PRIORITY_FAIL);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NULL_PTR);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_ILLEGAL_PARAM);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_BAD_ADDR);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NOT_SUPPORT);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NOT_INIT);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_BUF_EMPTY);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_BUF_FULL);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_QUEUE_EMPTY);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_QUEUE_FULL);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_EXIST);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_UNEXIST);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NOT_PERMIT);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_TIMEOUT);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_FLOW_END);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_ATTR_NOT_CFG);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_SYS_NOTREADY);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_INVALID_CHNID);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NOMEM);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_NOT_MATCH);
		AX_ENC_VALUE_2_STR_CASE(AX_ERR_VENC_INVALID_GRPID);

	default:
		// SAMPLE_CRIT_LOG("Unknown return code. 0x%x", value);
		snprintf(s_str_unknown, sizeof(s_str_unknown), "Unknown %d", value);
		return s_str_unknown;
	}
}

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

void *VencHelper::VencRecvThreadFunc(void *argv)
{
	int ret;
	VencHelper *self = (VencHelper *)argv;
	AX_VENC_STREAM_T stStream;

	while (!self->IsExit())
	{
		LOG_INFO("VencRecvThreadFunc %d\n", self->m_nChn);
		// -1：阻塞 0：非阻塞
		ret = AX_VENC_GetStream(self->m_nChn, &stStream, 100);
		// stStream.stPack.u64SeqNum
		if (AX_SUCCESS == ret)
		{
			self->m_pCallBack(stStream, self->m_nChn, self->m_pUserData);

			// 这里释放数据
			ret = AX_VENC_ReleaseStream(self->m_nChn, &stStream);
			if (AX_SUCCESS != ret)
			{
				LOG_ERROR("VencRecvThreadFunc chn-%d: AX_VENC_ReleaseStream failed! 0x%X\n", self->m_nChn, ret);
				return (void *)0;
			}
		}
		else if (AX_ERR_VENC_QUEUE_EMPTY == ret ||
				 AX_ERR_VENC_UNEXIST == ret ||
				 AX_ERR_VENC_FLOW_END == ret ||
				 AX_ERR_VENC_BUF_EMPTY == ret ||
				 AX_ERR_VENC_BUF_FULL == ret)
		{
			usleep(500);
		}
		else
		{
			LOG_ERROR("AX_VENC_GetStream failed!!! ret 0x%X %s", ret, AX_VencRetStr(ret));
		}
	}
	LOG_INFO("AX_VENC_GetStream Exit Success");
	return nullptr;
}

int VencHelper::Init()
{
	AX_S32 s32Ret = -1;

	AX_S32 widthSrc = m_nPictureWidth;
	AX_S32 heightSrc = m_nPictureHeight;

	// stJencChnAttr.stVencAttr.u32MaxPicWidth = ALIGN_UP(s32InputWidth, UVC_ENCODER_FBC_WIDTH_ALIGN_VAL);
	// stJencChnAttr.stVencAttr.u32MaxPicHeight = ALIGN_UP(s32InputHeight, UVC_ENCODER_FBC_WIDTH_ALIGN_VAL);

	AX_S32 maxPicWidth = ALIGN_UP(widthSrc, UVC_ENCODER_FBC_WIDTH_ALIGN_VAL);
	AX_S32 maxPicHeight = ALIGN_UP(heightSrc, UVC_ENCODER_FBC_WIDTH_ALIGN_VAL);
	// AX_U32 gopLen = 30;
	// AX_U32 virILen;

	/*QP:0-51, QPLevel:0-99, the lower qp/qp level value, the higher quality*/
	// AX_U32 bitRate = 2000; // kbps
	// AX_U16 qpMin = 10;
	// AX_U16 qpMax = 51;
	// AX_U16 qpMinI = 10;
	// AX_U16 qpMaxI = 51;
	// AX_U16 u32IQp = 25;
	// AX_U16 u32PQp = 30;

	// AX_S32 startQp = -1;
	// AX_S32 strmBitDep = 8;
	// AX_U8 inFifoDep = 1;
	// AX_U8 outFifoDep = 1;
	// AX_F32 srcFrameRate = f32SrcVideoFrameRate;
	// AX_F32 dstFrameRate = f32DstVideoFrameRate;
	// AX_U32 maxIprop = 10;
	// AX_U32 minIprop = 40;

	AX_U32 strmBufSize = widthSrc * heightSrc * 3 / 2;

	AX_VENC_CHN_ATTR_T stVencChnAttr;
	memset(&stVencChnAttr, 0, sizeof(AX_VENC_CHN_ATTR_T));

	stVencChnAttr.stVencAttr.enType = PT_H264;
	stVencChnAttr.stVencAttr.u32PicWidthSrc = widthSrc;	  /*the input picture width*/
	stVencChnAttr.stVencAttr.u32PicHeightSrc = heightSrc; /*the input picture height*/
	stVencChnAttr.stVencAttr.u32MaxPicWidth = maxPicWidth;
	stVencChnAttr.stVencAttr.u32MaxPicHeight = maxPicHeight;

	stVencChnAttr.stVencAttr.enLinkMode = AX_VENC_UNLINK_MODE;

	stVencChnAttr.stVencAttr.u8InFifoDepth = 1;
	stVencChnAttr.stVencAttr.u8OutFifoDepth = 1;
	stVencChnAttr.stVencAttr.u32BufSize = strmBufSize;

	/* GOP table setting */
	stVencChnAttr.stGopAttr.enGopMode = AX_VENC_GOPMODE_NORMALP;

	stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = m_nSrcFrameRate;
	stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = m_nDstFrameRate;

	stVencChnAttr.stVencAttr.enProfile = AX_VENC_H264_MAIN_PROFILE;
	// stVencChnAttr.stVencAttr.enStrmBitDepth = AX_VENC_STREAM_BIT_8;
	stVencChnAttr.stVencAttr.enLevel = AX_VENC_H264_LEVEL_5_1;

	// MARK: config H264 info

	AX_VENC_H264_CBR_T stH264Cbr;
	memset(&stH264Cbr, 0, sizeof(AX_VENC_H264_CBR_T));
	stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_H264CBR;
	stVencChnAttr.stRcAttr.s32FirstFrameStartQp = -1;

	stH264Cbr.u32Gop = 25;
	stH264Cbr.u32BitRate = 8000;
	stH264Cbr.u32MinQp = 16;
	stH264Cbr.u32MaxQp = 46;
	stH264Cbr.u32MinIQp = 10;
	stH264Cbr.u32MaxIQp = 51;
	stH264Cbr.u32MinIprop = 10;
	stH264Cbr.u32MaxIprop = 40;
	stH264Cbr.s32IntraQpDelta = -2;
	memcpy(&stVencChnAttr.stRcAttr.stH264Cbr, &stH264Cbr, sizeof(AX_VENC_H264_CBR_T));

	/* create channel */
	s32Ret = AX_VENC_CreateChn(m_nChn, &stVencChnAttr);
	if (AX_SUCCESS != s32Ret)
	{
		printf("VencChn [%d]: AX_VENC_CreateChn failed with %#x!", m_nChn, s32Ret);
		return s32Ret;
	}

	// LOG(INFO) << "Encoder Init Success!!";
	return s32Ret;
}

int VencHelper::Encode(VencProcessCallBack callback, void *user_data)
{
	AX_S32 s32Ret;
	m_pCallBack = callback;
	m_pUserData = user_data;
	AX_VENC_RECV_PIC_PARAM_T stRecvParam;
	stRecvParam.s32RecvPicNum = -1;
	s32Ret = AX_VENC_StartRecvFrame(m_nChn, &stRecvParam);
	if (AX_SUCCESS != s32Ret)
	{
		printf("VencChn [%d]: AX_VENC_StartRecvFrame failed with %#x!", m_nChn, s32Ret);
		return s32Ret;
	}

	s32Ret = pthread_create(&m_recvThd, NULL, VencRecvThreadFunc, (void *)this);

	if (s32Ret != 0)
	{
		// g_error_code = VENC_ERROR;
		// LOG(ERROR) << "pthread_create VencRecvThreadFunc " << ret;
	}
	return s32Ret;
}

int VencHelper::Write(ImageData *imageData, void *user_data)
{
	
	// imageData->data->FrameInfo()->stVFrame.u64UserData = (AX_LONG) user_data;
	// (AX_ULONG)user_data;
	TIME_START(WriteVenc);
	
	AX_VIDEO_FRAME_INFO_T *frameInfo = imageData->data->FrameInfo();
	frameInfo->stVFrame.u64UserData = (AX_ULONG) user_data;		
	// 写流 0 非阻塞
	
	int s32Ret = AX_VENC_SendFrame(m_nChn, imageData->data->FrameInfo(), 0);
	if (AX_SUCCESS != s32Ret)
	{
		LOG_ERROR("chn-%d: AX_VENC_SendFrame failed, code:%x msg:%s\n", m_nChn, s32Ret,AX_VencRetStr(s32Ret));
	}
	else
	{
		LOG_INFO("chn-%d: AX_VENC_SendFrame SUCCESS, code:%x msg:%s\n", m_nChn, s32Ret,AX_VencRetStr(s32Ret));
	}
	TIME_END(WriteVenc);

	TIME_USEC_SHOW(WriteVenc);

	return s32Ret;
};

int VencHelper::StopEncode()
{
	WriteEOF();
	AX_S32 s32Ret = AX_SUCCESS;
	m_isStop = true;

	s32Ret = AX_VENC_StopRecvFrame(m_nChn);
	if (AX_SUCCESS != s32Ret)
	{
		LOG_ERROR("chn-%d: AX_VENC_StopRecvFrame failed with%#x! \n", m_nChn, s32Ret);
		return s32Ret;
	}

	void *res = nullptr;
	int joinThreadErr = pthread_join(m_recvThd, &res);
	if (joinThreadErr)
	{
		LOG_ERROR("Join thread failed, threadId = %lu, err = %d",
				  m_recvThd, joinThreadErr);
	}
	else
	{
		if ((uint64_t)res != 0)
		{
			LOG_ERROR("thread run failed. ret is %lu.", (uint64_t)res);
		}
	}

	while (1)
	{
		AX_VENC_STREAM_T stStream;
		AX_VENC_CHN_STATUS_T pstStatus;
		s32Ret = AX_VENC_GetStream(m_nChn, &stStream, 100);
		if (s32Ret == AX_SUCCESS)
		{
			s32Ret = AX_VENC_ReleaseStream(m_nChn, &stStream);
		}
		s32Ret = AX_VENC_QueryStatus(m_nChn, &pstStatus);
		if (pstStatus.u32LeftStreamBytes == 0 && pstStatus.u32LeftPics == 0)
		{
			break;
		}
	}
	return 0;
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

int VencHelper::Destroy()
{
	AX_S32 s32Ret = AX_SUCCESS;

	s32Ret = AX_VENC_DestroyChn(m_nChn);
	if (AX_SUCCESS != s32Ret)
	{
		LOG_ERROR("chn-%d: AX_VENC_DestroyChn failed with%#x! \n", m_nChn, s32Ret);
		return s32Ret;
	}
	return s32Ret;
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

/*
** --------------------------------- METHODS ----------------------------------
*/

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
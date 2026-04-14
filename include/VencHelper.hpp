#ifndef VENCHELPER_HPP
#define VENCHELPER_HPP

#include <iostream>
#include <string>

#include "ax_venc_api.h"
#include "ax_global_type.h"
#include "logger.hpp"
#include "ImageData.hpp"
// class StreamData
// {
// public:
// 	AX_VENC_STREAM_T m_stStream;
// 	VENC_CHN m_nVeChn;
// 	StreamData(VENC_CHN veChn, AX_VENC_STREAM_T *pStStream) : m_nVeChn(veChn)
// 	{
// 		memset(&m_stStream, 0x0, sizeof(AX_VENC_STREAM_T));
// 		memcpy(&m_stStream, pStStream, sizeof(AX_VENC_STREAM_T));
// 	};

// 	~StreamData()
// 	{
// 		AX_S32 ret = AX_VENC_ReleaseStream(m_nVeChn, &m_stStream);
// 		if (AX_SUCCESS != ret)
// 		{
// 			LOG_ERROR("AX_VENC_ReleaseStream failed! chn-%d: 0x%X\n", m_nVeChn, ret);
// 		}
// 	}
// };

typedef int (*VencProcessCallBack)(AX_VENC_STREAM_T streamData,
								   int chn,
								   void *user_data);

class VencHelper
{
public:
	VencHelper(VENC_CHN VeChn, int picture_width, int picture_height, float srcFrameRate, float dstFrameRate) : m_nChn(VeChn), m_nPictureWidth(picture_width), m_nPictureHeight(picture_height),
																												m_nSrcFrameRate(srcFrameRate), m_nDstFrameRate(dstFrameRate) {

																												};

	int Init();

	int Encode(VencProcessCallBack callback, void *user_data);

	int StopEncode();

	int WriteEOF(){
		AX_VIDEO_FRAME_INFO_T pstFrame = {0};
		memset(&pstFrame,0x0,sizeof(AX_VIDEO_FRAME_INFO_T));
		pstFrame.bEndOfStream = AX_TRUE;

		int s32Ret = AX_VENC_SendFrame(m_nChn, &pstFrame, 0);
		return s32Ret;

	};
	int Write(ImageData *imageData, void *user_data);
	
	int Destroy();
	~VencHelper()
	{
		Destroy();
	};
	VencHelper(VencHelper const &src) = delete;

	VencHelper &operator=(VencHelper const &rhs) = delete;
	bool IsExit()
	{
		return m_isStop;
	}

private:
	static void *VencRecvThreadFunc(void *argv);
	VENC_CHN m_nChn;
	int m_nPictureWidth;
	int m_nPictureHeight;
	int m_nSrcFrameRate;
	int m_nDstFrameRate;
	bool m_isStop = false;
	pthread_t m_recvThd;
	void *m_pUserData;
	VencProcessCallBack m_pCallBack = nullptr;
};

std::ostream &operator<<(std::ostream &o, VencHelper const &i) = delete;

#endif /* ****************************************************** VENCHELPER_H */
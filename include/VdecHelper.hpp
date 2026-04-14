#ifndef AX_VDEC_HPP
#define AX_VDEC_HPP

#include <iostream>
#include <string>
#include "ax_global_type.h"
#include "ax_vdec_api.h"
#include "ImageData.hpp"
#include "ThreadSafeQueue.h"
#include "FrameData.hpp"
#include "ImageData.hpp"
typedef int (*VdecProcessCallBack)(ImageData imageData,
								   int grp, int chn,
								   void *user_data);

class VdecHelper
{

public:
	VdecHelper(AX_VDEC_GRP vdGrp, AX_PAYLOAD_TYPE_E codecType, AX_U32 frameWidth, AX_U32 frameHeight, int fps = 25) : m_nVdGrp(vdGrp), m_enCodecType(codecType), m_nFrameWidth(frameWidth), m_nFrameHeight(frameHeight),m_nFps(fps) , m_pCallBack(nullptr){};

	~VdecHelper();

	int Init();
	int Destory();
	static void *RecvStreamFunc(void *argv);
	int Decode(VdecProcessCallBack callbac, void *user_data);
	int StopDecode();
	// bool IsStop() { return m_isStop; };
	int WriteEOF();
	int Write(void *data, size_t data_size, void *user_data);
	// int FrameImageEnQueue(std::shared_ptr<ImageData> frameData)
	// {
	// 	for (int count = 0; count < 1000; count++)
	// 	{
	// 		if (m_tFrameImageQueue.Push(frameData))
	// 		{
	// 			return 0;
	// 		}

	// 		usleep(10000);
	// 	}
	// 	return -1;
	// }

	// std::shared_ptr<ImageData> FrameImageOutQueue(bool noWait = false)
	// {
	// 	std::shared_ptr<ImageData> image = m_tFrameImageQueue.Pop();

	// 	if (noWait || (image != nullptr))
	// 		return image;

	// 	for (int count = 0; count < 1000 - 1; count++)
	// 	{
	// 		usleep(10000);

	// 		image = m_tFrameImageQueue.Pop();
	// 		if (image != nullptr)
	// 			return image;
	// 	}

	// 	return nullptr;
	// }
	VdecHelper() = delete;
	VdecHelper(VdecHelper const &src) = delete;
	VdecHelper &operator=(VdecHelper const &rhs) = delete;

	AX_VDEC_GRP VdGrp()
	{
		return m_nVdGrp;
	};

private:
	// ThreadSafeQueue<std::shared_ptr<ImageData>> m_tFrameImageQueue;
	
	void *m_pUserData;
	/// @brief 设置停止解码器
	bool m_isStop = false;

	/// @brief 解码器退出状态
	bool m_isFinished = false;

	AX_VDEC_GRP m_nVdGrp = -1;
	AX_PAYLOAD_TYPE_E m_enCodecType = PT_BUTT;
	AX_U32 m_nFrameWidth = 0;
	AX_U32 m_nFrameHeight = 0;
	int m_nFps = 25;
	VdecProcessCallBack m_pCallBack = nullptr;
	
	/// @brief AX_FORMAT_YUV420_SEMIPLANAR
	AX_IMG_FORMAT_E m_enImgFormat{AX_FORMAT_YUV420_SEMIPLANAR};

	/// @brief 3 * 1024 * 1024
	AX_U32 m_nBufSize = 3 * 1024 * 1024;

	pthread_t m_thRecvTid{0};
	AX_MEMORY_ADDR_T m_tBufAddr;
};

std::ostream &operator<<(std::ostream &o, VdecHelper const &i) = delete;

#endif /* ********************************************************* AX_VDEC_H */
#ifndef FRAMEDATA_HPP
#define FRAMEDATA_HPP
#pragma once
#include <iostream>
#include <string>
#include <memory>
// #include <vector>
#include <string.h>
// #include "opencv2/opencv.hpp"

#include "ax_pool_type.h"
#include "ax_ivps_type.h"
#include "ax_vdec_type.h"

#include "ax_vdec_api.h"
#include "ax_ivps_api.h"

#include "ax_global_type.h"
#include "ax_buffer_tool.h"
#include "ax_sys_api.h"
typedef enum
{
	MEM_ID_MIN = 0x00,
	MEM_ID_VDEC = 0x01,
	MEM_ID_VENC = 0x02,
	MEM_ID_IVPS = 0x03,
	MEM_ID_IVES = 0x04,
	MEM_ID_JENC = 0x05,
	MEM_ID_JDEC = 0x06,
	MEM_ID_NPU = 0x07,
	MEM_ID_SYS = 0x08,
	MEM_ID_MAX = 0xFF /* 255 */
} MEM_ID_E;

/// @brief 管理AX_VIDEO_FRAME_INFO_T数据 生命周期,只进行接收,不进行创建.释放统一管理,上层只能使用只能指针进行数据管理
class FrameData
{

private:
	/// @brief 从硬件中读取数据初始化,有对应的释放函数
	/// @param frameData
	/// @param grp 输入通道号类型
	/// @param chn 输出通道号类型
	/// @param memId 内存类型ID
	explicit FrameData(AX_VIDEO_FRAME_INFO_T *frameData, AX_S32 grp, AX_S32 chn, MEM_ID_E memId)
	{
		m_pFrameData = frameData;
		m_nGrp = grp;
		m_nChn = chn;
		m_enMemID = memId;
	};

	/// @brief Copy内存
	/// @param frameData
	/// @param memId
	explicit FrameData(AX_VIDEO_FRAME_INFO_T *frameData, MEM_ID_E memId)
	{
		m_pFrameData = frameData;
		m_nGrp = -1;
		m_nChn = -1;
		m_enMemID = memId;
	};

	int Init() { return 0; };

	int Destroy();


	// FrameData(FrameData const &src)
	// {
	// 如果临时对象 是会释放之前的数据
	// 	m_enMemID = src.m_enMemID;
	// 	m_nGrp = src.m_nGrp;
	// 	m_nChn = src.m_nChn;
	// 	m_pFrameData = src.m_pFrameData;
	// };
	// friend std::shared_ptr<FrameData> FrameData::Create(AX_VIDEO_FRAME_INFO_T *frameData, AX_S32 grp, AX_S32 chn, MEM_ID_E memId);
public:
	~FrameData()
	{
		Destroy();
		delete m_pFrameData;
	};
	
	FrameData &operator=(FrameData const &rhs) = delete;
	FrameData(FrameData const &src) = delete;

	static std::shared_ptr<FrameData> Create(AX_VIDEO_FRAME_INFO_T *frameData, AX_S32 grp, AX_S32 chn, MEM_ID_E memId)
	{
		return std::shared_ptr<FrameData>(new FrameData(frameData, grp, chn, memId));
		// return std::make_shared<FrameData>(frameData, grp, chn, memId);
	}

	static std::shared_ptr<FrameData> Create(AX_VIDEO_FRAME_INFO_T *frameData, MEM_ID_E memId)
	{
		return std::shared_ptr<FrameData>(new FrameData(frameData, memId)); 
		// return std::make_shared<FrameData>(frameData, memId);
	}

	AX_VIDEO_FRAME_INFO_T *FrameInfo()
	{
		return m_pFrameData;
	}

private:
	MEM_ID_E m_enMemID = MEM_ID_MIN;
	AX_S32 m_nGrp = -1;
	AX_S32 m_nChn = -1;
	AX_VIDEO_FRAME_INFO_T *m_pFrameData = nullptr;
};

std::ostream &operator<<(std::ostream &o, FrameData const &i) = delete;

// namespace FrameDate::Create {

// };
#endif /* ******************************************************* FRAMEDATA_HPP */
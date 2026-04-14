#ifndef FRAMEINFOPROC_HPP
#define FRAMEINFOPROC_HPP

#include <ax_ivps_api.h>
#include <ax_ivps_type.h>
#include <memory.h>
#include <memory>
#include <mutex>
#include <vector>
#include "ImageData.hpp"
class IvpsHelper
{
public:
	IvpsHelper(IVPS_GRP IvpsGrp, AX_U64 blkSize,AX_U32 blkCnt);
	~IvpsHelper();
	// 缩放
	AX_S32 Resize(AX_IVPS_ASPECT_RATIO_E eMode, AX_U32 dest_width, AX_U32 dest_height);

	// 扣图 缩放 颜色转换
	AX_S32 Process(ImageData &dest_frame,
				   ImageData const& src_frame);

	AX_S32 CropAndCSC(AX_IMG_FORMAT_E eDstPicFormat,
					  AX_U16 nCropX,
					  AX_U16 nCropY,
					  AX_U16 nCropW,
					  AX_U16 nCropH);

	AX_S32 ResizeAndCSC(
		AX_IMG_FORMAT_E eDstPicFormat,
		AX_U32 nDstPicWidth,
		AX_U32 nDstPicHeight);
	AX_S32 CSC(AX_IMG_FORMAT_E eDstPicFormat);

	IVPS_GRP GetGrpID() const { return m_nIvpsGrp; };
private:
	AX_S32 Init();
	
	AX_S32 DestroyResource();

	AX_S32 CreatePool();

	AX_S32 CreateGrp();

private:
	IVPS_GRP m_nIvpsGrp{0};
	
    AX_U64 m_nBlkSize;
    AX_U32 m_nBlkCnt;	
	AX_IVPS_GRP_ATTR_T m_tGrpAttr;
	AX_IVPS_PIPELINE_ATTR_T m_tPipelineAttr;
	AX_IVPS_POOL_ATTR_T m_tPoolAttr;

	AX_POOL m_nPoolId{AX_INVALID_POOLID};
	/// @brief 只判断GRP 是否创建成功
	bool isInitialized{false};
	bool isReleased{false};
};

#endif /* *************************************************** FRAMEINFOPROC_H */
#include <memory.h>
#include <string.h>

#include "PipelineResource.h"
#include "Logger.h"

#include "ax_ivps_api.h"
#include "ax_vdec_api.h"
#include "ax_base_type.h"
#include "ax_sys_api.h"
#include "ax_venc_api.h"
#include "ax_venc_comm.h"

#include "ax_ivps_type.h"
#include "ax_pool_type.h"

using namespace std;

PipelineResource::PipelineResource() : m_isReleased(false), m_iChannelId(0)
{
}

PipelineResource::PipelineResource(int32_t channel) : m_isReleased(false),
                                                       m_iChannelId(channel)
                                                       
{
}

PipelineResource::~PipelineResource()
{
    Release();
}

int PipelineResource::Init()
{
    // MARK: 初始化系统资源

    int ret = 0;
    ret = AX_SYS_Init();
    if (AX_SUCCESS != ret)
    {
        LOG_ERROR("AX_SYS_Init Failed!! {:#x}", ret);
        return ret;
    }

    // ret = AX_IVPS_Init();
    // if (AX_SUCCESS != ret)
    // {
    //     LOG_ERROR("AX_IVPS_Init Failed!! %X\n", ret);
    //     return ret;
    // }

    AX_VDEC_MOD_ATTR_T stModAttr;
    memset(&stModAttr,0x0, sizeof(AX_VDEC_MOD_ATTR_T));

    stModAttr.enDecModule = AX_ENABLE_BOTH_VDEC_JDEC;
    stModAttr.u32MaxGroupCount = AX_VDEC_MAX_GRP_NUM;

    ret = AX_VDEC_Init(&stModAttr);
    if (AX_SUCCESS != ret)
    {
        LOG_ERROR("AX_VDEC_Init Failed!! {:#x}", ret);
        // LOG(ERROR) << ""
        return ret;
    }

    AX_VENC_MOD_ATTR_T stEncModAttr;
    memset(&stEncModAttr,0x0, sizeof(AX_VENC_MOD_ATTR_T));
    stEncModAttr.enVencType = AX_VENC_MULTI_ENCODER;
    stEncModAttr.stModThdAttr.u32TotalThreadNum = 1;
    stEncModAttr.stModThdAttr.bExplicitSched = AX_FALSE;
    ret = AX_VENC_Init(&stEncModAttr);
    if (AX_SUCCESS != ret)
    {
        LOG_ERROR("AX_VENC_Init Failed!! {:#x}", ret);
        return ret;
    }
    LOG_INFO("SYS INIT SUCCCESS !!!");
    return 0;
}

void PipelineResource::Release()
{
    int ret = 0;
    if (m_isReleased)
    {
        return;
    }

    // ret = AX_IVPS_Deinit();
    // if (ret != 0)
    // {
    //     LOG_ERROR("AX_IVPS_Deinit failed! Error Code:0x%x!\n", ret);
    // }

    ret = AX_VENC_Deinit();
    if (AX_SUCCESS != ret)
        LOG_ERROR("AX_VENC_Deinit failed! Error Code:{:#X}", ret);

    ret = AX_VDEC_Deinit();
    if (AX_SUCCESS != ret)
        LOG_ERROR("AX_VDEC_Deinit failed! Error Code:{:#X}", ret);

    ret = AX_SYS_Deinit();
    if (AX_SUCCESS != ret)
        LOG_ERROR("AX_SYS_Deinit failed! Error Code:{:#X}", ret);

    m_isReleased = true;
}

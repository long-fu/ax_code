#include "resource.h"
#include "ax_base_type.h"
#include "ax_ivps_api.h"
#include "ax_ivps_type.h"
#include "ax_pool_type.h"
#include "ax_sys_api.h"
#include "ax_vdec_api.h"
#include "ax_venc_api.h"
#include "ax_venc_comm.h"
namespace pipeline {

Resource::Resource() : is_released_(false), device_id_(0), config_path_("") {}

Resource::Resource(int32_t device_id, const std::string &config_path)
    : is_released_(false), device_id_(device_id), config_path_(config_path) {}

Resource::~Resource() { Release(); }

TaskError Resource::Init() {
  int ret = 0;
  ret = AX_SYS_Init();
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_SYS_Init Failed!! {:#x}", ret);
    return ret;
  }

  AX_VDEC_MOD_ATTR_T st_mod_attr;
  memset(&st_mod_attr, 0x0, sizeof(AX_VDEC_MOD_ATTR_T));

  st_mod_attr.enDecModule = AX_ENABLE_BOTH_VDEC_JDEC;
  st_mod_attr.u32MaxGroupCount = AX_VDEC_MAX_GRP_NUM;

  ret = AX_VDEC_Init(&st_mod_attr);
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_VDEC_Init Failed!! {:#x}", ret);
    return ret;
  }

  AX_VENC_MOD_ATTR_T st_enc_mod_attr;
  memset(&st_enc_mod_attr, 0x0, sizeof(AX_VENC_MOD_ATTR_T));
  st_enc_mod_attr.enVencType = AX_VENC_MULTI_ENCODER;
  st_enc_mod_attr.stModThdAttr.u32TotalThreadNum = 1;
  st_enc_mod_attr.stModThdAttr.bExplicitSched = AX_FALSE;
  ret = AX_VENC_Init(&st_enc_mod_attr);
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_VENC_Init Failed!! {:#x}", ret);
    return ret;
  }
  LOG_INFO("SYS INIT SUCCCESS !!!");
  return kOk;
}

void Resource::Release() {
  if (is_released_) {
    return;
  }

int ret = 0;
  if (is_released_) {
    return;
  }

  ret = AX_VENC_Deinit();
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_VENC_Deinit failed! Error Code:{:#X}", ret);
  }

  ret = AX_VDEC_Deinit();
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_VDEC_Deinit failed! Error Code:{:#X}", ret);
  }

  ret = AX_SYS_Deinit();
  if (AX_SUCCESS != ret) {
    LOG_ERROR("AX_SYS_Deinit failed! Error Code:{:#X}", ret);
  }

  

  is_released_ = true;
}

} // namespace pipeline

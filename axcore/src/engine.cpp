#include "engine.hpp"

#include <cstring>
#include <vector>

#include "logger.h"
#include "detection.hpp"
#include "file.hpp"
#include "io.hpp"

Engine::Engine(const std::string& model_config) : config_(model_config) {}

Engine::~Engine() { Destroy(); }

int Engine::Init() {
  
  config_.Init();

  AX_ENGINE_NPU_ATTR_T npu_attr;
  memset(&npu_attr, 0, sizeof(npu_attr));
  npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_STD;
  auto ret = AX_ENGINE_Init(&npu_attr);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_Init failed!!! code:{:#x}", static_cast<uint32_t> (ret));
    return ret;
  }

  LOG_INFO("model file:{}",config_.model_file);
  std::vector<char> model_buffer;
  if (!utilities::ReadFile(config_.model_file, model_buffer)) {
    LOG_ERROR("Read Run-Joint model file failed. file: {}",
                  config_.model_file);
    return -1;
  }
  LOG_INFO("model file:{} size:{}",config_.model_file,model_buffer.size());
  ret = AX_ENGINE_CreateHandle(&handle_, model_buffer.data(),
                               model_buffer.size());
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_CreateHandle failed!!! code:{:#x}", static_cast<uint32_t> (ret));
    // AX_ENGINE_DestroyHandle(handle_);
    return ret;
  }
  LOG_INFO("Engine creating handle is done.");

  ret = AX_ENGINE_CreateContext(handle_);
  if (0 != ret) {
    AX_ENGINE_DestroyHandle(handle_);
    LOG_ERROR("AX_ENGINE_CreateContext failed!!! code:{:#x}", ret);
    return ret;
  }
  LOG_INFO("Engine creating context is done.");

  ret = AX_ENGINE_GetIOInfo(handle_, &io_info_);
  if (0 != ret) {
    AX_ENGINE_DestroyHandle(handle_);
    LOG_ERROR("AX_ENGINE_GetIOInfo failed!!! code:{:#x}", ret);
    return ret;
  }
  LOG_INFO("Engine get io info is done.");

  ret = middleware::PrepareIo(
      io_info_, &io_data_,
      std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
  if (0 != ret) {
    AX_ENGINE_DestroyHandle(handle_);
    LOG_ERROR("middleware::PrepareIo failed!!! code:{:#x}", ret);
    return ret;
  }
  LOG_INFO("Engine alloc io is done.");
  return 0;
}

int Engine::Process(const std::vector<uint8_t>& input_data) {
  // TIME_START(EngineProcess);

  int ret = middleware::PushInput(input_data, &io_data_, io_info_);
  if (0 != ret) {
    LOG_ERROR("push_input data failed!!! code:{:#x}", ret);
    return ret;
  }

  ret = AX_ENGINE_RunSync(handle_, &io_data_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_RunSync failed!!! code:{:#x}", ret);
    return ret;
  }

  // TIME_END(EngineProcess);
  // TIME_USEC_SHOW(EngineProcess);
  return ret;
}

int Engine::Process(const uint8_t* data, size_t size) {
  int ret = middleware::PushInput(data, size, &io_data_, io_info_);
  if (0 != ret) {
    LOG_ERROR("push_input data failed!!! code:{:#x}", ret);
    return ret;
  }

  ret = AX_ENGINE_RunSync(handle_, &io_data_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_RunSync failed!!! code:{:#x}", ret);
    return ret;
  }
  return ret;
}

int Engine::Destroy() {
  if (is_released_) {
    LOG_INFO("Engine already released");
    return 0;
  }
  is_released_ = true;

  middleware::FreeIo(&io_data_);
  AX_S32 ret = AX_ENGINE_DestroyHandle(handle_);

  AX_ENGINE_Deinit();

  return ret;
}
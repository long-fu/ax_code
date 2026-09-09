#include "engine.h"

#include <cstring>
#include <vector>

#include "ax_sys_api.h"
#include "ax_engine_api.h"

#include "logger.h"
#include "file.h"
#include "io.h"

Engine::Engine(const EngineConfig& config) : model_path_(config.ModelFile()) {}

Engine::~Engine() { Destroy(); }

int Engine::Init() {
  // 允许 Init 失败后重试：下面的失败路径会调 Destroy() 置上 is_released_。
  is_released_ = false;

  AX_ENGINE_NPU_ATTR_T npu_attr;
  memset(&npu_attr, 0, sizeof(npu_attr));
  npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_STD;
  // npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
  auto ret = AX_ENGINE_Init(&npu_attr);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_Init failed!!! code:{:#x}", static_cast<uint32_t> (ret));
    return ret;
  }
  engine_inited_ = true;

  // 此后所有失败路径统一走 Destroy() 回收，不再各自手写 DestroyHandle：
  // 原实现漏了 AX_ENGINE_Deinit，Init 失败而对象仍存活时，
  // 全局 NPU runtime 会停在"已 Init 但无有效 handle"的状态。
  // Destroy() 由 is_released_ 保证幂等，析构再调一次无副作用。

  LOG_INFO("model file:{}",model_path_);
  std::vector<char> model_buffer;
  if (!utilities::ReadFile(model_path_, model_buffer)) {
    LOG_ERROR("Read Run-Joint model file failed. file: {}",
                  model_path_);
    Destroy();
    return -1;
  }
  LOG_INFO("model file:{} size:{}",model_path_,model_buffer.size());
  ret = AX_ENGINE_CreateHandle(&handle_, model_buffer.data(),
                               model_buffer.size());
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_CreateHandle failed!!! code:{:#x}", static_cast<uint32_t> (ret));
    handle_ = nullptr;
    handle_valid_ = false;
    Destroy();
    return ret;
  }
  handle_valid_ = true;
  LOG_INFO("Engine creating handle is done.");

  ret = AX_ENGINE_CreateContext(handle_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_CreateContext failed!!! code:{:#x}", ret);
    Destroy();
    return ret;
  }
  LOG_INFO("Engine creating context is done.");

  ret = AX_ENGINE_GetIOInfo(handle_, &io_info_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_GetIOInfo failed!!! code:{:#x}", ret);
    io_info_ = nullptr;
    Destroy();
    return ret;
  }
  LOG_INFO("Engine get io info is done.");

  ret = middleware::PrepareIo(
      io_info_, &io_data_,
      std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
  if (0 != ret) {
    LOG_ERROR("middleware::PrepareIo failed!!! code:{:#x}", ret);
    Destroy();
    return ret;
  }
  LOG_INFO("Engine alloc io is done.");
  return 0;
}

bool Engine::IsReady() const {
  return handle_valid_ && handle_ != nullptr && io_info_ != nullptr &&
         !is_released_;
}

int Engine::Process(const std::vector<uint8_t>& input_data) {
  // TIME_START(EngineProcess);

  // Init 失败（或已 Destroy）时 io_info_ 可能为空，PushInput 里的
  // info_t->nInputSize 就是空指针解引用。
  if (!IsReady()) {
    LOG_ERROR("Engine::Process on uninitialized engine, model:{}", model_path_);
    return -1;
  }

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
  if (!IsReady()) {
    LOG_ERROR("Engine::Process on uninitialized engine, model:{}", model_path_);
    return -1;
  }

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

  // FreeIo 内部会复位 io_data_，故此处对"从未 PrepareIo"和
  // "PrepareIo 失败已回滚"两种状态都安全。
  middleware::FreeIo(&io_data_);
  io_info_ = nullptr;
  AX_S32 ret = 0;
  if (handle_valid_ && handle_ != nullptr) {
    ret = AX_ENGINE_DestroyHandle(handle_);
    handle_ = nullptr;
    handle_valid_ = false;
  }

  if (engine_inited_) {
    AX_ENGINE_Deinit();
    engine_inited_ = false;
  }

  return ret;
}
#pragma once

#include <cstdio>
#include <cstring>
#include <vector>
#include <utility>
#include <stdint.h>
#include <ax_sys_api.h>
#include <ax_engine_api.h>

#define AX_CMM_ALIGN_SIZE 128

typedef enum
{
    AX_ENGINE_ABST_DEFAULT = 0,
    AX_ENGINE_ABST_CACHED = 1,
} AX_ENGINE_ALLOC_BUFFER_STRATEGY_T;

typedef std::pair<AX_ENGINE_ALLOC_BUFFER_STRATEGY_T, AX_ENGINE_ALLOC_BUFFER_STRATEGY_T> INPUT_OUTPUT_ALLOC_STRATEGY;

#define SAMPLE_AX_ENGINE_DEAL_HANDLE            \
    if (0 != ret)                               \
    {                                           \
        return AX_ENGINE_DestroyHandle(handle); \
    }

#define SAMPLE_AX_ENGINE_DEAL_HANDLE_IO         \
    if (0 != ret)                               \
    {                                           \
        middleware::FreeIo(&io_data);          \
        return AX_ENGINE_DestroyHandle(handle); \
    }

namespace middleware
{
    void FreeIoIndex(AX_ENGINE_IO_BUFFER_T *io_buf, size_t index);

    // 释放 CMM 并把 io 复位为空状态，可安全重复调用。
    void FreeIo(AX_ENGINE_IO_T *io);

    // 只 delete[] 数组并清零 io，不触碰 CMM。供 PrepareIo 的回滚路径使用
    // （CMM 此时已由 FreeIoIndex 释放，再走 FreeIo 会二次释放）。
    void ResetIo(AX_ENGINE_IO_T *io);

    int PrepareIo(AX_ENGINE_IO_INFO_T *info, AX_ENGINE_IO_T *io_data, INPUT_OUTPUT_ALLOC_STRATEGY strategy);

    int PushInput(const std::vector<uint8_t> &data, AX_ENGINE_IO_T *io_t, AX_ENGINE_IO_INFO_T *info_t);
    int PushInput(const uint8_t *data, size_t data_size, AX_ENGINE_IO_T *io_t, AX_ENGINE_IO_INFO_T *info_t);
} // namespace middleware

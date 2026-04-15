#ifndef __IO__
#define __IO__

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
        middleware::free_io(&io_data);          \
        return AX_ENGINE_DestroyHandle(handle); \
    }

namespace middleware
{
    void free_io_index(AX_ENGINE_IO_BUFFER_T *io_buf, size_t index);

    void free_io(AX_ENGINE_IO_T *io);

    int prepare_io(AX_ENGINE_IO_INFO_T *info, AX_ENGINE_IO_T *io_data, INPUT_OUTPUT_ALLOC_STRATEGY strategy);

    int push_input(const std::vector<uint8_t> &data, AX_ENGINE_IO_T *io_t, AX_ENGINE_IO_INFO_T *info_t);
    int push_input(const uint8_t *data, size_t data_size, AX_ENGINE_IO_T *io_t, AX_ENGINE_IO_INFO_T *info_t);
} // namespace middleware

#endif

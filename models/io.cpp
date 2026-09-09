
#include "io.h"
#include "logger.h"


const char* AX_CMM_SESSION_NAME = "npu";


namespace middleware
{

    void FreeIoIndex(AX_ENGINE_IO_BUFFER_T* io_buf, size_t index)
    {
        for (size_t i = 0; i < index; ++i)
        {
            AX_ENGINE_IO_BUFFER_T* pBuf = io_buf + i;
            AX_SYS_MemFree(pBuf->phyAddr, pBuf->pVirAddr);
        }
    }

    // PrepareIo 失败后把 io_data 复位成"干净的空状态"。
    // CMM 已由 FreeIoIndex 释放，但数组与 nInputSize/nOutputSize 仍然有效，
    // 若不复位，调用方后续走 Destroy() -> FreeIo() 会对同一批缓冲二次
    // AX_SYS_MemFree（双重释放）。复位后 FreeIo 可安全地重复调用。
    void ResetIo(AX_ENGINE_IO_T* io)
    {
        delete[] io->pInputs;
        delete[] io->pOutputs;
        memset(io, 0, sizeof(*io));
    }

    void FreeIo(AX_ENGINE_IO_T* io)
    {
        for (size_t j = 0; j < io->nInputSize; ++j)
        {
            AX_ENGINE_IO_BUFFER_T* pBuf = io->pInputs + j;
            AX_SYS_MemFree(pBuf->phyAddr, pBuf->pVirAddr);
        }
        for (size_t j = 0; j < io->nOutputSize; ++j)
        {
            AX_ENGINE_IO_BUFFER_T* pBuf = io->pOutputs + j;
            AX_SYS_MemFree(pBuf->phyAddr, pBuf->pVirAddr);
        }

        // 复位后本函数可安全重复调用（Engine::Init 的失败路径会走 Destroy()）
        ResetIo(io);
    }

    int PrepareIo(AX_ENGINE_IO_INFO_T* info, AX_ENGINE_IO_T* io_data, INPUT_OUTPUT_ALLOC_STRATEGY strategy)
    {
        memset(io_data, 0, sizeof(*io_data));
        // 用 new T[n]() 零初始化：失败回滚路径会遍历整个数组，若残留 new[] 的
        // 不定值，FreeIo 会拿垃圾指针去 AX_SYS_MemFree。
        io_data->pInputs = new AX_ENGINE_IO_BUFFER_T[info->nInputSize]();
        io_data->nInputSize = info->nInputSize;
        // printf("info->nInputSize: %d\n", info->nInputSize);
        // INPUT
        auto ret = 0;
        for (AX_U32 i = 0; i < info->nInputSize; ++i)
        {
            auto meta = info->pInputs[i];
            // printf("meta->nInputSize: %d\n", meta.nSize);
            auto buffer = &io_data->pInputs[i];
            buffer->nSize = meta.nSize;
            if (strategy.first == AX_ENGINE_ABST_CACHED)
            {
                ret = AX_SYS_MemAllocCached((AX_U64*)(&buffer->phyAddr), &buffer->pVirAddr, meta.nSize, AX_CMM_ALIGN_SIZE, (const AX_S8*)(AX_CMM_SESSION_NAME));
            }
            else
            {
                ret = AX_SYS_MemAlloc((AX_U64*)(&buffer->phyAddr), &buffer->pVirAddr, meta.nSize, AX_CMM_ALIGN_SIZE, (const AX_S8*)(AX_CMM_SESSION_NAME));
            }

            if (ret != 0)
            {
                LOG_ERROR("Allocate input{} {{ phy: {}, vir: {}, size: {} Bytes }}. fail", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
                FreeIoIndex(io_data->pInputs, i);
                ResetIo(io_data);
                return ret;
            }
            LOG_INFO("Allocate input {} [ phy: {}, vir: {}, size: {} Bytes ]. ", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
        }

        //OUTPUT
        io_data->pOutputs = new AX_ENGINE_IO_BUFFER_T[info->nOutputSize]();
        io_data->nOutputSize = info->nOutputSize;
        for (AX_U32 i = 0; i < info->nOutputSize; ++i)
        {
            auto meta = info->pOutputs[i];
            auto buffer = &io_data->pOutputs[i];
            buffer->nSize = meta.nSize;
            if (strategy.second == AX_ENGINE_ABST_CACHED)
            {
                ret = AX_SYS_MemAllocCached((AX_U64*)(&buffer->phyAddr), &buffer->pVirAddr, meta.nSize, AX_CMM_ALIGN_SIZE, (const AX_S8*)(AX_CMM_SESSION_NAME));
            }
            else
            {
                ret = AX_SYS_MemAlloc((AX_U64*)(&buffer->phyAddr), &buffer->pVirAddr, meta.nSize, AX_CMM_ALIGN_SIZE, (const AX_S8*)(AX_CMM_SESSION_NAME));
            }
            if (ret != 0)
            {
                LOG_ERROR("Allocate output{} {{ phy: {}, vir: {}, size: {} Bytes }}. fail", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
                FreeIoIndex(io_data->pInputs, io_data->nInputSize);
                FreeIoIndex(io_data->pOutputs, i);
                ResetIo(io_data);
                return ret;
            }
            LOG_INFO("Allocate output {} [ phy: {}, vir: {}, size: {} Bytes ].", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
        }

        return 0;
    }

    int PushInput(const std::vector<uint8_t>& data, AX_ENGINE_IO_T* io_t, AX_ENGINE_IO_INFO_T* info_t)
    {
        if (info_t->nInputSize != 1)
        {
            LOG_ERROR("Only support Input size == 1 current now");
            return -1;
        }

        if (data.size() != info_t->pInputs[0].nSize)
        {
            LOG_ERROR("The input data size is not matched with tensor {{name: {}, size: {}}}.", info_t->pInputs[0].pName, info_t->pInputs[0].nSize);
            return -1;
        }

        memcpy(io_t->pInputs[0].pVirAddr, data.data(), data.size());
        // AX_SYS_MemAllocCached
        AX_SYS_MflushCache(io_t->pInputs[0].phyAddr, (void*)data.data(), data.size());
        return 0;
    }

    int PushInput(const uint8_t* data, size_t data_size, AX_ENGINE_IO_T* io_t, AX_ENGINE_IO_INFO_T* info_t)
    {
        if (info_t->nInputSize != 1)
        {
            LOG_ERROR("Only support Input size == 1 current now");
            return -1;
        }

        if (data_size != info_t->pInputs[0].nSize)
        {
            LOG_ERROR("The input data size is not matched with tensor {{name: {}, size: {}}}.", info_t->pInputs[0].pName, info_t->pInputs[0].nSize);
            return -1;
        }

        memcpy(io_t->pInputs[0].pVirAddr, data, data_size);
        // AX_SYS_MemAllocCached
        AX_SYS_MflushCache(io_t->pInputs[0].phyAddr, (void*)data, data_size);
        return 0;
    }

} // namespace middleware

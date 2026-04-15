
#include "io.hpp"


const char* AX_CMM_SESSION_NAME = "npu";


namespace middleware
{

    void free_io_index(AX_ENGINE_IO_BUFFER_T* io_buf, size_t index)
    {
        for (size_t i = 0; i < index; ++i)
        {
            AX_ENGINE_IO_BUFFER_T* pBuf = io_buf + i;
            AX_SYS_MemFree(pBuf->phyAddr, pBuf->pVirAddr);
        }
    }

    void free_io(AX_ENGINE_IO_T* io)
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
        
        if(io->pInputs){
            delete[] io->pInputs;
        }
        
        if(io->pOutputs) {
            delete[] io->pOutputs;
        }
    }

    int prepare_io(AX_ENGINE_IO_INFO_T* info, AX_ENGINE_IO_T* io_data, INPUT_OUTPUT_ALLOC_STRATEGY strategy)
    {
        memset(io_data, 0, sizeof(*io_data));
        io_data->pInputs = new AX_ENGINE_IO_BUFFER_T[info->nInputSize];
        io_data->nInputSize = info->nInputSize;
        // printf("info->nInputSize: %d\n", info->nInputSize);
        // INPUT
        auto ret = 0;
        for (AX_U32 i = 0; i < info->nInputSize; ++i)
        {
            auto meta = info->pInputs[i];
            auto buffer = &io_data->pInputs[i];
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
                free_io_index(io_data->pInputs, i);
                fprintf(stderr, "Allocate input{%d} { phy: %p, vir: %p, size: %lu Bytes }. fail \n", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
                return ret;
            }
            // fprintf(stderr, "Allocate input{%d} { phy: %p, vir: %p, size: %lu Bytes }. \n", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
        }

        //OUTPUT
        io_data->pOutputs = new AX_ENGINE_IO_BUFFER_T[info->nOutputSize];
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
                fprintf(stderr, "Allocate output{%d} { phy: %p, vir: %p, size: %lu Bytes }. fail \n", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
                free_io_index(io_data->pInputs, io_data->nInputSize);
                free_io_index(io_data->pOutputs, i);
                return ret;
            }
            // fprintf(stderr, "Allocate output{%d} { phy: %p, vir: %p, size: %lu Bytes }.\n", i, (void*)buffer->phyAddr, buffer->pVirAddr, (long)meta.nSize);
        }

        return 0;
    }

    int push_input(const std::vector<uint8_t>& data, AX_ENGINE_IO_T* io_t, AX_ENGINE_IO_INFO_T* info_t)
    {
        if (info_t->nInputSize != 1)
        {
            fprintf(stderr, "Only support Input size == 1 current now");
            return -1;
        }

        if (data.size() != info_t->pInputs[0].nSize)
        {
            fprintf(stderr, "The input data size is not matched with tensor {name: %s, size: %d}.\n", info_t->pInputs[0].pName, info_t->pInputs[0].nSize);
            return -1;
        }

        memcpy(io_t->pInputs[0].pVirAddr, data.data(), data.size());
        // AX_SYS_MemAllocCached
        AX_SYS_MflushCache(io_t->pInputs[0].phyAddr, (void*)data.data(), data.size());
        return 0;
    }

    int push_input(const uint8_t* data, size_t data_size, AX_ENGINE_IO_T* io_t, AX_ENGINE_IO_INFO_T* info_t)
    {
        if (info_t->nInputSize != 1)
        {
            fprintf(stderr, "Only support Input size == 1 current now");
            return -1;
        }

        if (data_size != info_t->pInputs[0].nSize)
        {
            fprintf(stderr, "The input data size is not matched with tensor {name: %s, size: %d}.\n", info_t->pInputs[0].pName, info_t->pInputs[0].nSize);
            return -1;
        }

        memcpy(io_t->pInputs[0].pVirAddr, data, data_size);
        // AX_SYS_MemAllocCached
        AX_SYS_MflushCache(io_t->pInputs[0].phyAddr, (void*)data, data_size);
        return 0;
    }

} // namespace middleware

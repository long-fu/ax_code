
#ifndef RESOURCE_H
#define RESOURCE_H
#pragma once

#include <unistd.h>
#include <string>
#include "Logger.h"


class PipelineResource {
public:
    PipelineResource();

    PipelineResource(int32_t channel);
    ~PipelineResource();
    
    int Init();
    
    void Release();

    int32_t GetChannelId() {
        return m_iChannelId;
    }

private:
    bool m_isReleased;
    int32_t m_iChannelId;
};

#endif
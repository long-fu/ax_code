
#ifndef THREAD_H
#define THREAD_H
#pragma once
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include "ThreadSafeQueue.h"
#include "Logger.h"


#define INVALID_INSTANCE_ID (-1)


class PipelineThread {
public:
    PipelineThread();
    virtual ~PipelineThread() {};
    virtual int Init()
    {
        return 0;
    };
    virtual int Process(int msgId, std::shared_ptr<void> msgData) = 0;
    int SelfInstanceId()
    {
        return m_iInstanceId;
    }
    std::string& SelfInstanceName()
    {
        return m_sInstanceName;
    }

    int BaseConfig(int instanceId, const std::string& threadName);
private:

    int m_iInstanceId;
    std::string m_sInstanceName;
    bool m_isBaseConfiged;
    bool m_isExit;
};

struct PipelineThreadParam {
    PipelineThread* threadInst = nullptr;
    std::string threadInstName = "";
    int threadInstId = INVALID_INSTANCE_ID;
    uint32_t queueSize = 256;
};
#endif
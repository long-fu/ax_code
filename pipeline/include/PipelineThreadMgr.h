
#ifndef THREADMGR_H
#define THREADMGR_H
#pragma once
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include "ThreadSafeQueue.h"
#include "PipelineThread.h"
#include "Logger.h"


enum PipelineThreadStatus {
    THREAD_READY = 0,
    THREAD_RUNNING = 1,
    THREAD_EXITING = 2,
    THREAD_EXITED = 3,
    THREAD_ERROR = 4,
};

struct PipelineMessage {
    int dest;
    int msgId;
    std::shared_ptr<void> data = nullptr;
};


class PipelineThreadMgr {
public:
    PipelineThreadMgr(PipelineThread* userThreadInstance,
                     const std::string& threadName, const uint32_t msgQueueSize);
    ~PipelineThreadMgr();
    // Thread function
    static void ThreadEntry(void* data);
    PipelineThread* GetUserInstance()
    {
        return this->m_pUserInstance;
    }
    const std::string& GetThreadName()
    {
        return m_sName;
    }
    
    int PushMsgToQueue(std::shared_ptr<PipelineMessage>& pMessage);
    
    std::shared_ptr<PipelineMessage> PopMsgFromQueue()
    {
        return this->m_qMsgQueue.Pop();
    }
    void CreateThread();
    void SetStatus(PipelineThreadStatus status)
    {
        m_eStatus = status;
    }
    PipelineThreadStatus GetStatus()
    {
        return m_eStatus;
    }
    int WaitThreadInitEnd();
 
public:
    bool m_isExit;
    PipelineThreadStatus m_eStatus;
    PipelineThread* m_pUserInstance;
    std::string m_sName;
    ThreadSafeQueue<std::shared_ptr<PipelineMessage>> m_qMsgQueue;
};
#endif
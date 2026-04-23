
#ifndef APP_H
#define APP_H
#pragma once

#include <iostream>
#include <memory>
#include "PipelineThreadMgr.h"
#include "Logger.h"

namespace {
    int g_MainThreadId = 0;
}

typedef int (*AclLiteMsgProcess)(uint32_t msgId, std::shared_ptr<void> msgData, void* userData);

class Pipeline {
public:
    /**
    * @brief Constructor
    */
    Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    /**
    * @brief Destructor
    */
    ~Pipeline();

    /**
     * @brief Get the single instance of Pipeline
     * @return Instance of Pipeline
     */
    static Pipeline& GetInstance()
    {
        static Pipeline instance;
        return instance;
    }



    int Start(std::vector<PipelineThreadParam>& threadParamTbl);
    void Wait();
    void Wait(AclLiteMsgProcess msgProcess, void* param);
    int GetPipelineThreadIdByName(const std::string& threadName);
    int SendMessage(int dest, int msgId, std::shared_ptr<void> data);
    void WaitEnd()
    {
        m_isWaitEnd = true;
    }
    void Exit();

private:
    int Init();
    /**
     * @brief Create one app thread
     * @return Result of create thread
     */
    int CreatePipelineThread(PipelineThread* thInst, const std::string& instName,const uint32_t msgQueueSize);    
    int CreatePipelineThreadMgr(PipelineThread* thInst, const std::string& instName, const uint32_t msgQueueSize);
    bool CheckThreadAbnormal();
    bool CheckThreadNameUnique(const std::string& threadName);
    void ReleaseThreads();

private:
    bool m_isReleased;
    bool m_isWaitEnd;
    std::vector<PipelineThreadMgr*> m_vThreadList;
};

Pipeline& CreatePipelineInstance();
Pipeline& GetPipelineInstance();
int SendMessage(int dest, int msgId, std::shared_ptr<void> data);
int GetPipelineThreadIdByName(const std::string& threadName);
#endif

#include "Pipeline.h"
#include "PipelineThreadMgr.h"
#include "Logger.h"


using namespace std;
namespace {
const uint32_t kWaitInterval = 10000;
const uint32_t kThreadExitRetry = 3;
}

Pipeline::Pipeline():m_isReleased(false), m_isWaitEnd(false)
{
    Init();
}

Pipeline::~Pipeline()
{
    LOG_INFO("调用释放 ~~Pipeline");
    ReleaseThreads();
}

int Pipeline::Init()
{
    const uint32_t msgQueueSize = 256;
    PipelineThreadMgr* thMgr = new PipelineThreadMgr(nullptr, "main", msgQueueSize);
    m_vThreadList.push_back(thMgr);
    thMgr->SetStatus(THREAD_RUNNING);
    return 0;
}

int Pipeline::CreatePipelineThread(PipelineThread* thInst, const string& instName, const uint32_t msgQueueSize)
{
    int instId = CreatePipelineThreadMgr(thInst, instName, msgQueueSize);
    if (instId == INVALID_INSTANCE_ID) {
        LOG_ERROR("Add thread instance {} failed", instName);
        return INVALID_INSTANCE_ID;
    }

    m_vThreadList[instId]->CreateThread();
    int ret = m_vThreadList[instId]->WaitThreadInitEnd();
    if (ret != 0) {
        LOG_ERROR("Create thread failed, error {}", ret);
        return INVALID_INSTANCE_ID;
    }

    return instId;
}

int Pipeline::CreatePipelineThreadMgr(PipelineThread* thInst, const string& instName,const uint32_t msgQueueSize)
{
    if (!CheckThreadNameUnique(instName)) {
        LOG_ERROR("The thread instance name is not unique");
        return INVALID_INSTANCE_ID;
    }

    int instId = m_vThreadList.size();
    int ret = thInst->BaseConfig(instId, instName);
    if (ret != 0) {
        LOG_ERROR("Create thread instance failed for error {}", ret);
        return INVALID_INSTANCE_ID;
    }

    PipelineThreadMgr* thMgr = new PipelineThreadMgr(thInst, instName, msgQueueSize);
    m_vThreadList.push_back(thMgr);

    return instId;
}

bool Pipeline::CheckThreadNameUnique(const string& threadName)
{
    if (threadName.size() == 0) {
        return true;
    }

    for (size_t i = 0; i < m_vThreadList.size(); i++) {
        if (threadName == m_vThreadList[i]->GetThreadName()) {
            return false;
        }
    }

    return true;
}

int Pipeline::Start(vector<PipelineThreadParam>& threadParamTbl)
{
    for (size_t i = 0; i < threadParamTbl.size(); i++) {
        int instId = CreatePipelineThreadMgr(threadParamTbl[i].threadInst,
                                            threadParamTbl[i].threadInstName,
                                            threadParamTbl[i].queueSize);
        if (instId == INVALID_INSTANCE_ID) {
            LOG_ERROR("Create thread instance failed");
            return -1;
        }
        threadParamTbl[i].threadInstId = instId;
    }
    // Note:The instance id must generate first, then create thread,
    // for the user thread get other thread instance id in Init function
    for (size_t i = 0; i < threadParamTbl.size(); i++) {
        m_vThreadList[threadParamTbl[i].threadInstId]->CreateThread();
    }

    for (size_t i = 0; i < threadParamTbl.size(); i++) {
        int instId = threadParamTbl[i].threadInstId;
        int ret = m_vThreadList[instId]->WaitThreadInitEnd();
        if (ret != 0) {
            LOG_ERROR("Create thread {} failed, error {}",
                              threadParamTbl[i].threadInstName, ret);
            return ret;
        }
    }
    return 0;
}

int Pipeline::GetPipelineThreadIdByName(const string& threadName)
{
    if (threadName.empty()) {
        LOG_ERROR("search name is empty");
        return INVALID_INSTANCE_ID;
    }

    for (uint32_t i = 0; i < m_vThreadList.size(); i++) {
        if (m_vThreadList[i]->GetThreadName() == threadName) {
            return i;
        }
    }
    
    return INVALID_INSTANCE_ID;
}

int Pipeline::SendMessage(int dest, int msgId, shared_ptr<void> data)
{
    if ((uint32_t)dest > m_vThreadList.size()) {
        LOG_ERROR("Send message to {} failed for thread not exist", dest);
        return -1;
    }

    shared_ptr<PipelineMessage> pMessage = make_shared<PipelineMessage>();
    pMessage->dest = dest;
    pMessage->msgId = msgId;
    pMessage->data = data;
    // TODO： 消息队列 溢出错误 没有启动
    return m_vThreadList[dest]->PushMsgToQueue(pMessage);
}

void Pipeline::Wait()
{
    while (true) {
        usleep(kWaitInterval);
        if (m_isWaitEnd) break;
    }
    m_vThreadList[g_MainThreadId]->SetStatus(THREAD_EXITED);
}

bool Pipeline::CheckThreadAbnormal()
{
    for (size_t i = 0; i < m_vThreadList.size(); i++) {
        if (m_vThreadList[i]->GetStatus() == THREAD_ERROR) {
            return true;
        }
    }

    return false;
}

void Pipeline::Wait(AclLiteMsgProcess msgProcess, void* param)
{
    PipelineThreadMgr* mainMgr = m_vThreadList[0];

    if (mainMgr == nullptr) {
        LOG_ERROR("AclLite app wait exit for message process function is nullptr");
        return;
    }

    while (true) {
        if (m_isWaitEnd) break;

        shared_ptr<PipelineMessage> msg = mainMgr->PopMsgFromQueue();
        if (msg == nullptr) {
            usleep(kWaitInterval);
            continue;
        }
        int ret = msgProcess(msg->msgId, msg->data, param);
        if (ret) {
            LOG_ERROR("AclLite app exit for message {} process error:{}", msg->msgId, ret);
            break;
        }
    }
    m_vThreadList[g_MainThreadId]->SetStatus(THREAD_EXITED);
}

void Pipeline::Exit()
{
    // LOG_INFO("调用 Exit");
    ReleaseThreads();
}

void Pipeline::ReleaseThreads()
{
    if (m_isReleased) return;
    m_vThreadList[g_MainThreadId]->SetStatus(THREAD_EXITED);

    for (uint32_t i = 1; i < m_vThreadList.size(); i++) {
        if ((m_vThreadList[i] != nullptr) &&
            (m_vThreadList[i]->GetStatus() == THREAD_RUNNING))
             m_vThreadList[i]->SetStatus(THREAD_EXITING);
    }

    int retry = kThreadExitRetry;
    while (retry >= 0) {
        bool exitFinish = true;
        for (uint32_t i = 0; i < m_vThreadList.size(); i++) {
            if (m_vThreadList[i] == nullptr)
                continue;
            if (m_vThreadList[i]->GetStatus() > THREAD_EXITING) {
                delete m_vThreadList[i];
                m_vThreadList[i] = nullptr;
                LOG_INFO("AclLite thread {} released", i);
            } else {
                m_vThreadList[i]->SetStatus(THREAD_EXITING);
                exitFinish = false;
            }
        }

        if (exitFinish)
            break;

        sleep(1);
        retry--;
    }
    m_isReleased = true;
}

Pipeline& CreatePipelineInstance()
{
    return Pipeline::GetInstance();
}

Pipeline& GetPipelineInstance()
{
    return Pipeline::GetInstance();
}

int SendMessage(int dest, int msgId, shared_ptr<void> data)
{
    Pipeline& app = Pipeline::GetInstance();
    return app.SendMessage(dest, msgId, data);
}

int GetPipelineThreadIdByName(const string& threadName)
{
    Pipeline& app = Pipeline::GetInstance();
    return app.GetPipelineThreadIdByName(threadName);
}


#include "PipelineThreadMgr.h"
#include "Logger.h"

using namespace std;
namespace {
    const uint32_t kWait10Milliseconds = 10000;
    const uint32_t kWaitThreadStart = 1000;
}

PipelineThreadMgr::PipelineThreadMgr(PipelineThread* userThreadInstance,
    const string& threadName, const uint32_t msgQueueSize):m_isExit(false),
    m_eStatus(THREAD_READY), m_pUserInstance(userThreadInstance),
    m_sName(threadName), m_qMsgQueue(msgQueueSize)
{
}

PipelineThreadMgr::~PipelineThreadMgr()
{
    m_pUserInstance = nullptr;
    while (!m_qMsgQueue.Empty()) {
        m_qMsgQueue.Pop();
        // TODO: 需要主动释放消息队列中的 数据 
    }
}

void PipelineThreadMgr::CreateThread()
{
    thread engine(&PipelineThreadMgr::ThreadEntry, (void *)this);
    engine.detach();
}

void PipelineThreadMgr::ThreadEntry(void* arg)
{
    PipelineThreadMgr* thMgr = (PipelineThreadMgr*)arg;
    PipelineThread* userInstance = thMgr->GetUserInstance();
    if (userInstance == nullptr) {
        LOG_ERROR("Pipeline thread exit for user thread instance is null");
        return;
    }

    string& instName = userInstance->SelfInstanceName();

    int ret = userInstance->Init();
    if (ret) {
        LOG_ERROR("Thread {} init error {}, thread exit",
                          instName, ret);
        thMgr->SetStatus(THREAD_ERROR);
        return;
    }

    thMgr->SetStatus(THREAD_RUNNING);
    while (THREAD_RUNNING == thMgr->GetStatus()) {
        // get data from queue
        shared_ptr<PipelineMessage> msg = thMgr->PopMsgFromQueue();
        if (msg == nullptr) {
            usleep(kWait10Milliseconds);
            continue;
        }
        // call function to process thread msg
        ret = userInstance->Process(msg->msgId, msg->data);
        msg->data = nullptr;
        if (ret) {
            // LOG_ERROR("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
            LOG_ERROR("Thread {} process function return "
                              "error {}, thread exit", instName, ret);
            thMgr->SetStatus(THREAD_ERROR);
            
            return;
        }
        usleep(1);
    }
    // if(thMgr->GetThreadName() == "pushstream_0") {
    //         LOG_ERROR("编码线程数据为空 THREAD_EXITED");
    //         // usleep(kWait10Milliseconds);
                        
    //     }

    thMgr->SetStatus(THREAD_EXITED);

    return;
}

int PipelineThreadMgr::WaitThreadInitEnd()
{
    while (true) {
        if (m_eStatus == THREAD_RUNNING) {
            break;
        } else if (m_eStatus > THREAD_RUNNING) {
            string& instName = m_pUserInstance->SelfInstanceName();
            LOG_ERROR("Thread instance {} status change to {}, "
                              "app start failed", instName, m_eStatus);
            return -1;
        } else {
            usleep(kWaitThreadStart);
        }
    }

    return 0;
}

int PipelineThreadMgr::PushMsgToQueue(shared_ptr<PipelineMessage>& pMessage)
{
    if (m_eStatus != THREAD_RUNNING) {
        LOG_ERROR("Thread instance {} status({}) is invalid, "
                          "can not reveive message", m_sName, m_eStatus);
        return -1;
    }
    // TODO: 这里判断消息推列已满
    return m_qMsgQueue.Push(pMessage) ? 0 : -1;
}

#include "PipelineThread.h"
#include "Logger.h"
using namespace std;
PipelineThread::PipelineThread():
    m_iInstanceId(INVALID_INSTANCE_ID), m_sInstanceName(""),
    m_isBaseConfiged(false)
{
}

int PipelineThread::BaseConfig(int instanceId, const string& threadName
                                       )
{
    if (m_isBaseConfiged) {
        return -1;
    }

    m_iInstanceId = instanceId;
    m_sInstanceName.assign(threadName.c_str());


    m_isBaseConfiged = true;

    return 0;
}


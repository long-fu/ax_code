#pragma once

#include "sort_track.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "Pipeline.h"

class BusProcess : public PipelineThread
{
private:
    /* data */
    SORT_TRACKER m_tracker;
    uint64_t m_nFrameID = 0;
    int m_nNextThreadId = -1;

public:
    BusProcess()
    {
    }
    virtual int Init() override
    {
        m_nNextThreadId = GetPipelineThreadIdByName("VencThread");
        return 0;
    };

    virtual int Process(int msgId, std::shared_ptr<void> msgData) override
    {
        int ret = 0;
        std::shared_ptr<InfData> inData;
        std::shared_ptr<BusData> outData;
        switch (msgId)
        {
        case MSG_APP_START:

            break;
        case MSG_INFPROC_DATA:

            inData = std::static_pointer_cast<InfData>(msgData);
            {
                vector<TrackingBox> detFrameData;
                for (size_t i = 0; i < inData->objects.size(); i++)
                {
                    auto item = inData->objects[i];
                    TrackingBox cur_box;
                    cur_box.box = item.rect;
                    cur_box.frame_id = m_nFrameID;
                    detFrameData.push_back(cur_box);
                }
                m_nFrameID++;
                m_tracker.update(detFrameData);
                vector<TrackingBox> tracking_results = m_tracker.getReport();
            }

            outData = std::make_shared<BusData>();

            outData->image = inData->image;

            ret = SendMessage(m_nNextThreadId, MSG_BUSPROC_DATA, outData);

            break;
        case MSG_APP_EXIT:

            break;
        default:
            break;
        }

        return ret;
    };
    ~BusProcess() {};
};

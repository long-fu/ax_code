#pragma once
#include <iostream>
#include <string>
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "Yolov5.hpp"
#include "FFmpegDecoder.hpp"
#include <Pipeline.h>
class InfProccess : public PipelineThread
{
private:
    /* data */
    Yolov5 m_yolov5;
    FFmpegDecoder *m_pFFDecoder = nullptr;
    int m_nNextThreadId = -1;

public:
    InfProccess(std::string modelConfig, FFmpegDecoder *ffDecoder) : m_yolov5(modelConfig), m_pFFDecoder(ffDecoder) {

                                                                     };

    virtual int Init() override
    {
        m_nNextThreadId = GetPipelineThreadIdByName("BusThread");

        return m_yolov5.Init();
    };

    virtual int Process(int msgId, std::shared_ptr<void> msgData) override
    {
        int ret = 0;
        std::shared_ptr<PreData> inData;
        std::shared_ptr<InfData> outData;
        switch (msgId)
        {
        case MSG_APP_START:

            break;
        case MSG_PREPROC_DATA:

            inData = std::static_pointer_cast<PreData>(msgData);
            m_yolov5.Process(inData->data);
            inData->data.clear();

            outData = std::make_shared<InfData>();

            outData->image = inData->image;

            m_yolov5.Postprocess(m_pFFDecoder->GetFrameWidth(), m_pFFDecoder->GetFrameHeight(), outData->objects);

            ret = SendMessage(m_nNextThreadId, MSG_INFPROC_DATA, outData);

            break;
        case MSG_APP_EXIT:

            break;
        default:
            break;
        }

        return ret;
    };

    ~InfProccess() {

    };
};

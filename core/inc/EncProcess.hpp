#pragma once
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "Pipeline.h"
#include "FFmpegEncoder.hpp"
#include "VencHelper.hpp"
#include "FFmpegDecoder.hpp"

class EncProcess : public PipelineThread
{
private:
    FFmpegEncoder *m_pFFEncoder = nullptr;
    VencHelper *m_pVenc = nullptr;

public:
    EncProcess(std::string rtmp,FFmpegDecoder *ffDecoder)
    {
        m_pVenc = new VencHelper(0, ffDecoder->GetFrameWidth(), ffDecoder->GetFrameHeight(), 25, 25);
        m_pFFEncoder = new FFmpegEncoder(rtmp, 25, ffDecoder->GetFrameWidth(), ffDecoder->GetFrameHeight(), AV_PIX_FMT_NV12, 25, "main");
    };

    static int VencProcessCallBackFunc(AX_VENC_STREAM_T streamData,
                                int chn,
                                void *user_data)
    {

        EncProcess *self = (EncProcess *)user_data;
        // 这个时间多久
        TIME_START(WritePacket);
        self->m_pFFEncoder->WritePacket(streamData.stPack.pu8Addr, streamData.stPack.u32Len);
        TIME_END(WritePacket);

        TIME_USEC_SHOW(WritePacket);
        return 0;
    }

    virtual int Init() override
    {

        if (0 != m_pFFEncoder->Init())
        {
            LOG_ERROR_LOC("FFmpeg Encoder Init failled!");
            return -1;
        }
        if (0 != m_pVenc->Init())
        {
            LOG_ERROR_LOC("VENC Init failed!");
            return -2;
        };
        
        return 0;
    };

    int Start()
    {
        int ret = m_pVenc->Encode(VencProcessCallBackFunc, this);
        return ret;
    }

    virtual int Process(int msgId, std::shared_ptr<void> msgData) override
    {
        int ret;
        std::shared_ptr<BusData> inData;
        switch (msgId)
        {
        case MSG_APP_START:
            ret = Start();
            break;
        case MSG_BUSPROC_DATA:
            ret = m_pVenc->Write(&inData->image,nullptr);
            break;
        case MSG_APP_EXIT:
            // 发送结束消息
            // m_pFFEncoder->s;
            m_pVenc->StopEncode();
            break;
        default:
            break;
        }

        return ret;
    };
    ~EncProcess()
    {
        delete m_pFFEncoder;
        delete m_pVenc;
    };
};
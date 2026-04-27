#pragma once

#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "FFmpegDecoder.hpp"
#include "VdecHelper.hpp"
#include "IvpsHelper.hpp"
#include "Pipeline.h"

class PreProcess : public PipelineThread
{
private:
    FFmpegDecoder *m_pFFDecoder = nullptr;
    VdecHelper *m_pVdec = nullptr;
    IvpsHelper *m_pIvps = nullptr;
    pthread_t m_tFFmpegThread = -1;
    int m_nNextThreadId = -1;

public:
    PreProcess(FFmpegDecoder *const ffDecoder) : m_pFFDecoder(ffDecoder)
    {
        m_pVdec = new VdecHelper(0, PT_H264, ffDecoder->GetFrameWidth(), ffDecoder->GetFrameHeight(), ffDecoder->GetFps());
        m_pIvps = new IvpsHelper(0, ffDecoder->GetFrameWidth() * ffDecoder->GetFrameHeight() * 3, 32);
    };

    virtual int Init() override
    {
        if (0 != m_pVdec->Init())
        {
            LOG_ERROR_LOC("VDEC Init failled!");
            return -1;
        };

        if (0 != m_pIvps->Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640))
        {
            LOG_ERROR_LOC("IVPS Init failed!");
            return -2;
        }

        m_nNextThreadId = GetPipelineThreadIdByName("InferThread");
        return 0;
    };

    static int FrameProcessCallBackFunc(void *user_data, void *frame_data,
                                        int frame_size)
    {
        PreProcess *self = (PreProcess *)user_data;
        self->m_pVdec->Write(frame_data, frame_size, nullptr);
        return 0;
    }

    static int VdecProcessCallBackFunc(ImageData image,
                                       int grp, int chn,
                                       void *user_data)
    {

        std::shared_ptr<ImageData> data = std::make_shared<ImageData>(image);
        data->timePoint = std::chrono::steady_clock::now();

        PreProcess *self = (PreProcess *)user_data;

        int ret = SendMessage(self->SelfInstanceId(), MSG_VDEC_DATA, data);

        return 0;
    }

    // 拉流线程
    static void *FFmpegDecodeCallBackFunc(void *argv)
    {
        pthread_setname_np(pthread_self(), "FFDec");
        PreProcess *self = (PreProcess *)argv;
        self->m_pFFDecoder->Decode(FrameProcessCallBackFunc, argv);
        return nullptr;
    }
    int Start()
    {
        int ret = m_pVdec->Decode(VdecProcessCallBackFunc, this);

        pthread_create(&m_tFFmpegThread, nullptr, FFmpegDecodeCallBackFunc, this);

        return ret;
    }

    int Proprocess(std::shared_ptr<ImageData> imgData)
    {
        ImageData dest;
        ImageData src = *imgData.get();

        if (m_pIvps->Process(dest, src) != 0)
        {
            LOG_ERROR_LOC("CSC 异常");
            exit(-1);
        }

        std::shared_ptr<PreData> data = std::make_shared<PreData>();
        data->image = src;
        Copy2Host(data->data, dest);

        int ret = SendMessage(m_nNextThreadId, MSG_PREPROC_DATA, data);

        return 0;
    }

    virtual int Process(int msgId, std::shared_ptr<void> msgData) override
    {
        int ret;
        std::shared_ptr<ImageData> inData;
        switch (msgId)
        {
        case MSG_APP_START:
            ret = Start();
            break;
        case MSG_VDEC_DATA:
            
        inData = std::static_pointer_cast<ImageData>(msgData);
            ret = Proprocess(inData);

            break;
        case MSG_APP_EXIT:
            // 发送结束消息
            m_pFFDecoder->StopDecode();
            m_pVdec->StopDecode();

            break;
        default:
            break;
        }

        return ret;
    };
    ~PreProcess()
    {

        delete m_pVdec;
        delete m_pIvps;
    };
};

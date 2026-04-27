#ifndef CAPTURE_DECODE_THREAD_H
#define CAPTURE_DECODE_THREAD_H

#pragma once

#include "PipelineThread.h"
#include "axcore/include/FFmpegDecoder.hpp"
#include "axcore/include/VdecHelper.hpp"
#include "axcore/include/IvpsHelper.hpp"
#include "axcore/include/ImageData.hpp"

class CaptureDecodeThread : public PipelineThread {
public:
    CaptureDecodeThread();
    ~CaptureDecodeThread();

    int Init() override;
    int Process(int msgId, std::shared_ptr<void> msgData) override;

    // 设置输入源
    void SetStreamUrl(const std::string& url);

    // 获取输出队列（给下一个线程用）
    ThreadSafeQueue<std::shared_ptr<ImageData>>* GetOutputQueue() {
        return &m_outputQueue;
    }

private:
    int InitFFmpeg();
    int InitVdec();
    int InitIvps();

    static int VdecCallback(ImageData imageData, int grp, int chn, void* userData);

private:
    std::string m_streamUrl;
    FFmpegDecoder* m_ffDecoder;
    VdecHelper* m_vdec;
    IvpsHelper* m_ivps;

    int m_width;
    int m_height;
    int m_fps;

    ThreadSafeQueue<std::shared_ptr<ImageData>> m_outputQueue;
    pthread_t m_ffmpegThread;
    bool m_isRunning;
};

#endif
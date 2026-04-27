#ifndef ENCODE_STREAM_THREAD_H
#define ENCODE_STREAM_THREAD_H

#pragma once

#include "PipelineThread.h"
#include "axcore/include/VencHelper.hpp"
#include "axcore/include/FFmpegEncoder.hpp"
#include "axcore/include/ImageData.hpp"

class EncodeStreamThread : public PipelineThread {
public:
    EncodeStreamThread();
    ~EncodeStreamThread();

    int Init() override;
    int Process(int msgId, std::shared_ptr<void> msgData) override;

    // 设置推流地址
    void SetStreamUrl(const std::string& url);

    // 设置编码参数
    void SetEncodeParams(int width, int height, int fps, int bitrate);

    // 设置输入队列（从BusinessThread获取）
    void SetInputQueue(ThreadSafeQueue<std::shared_ptr<ImageData>>* queue) {
        m_inputQueue = queue;
    }

private:
    int InitVenc();
    int InitEncoder();
    static int VencCallback(AX_VENC_STREAM_T streamData, int chn, void* userData);
    static int EncoderCallback(void* userData, uint8_t* data, int size);

private:
    std::string m_streamUrl;
    VencHelper* m_venc;
    FFmpegEncoder* m_ffEncoder;

    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;

    ThreadSafeQueue<std::shared_ptr<ImageData>>* m_inputQueue;
    bool m_isRunning;
};

#endif
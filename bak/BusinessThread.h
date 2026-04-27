#ifndef BUSINESS_THREAD_H
#define BUSINESS_THREAD_H

#pragma once

#include "PipelineThread.h"
#include "axcore/include/ImageData.hpp"
#include "tracker/sort/sort_track.h"
#include "detector/Yolov5.hpp"

class BusinessThread : public PipelineThread {
public:
    BusinessThread();
    ~BusinessThread();

    int Init() override;
    int Process(int msgId, std::shared_ptr<void> msgData) override;

    // 设置输入队列（从InferenceThread获取）
    void SetInputQueue(ThreadSafeQueue<std::shared_ptr<ImageData>>* queue) {
        m_inputQueue = queue;
    }

    // 设置推理结果队列
    void SetInferResultQueue(ThreadSafeQueue<std::shared_ptr<std::vector<detection::Object>>>* queue) {
        m_inferResultQueue = queue;
    }

    // 获取输出队列（给EncodeThread用）
    ThreadSafeQueue<std::shared_ptr<ImageData>>* GetOutputQueue() {
        return &m_outputQueue;
    }

private:
    int RunTrackingAndDrawing(std::shared_ptr<ImageData> imageData,
                              std::shared_ptr<std::vector<detection::Object>> objects);

private:
    SORT_TRACKER* m_tracker;
    unsigned int m_frameId;

    ThreadSafeQueue<std::shared_ptr<ImageData>>* m_inputQueue;
    ThreadSafeQueue<std::shared_ptr<std::vector<detection::Object>>>* m_inferResultQueue;
    ThreadSafeQueue<std::shared_ptr<ImageData>> m_outputQueue;

    bool m_isRunning;
};

#endif
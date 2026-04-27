#ifndef INFERENCE_THREAD_H
#define INFERENCE_THREAD_H

#pragma once

#include "PipelineThread.h"
#include "axcore/include/ImageData.hpp"
#include "detector/Yolov5.hpp"

class InferenceThread : public PipelineThread {
public:
    InferenceThread();
    ~InferenceThread();

    int Init() override;
    int Process(int msgId, std::shared_ptr<void> msgData) override;

    // 设置模型路径
    void SetModelPath(const std::string& modelPath);

    // 设置输入队列（从CaptureDecodeThread获取）
    void SetInputQueue(ThreadSafeQueue<std::shared_ptr<ImageData>>* queue) {
        m_inputQueue = queue;
    }

    // 获取输出队列（给BusinessThread用）
    ThreadSafeQueue<std::shared_ptr<ImageData>>* GetOutputQueue() {
        return &m_outputQueue;
    }

    // 获取推理结果队列
    ThreadSafeQueue<std::shared_ptr<std::vector<detection::Object>>>* GetInferResultQueue() {
        return &m_inferResultQueue;
    }

private:
    int RunInference(std::shared_ptr<ImageData> imageData);

private:
    std::string m_modelPath;
    Yolov5* m_engine;

    ThreadSafeQueue<std::shared_ptr<ImageData>>* m_inputQueue;
    ThreadSafeQueue<std::shared_ptr<ImageData>> m_outputQueue;
    ThreadSafeQueue<std::shared_ptr<std::vector<detection::Object>>> m_inferResultQueue;

    int m_modelWidth;
    int m_modelHeight;
    bool m_isRunning;
};

#endif
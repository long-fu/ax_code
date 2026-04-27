#include "InferenceThread.h"
#include "common/Logger.h"

InferenceThread::InferenceThread()
    : m_engine(nullptr), m_inputQueue(nullptr)
    , m_modelWidth(640), m_modelHeight(640), m_isRunning(false)
{
}

InferenceThread::~InferenceThread() {
    if (m_engine) {
        delete m_engine;
        m_engine = nullptr;
    }
}

void InferenceThread::SetModelPath(const std::string& modelPath) {
    m_modelPath = modelPath;
}

int InferenceThread::Init() {
    LOG_INFO("InferenceThread init: {}", SelfInstanceName());

    // 初始化推理引擎
    m_engine = new Yolov5();
    int ret = m_engine->Init(m_modelPath);
    if (ret != 0) {
        LOG_ERROR("Yolov5 init failed: {}", ret);
        return ret;
    }

    m_isRunning = true;
    LOG_INFO("InferenceThread init success");
    return 0;
}

int InferenceThread::Process(int msgId, std::shared_ptr<void> msgData) {
    if (!m_inputQueue) {
        LOG_ERROR("Input queue is null");
        return -1;
    }

    // 从输入队列获取数据
    while (m_isRunning) {
        auto imageData = m_inputQueue->Pop();
        if (imageData == nullptr) {
            usleep(10000);  // 10ms
            continue;
        }

        int ret = RunInference(imageData);
        if (ret != 0) {
            LOG_ERROR("RunInference failed: {}", ret);
        }
    }

    return 0;
}

int InferenceThread::RunInference(std::shared_ptr<ImageData> imageData) {
    if (!m_engine || !imageData) {
        return -1;
    }

    // 推理
    std::vector<detection::Object> objects;
    // m_engine->Inference(imageData->data, objects);

    // 后处理
    // m_engine->Postprocess(imageData->u32Width, imageData->u32Height, objects);

    // 推送图像到输出队列（用于绘制）
    m_outputQueue.Push(imageData);

    // 推送推理结果到结果队列（用于跟踪）
    auto result = std::make_shared<std::vector<detection::Object>>(objects);
    m_inferResultQueue.Push(result);

    return 0;
}
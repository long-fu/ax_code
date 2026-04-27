#include "BusinessThread.h"
#include "common/Logger.h"
#include "drawing.h"

BusinessThread::BusinessThread()
    : m_tracker(nullptr), m_frameId(0), m_inputQueue(nullptr)
    , m_inferResultQueue(nullptr), m_isRunning(false)
{
}

BusinessThread::~BusinessThread() {
    if (m_tracker) {
        delete m_tracker;
        m_tracker = nullptr;
    }
}

int BusinessThread::Init() {
    LOG_INFO("BusinessThread init: {}", SelfInstanceName());

    // 初始化跟踪器
    m_tracker = new SORT_TRACKER();
    m_frameId = 0;

    m_isRunning = true;
    LOG_INFO("BusinessThread init success");
    return 0;
}

int BusinessThread::Process(int msgId, std::shared_ptr<void> msgData) {
    if (!m_inputQueue || !m_inferResultQueue) {
        LOG_ERROR("Input queue is null");
        return -1;
    }

    while (m_isRunning) {
        auto imageData = m_inputQueue->Pop();
        auto objects = m_inferResultQueue->Pop();

        if (imageData == nullptr || objects == nullptr) {
            usleep(10000);  // 10ms
            continue;
        }

        int ret = RunTrackingAndDrawing(imageData, objects);
        if (ret != 0) {
            LOG_ERROR("RunTrackingAndDrawing failed: {}", ret);
        }
    }

    return 0;
}

int BusinessThread::RunTrackingAndDrawing(std::shared_ptr<ImageData> imageData,
                                           std::shared_ptr<std::vector<detection::Object>> objects) {
    if (!m_tracker || !imageData || !objects) {
        return -1;
    }

    // 构建跟踪输入
    vector<TrackingBox> detFrameData;
    for (size_t i = 0; i < objects->size(); ++i) {
        if ((*objects)[i].label == 1) {  // person
            TrackingBox cur_box;
            cur_box.box = (*objects)[i].rect;
            cur_box.frame_id = m_frameId;
            detFrameData.push_back(cur_box);
        }
    }

    m_frameId++;

    // 更新跟踪器
    m_tracker->update(detFrameData);
    vector<TrackingBox> tracking_results = m_tracker->getReport();

    LOG_DEBUG("frame_id: {}, det: {}, track: {}",
              m_frameId, detFrameData.size(), tracking_results.size());

    // 绘制跟踪结果
    // for (size_t i = 0; i < tracking_results.size(); i++) {
    //     auto& item = tracking_results[i];
    //     // 绘制框和ID
    //     // DrawRect(...);
    // }

    // 推送到输出队列（编码）
    m_outputQueue.Push(imageData);

    return 0;
}
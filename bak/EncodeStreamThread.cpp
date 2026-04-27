#include "EncodeStreamThread.h"
#include "common/Logger.h"

EncodeStreamThread::EncodeStreamThread()
    : m_venc(nullptr), m_ffEncoder(nullptr), m_inputQueue(nullptr)
    , m_width(1920), m_height(1080), m_fps(25), m_bitrate(2000000)
    , m_isRunning(false)
{
}

EncodeStreamThread::~EncodeStreamThread() {
    if (m_venc) {
        m_venc->StopEncode();
        m_venc->Destroy();
        delete m_venc;
        m_venc = nullptr;
    }
    if (m_ffEncoder) {
        delete m_ffEncoder;
        m_ffEncoder = nullptr;
    }
}

void EncodeStreamThread::SetStreamUrl(const std::string& url) {
    m_streamUrl = url;
}

void EncodeStreamThread::SetEncodeParams(int width, int height, int fps, int bitrate) {
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_bitrate = bitrate;
}

int EncodeStreamThread::Init() {
    LOG_INFO("EncodeStreamThread init: {}", SelfInstanceName());

    int ret = InitVenc();
    if (ret != 0) {
        LOG_ERROR("InitVenc failed: {}", ret);
        return ret;
    }

    ret = InitEncoder();
    if (ret != 0) {
        LOG_ERROR("InitEncoder failed: {}", ret);
        return ret;
    }

    m_isRunning = true;
    LOG_INFO("EncodeStreamThread init success");
    return 0;
}

int EncodeStreamThread::InitVenc() {
    m_venc = new VencHelper(0, m_width, m_height, m_fps, m_fps);
    int ret = m_venc->Init();
    if (ret != 0) {
        LOG_ERROR("VencHelper init failed: {}", ret);
        return ret;
    }

    ret = m_venc->Encode(VencCallback, this);
    if (ret != 0) {
        LOG_ERROR("VencHelper encode failed: {}", ret);
        return ret;
    }

    return 0;
}

int EncodeStreamThread::InitEncoder() {
    if (m_streamUrl.length() > 0) {
        m_ffEncoder = new FFmpegEncoder("rtmp_encoder");
        int ret = m_ffEncoder->Init(m_width, m_height, m_fps, m_streamUrl.c_str());
        if (ret != 0) {
            LOG_ERROR("FFmpegEncoder init failed: {}", ret);
            return ret;
        }
    }
    return 0;
}

int EncodeStreamThread::VencCallback(AX_VENC_STREAM_T streamData, int chn, void* userData) {
    EncodeStreamThread* thiz = (EncodeStreamThread*)userData;
    if (!thiz || !thiz->m_isRunning) {
        return -1;
    }

    // 推送到 RTMP
    if (thiz->m_ffEncoder) {
        // thiz->m_ffEncoder->SendStream(...);
    }

    return 0;
}

int EncodeStreamThread::EncoderCallback(void* userData, uint8_t* data, int size) {
    // 处理编码数据
    return 0;
}

int EncodeStreamThread::Process(int msgId, std::shared_ptr<void> msgData) {
    if (!m_inputQueue) {
        LOG_ERROR("Input queue is null");
        return -1;
    }

    while (m_isRunning) {
        auto imageData = m_inputQueue->Pop();
        if (imageData == nullptr) {
            usleep(10000);  // 10ms
            continue;
        }

        // 编码
        if (m_venc) {
            m_venc->Write(imageData.get(), this);
        }
    }

    return 0;
}
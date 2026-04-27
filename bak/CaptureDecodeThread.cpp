#include "CaptureDecodeThread.h"
#include "common/Logger.h"

CaptureDecodeThread::CaptureDecodeThread()
    : m_ffDecoder(nullptr), m_vdec(nullptr), m_ivps(nullptr)
    , m_width(1920), m_height(1080), m_fps(25), m_isRunning(false)
{
}

CaptureDecodeThread::~CaptureDecodeThread() {
    if (m_ffDecoder) {
        m_ffDecoder->StopDecode();
        delete m_ffDecoder;
        m_ffDecoder = nullptr;
    }
    if (m_vdec) {
        m_vdec->Destory();
        delete m_vdec;
        m_vdec = nullptr;
    }
    if (m_ivps) {
        delete m_ivps;
        m_ivps = nullptr;
    }
}

void CaptureDecodeThread::SetStreamUrl(const std::string& url) {
    m_streamUrl = url;
}

int CaptureDecodeThread::Init() {
    LOG_INFO("CaptureDecodeThread init: {}", SelfInstanceName());

    int ret = InitFFmpeg();
    if (ret != 0) {
        LOG_ERROR("InitFFmpeg failed: {}", ret);
        return ret;
    }

    ret = InitVdec();
    if (ret != 0) {
        LOG_ERROR("InitVdec failed: {}", ret);
        return ret;
    }

    ret = InitIvps();
    if (ret != 0) {
        LOG_ERROR("InitIvps failed: {}", ret);
        return ret;
    }

    m_isRunning = true;
    return 0;
}

int CaptureDecodeThread::InitFFmpeg() {
    m_ffDecoder = new FFmpegDecoder("rtsp_decoder");
    m_ffDecoder->SetTransport("tcp");

    int ret = m_ffDecoder->GetVideoInfo();
    if (ret != 0) {
        LOG_ERROR("FFmpegDecoder GetVideoInfo failed: {}", ret);
        return ret;
    }

    m_width = m_ffDecoder->GetFrameWidth();
    m_height = m_ffDecoder->GetFrameHeight();
    m_fps = m_ffDecoder->GetFps();

    LOG_INFO("Video info: {}x{} @ {}fps", m_width, m_height, m_fps);
    return 0;
}

int CaptureDecodeThread::InitVdec() {
    m_vdec = new VdecHelper(0, PT_H264, m_width, m_height, m_fps);
    int ret = m_vdec->Init();
    if (ret != 0) {
        LOG_ERROR("VdecHelper init failed: {}", ret);
        return ret;
    }

    ret = m_vdec->Decode(VdecCallback, this);
    if (ret != 0) {
        LOG_ERROR("VdecHelper decode failed: {}", ret);
        return ret;
    }

    return 0;
}

int CaptureDecodeThread::InitIvps() {
    AX_U64 blkSize = m_width * m_height * 3 / 2;
    m_ivps = new IvpsHelper(0, blkSize, 10);

    int ret = m_ivps->ResizeAndCSC(AX_FORMAT_YUV420_SEMIPLANAR, m_width, m_height);
    if (ret != 0) {
        LOG_ERROR("IvpsHelper resize failed: {}", ret);
        return ret;
    }

    return 0;
}

int CaptureDecodeThread::VdecCallback(ImageData imageData, int grp, int chn, void* userData) {
    CaptureDecodeThread* thiz = (CaptureDecodeThread*)userData;
    if (!thiz || !thiz->m_isRunning) {
        return -1;
    }

    // IVPS 处理
    ImageData outImage;
    int ret = thiz->m_ivps->Process(outImage, imageData);
    if (ret != 0) {
        LOG_ERROR("IvpsHelper process failed: {}", ret);
        return ret;
    }

    // 推送到输出队列
    auto pImage = std::make_shared<ImageData>(outImage);
    thiz->m_outputQueue.Push(pImage);

    return 0;
}

int CaptureDecodeThread::Process(int msgId, std::shared_ptr<void> msgData) {
    // 拉流线程独立运行，通过回调处理数据
    // 这里处理控制消息
    switch (msgId) {
        case 0:  // 开始拉流
            if (m_ffDecoder && m_streamUrl.length() > 0) {
                m_ffDecoder->Decode(VdecCallback, this);
            }
            break;
        case 1:  // 停止拉流
            m_isRunning = false;
            if (m_ffDecoder) {
                m_ffDecoder->StopDecode();
            }
            break;
        default:
            break;
    }
    return 0;
}
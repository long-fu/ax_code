#include "FFmpegDecoder.hpp"
#include "Logger.h"
using namespace std;
#define DEVICE_MAX 4

namespace
{
    const int64_t kUsec = 1000000;
    const uint32_t kDecodeFrameQueueSize = 256;
    const int kDecodeQueueOpWait = 10000;     // decode wait 10ms/frame
    const int kFrameEnQueueRetryTimes = 1000; // max wait time for the frame to enter in queue
    const int kQueueOpRetryTimes = 1000;
    const int kOutputJamWait = 10000;
    const int kInvalidTpye = -1;
    const int kWaitDecodeFinishInterval = 1000;
    const int kDefaultFps = 1;
    const int kReadSlow = 5;
    const uint32_t kVideoChannelMax310 = 32;
    const uint32_t kVideoChannelMax310B = 128;
    const uint32_t kVideoChannelMax310P = 256;


    const int kNoFlag = 0;                          // no flag
    const int kInvalidVideoIndex = -1;              // invalid video index
    const string kRtspTransport = "rtsp_transport"; // rtsp transport
    const string kUdp = "udp";                      // video format udp
    const string kTcp = "tcp";
    const string kBufferSize = "buffer_size";              // buffer size string
    const string kMaxBufferSize = "10485760";              // maximum buffer size:10MB
    const string kMaxDelayStr = "max_delay";               // maximum delay string
    const string kMaxDelayValue = "100000000";             // maximum delay time:100s
    const string kTimeoutStr = "stimeout";                 // timeout string
    const string kTimeoutValue = "5000000";                // timeout:5s
    const string kPktSize = "pkt_size";                    // ffmpeg pakect size string
    const string kPktSizeValue = "10485760";               // ffmpeg packet size value:10MB
    const string kReorderQueueSize = "reorder_queue_size"; // reorder queue size
    const string kReorderQueueSizeValue = "0";             // reorder queue size value
    const int kErrorBufferSize = 1024;                     // buffer size for error info
    const uint32_t kDefaultStreamFps = 5;
    const uint32_t kOneSecUs = 1000 * 1000;
}

FFmpegDecoder::FFmpegDecoder(const std::string &streamName) : m_streamName(streamName)
{
    m_rtspTransport.assign(kTcp.c_str());
    m_isFinished = false;
    m_isStop = false;
    // GetVideoInfo();
}

void FFmpegDecoder::SetTransport(const std::string &transportType)
{
    m_rtspTransport.assign(transportType.c_str());
};

int FFmpegDecoder::GetVideoIndex(AVFormatContext *avFormatContext)
{
    if (avFormatContext == nullptr)
    { // verify input pointer
        return kInvalidVideoIndex;
    }

    // get video index in streams
    for (uint32_t i = 0; i < avFormatContext->nb_streams; i++)
    {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        { // check is media type is video
            return i;
        }
    }

    return kInvalidVideoIndex;
}

void FFmpegDecoder::InitVideoStreamFilter(const AVBitStreamFilter *&videoFilter)
{
    if (m_nVideoType == AV_CODEC_ID_H264)
    { // check video type is h264
        videoFilter = av_bsf_get_by_name("h264_mp4toannexb");
    }
    else
    { // the video type is h265
        videoFilter = av_bsf_get_by_name("hevc_mp4toannexb");
    }
}

void FFmpegDecoder::SetDictForRtsp(AVDictionary *&avdic)
{
    LOG_INFO("Set parameters for %s\n", m_streamName.c_str());

    av_dict_set(&avdic, kRtspTransport.c_str(), m_rtspTransport.c_str(), kNoFlag);
    av_dict_set(&avdic, kBufferSize.c_str(), kMaxBufferSize.c_str(), kNoFlag);
    av_dict_set(&avdic, kMaxDelayStr.c_str(), kMaxDelayValue.c_str(), kNoFlag);
    av_dict_set(&avdic, kTimeoutStr.c_str(), kTimeoutValue.c_str(), kNoFlag);
    av_dict_set(&avdic, kReorderQueueSize.c_str(),
                kReorderQueueSizeValue.c_str(), kNoFlag);
    av_dict_set(&avdic, kPktSize.c_str(), kPktSizeValue.c_str(), kNoFlag);
    LOG_INFO("Set parameters for %s end\n", m_streamName.c_str());
}

bool FFmpegDecoder::OpenVideo(AVFormatContext *&avFormatContext)
{
    bool ret = true;
    AVDictionary *avdic = nullptr;

    // av_log_set_level(AV_LOG_DEBUG);

    LOG_INFO("Open video %s ...\n", m_streamName.c_str());
    SetDictForRtsp(avdic);
    int openRet = avformat_open_input(&avFormatContext,
                                      m_streamName.c_str(), nullptr,
                                      &avdic);
    if (openRet < 0)
    { // check open video result
        char buf_error[kErrorBufferSize];
        av_strerror(openRet, buf_error, kErrorBufferSize);
        
        LOG_ERROR("Could not open video:%s, return :%d, error info:%s\n",
                          m_streamName.c_str(), openRet, buf_error);
        ret = false;
    }

    if (avdic != nullptr)
    { // free AVDictionary
        av_dict_free(&avdic);
    }

    return ret;
}

bool FFmpegDecoder::InitVideoParams(int videoIndex,
                                    AVFormatContext *avFormatContext,
                                    AVBSFContext *&bsfCtx)
{
    const AVBitStreamFilter *videoFilter = nullptr;
    InitVideoStreamFilter(videoFilter);
    if (videoFilter == nullptr)
    { // check video fileter is nullptr
        LOG_ERROR("Unkonw bitstream filter, videoFilter is nullptr!\n");
        return false;
    }

    // checke alloc bsf context result
    if (av_bsf_alloc(videoFilter, &bsfCtx) < 0)
    {
        LOG_ERROR("Fail to call av_bsf_alloc!\n");
        return false;
    }

    // check copy parameters result
    if (avcodec_parameters_copy(bsfCtx->par_in,
                                avFormatContext->streams[videoIndex]->codecpar) < 0)
    {
        LOG_ERROR("Fail to call avcodec_parameters_copy!\n");
        return false;
    }

    bsfCtx->time_base_in = avFormatContext->streams[videoIndex]->time_base;

    // check initialize bsf contextreult
    if (av_bsf_init(bsfCtx) < 0)
    {
        LOG_ERROR("Fail to call av_bsf_init!\n");
        return false;
    }

    return true;
}

void FFmpegDecoder::Decode(FrameProcessCallBack callback,
                           void *callbackParam)
{
    LOG_INFO("Start ffmpeg decode video %s ...\n", m_streamName.c_str());
    avformat_network_init(); // init network

    AVFormatContext *avFormatContext = avformat_alloc_context();

    // check open video result
    if (!OpenVideo(avFormatContext))
    {
        return;
    }

    int videoIndex = GetVideoIndex(avFormatContext);
    if (videoIndex == kInvalidVideoIndex)
    { // check video index is valid
        LOG_ERROR("Rtsp %s index is -1\n", m_streamName.c_str());
        return;
    }

    AVBSFContext *bsfCtx = nullptr;
    // check initialize video parameters result
    if (!InitVideoParams(videoIndex, avFormatContext, bsfCtx))
    {
        return;
    }

    LOG_INFO("Start decode frame of video %s ...\n", m_streamName.c_str());

    AVPacket avPacket;
    int processOk = true;
    // loop to get every frame from video stream
    while ((av_read_frame(avFormatContext, &avPacket) == 0) && processOk && !m_isStop)
    {
        if (avPacket.stream_index == videoIndex)
        {   // check current stream is video
            // send video packet to ffmpeg
            if (av_bsf_send_packet(bsfCtx, &avPacket))
            {
                LOG_ERROR("Fail to call av_bsf_send_packet, channel id:%s\n",
                                  m_streamName.c_str());
            }

            // receive single frame from ffmpeg
            while ((av_bsf_receive_packet(bsfCtx, &avPacket) == 0) && !m_isStop)
            {
                int ret = callback(callbackParam, avPacket.data, avPacket.size);
                if (ret != 0)
                {
                    processOk = false;
                    break;
                }
            }
        }
        av_packet_unref(&avPacket);
    }

    av_bsf_free(&bsfCtx);                   // free AVBSFContext pointer
    avformat_close_input(&avFormatContext); // close input video

    m_isFinished = true;
    LOG_INFO("Ffmpeg decoder %s finished\n", m_streamName.c_str());
}

int FFmpegDecoder::GetVideoInfo()
{
    avformat_network_init(); // init network
    AVFormatContext *avFormatContext = avformat_alloc_context();
    bool ret = OpenVideo(avFormatContext);
    if (ret == false)
    {
        LOG_ERROR("Open %s failed\n", m_streamName.c_str());
        return -1;
    }

    if (avformat_find_stream_info(avFormatContext, NULL) < 0)
    {
        LOG_ERROR("Get stream info of %s failed\n", m_streamName.c_str());
        return -1;
    }

    int videoIndex = GetVideoIndex(avFormatContext);
    if (videoIndex == kInvalidVideoIndex)
    { // check video index is valid
        LOG_ERROR("Video index is %d, current media stream has no "
                          "video info:%s\n",
                          kInvalidVideoIndex, m_streamName.c_str());
        avformat_close_input(&avFormatContext);
        return -1;
    }

    AVStream *inStream = avFormatContext->streams[videoIndex];

    m_nFrameWidth = inStream->codecpar->width;
    m_nFrameHeight = inStream->codecpar->height;
    if (inStream->avg_frame_rate.den)
    {
        m_nFps = inStream->avg_frame_rate.num / inStream->avg_frame_rate.den;
    }
    else
    {
        m_nFps = kDefaultStreamFps;
    }

    m_nVideoType = inStream->codecpar->codec_id;
    m_nProfile = inStream->codecpar->profile;

    avformat_close_input(&avFormatContext);

    LOG_INFO("Video %s, type %d, profile %d, width:%d, height:%d, fps:%d\n",
                     m_streamName.c_str(), m_nVideoType, m_nProfile, m_nFrameWidth, m_nFrameHeight, m_nFps);
    return 0;
}

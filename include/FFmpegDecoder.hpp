#include <dirent.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

enum StreamType
{
    STREAM_VIDEO = 0,
    STREAM_RTSP,
};

enum DecodeStatus
{
    DECODE_ERROR = -1,
    DECODE_UNINIT = 0,
    DECODE_READY = 1,
    DECODE_START = 2,
    DECODE_FFMPEG_FINISHED = 3,
    DECODE_DVPP_FINISHED = 4,
    DECODE_FINISHED = 5
};

#define INVALID_CHANNEL_ID (-1)
#define INVALID_STREAM_FORMAT (-1)
#define VIDEO_CHANNEL_MAX (256)
#define RTSP_TRANSPORT_UDP "udp"
#define RTSP_TRANSPORT_TCP "tcp"

typedef int (*FrameProcessCallBack)(void *callback_param, void *frame_data,
                                    int frame_size);



class FFmpegDecoder
{
public:
    FFmpegDecoder(const std::string &name);
    ~FFmpegDecoder() {}
    void Decode(FrameProcessCallBack callback_func, void *callback_param);
    int GetVideoInfo();
    int GetFrameWidth()
    {
        return m_nFrameWidth;
    }
    int GetFrameHeight()
    {
        return m_nFrameHeight;
    }
    int GetVideoType()
    {
        return m_nVideoType;
    }
    int GetFps()
    {
        return m_nFps;
    }
    bool IsFinished()
    {
        return m_isFinished;
    }
    int GetProfile()
    {
        return m_nProfile;
    }
    void SetTransport(const std::string &transportType);
    void StopDecode()
    {
        m_isStop = true;
    }

private:
    int GetVideoIndex(AVFormatContext *av_format_context);

    void InitVideoStreamFilter(const AVBitStreamFilter *&video_filter);
    bool OpenVideo(AVFormatContext *&av_format_context);
    void SetDictForRtsp(AVDictionary *&avdic);
    bool InitVideoParams(int videoIndex,
                         AVFormatContext *av_format_context,
                         AVBSFContext *&bsf_ctx);

private:
    bool m_isFinished;
    bool m_isStop;
    int m_nFrameWidth;
    int m_nFrameHeight;
    int m_nVideoType;
    int m_nProfile;
    int m_nFps;
    std::string m_streamName;
    std::string m_rtspTransport;
};
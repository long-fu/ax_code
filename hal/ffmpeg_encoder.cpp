#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "logger.h"
#include "ffmpeg_encoder.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

FFmpegEncoder::FFmpegEncoder(std::string stream_name, int frame_rate,
                             size_t pic_width, size_t pic_height, AVPixelFormat pix_fmt,
                             size_t gop_size, std::string profile) : push_name_(stream_name), frame_rate_(frame_rate),
                                                                     pic_width_(pic_width), pic_height_(pic_height), pix_fmt_(pix_fmt),
                                                                     gop_size_(gop_size), profile_(profile)
{
}

FFmpegEncoder::~FFmpegEncoder()
{
    Release();
}

static std::string GuessFormatFromName(const std::string &name)
{
    std::string format;
    if (name.find("rtmp:") == 0)
    {
        format = "flv";
    }
    else if (name.find("rtsp:") == 0)
    {
        format = "rtsp";
    }
    return format;
}

int FFmpegEncoder::Init()
{
    std::string format = GuessFormatFromName(push_name_);

    const char *output = push_name_.c_str();
    const char *profile = profile_.c_str();
    AVRational av_framerate;
    av_framerate.num = frame_rate_;
    av_framerate.den = 1;

    int ret = 0;

    encoder_avfc_ = NULL;

    ret = avformat_alloc_output_context2(&encoder_avfc_, NULL, format.empty() ? NULL : format.c_str(), output);

    if (ret < 0)
    {
        LOG_ERROR("avformat_alloc_output_context2 failed");
        return ret;
    }

    // encoder_avfc->flags |= AVFMT_FLAG_NOBUFFER;
    encoder_avfc_->flags |= AVFMT_FLAG_FLUSH_PACKETS;

    if (!(encoder_avfc_->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open2(&encoder_avfc_->pb, output, AVIO_FLAG_WRITE, NULL, NULL);
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("avio_open2 failed err code: {} Reason: {}", ret, av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));
            return ret;
        }
    }

    video_avc_ = avcodec_find_encoder(AV_CODEC_ID_H264);

    encoder_avfc_->video_codec = (AVCodec *)video_avc_;
    encoder_avfc_->video_codec_id = AV_CODEC_ID_H264;

    video_avcc_ = avcodec_alloc_context3(video_avc_);

    video_avcc_->codec_tag = 0;
    video_avcc_->codec_id = AV_CODEC_ID_H264;
    video_avcc_->codec_type = AVMEDIA_TYPE_VIDEO;
    video_avcc_->gop_size = frame_rate_ / 2;
    video_avcc_->height = pic_height_;
    video_avcc_->width = pic_width_;
    video_avcc_->pix_fmt = (AVPixelFormat)pix_fmt_; // AV_PIX_FMT_NV12;// NV12 IS YUV420
    // control rate
    video_avcc_->bit_rate = 0;
    video_avcc_->rc_buffer_size = 0;
    video_avcc_->rc_max_rate = 0;
    video_avcc_->rc_min_rate = 0;
    video_avcc_->time_base.num = av_framerate.den;
    video_avcc_->time_base.den = av_framerate.num;

    if (encoder_avfc_->oformat->flags & AVFMT_GLOBALHEADER)
    {
        video_avcc_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    avs_ = avformat_new_stream(encoder_avfc_, video_avc_);

    ret = avcodec_parameters_from_context(avs_->codecpar, video_avcc_);

    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avcodec_parameters_from_context failed err code: {} Reason: {}",
                  ret, av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));
        return ret;
    }

    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "profile", profile, 0);
    av_dict_set(&codec_options, "preset", "superfast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    ret = avcodec_open2(video_avcc_, video_avc_, &codec_options);
    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avcodec_open2 failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        return ret;
    }

    avs_->codecpar->extradata = video_avcc_->extradata;
    avs_->codecpar->extradata_size = video_avcc_->extradata_size;

    av_dump_format(encoder_avfc_, 0, output, 1);

    ret = avformat_write_header(encoder_avfc_, NULL);
    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avformat_write_header failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        return ret;
    }

    video_frame_ = av_frame_alloc();

    // int frame_buf_size = av_image_get_buffer_size(
    //     video_avcc->pix_fmt, video_avcc->width, video_avcc->height, 1);

    video_frame_->width = video_avcc_->width;
    video_frame_->height = video_avcc_->height;
    video_frame_->format = video_avcc_->pix_fmt;
    video_frame_->pts = 1;
    // ACLLITE_LOG_INFO("FFmpeg encoder success");
    // valid = true;
    // std::ifstream test_f(name.c_str());
    // output_is_file = test_f.good();
    return 0;
}

int FFmpegEncoder::Release()
{
    if (encoder_avfc_ != nullptr)
    {
        av_write_trailer(encoder_avfc_);
        avformat_close_input(&encoder_avfc_);
        encoder_avfc_ = nullptr;
    }

    if (video_frame_ != nullptr) {
        av_frame_free(&video_frame_);
        video_frame_ = nullptr;
    }

    if (video_avcc_ != nullptr) {
        avcodec_free_context(&video_avcc_);
        video_avcc_ = nullptr;
    }

    return 0;
}

int FFmpegEncoder::WriteFrame(void *data, size_t data_size)
{
    (void)data_size;
    int ret = 0;
    av_image_fill_arrays(video_frame_->data, video_frame_->linesize, (const uint8_t *)data,
                         video_avcc_->pix_fmt, video_avcc_->width,
                         video_avcc_->height, 1);

    ret = avcodec_send_frame(video_avcc_, video_frame_);

    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avcodec_send_frame failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        return -1;
    }

    while (true)
    {
        AVPacket pkt = {0};
        av_init_packet(&pkt);
        ret = avcodec_receive_packet(video_avcc_, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_unref(&pkt);
            break;
        }
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("avcodec_receive_packet failed err code: {} Reason: {}", ret,
                      av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

            av_packet_unref(&pkt);
            return -1;
        }
        ret = av_interleaved_write_frame(encoder_avfc_, &pkt);
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("av_interleaved_write_frame failed err code: {} Reason: {}", ret,
                      av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));
            av_packet_unref(&pkt);
            return -1;
        }
        av_packet_unref(&pkt);
        video_frame_->pts += av_rescale_q(1, video_avcc_->time_base, avs_->time_base);
    }
    return 0;
}

int FFmpegEncoder::WritePacket(void *data, size_t data_size)
{
    int ret = 0;
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LOG_ERROR("av_packet_alloc failed");
        return -1;
    }

    ret = av_new_packet(pkt, (int)data_size);
    if (ret < 0) {
        LOG_ERROR("av_new_packet failed err code:{}", ret);
        av_packet_free(&pkt);
        return -1;
    }

    memcpy(pkt->data, data, data_size);

    pkt->pts = video_frame_->pts;
    pkt->dts = pkt->pts;
    pkt->flags = AV_PKT_FLAG_KEY;

    ret = av_write_frame(encoder_avfc_, pkt);

    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("av_write_frame failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        av_packet_free(&pkt);
        return -1;
    }

    av_packet_free(&pkt);

    video_frame_->pts += av_rescale_q(1, video_avcc_->time_base, avs_->time_base);
    return 0;
}
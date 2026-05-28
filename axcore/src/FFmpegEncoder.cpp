#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "Logger.h"
#include "FFmpegEncoder.hpp"


FFmpegEncoder::FFmpegEncoder(std::string stream_name, int frame_rate,
                             size_t pic_width, size_t pic_height, AVPixelFormat pix_fmt,
                             size_t gop_size, std::string profile) : m_sPushName(stream_name), m_iFrameRate(frame_rate),
                                                                     m_iPicWidth(pic_width), m_iPicHeight(pic_height), m_ePixFmt(pix_fmt),
                                                                     m_iGopSize(gop_size), m_sProfile(profile)
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
    std::string format = GuessFormatFromName(m_sPushName);

    const char *output = m_sPushName.c_str();
    const char *profile = m_sProfile.c_str();
    AVRational av_framerate;
    av_framerate.num = m_iFrameRate;
    av_framerate.den = 1;

    int ret = 0;

    m_pEncoder_avfc = NULL;

    ret = avformat_alloc_output_context2(&m_pEncoder_avfc, NULL, format.empty() ? NULL : format.c_str(), output);

    if (ret < 0)
    {
        LOG_ERROR("avformat_alloc_output_context2 failed");
        return ret;
    }

    // encoder_avfc->flags |= AVFMT_FLAG_NOBUFFER;
    m_pEncoder_avfc->flags |= AVFMT_FLAG_FLUSH_PACKETS;

    if (!(m_pEncoder_avfc->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open2(&m_pEncoder_avfc->pb, output, AVIO_FLAG_WRITE, NULL, NULL);
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("avio_open2 failed err code: {} Reason: {}", ret, av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));
            return ret;
        }
    }

    m_pVideo_avc = avcodec_find_encoder(AV_CODEC_ID_H264);

    m_pEncoder_avfc->video_codec = (AVCodec *)m_pVideo_avc;
    m_pEncoder_avfc->video_codec_id = AV_CODEC_ID_H264;

    m_pVideo_avcc = avcodec_alloc_context3(m_pVideo_avc);

    m_pVideo_avcc->codec_tag = 0;
    m_pVideo_avcc->codec_id = AV_CODEC_ID_H264;
    m_pVideo_avcc->codec_type = AVMEDIA_TYPE_VIDEO;
    m_pVideo_avcc->gop_size = m_iFrameRate / 2;
    m_pVideo_avcc->height = m_iPicHeight;
    m_pVideo_avcc->width = m_iPicWidth;
    m_pVideo_avcc->pix_fmt = (AVPixelFormat)m_ePixFmt; // AV_PIX_FMT_NV12;// NV12 IS YUV420
    // control rate
    m_pVideo_avcc->bit_rate = 0;
    m_pVideo_avcc->rc_buffer_size = 0;
    m_pVideo_avcc->rc_max_rate = 0;
    m_pVideo_avcc->rc_min_rate = 0;
    m_pVideo_avcc->time_base.num = av_framerate.den;
    m_pVideo_avcc->time_base.den = av_framerate.num;

    if (m_pEncoder_avfc->oformat->flags & AVFMT_GLOBALHEADER)
    {
        m_pVideo_avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    m_pAvs = avformat_new_stream(m_pEncoder_avfc, m_pVideo_avc);

    ret = avcodec_parameters_from_context(m_pAvs->codecpar, m_pVideo_avcc);

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

    ret = avcodec_open2(m_pVideo_avcc, m_pVideo_avc, &codec_options);
    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avcodec_open2 failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        return ret;
    }

    m_pAvs->codecpar->extradata = m_pVideo_avcc->extradata;
    m_pAvs->codecpar->extradata_size = m_pVideo_avcc->extradata_size;

    av_dump_format(m_pEncoder_avfc, 0, output, 1);

    ret = avformat_write_header(m_pEncoder_avfc, NULL);
    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("avformat_write_header failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        return ret;
    }

    m_pVideo_frame = av_frame_alloc();

    // int frame_buf_size = av_image_get_buffer_size(
    //     video_avcc->pix_fmt, video_avcc->width, video_avcc->height, 1);

    m_pVideo_frame->width = m_pVideo_avcc->width;
    m_pVideo_frame->height = m_pVideo_avcc->height;
    m_pVideo_frame->format = m_pVideo_avcc->pix_fmt;
    m_pVideo_frame->pts = 1;
    // ACLLITE_LOG_INFO("FFmpeg encoder success");
    // valid = true;
    // std::ifstream test_f(name.c_str());
    // output_is_file = test_f.good();
    return 0;
}

int FFmpegEncoder::Release()
{
    if (m_pEncoder_avfc != nullptr)
    {
        av_write_trailer(m_pEncoder_avfc);

        avformat_close_input(&m_pEncoder_avfc);

        av_frame_free(&m_pVideo_frame);

        m_pEncoder_avfc = nullptr;
    }
    
    return 0;
}

int FFmpegEncoder::WriteFrame(void *data, size_t data_size)
{
    int ret = 0;
    av_image_fill_arrays(m_pVideo_frame->data, m_pVideo_frame->linesize, (const uint8_t *)data,
                         m_pVideo_avcc->pix_fmt, m_pVideo_avcc->width,
                         m_pVideo_avcc->height, 1);

    ret = avcodec_send_frame(m_pVideo_avcc, m_pVideo_frame);

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
        ret = avcodec_receive_packet(m_pVideo_avcc, &pkt);
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("avcodec_receive_packet failed err code: {} Reason: {}", ret,
                      av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

            av_packet_unref(&pkt);
            return -1;
        }
        ret = av_interleaved_write_frame(m_pEncoder_avfc, &pkt);
        if (ret < 0)
        {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            LOG_ERROR("av_interleaved_write_frame failed err code: {} Reason: {}", ret,
                      av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

            return -1;
        }
        av_packet_unref(&pkt);
        m_pVideo_frame->pts += av_rescale_q(1, m_pVideo_avcc->time_base, m_pAvs->time_base);
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

    pkt->pts = m_pVideo_frame->pts;
    pkt->dts = pkt->pts;
    pkt->flags = AV_PKT_FLAG_KEY;

    ret = av_write_frame(m_pEncoder_avfc, pkt);

    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("av_write_frame failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));

        av_packet_free(&pkt);
        return -1;
    }

    av_packet_free(&pkt);

    m_pVideo_frame->pts += av_rescale_q(1, m_pVideo_avcc->time_base, m_pAvs->time_base);
    return 0;
}

#pragma once

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>


extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}

class FFmpegEncoder
{
private:
    /* data */
    std::string push_name_;
    int frame_rate_;
    size_t pic_width_;
    size_t pic_height_;
    AVPixelFormat pix_fmt_;
    size_t gop_size_;
    std::string profile_;

    AVFormatContext *encoder_avfc_{nullptr};
    AVCodecContext *video_avcc_{nullptr};
    const AVCodec *video_avc_{nullptr};
    AVStream *avs_{nullptr};
    AVFrame *video_frame_{nullptr};

public:
    FFmpegEncoder(std::string stream_name, int frame_rate,
                  size_t pic_width, size_t pic_height, AVPixelFormat pix_fmt,
                  size_t gop_size, std::string profile);
    int Init();
    
    int Release();

    // int WritrHeader()
    // {
    //     int ret = avformat_write_header(m_pEncoder_avfc, NULL);
    //     if (ret < 0)
    //     {
    //         // fprintf(stderr, "avformat_write_header failed");
    //         LOG_ERROR("avformat_write_header failed!! %d", ret);
    //         return ret;
    //     }
    //     return 0;
    // };

    // int WriteEnder()
    // {
    // // if (encoder_avfc != nullptr)
    // // {
    // //     av_write_trailer(encoder_avfc);

    // //     // ACLLITE_LOG_INFO("FFmpeg deinit 1");

    // //     // avcodec_free_context(&video_avcc);
        
    // //     // ACLLITE_LOG_INFO("FFmpeg deinit 2");
    
    // //     avformat_close_input(&encoder_avfc);

    // //     // ACLLITE_LOG_INFO("FFmpeg deinit 3");

    // //     av_frame_free(&video_frame);
    // // }
    //     if(m_pEncoder_avfc == nullptr) {
    //         LOG_ERROR("WriteEnder m_pEncoder_avfc == null");
    //         return 0;
    //     }   
    //     int ret = av_write_trailer(m_pEncoder_avfc);
    //     if (ret < 0)
    //     {
    //         LOG_ERROR("av_write_trailer failed!! %d", ret);
    //         return ret;
    //     }
    //     return 0;
    // };

    int WriteFrame(void *data, size_t data_size);
    int WritePacket(void *data, size_t data_size);
    ~FFmpegEncoder();
};

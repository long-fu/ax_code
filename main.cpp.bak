#include <iostream>

#include <iostream>
#include <string.h>
#include <memory>
// #include "RequestChannelId.hpp"
#include "FFmpegDecoder.hpp"
#include "VdecHelper.hpp"
#include "ImageData.hpp"
#include "FrameData.hpp"
#include "ax_venc_comm.h"
#include "ax_venc_api.h"
#include "IvpsHelper.hpp"

int g_resourceID;
#include "FFmpegEncoder.hpp"
#include "VencHelper.hpp"
// #include "Engine.hpp"
#include "Yolov5.hpp"

#include "sort_track.h"
#include "utils.h"
#include "Logger.h"

#include "drawing.h"

struct InferData
{
    ImageData img;
    std::vector<uint8_t> inferData;
    std::vector<detection::Object> objects;
};

struct AXContext
{
    FFmpegDecoder *ffDecoder;
    FFmpegEncoder *ffEncoder;
    VdecHelper *vdec;
    VencHelper *venc;
    IvpsHelper *ivps;
    Yolov5 *engine;
    ThreadSafeQueue<std::shared_ptr<ImageData>> *imageQueue;
    ThreadSafeQueue<std::shared_ptr<InferData>> *inferQueue;
    SORT_TRACKER *sort_tracker;
    unsigned int frame_id = 0;
};

// 拉流回调
int FrameProcessCallBackFunc(void *user_data, void *frame_data,
                             int frame_size)
{
    AXContext *ctx = (AXContext *)user_data;
    ctx->vdec->Write(frame_data, frame_size, nullptr);
    return 0;
}

// 拉流线程
void *FFmpegDecodeCallBack(void *argv)
{
    pthread_setname_np(pthread_self(), "FFDec");
    AXContext *ctx = (AXContext *)argv;
    ctx->ffDecoder->Decode(FrameProcessCallBackFunc, ctx);
    return nullptr;
}

void test_sort(AXContext *ctx,ImageData *imageData ,std::vector<detection::Object>& results) {
    
    TIME_START(test_sort);

    vector<TrackingBox> detFrameData;
    // vector<Bbox> bboxes = det_results[frame_id];
    for (int i = 0; i <  static_cast<int>(results.size()); ++i)
    {
        if(results[i].label == 1) //person 
        {
            TrackingBox cur_box;
            cur_box.box = results[i].rect;
            cur_box.frame_id = ctx->frame_id;
            detFrameData.push_back(cur_box);
        }

    }
    ctx->frame_id++;
    // 跟踪 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    LOG_INFO("frame_id: {}, det num: {}", ctx->frame_id, detFrameData.size());
    ctx->sort_tracker->update(detFrameData);
    vector<TrackingBox> tracking_results = ctx->sort_tracker->getReport();
    LOG_INFO("tracker out: {}", tracking_results.size());
	
    TIME_END(test_sort);
	TIME_USEC_SHOW(test_sort);

// void DrawText(AX_VIDEO_FRAME_INFO_T *frame_info, int x, int y, const std::string &text, const YUVColor &color);

// void DrawRect(AX_VIDEO_FRAME_INFO_T *frame_info, int x1, int y1, int x2, int y2, const YUVColor &color, int lineWidth);

    for (size_t i = 0; i < tracking_results.size(); i++)
    {
        /* code */
        auto item = tracking_results[i];

        DrawText(imageData->data->FrameInfo(),item.box.x,item.box.y + 5, std::to_string(item.track_id),{255,255,255});

        DrawRect(imageData->data->FrameInfo(),item.box.x,item.box.y,item.box.x + item.box.width, item.box.y + item.box.height, {255,255,255}, 2);
        

    }
    
	// drawPic(frame, tracking_results, sort_tracker);
   
};

void *InferCallBack(void *argv)
{
    pthread_setname_np(pthread_self(), "Infer");
    AXContext *ctx = (AXContext *)argv;
    while (true)
    {
        auto data = ctx->inferQueue->Pop();
        if (data == nullptr)
        {
            usleep(100);
            continue;
        }
        // 6 ms
        ctx->engine->Process(data->inferData);

        data->inferData.clear();
        // 1.7ms
        ctx->engine->Postprocess(ctx->ffDecoder->GetFrameWidth(),ctx->ffDecoder->GetFrameHeight(),data->objects);

        LOG_DEBUG("---------------------");
        for (size_t i = 0; i < data->objects.size(); i++)
        {
            auto item = data->objects[i];
            LOG_DEBUG("[{}->{}][{},{},{},{}]", item.label, item.prob, item.rect.x, item.rect.y, item.rect.width, item.rect.height);
        }
        
        test_sort(ctx,&data->img,data->objects);


        ctx->venc->Write(&data->img,nullptr);
    }
    return nullptr;
}

void *ReadImageDataCallBack(void *argv)
{
    // std::make_shared
    pthread_setname_np(pthread_self(), "ReadImg");
    AXContext *ctx = (AXContext *)argv;
    while (true)
    {
        auto img = ctx->imageQueue->Pop();
        if (img == nullptr)
        {
            usleep(100);
            continue;
        }
        ImageData src = *img.get();

        ImageData dest;
        // std::shared_ptr<ImageData> dest = std::make_shared<ImageData>();
        if (ctx->ivps->Process(dest, src) != 0)
        {
            LOG_ERROR_LOC("CSC 异常");
            exit(-1);
        }
        std::shared_ptr<InferData> infer = std::make_shared<InferData>();
        infer->img = src;

        Copy2Host(infer->inferData, dest);

        ctx->inferQueue->Push(infer);
        
    }
}

int AX_INIT()
{

    int ret = 0;
    ret = AX_SYS_Init();
    if (AX_SUCCESS != ret)
    {
        // LOG_ERROR_LOC("AX_SYS_Init Failed!! %X\n", ret);
        return ret;
    }

    ret = AX_IVPS_Init();
    if (AX_SUCCESS != ret)
    {
        LOG_ERROR_LOC("AX_IVPS_Init Failed!! {:#x}", ret);
        return ret;
    }
    AX_POOL_Init();
    AX_VDEC_MOD_ATTR_T stModAttr;
    memset(&stModAttr, 0x0, sizeof(AX_VDEC_MOD_ATTR_T));

    stModAttr.enDecModule = AX_ENABLE_BOTH_VDEC_JDEC;
    stModAttr.u32MaxGroupCount = AX_VDEC_MAX_GRP_NUM;

    ret = AX_VDEC_Init(&stModAttr);
    if (AX_SUCCESS != ret)
    {
        // LOG_ERROR_LOC("AX_VDEC_Init Failed!! %X\n", ret);
        // LOG(ERROR) << ""
        return ret;
    }

    AX_VENC_MOD_ATTR_T stEncModAttr;
    memset(&stEncModAttr, 0x0, sizeof(AX_VENC_MOD_ATTR_T));
    stEncModAttr.enVencType = AX_VENC_MULTI_ENCODER;
    stEncModAttr.stModThdAttr.u32TotalThreadNum = 9;
    stEncModAttr.stModThdAttr.bExplicitSched = AX_FALSE;
    ret = AX_VENC_Init(&stEncModAttr);
    if (AX_SUCCESS != ret)
    {
        // LOG_ERROR_LOC("AX_VENC_Init Failed!! %X\n", ret);
        return ret;
    }
    // LOG_INFO("SYS INIT SUCCCESS !!!");
    LOG_INFO("AX SYS Init Success!!");
    return 0;
}

int VdecProcessCallBackFunc(ImageData image,
                            int grp, int chn,
                            void *user_data)
{
    // ImageData image;

    std::shared_ptr<ImageData> data = std::make_shared<ImageData>(image);
    AXContext *ctx = (AXContext *)user_data;
    ctx->imageQueue->Push(data);

    return 0;
}

int VencProcessCallBack_(AX_VENC_STREAM_T streamData,
                         int chn,
                         void *user_data)
{

    AXContext *ctx = (AXContext *)user_data;
    // 这个时间多久
    TIME_START(WritePacket);
    ctx->ffEncoder->WritePacket(streamData.stPack.pu8Addr, streamData.stPack.u32Len);
    TIME_END(WritePacket);

    TIME_USEC_SHOW(WritePacket);
    return 0;
}

int main(int, char **)
{
    // 拉流
    // 解码
    // 提到外面
    // 推理
    // 跟踪
    // 绘制
    // 编码
    // 推流

    // 初始化日志系统
    // Logger::GetInstance().Init("./logs", 10, 5, true);
    LOG_INIT("logs/app.log", spdlog::level::debug);

    AX_INIT();

    ThreadSafeQueue<std::shared_ptr<ImageData>> imageQueue(128);
    ThreadSafeQueue<std::shared_ptr<InferData>> inferQueue(128);

    std::string rtsp = "rtsp://123:123@22.10.54.60:8555/live21";
    FFmpegDecoder ffDecoder(rtsp);

    if (0 != ffDecoder.GetVideoInfo())
    {
        LOG_ERROR_LOC("FFmpeg Decoder init error");
        return -1;
    }

    std::string rtmp = "rtmp://123:123@22.10.57.15/mylive/live";
    FFmpegEncoder ffEncoder(rtmp, 25, ffDecoder.GetFrameWidth(), ffDecoder.GetFrameHeight(), AV_PIX_FMT_NV12, 25, "main");
    if (0 != ffEncoder.Init())
    {
        LOG_ERROR_LOC("FFmpeg Encoder Init failled!");
        return -1;
    }

    VdecHelper vdec(g_resourceID, PT_H264, ffDecoder.GetFrameWidth(), ffDecoder.GetFrameHeight(), ffDecoder.GetFps());
    if (0 != vdec.Init())
    {
        // printf("vdec init failled\n");
        LOG_ERROR_LOC("VDEC Init failled!");
        return -1;
    };

    VencHelper venc(g_resourceID, ffDecoder.GetFrameWidth(), ffDecoder.GetFrameHeight(), 25, 25);
    if (0 != venc.Init())
    {
        LOG_ERROR_LOC("VENC Init failed!");
        return -1;
    };

    IvpsHelper ivps(g_resourceID, ffDecoder.GetFrameWidth() * ffDecoder.GetFrameHeight() * 3, 32);
    // ivps.Resize(AX_IVPS_ASPECT_RATIO_AUTO,640,640);

    if (0 != ivps.ResizeAndCSC(AX_FORMAT_YUV420_SEMIPLANAR, 640, 640))
    {
        LOG_ERROR_LOC("IVPS Init failed!");
        return -1;
    }

    Yolov5 engine("");
    if (0 != engine.Init())
    {
        LOG_ERROR_LOC("Yolov5 Engine Init failed!");
        return -1;
    }
    SORT_TRACKER sort_tracker; 
    AXContext ctx;
    ctx.ffDecoder = &ffDecoder;
    ctx.ffEncoder = &ffEncoder;
    ctx.vdec = &vdec;
    ctx.venc = &venc;
    ctx.imageQueue = &imageQueue;
    ctx.ivps = &ivps;
    ctx.engine = &engine;
    ctx.inferQueue = &inferQueue;
    ctx.sort_tracker = &sort_tracker;

    vdec.Decode(VdecProcessCallBackFunc, &ctx);
    venc.Encode(VencProcessCallBack_, &ctx);

    pthread_t ffmpegThread;
    pthread_t readImgThread;
    pthread_t inferThread;

    pthread_create(&ffmpegThread, nullptr, FFmpegDecodeCallBack, &ctx);
    pthread_create(&readImgThread, nullptr, ReadImageDataCallBack, &ctx);
    pthread_create(&inferThread, nullptr, InferCallBack, &ctx);

    pthread_join(ffmpegThread, nullptr);
    pthread_join(readImgThread, nullptr);
    pthread_join(inferThread, nullptr);
    return 0;
}

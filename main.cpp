
#include "Pipeline.h"
#include "PreProcess.hpp"
#include "InfProccess.hpp"
#include "BusProcess.hpp"
#include "EncProcess.hpp"
#include <memory>
#include "ProcessMsg.h"
int MainThreadProcess(uint32_t msgId,
                      std::shared_ptr<void> msgData, void *userData)
{
    if (msgId == MSG_APP_EXIT)
    {
        Pipeline &app = GetPipelineInstance();
        app.WaitEnd();
    }

    LOG_INFO_LOC("Receive exit message, exit now");

    return 0;
}

void ExitPipeline(Pipeline &app, std::vector<PipelineThreadParam> &threadTbl)
{
    for (size_t i = 0; i < threadTbl.size(); i++)
    {
        // 释放逻辑对象
        delete threadTbl[i].threadInst;
        LOG_INFO("delete threadInst %d", i);
    }

    app.Exit();
    LOG_INFO("app.Exit() ");
}

int main(int argc, char const *argv[])
{

    std::string rtsp = "rtsp://123:123@22.10.54.60:8555/live21";
    FFmpegDecoder ffDecoder(rtsp);
    if (0 != ffDecoder.GetVideoInfo())
    {
        LOG_ERROR_LOC("FFmpeg Decoder init error");
        return -1;
    }

    std::vector<PipelineThreadParam> threadTbl;

    {

        PipelineThreadParam param;
        param.threadInst = new PreProcess(&ffDecoder);
        param.threadInstName.assign("PreProcess");
        threadTbl.push_back(param);
    }

    {
        PipelineThreadParam param;
        param.threadInst = new InfProccess("",&ffDecoder);
        param.threadInstName.assign("InfProccess");
        threadTbl.push_back(param);
    }

    {
        PipelineThreadParam param;
        param.threadInst = new BusProcess();
        param.threadInstName.assign("BusProcess");
        threadTbl.push_back(param);
    }   
    
    
    {
        PipelineThreadParam param;
        param.threadInst = new EncProcess("rtmp://123:123@22.10.57.15/mylive/live",&ffDecoder);
        param.threadInstName.assign("BusProcess");
        threadTbl.push_back(param);
    }    

    Pipeline &app = CreatePipelineInstance();
    int ret = app.Start(threadTbl); // 调用工作业务线程的Init 初始化失败 就进行退出
    if (ret != 0)
    {
        LOG_ERROR_LOC("Start app failed, error {}", ret);
        ExitPipeline(app, threadTbl);
        return -1;
    }

    for (size_t i = 0; i < threadTbl.size(); i++)
    {
        ret = SendMessage(threadTbl[i].threadInstId, MSG_APP_START, nullptr);
        if (ret != 0)
        {
            LOG_ERROR_LOC("Start MSG app failed, error {} {}", threadTbl[i].threadInstId, ret);
        }
    }

    LOG_INFO("Wait Exit App");
    app.Wait(MainThreadProcess, nullptr);

    ExitPipeline(app, threadTbl);
    LOG_INFO("Exit App");
    return 0;
}

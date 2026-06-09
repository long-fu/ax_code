
#include <memory>

#include "Pipeline.h"
#include "PreProcess.hpp"
#include "InfProccess.hpp"
#include "BusProcess.hpp"
#include "EncProcess.hpp"
#include "ProcessMsg.h"
#include "Logger.h"
#include "PipelineResource.h"

// static std::atomic<bool> g_running{true};
// static void SignalHandler(int sig)
// {
//     // NLOG_WARN("signal {} received, shutting down...", sig);
//     g_running = false;
// }

int MainThreadProcess(uint32_t msg_id,
                      std::shared_ptr<void> msg_data, void *user_data)
{
  if (msg_id == kMsgAppExit)
  {
    Pipeline &app = GetPipelineInstance();
    app.WaitEnd();
  }

  LOG_INFO("Receive exit message, exit now");

  return 0;
}

void ExitPipeline(Pipeline &app,
                  std::vector<PipelineThreadParam> &thread_tbl)
{
  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    delete thread_tbl[i].thread_inst;
    LOG_INFO("delete thread_inst %d", i);
  }

  app.Exit();
  LOG_INFO("app.Exit() ");
}

int main(int argc, char const *argv[])
{

    // auto lvl = spdlog::level::from_str(spdlog::level::debug);
    // InitLogger(cfg.log_file, lvl);

  LOG_INIT("logs/app.log", spdlog::level::debug);
    // ── 信号处理（logger 初始化后注册）──────────────────────────────────────
  // std::signal(SIGINT, SignalHandler);
  // std::signal(SIGTERM, SignalHandler);

  PipelineResource aclDev = PipelineResource();
  int ret = aclDev.Init();
  if (ret != 0)
  {
    // ACLLITE_LOG_ERROR("Init app failed");
    // LOG(ERROR) << "Init app failed";
    return -1;
  }

  std::string rtsp = "rtsp://123:123@22.10.54.60:8555/live21";
  FFmpegDecoder ff_decoder(rtsp);
  if (0 != ff_decoder.GetVideoInfo())
  {
    // LOG_ERROR("FFmpeg Decoder init error");
    return -1;
  }

  std::vector<PipelineThreadParam> thread_tbl;

  {
    PipelineThreadParam param;
    param.thread_inst = new PreProcess(&ff_decoder);
    param.thread_inst_name.assign("PreProcess");
    thread_tbl.push_back(param);
  }

  {
    PipelineThreadParam param;
    param.thread_inst = new InfProccess("", &ff_decoder);
    param.thread_inst_name.assign("InfProccess");
    thread_tbl.push_back(param);
  }

  {
    PipelineThreadParam param;
    param.thread_inst = new BusProcess();
    param.thread_inst_name.assign("BusProcess");
    thread_tbl.push_back(param);
  }

  {
    PipelineThreadParam param;
    param.thread_inst = new EncProcess(
        "rtmp://123:123@22.10.57.15/mylive/live", &ff_decoder);
    param.thread_inst_name.assign("EncProcess");
    thread_tbl.push_back(param);
  }

  Pipeline &app = CreatePipelineInstance();
  ret = app.Start(thread_tbl);
  if (ret != 0)
  {
    LOG_ERROR("Start app failed, error {}", ret);
    ExitPipeline(app, thread_tbl);
    return -1;
  }

  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    ret = SendMessage(thread_tbl[i].thread_inst_id, kMsgAppStart, nullptr);
    if (ret != 0)
    {
      LOG_ERROR("Start MSG app failed, error {} {}",
                    thread_tbl[i].thread_inst_id, ret);
    }
  }

  LOG_INFO("Wait Exit App");
  app.Wait(MainThreadProcess, nullptr);

  ExitPipeline(app, thread_tbl);
  LOG_INFO("Exit App");
  LOG_FLUSH();
  LOG_SHUTDOWN();
  return 0;
}

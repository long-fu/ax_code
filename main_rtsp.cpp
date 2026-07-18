
#include <memory>

#include "pipeline.h"
#include "pre_process.hpp"
#include "inf_process.hpp"
#include "bus_process.hpp"
#include "enc_process.hpp"
#include "process_msg.h"
#include "logger.h"
#include "pipeline_resource.h"

static std::atomic<bool> g_running{true};
static void SignalHandler(int sig)
{
    // NLOG_WARN("signal {} received, shutting down...", sig);
    g_running = false;
}

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
  LOG_INFO("ExitPipeline {}", thread_tbl.size());
  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    LOG_INFO("ExitPipeline delete thread_inst {} {}", i,thread_tbl[i].thread_inst->SelfInstanceName());
    delete thread_tbl[i].thread_inst;
    LOG_INFO("ExitPipeline delete thread_inst {}", i);
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
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  PipelineResource aclDev = PipelineResource();
  int ret = aclDev.Init();
  if (ret != 0)
  {
    // ACLLITE_LOG_ERROR("Init app failed");
    // LOG(ERROR) << "Init app failed";
    LOG_ERROR("Init app failed");
    LOG_INFO("Exit App");
    LOG_FLUSH();
    LOG_SHUTDOWN();      
    return -1;
  }

  std::string rtsp = "rtsp://123:123@22.10.57.33:8554/live10";
  FFmpegDecoder ff_decoder(rtsp);
  if (0 != ff_decoder.GetVideoInfo())
  {
    LOG_ERROR("FFmpeg Decoder init error");
    LOG_INFO("Exit App");
    LOG_FLUSH();
    LOG_SHUTDOWN();    
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
  LOG_INFO("Wait Exit App Done!!");
  ExitPipeline(app, thread_tbl);

  LOG_INFO("Exit App");
  LOG_FLUSH();
  LOG_SHUTDOWN();
  return 0;
}


#include <memory>

#include "task_scheduler.h"
#include "pre_process.h"
#include "inf_process.h"
#include "bus_process.h"
#include "enc_process.h"
#include "process_msg.h"
#include "logger.h"
#include "resource.h"

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
    pipeline::TaskScheduler &app = pipeline::GetTaskSchedulerInstance();
    app.SignalWaitEnd();
  }

  LOG_INFO("Receive exit message, exit now");

  return 0;
}

void ExitPipeline(pipeline::TaskScheduler &app,
                  std::vector<pipeline::TaskNodeParam> &thread_tbl)
{
  LOG_INFO("ExitPipeline {}", thread_tbl.size());
  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    LOG_INFO("ExitPipeline delete thread_inst {} {}", i,thread_tbl[i].node->InstanceName());
    delete thread_tbl[i].node;
    LOG_INFO("ExitPipeline delete thread_inst {}", i);
  }

  app.Exit();
  LOG_INFO("app.Exit() ");
}

int main(int argc, char const *argv[])
{

    // auto lvl = spdlog::level::from_str(spdlog::level::debug);
    // InitLogger(cfg.log_file, lvl);

  InitLogger("logs/app.log", spdlog::level::debug);
    // ── 信号处理（logger 初始化后注册）──────────────────────────────────────
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  pipeline::Resource aclDev = pipeline::Resource();
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

  std::vector<pipeline::TaskNodeParam> thread_tbl;

  {
    pipeline::TaskNodeParam param;
    param.node = new PreProcess(&ff_decoder);
    param.node_name.assign("PreProcess");
    thread_tbl.push_back(param);
  }

  {
    pipeline::TaskNodeParam param;
    param.node = new InfProccess("", &ff_decoder);
    param.node_name.assign("InfProccess");
    thread_tbl.push_back(param);
  }

  {
    pipeline::TaskNodeParam param;
    param.node = new BusProcess();
    param.node_name.assign("BusProcess");
    thread_tbl.push_back(param);
  }

  {
    pipeline::TaskNodeParam param;
    param.node = new EncProcess(
        "rtmp://123:123@22.10.57.15/mylive/live", &ff_decoder);
    param.node_name.assign("EncProcess");
    thread_tbl.push_back(param);
  }

  pipeline::TaskScheduler &app = pipeline::CreateTaskSchedulerInstance();
  ret = app.Start(thread_tbl);
  if (ret != 0)
  {
    LOG_ERROR("Start app failed, error {}", ret);
    ExitPipeline(app, thread_tbl);
    return -1;
  }

  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    ret = pipeline::SendMessage(thread_tbl[i].node_id, kMsgAppStart, nullptr);
    if (ret != 0)
    {
      LOG_ERROR("Start MSG app failed, error {} {}",
                    thread_tbl[i].node_id, ret);
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

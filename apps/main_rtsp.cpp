
#include <atomic>
#include <csignal>
#include <memory>
#include <vector>

#include "spdlog/common.h"
#include "task_scheduler.h"
#include "pre_process.h"
#include "inf_process.h"
#include "bus_process.h"
#include "enc_process.h"
#include "process_msg.h"
#include "logger.h"
#include "resource.h"
#include "scene_config.h"

static std::atomic<bool> g_running{true};
static std::atomic<pipeline::TaskScheduler*> g_scheduler{nullptr};

static void SignalHandler(int /*sig*/) {
  g_running = false;
  pipeline::TaskScheduler* app = g_scheduler.load();
  if (app != nullptr) {
    app->SignalWaitEnd();
  }
}

int MainThreadProcess(uint32_t msg_id,
                      std::shared_ptr<void> msg_data, void *user_data)
{
  (void)msg_data;
  (void)user_data;
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

  // 必须先停外部线程：app.Exit() 会销毁 TaskNodeMgr 并把 thread_list_ 元素
  // 置空，而 size 保持不变，于是 SendMessage 的边界检查形同虚设，解码回调
  // 会解引用空指针。StopSources() 同步返回后外部线程已 join。
  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    if (thread_tbl[i].node == nullptr) {
      continue;
    }
    thread_tbl[i].node->StopSources();
  }
  LOG_INFO("ExitPipeline sources stopped");

  // Stop and join worker threads before deleting TaskNode objects they use.
  app.Exit();
  LOG_INFO("app.Exit()");

  for (size_t i = 0; i < thread_tbl.size(); i++)
  {
    if (thread_tbl[i].node == nullptr) {
      continue;
    }
    LOG_INFO("ExitPipeline delete node {} {}", i,
             thread_tbl[i].node->InstanceName());
    delete thread_tbl[i].node;
    thread_tbl[i].node = nullptr;
  }
}

int main(int argc, char const *argv[])
{
  const std::string app_config_path =
      argc > 1 ? argv[1] : plugin::DefaultAppConfigPath();
  std::string scene_config_path;
  std::string config_error;
  if (!plugin::LoadSceneConfigPath(app_config_path, scene_config_path,
                                   config_error)) {
    LOG_ERROR("load scene config path failed: {}", config_error);
    return -1;
  }

  // InitLogger("logs/app.log", spdlog::level::trace);

  pipeline::Resource aclDev = pipeline::Resource();
  int ret = aclDev.Init();
  if (ret != 0)
  {
    LOG_ERROR("Init app failed");
    LOG_INFO("Exit App");
    // LOG_FLUSH();
    // LOG_SHUTDOWN();
    return -1;
  }

  std::string rtsp = "rtsp://admin:ad123456@192.168.137.199:554/Streaming/Channels/101";
  
  FFmpegDecoder ff_decoder(rtsp);
  if (0 != ff_decoder.GetVideoInfo())
  {
    LOG_ERROR("FFmpeg Decoder init error");
    LOG_INFO("Exit App");
    // LOG_FLUSH();
    // LOG_SHUTDOWN();
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
    param.node = new InfProcess("configs/scrfd.yaml", &ff_decoder);
    param.node_name.assign("InfProcess");
    thread_tbl.push_back(param);
  }

  {
    pipeline::TaskNodeParam param;
    param.node = new BusProcess(scene_config_path);
    param.node_name.assign("BusProcess");
    thread_tbl.push_back(param);
  }

  {
    pipeline::TaskNodeParam param;
    param.node = new EncProcess(
        "rtmp://123:123@192.168.8.108/mylive/live", &ff_decoder);
    param.node_name.assign("EncProcess");
    thread_tbl.push_back(param);
  }

  pipeline::TaskScheduler &app = pipeline::CreateTaskSchedulerInstance();
  g_scheduler.store(&app);
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  ret = app.Start(thread_tbl);
  if (ret != 0)
  {
    LOG_ERROR("Start app failed, error {}", ret);
    ExitPipeline(app, thread_tbl);
    g_scheduler.store(nullptr);
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
  g_scheduler.store(nullptr);
  LOG_INFO("Exit App");
  // LOG_FLUSH();
  // LOG_SHUTDOWN();
  return 0;
}

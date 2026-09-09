#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "image_data.h"
#include "logger.h"
#include "process_msg.h"
#include "task_node.h"
#include "task_scheduler.h"

#include "host_services.h"
#include "bus_process_dispatch.h"
#include "scene_config.h"
#include "scene_runtime.h"

class BusProcess : public pipeline::TaskNode {
public:
    explicit BusProcess(std::string scene_config_path)
        : scene_config_path_(std::move(scene_config_path))
    {
    }

    ~BusProcess() { Shutdown(); }

    int Init() override
    {
        next_thread_id_ = pipeline::TaskNodeIdByName("EncProcess");
        if (next_thread_id_ < 0)
        {
            LOG_ERROR("BusProcess: 找不到下游节点 EncProcess");
            return -1;
        }

        const int host_ret = host_.Init();
        if (host_ret != 0)
        {
            Shutdown();
            return host_ret;
        }

        plugin::SceneConfig scene_config;
        std::string config_error;
        if (!plugin::LoadSceneConfig(scene_config_path_, scene_config,
                                     config_error))
        {
            LOG_ERROR("BusProcess: load scene config failed: {}", config_error);
            Shutdown();
            return -1;
        }
        const int load_ret = runtime_.Init(&host_, scene_config);
        if (load_ret != 0)
        {
            LOG_ERROR("BusProcess: scene runtime init failed, ret={}",
                      load_ret);
            Shutdown();
            return load_ret;
        }
        return 0;
    }

    int Process(int msg_id, std::shared_ptr<void> msg_data) override
    {
        int ret = 0;
        switch (msg_id)
        {
        case kMsgAppStart:
            break;
        case kMsgInfprocData:
        {
            if (msg_data == nullptr)
            {
                LOG_ERROR("BusProcess: received null inference message");
                break;
            }
            auto in_data = std::static_pointer_cast<InfData>(msg_data);
            if (in_data == nullptr)
            {
                LOG_ERROR("BusProcess: invalid inference message");
                break;
            }

            bool mapped = false;
            bool pixels_accessible = false;
            if (EnsureMapped(in_data->image) != 0)
            {
                LOG_ERROR(
                    "BusProcess: frame mapping failed, skip scene runtime");
            }
            else
            {
                mapped = true;
                if (in_data->image.data == nullptr ||
                    in_data->image.data->FrameInfo() == nullptr ||
                    in_data->image.data->FrameInfo()
                            ->stVFrame.u64VirAddr[0] == 0)
                {
                    LOG_ERROR(
                        "BusProcess: mapped frame invalid, skip scene runtime");
                }
                else
                {
                    pixels_accessible = true;
                }
            }

            const auto dispatch =
                MakeBusFrameDispatchPlan(true, pixels_accessible);
            const uint64_t frame_seq = ++frame_seq_;
            if (dispatch.run_runtime)
            {
                const auto result = runtime_.Process(
                    in_data->image, in_data->objects, frame_seq);
                ReportResult(result);
            }

            if (mapped)
            {
                Unmap(in_data->image);
            }

            auto out_data = std::make_shared<BusData>();
            out_data->image = in_data->image;
            if (dispatch.forward_count == 1)
            {
                ret = pipeline::SendMessage(next_thread_id_, kMsgBusprocData,
                                            out_data);
            }
            break;
        }
        case kMsgAppExit:
            Shutdown();
            break;
        default:
            break;
        }
        return ret;
    }

private:
    struct FailureStreak
    {
        uint64_t streak = 0;
        uint64_t total = 0;
        std::chrono::steady_clock::time_point started;
        std::chrono::steady_clock::time_point last_log;
        int last_code = 0;
    };

    void ReportFailure(FailureStreak& failure, const char* stage, int code)
    {
        const auto now = std::chrono::steady_clock::now();
        ++failure.total;
        if (failure.streak == 0)
        {
            LOG_ERROR("BusProcess: {} failed ret={}, start skipping frames",
                      stage, code);
            failure.started = now;
            failure.last_log = now;
            failure.last_code = code;
        }
        else if (failure.last_code != code)
        {
            LOG_ERROR(
                "BusProcess: {} failure code changed ret={} -> ret={}, start new streak",
                stage, failure.last_code, code);
            failure.streak = 0;
            failure.started = now;
            failure.last_log = now;
            failure.last_code = code;
        }
        else if (now - failure.last_log >= kFailLogInterval)
        {
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                     now - failure.started)
                                     .count();
            LOG_ERROR(
                "BusProcess: {} continuously failed ret={} for {} seconds, skipped {} frames (total {})",
                stage, failure.last_code, seconds, failure.streak,
                failure.total);
            failure.last_log = now;
        }
        ++failure.streak;
    }

    void ReportRecovered(FailureStreak& failure, const char* stage)
    {
        if (failure.streak == 0)
        {
            return;
        }
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() -
                                 failure.started)
                                 .count();
        LOG_WARN(
            "BusProcess: {} recovered after {} seconds, skipped {} frames (total {})",
            stage, seconds, failure.streak, failure.total);
        failure.streak = 0;
    }

    void ReportResult(const plugin::SceneProcessResult& result)
    {
        if (result.stage == plugin::SceneProcessStage::kPostProcessor)
        {
            ReportFailure(postprocessor_failure_, "postprocessor", result.code);
            return;
        }
        ReportRecovered(postprocessor_failure_, "postprocessor");

        if (result.stage == plugin::SceneProcessStage::kBusinessPlugin)
        {
            ReportFailure(business_failure_, "business plugin", result.code);
            return;
        }
        ReportRecovered(business_failure_, "business plugin");
    }

    void Shutdown()
    {
        if (shutdown_)
        {
            return;
        }
        shutdown_ = true;
        runtime_.Shutdown();
        host_.Shutdown();
    }

    static constexpr auto kFailLogInterval = std::chrono::seconds(5);

    std::string scene_config_path_;
    HostServices host_;
    plugin::SceneRuntime runtime_;
    int next_thread_id_ = -1;
    uint64_t frame_seq_ = 0;
    FailureStreak postprocessor_failure_;
    FailureStreak business_failure_;
    bool shutdown_ = false;
};

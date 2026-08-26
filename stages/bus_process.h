#pragma once

#include <cstdlib>
#include <memory>
#include <string>

#include "image_data.h"
#include "logger.h"
#include "process_msg.h"
#include "task_node.h"
#include "task_scheduler.h"

#include "host_services.h"
#include "plugin_loader.h"

class BusProcess : public pipeline::TaskNode {
public:
    BusProcess() = default;

    ~BusProcess()
    {
        plugins_.Unload();
        host_.Shutdown();
    }

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
            return host_ret;
        }

        const auto names =
            plugin::ParsePluginList(std::getenv("AX_BUS_PLUGINS"));
        std::string dir = plugin::DefaultPluginDir();
        if (const char* env_dir = std::getenv("AX_PLUGIN_DIR"))
        {
            if (env_dir[0] != '\0')
            {
                dir = env_dir;
            }
        }
        const int load_ret = plugins_.Load(&host_, names, dir);
        if (load_ret != 0)
        {
            LOG_ERROR("BusProcess: 加载插件失败");
            plugins_.Unload();
            host_.Shutdown();
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
            auto in_data = std::static_pointer_cast<InfData>(msg_data);

            bool mapped = false;
            if (EnsureMapped(in_data->image) != 0)
            {
                LOG_ERROR("BusProcess EnsureMapped failed, skip draw");
            }
            else if (in_data->image.data == nullptr ||
                     in_data->image.data->FrameInfo() == nullptr ||
                     in_data->image.data->FrameInfo()->stVFrame.u64VirAddr[0] ==
                         0)
            {
                LOG_ERROR("BusProcess mapped frame invalid, skip draw");
                Unmap(in_data->image);
            }
            else
            {
                mapped = true;
            }

            plugins_.OnFrame(in_data->image, in_data->objects);

            if (mapped)
            {
                Unmap(in_data->image);
            }

            auto out_data = std::make_shared<BusData>();
            out_data->image = in_data->image;
            ret = pipeline::SendMessage(next_thread_id_, kMsgBusprocData,
                                        out_data);
            break;
        }
        case kMsgAppExit:
            plugins_.Unload();
            break;
        default:
            break;
        }
        return ret;
    }

private:
    HostServices host_;
    plugin::PluginManager plugins_;
    int next_thread_id_ = -1;
};

#pragma once

#include <cstdio>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include "ivps_helper.h"
#include "logger.h"
#include "task_scheduler.h"
#include "task_node.h"
#include "process_msg.h"
// #include "sort_track.h"
#include "drawing.h"
#include "rule_engine.h"
#include "engine_factory.h"
#include "arcface.h"

class BusProcess : public pipeline::TaskNode
{
public:
    BusProcess()
    {
        ivps_ = new IvpsHelper(1,
                               1920 * 1080 * 3,
                               32);
        ArcfaceConfig config;
        engine_ = std::make_unique<Arcface>(config); 
        if(engine_ != nullptr) {
          if(engine_->Init() != 0) {
            engine_ = nullptr;
          }
        }
    };

    ~BusProcess() {
      delete ivps_;
    };

    int Init() override
    {
        next_thread_id_ = pipeline::TaskNodeIdByName("EncProcess");

        if (0 != ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, AX_FORMAT_RGB888, 112, 112))
        {
            LOG_ERROR("IVPS Init failed!");
            return -2;
        }
        // const std::string model_config = "configs/arcface.yaml";
        if (engine_ == nullptr)
        {
            LOG_ERROR("InfProcess: CreateEngine failed for");
            return -1;
        } 


        // // Load rule engine configuration
        // std::ifstream config_file("config.yaml");
        // if (config_file.is_open()) {
        //   std::stringstream buf;
        //   buf << config_file.rdbuf();
        //   std::string yaml_content = buf.str();

        //   // Extract rules section
        //   size_t pos = yaml_content.find("rules:");
        //   if (pos != std::string::npos) {
        //     size_t first_item = yaml_content.find("- ", pos);
        //     if (first_item != std::string::npos) {
        //       std::string rules_yaml = yaml_content.substr(first_item);
        //       RuleEngine::Instance().Load(rules_yaml);
        //     }
        //   }
        // }

        // return engine_->Init();
        
        return 0;
    }

    int Start() {

      return 0;
    }

    int Process(int msg_id, std::shared_ptr<void> msg_data) override
    {
        int ret = 0;
        switch (msg_id)
        {
        case kMsgAppStart:
            Start();
            break;
        case kMsgInfprocData:
        {
            auto in_data = std::static_pointer_cast<InfData>(msg_data);

            TIME_START(arcface);

            auto img_data = in_data->image;

            auto faces = in_data->objects;
            std::vector<std::vector<float> > feats;
            engine_->InferBatch(*ivps_, img_data, faces, feats);
            for (auto& item : feats)
            {
                // printf(const char *__restrict  _Nonnull format, ...)
                LOG_INFO("feats size {}", item.size());
            }

            TIME_END(arcface);
            TIME_USEC_SHOW(arcface);

            // TIME_START(test_sort);
            // std::vector<TrackingBox> det_frame_data;
            // for (size_t i = 0; i < in_data->objects.size(); i++)
            // {
            //   auto &item = in_data->objects[i];
            //   TrackingBox cur_box;
            //   if (item.label == 1)
            //   {
            //     cur_box.box = item.rect;
            //     cur_box.frame_id = frame_id_;
            //     det_frame_data.push_back(cur_box);
            //   }
            // }
            // frame_id_++;
            // tracker_.Update(det_frame_data);
            // std::vector<TrackingBox> tracking_results = tracker_.GetReport();
            // LOG_INFO("tracker out: {}", tracking_results.size());
            // TIME_END(test_sort);
            // TIME_USEC_SHOW(test_sort);

            // Rule engine judgment — evaluate all objects against loaded rules
            // std::vector<bool> rule_results;
            // RuleEngine::Instance().ProcessBoxes(in_data->objects, rule_results);

            // TIME_START(test_draw);

            if (Map(in_data->image) != 0)
            {
                LOG_ERROR("BusProcess Map failed, skip draw");
            }
            else if (in_data->image.data == nullptr || in_data->image.data->FrameInfo() == nullptr || in_data->image.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
            {
                LOG_ERROR("BusProcess mapped frame invalid, skip draw");
                Unmap(in_data->image);
            }
            else
            {
                // LOG_INFO("BusProcess draw: width={} height={} format={} size={}",
                //          in_data->image.width, in_data->image.height,
                //          (int)in_data->image.img_format,
                //          (int)in_data->objects.size());
                for (size_t i = 0; i < in_data->objects.size(); i++)
                {
                    auto item = in_data->objects[i];
                    std::string txt = std::to_string(item.label) + " " + std::to_string(item.prob);
                    DrawText(in_data->image.data->FrameInfo(), item.rect.x, item.rect.y + 5, txt, YUVColors::kRed);
                    DrawRect(in_data->image.data->FrameInfo(),
                             static_cast<int>(item.rect.x),
                             static_cast<int>(item.rect.y),
                             static_cast<int>(item.rect.x + item.rect.width),
                             static_cast<int>(item.rect.y + item.rect.height),
                             YUVColors::kRed, 2);
                    // LOG_INFO("BusProcess draw: label={} prob={} rect={} {} {} {}",
                    //  item.label, item.prob, item.rect.x, item.rect.y,
                    //  item.rect.width, item.rect.height);
                }
                Unmap(in_data->image);
            }

            // TIME_END(test_draw);
            // TIME_USEC_SHOW(test_draw);
            // Func test_draw cost : 663 us

            auto out_data = std::make_shared<BusData>();
            out_data->image = in_data->image;
            ret = pipeline::SendMessage(next_thread_id_, kMsgBusprocData, out_data);

            break;
        }
        case kMsgAppExit:
            // RuleEngine::Instance().Unload();
            break;
        default:
            break;
        }
        return ret;
    }

private:
    // SortTracker tracker_;
    IvpsHelper* ivps_ = nullptr;
    std::unique_ptr<Arcface> engine_;
    // uint64_t frame_id_ = 0;
    int next_thread_id_ = -1;
};

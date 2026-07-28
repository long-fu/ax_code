#pragma once

#include <memory>
#include <fstream>
#include <sstream>

#include "pipeline.h"
#include "pipeline_thread.h"
#include "process_msg.h"
#include "sort_track.h"
#include "drawing.h"
#include "rule_engine.hpp"

class BusProcess : public PipelineThread
{
public:
  BusProcess() = default;
  ~BusProcess() = default;

  int Init() override
  {
    next_thread_id_ = GetPipelineThreadIdByName("EncProcess");

    // Load rule engine configuration
    std::ifstream config_file("config.yaml");
    if (config_file.is_open()) {
      std::stringstream buf;
      buf << config_file.rdbuf();
      std::string yaml_content = buf.str();

      // Extract rules section
      size_t pos = yaml_content.find("rules:");
      if (pos != std::string::npos) {
        size_t first_item = yaml_content.find("- ", pos);
        if (first_item != std::string::npos) {
          std::string rules_yaml = yaml_content.substr(first_item);
          RuleEngine::Instance().Load(rules_yaml);
        }
      }
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
      TIME_START(test_sort);

      auto in_data = std::static_pointer_cast<InfData>(msg_data);
      std::vector<TrackingBox> det_frame_data;
      for (size_t i = 0; i < in_data->objects.size(); i++)
      {
        auto &item = in_data->objects[i];
        TrackingBox cur_box;
        if (item.label == 1)
        {
          cur_box.box = item.rect;
          cur_box.frame_id = frame_id_;
          det_frame_data.push_back(cur_box);
        }
      }
      frame_id_++;
      tracker_.Update(det_frame_data);

      std::vector<TrackingBox> tracking_results = tracker_.GetReport();
      // LOG_INFO("tracker out: {}", tracking_results.size());

      TIME_END(test_sort);
      // TIME_USEC_SHOW(test_sort);

      // Rule engine judgment — evaluate all objects against loaded rules
      std::vector<bool> rule_results;
      RuleEngine::Instance().ProcessBoxes(in_data->objects, rule_results);

      TIME_START(test_draw);
      Map(in_data->image);
      for (size_t i = 0; i < tracking_results.size(); i++)
      {
        auto item = tracking_results[i];
        DrawText(in_data->image.data->FrameInfo(), item.box.x, item.box.y + 5, std::to_string(item.track_id), {255, 255, 255});

        DrawRect(in_data->image.data->FrameInfo(), item.box.x, item.box.y, item.box.x + item.box.width, item.box.y + item.box.height, {255, 255, 255}, 2);
      }
      Unmap(in_data->image);
      TIME_END(test_draw);
      // TIME_USEC_SHOW(test_draw);

      auto out_data = std::make_shared<BusData>();
      out_data->image = in_data->image;
      ret = SendMessage(next_thread_id_, kMsgBusprocData, out_data);
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
  SortTracker tracker_;
  uint64_t frame_id_ = 0;
  int next_thread_id_ = -1;
};

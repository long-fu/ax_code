#pragma once

#include <memory>

#include "Pipeline.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "sort_track.h"

class BusProcess : public PipelineThread {
 public:
  BusProcess() = default;
  ~BusProcess() = default;

  int Init() override {
    m_next_thread_id_ = GetPipelineThreadIdByName("VencThread");
    return 0;
  }

  int Process(int msg_id, std::shared_ptr<void> msg_data) override {
    int ret = 0;
    switch (msg_id) {
      case kMsgAppStart:
        break;
      case kMsgInfprocData: {
        auto in_data = std::static_pointer_cast<InfData>(msg_data);
        std::vector<TrackingBox> det_frame_data;
        for (size_t i = 0; i < in_data->objects.size(); i++) {
          auto& item = in_data->objects[i];
          TrackingBox cur_box;
          cur_box.box = item.rect;
          cur_box.frame_id = m_frame_id_;
          det_frame_data.push_back(cur_box);
        }
        m_frame_id_++;
        m_tracker_.update(det_frame_data);

        auto out_data = std::make_shared<BusData>();
        out_data->image = in_data->image;
        ret = SendMessage(m_next_thread_id_, kMsgBusprocData, out_data);
        break;
      }
      case kMsgAppExit:
        break;
      default:
        break;
    }
    return ret;
  }

 private:
  SORT_TRACKER m_tracker_;
  uint64_t m_frame_id_ = 0;
  int m_next_thread_id_ = -1;
};
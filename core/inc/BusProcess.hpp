#pragma once

#include <memory>

#include "Pipeline.h"
#include "PipelineThread.h"
#include "ProcessMsg.h"
#include "sort_track.h"
#include "drawing.h"
class BusProcess : public PipelineThread
{
public:
  BusProcess() = default;
  ~BusProcess() = default;

  int Init() override
  {
    m_next_thread_id_ = GetPipelineThreadIdByName("EncProcess");
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
        // printf("item.label %d\n",item.label);
        TrackingBox cur_box;
        if (item.label == 1)
        {
          cur_box.box = item.rect;
          cur_box.frame_id = m_frame_id_;
          det_frame_data.push_back(cur_box);
        }
      }
      m_frame_id_++;
      m_tracker_.update(det_frame_data);

      vector<TrackingBox> tracking_results = m_tracker_.getReport();
      LOG_INFO("tracker out: {}", tracking_results.size());

      TIME_END(test_sort);
      TIME_USEC_SHOW(test_sort);

      TIME_START(test_draw);
      // 需要主动映射
      Map(in_data->image);
      for (size_t i = 0; i < tracking_results.size(); i++)
      {
        /* code */
        auto item = tracking_results[i];
        DrawText(in_data->image.data->FrameInfo(), item.box.x, item.box.y + 5, std::to_string(item.track_id), {255, 255, 255});

        DrawRect(in_data->image.data->FrameInfo(), item.box.x, item.box.y, item.box.x + item.box.width, item.box.y + item.box.height, {255, 255, 255}, 2);
      }
      Unmap(in_data->image);
      TIME_END(test_draw);
      TIME_USEC_SHOW(test_draw);

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
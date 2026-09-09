#include "tracker_processor.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int kInvalidConfig = -2;
constexpr float kMinimumAssignmentIou = 0.1f;

int ConfigError(const std::string& detail)
{
    std::cerr << "TrackerProcessor: invalid configuration: " << detail
              << '\n';
    return kInvalidConfig;
}

bool ValidThreshold(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

bool IsValidLabelName(const std::string& label)
{
    const bool blank = std::all_of(label.begin(), label.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    });
    size_t numeric_start = 0;
    if (!label.empty() && (label.front() == '+' || label.front() == '-'))
    {
        numeric_start = 1;
    }
    const bool numeric = numeric_start < label.size() &&
                         std::all_of(label.begin() + numeric_start,
                                     label.end(), [](char value) {
                                         return std::isdigit(
                                                    static_cast<unsigned char>(
                                                        value)) != 0;
                                     });
    return !label.empty() && !blank && !numeric;
}

float IntersectionOverUnion(const cv::Rect_<float>& detection,
                            const std::vector<float>& track_tlbr)
{
    if (track_tlbr.size() != 4)
    {
        return 0.0f;
    }
    const float left = std::max(detection.x, track_tlbr[0]);
    const float top = std::max(detection.y, track_tlbr[1]);
    const float right =
        std::min(detection.x + detection.width, track_tlbr[2]);
    const float bottom =
        std::min(detection.y + detection.height, track_tlbr[3]);
    const float intersection =
        std::max(0.0f, right - left) * std::max(0.0f, bottom - top);
    const float track_area = std::max(0.0f, track_tlbr[2] - track_tlbr[0]) *
                             std::max(0.0f, track_tlbr[3] - track_tlbr[1]);
    const float union_area = detection.area() + track_area - intersection;
    return union_area > 0.0f ? intersection / union_area : 0.0f;
}

} // namespace

const char* TrackerProcessor::Name() const
{
    return "tracker";
}

uint32_t TrackerProcessor::ApiVersion() const
{
    return kPostProcessorApiVersion;
}

int TrackerProcessor::Init(const plugin::ComponentConfig& config)
{
    Shutdown();
    try
    {
        const YAML::Node params = YAML::Load(config.params_yaml);
        if (!params.IsMap())
        {
            return ConfigError("params must be a map");
        }
        if (!params["algorithm"] ||
            params["algorithm"].as<std::string>() != "bytetrack")
        {
            return ConfigError("algorithm must be 'bytetrack'");
        }
        const YAML::Node labels = params["track_labels"];
        if (!labels || !labels.IsSequence() || labels.size() == 0)
        {
            return ConfigError("track_labels must be a nonempty sequence");
        }

        for (const auto& label_node : labels)
        {
            const std::string label = label_node.as<std::string>();
            if (!IsValidLabelName(label))
            {
                return ConfigError(
                    "track_labels must contain nonempty label names, not numeric IDs");
            }
            track_labels_.insert(label);
        }

        BYTETrackerConfig tracker_config;
        if (params["frame_rate"])
        {
            tracker_config.frame_rate = params["frame_rate"].as<int>();
        }
        if (params["track_buffer"])
        {
            tracker_config.track_buffer = params["track_buffer"].as<int>();
        }
        if (params["track_thresh"])
        {
            tracker_config.track_thresh = params["track_thresh"].as<float>();
        }
        if (params["high_thresh"])
        {
            tracker_config.high_thresh = params["high_thresh"].as<float>();
        }
        if (params["match_thresh"])
        {
            tracker_config.match_thresh = params["match_thresh"].as<float>();
        }

        if (tracker_config.frame_rate <= 0)
        {
            return ConfigError("frame_rate must be positive");
        }
        if (tracker_config.track_buffer <= 0)
        {
            return ConfigError("track_buffer must be positive");
        }
        if (!ValidThreshold(tracker_config.track_thresh))
        {
            return ConfigError("track_thresh must be in [0, 1]");
        }
        if (!ValidThreshold(tracker_config.high_thresh))
        {
            return ConfigError("high_thresh must be in [0, 1]");
        }
        if (!ValidThreshold(tracker_config.match_thresh))
        {
            return ConfigError("match_thresh must be in [0, 1]");
        }

        for (const auto& label : track_labels_)
        {
            trackers_.emplace(label,
                              std::make_unique<BYTETracker>(tracker_config));
        }
        next_track_id_ = 1;
        return 0;
    }
    catch (const YAML::Exception& error)
    {
        Shutdown();
        return ConfigError(error.what());
    }
}

int TrackerProcessor::Process(plugin::PostProcessContext& context)
{
    for (auto& object : context.objects)
    {
        object.track_id = -1;
    }

    std::unordered_map<std::string, std::vector<size_t>> grouped_indices;
    for (size_t index = 0; index < context.objects.size(); ++index)
    {
        const std::string& label = context.objects[index].label_name;
        if (track_labels_.count(label) != 0)
        {
            grouped_indices[label].push_back(index);
        }
    }

    for (const auto& label : track_labels_)
    {
        std::vector<detection::Object> detections;
        const auto grouped = grouped_indices.find(label);
        if (grouped != grouped_indices.end())
        {
            detections.reserve(grouped->second.size());
            for (const size_t index : grouped->second)
            {
                detections.push_back(context.objects[index]);
            }
        }

        const auto tracks = trackers_.at(label)->update(detections,
                                                        next_track_id_);
        std::vector<bool> assigned(detections.size(), false);
        for (const auto& track : tracks)
        {
            float best_iou = kMinimumAssignmentIou;
            size_t best_detection = std::numeric_limits<size_t>::max();
            for (size_t index = 0; index < detections.size(); ++index)
            {
                if (assigned[index])
                {
                    continue;
                }
                const float iou =
                    IntersectionOverUnion(detections[index].rect, track.tlbr);
                if (iou >= best_iou)
                {
                    best_iou = iou;
                    best_detection = index;
                }
            }
            if (best_detection != std::numeric_limits<size_t>::max())
            {
                const size_t original_index = grouped->second[best_detection];
                context.objects[original_index].track_id = track.track_id;
                assigned[best_detection] = true;
            }
        }
    }
    return 0;
}

void TrackerProcessor::Shutdown()
{
    trackers_.clear();
    track_labels_.clear();
    next_track_id_ = 1;
}

extern "C" PostProcessor* CreatePostProcessor()
{
    return new TrackerProcessor();
}

extern "C" void DestroyPostProcessor(PostProcessor* processor)
{
    delete processor;
}

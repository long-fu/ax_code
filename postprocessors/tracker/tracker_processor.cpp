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

bool IsBaseDigit(char value, int base)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0' < base;
    }
    const char lower = static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
    return base == 16 && lower >= 'a' && lower <= 'f';
}

bool IsDigitSequence(const std::string& value, size_t start, int base)
{
    bool has_digit = false;
    for (size_t index = start; index < value.size(); ++index)
    {
        if (value[index] == '_')
        {
            continue;
        }
        if (!IsBaseDigit(value[index], base))
        {
            return false;
        }
        has_digit = true;
    }
    return has_digit;
}

bool IsYamlNumeric(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](char item) {
        return static_cast<char>(
            std::tolower(static_cast<unsigned char>(item)));
    });
    if (lower == ".inf" || lower == "+.inf" || lower == "-.inf" ||
        lower == ".nan" || lower == "+.nan" || lower == "-.nan")
    {
        return true;
    }

    size_t index = 0;
    if (value[index] == '+' || value[index] == '-')
    {
        ++index;
    }
    if (index >= value.size())
    {
        return false;
    }
    if (index + 2 <= value.size() && value[index] == '0')
    {
        const char prefix = static_cast<char>(
            std::tolower(static_cast<unsigned char>(value[index + 1])));
        if (prefix == 'b' || prefix == 'o' || prefix == 'x')
        {
            const int base = prefix == 'b' ? 2 : (prefix == 'o' ? 8 : 16);
            return IsDigitSequence(value, index + 2, base);
        }
    }

    bool mantissa_digit = false;
    while (index < value.size() &&
           (IsBaseDigit(value[index], 10) || value[index] == '_'))
    {
        mantissa_digit = mantissa_digit || value[index] != '_';
        ++index;
    }
    if (index < value.size() && value[index] == '.')
    {
        ++index;
        while (index < value.size() &&
               (IsBaseDigit(value[index], 10) || value[index] == '_'))
        {
            mantissa_digit = mantissa_digit || value[index] != '_';
            ++index;
        }
    }
    if (!mantissa_digit)
    {
        return false;
    }
    if (index < value.size() &&
        (value[index] == 'e' || value[index] == 'E'))
    {
        ++index;
        if (index < value.size() &&
            (value[index] == '+' || value[index] == '-'))
        {
            ++index;
        }
        const size_t exponent_start = index;
        if (!IsDigitSequence(value, exponent_start, 10))
        {
            return false;
        }
        index = value.size();
    }
    return index == value.size();
}

bool IsValidLabelName(const std::string& label)
{
    const auto first = std::find_if_not(label.begin(), label.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    });
    const auto last = std::find_if_not(label.rbegin(), label.rend(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    }).base();
    if (first == last)
    {
        return false;
    }
    const std::string trimmed(first, last);
    return !IsYamlNumeric(trimmed);
}

template <typename T>
bool ReadOptional(const YAML::Node& params, const char* field, T& value)
{
    const YAML::Node node = params[field];
    if (!node)
    {
        return true;
    }
    try
    {
        value = node.as<T>();
        return true;
    }
    catch (const YAML::Exception& error)
    {
        ConfigError(std::string(field) + ": " + error.what());
        return false;
    }
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

        const std::unordered_set<std::string> known_fields = {
            "algorithm",   "track_labels", "frame_rate", "track_buffer",
            "track_thresh", "high_thresh", "match_thresh"};
        std::unordered_set<std::string> seen_fields;
        for (const auto& entry : params)
        {
            std::string field;
            try
            {
                field = entry.first.as<std::string>();
            }
            catch (const YAML::Exception& error)
            {
                return ConfigError(std::string("parameter key: ") + error.what());
            }
            if (!seen_fields.insert(field).second)
            {
                return ConfigError("duplicate key '" + field + "'");
            }
            if (known_fields.count(field) == 0)
            {
                std::cerr << "TrackerProcessor: warning: unknown tracker parameter '"
                          << field << "'\n";
            }
        }

        std::string algorithm;
        if (!params["algorithm"])
        {
            return ConfigError("algorithm must be 'bytetrack'");
        }
        try
        {
            algorithm = params["algorithm"].as<std::string>();
        }
        catch (const YAML::Exception& error)
        {
            return ConfigError(std::string("algorithm: ") + error.what());
        }
        if (algorithm != "bytetrack")
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
            std::string label;
            try
            {
                label = label_node.as<std::string>();
            }
            catch (const YAML::Exception& error)
            {
                return ConfigError(std::string("track_labels item: ") +
                                   error.what());
            }
            if (!IsValidLabelName(label))
            {
                return ConfigError(
                    "track_labels must contain nonempty label names, not numeric IDs");
            }
            track_labels_.insert(label);
        }

        BYTETrackerConfig tracker_config;
        if (!ReadOptional(params, "frame_rate", tracker_config.frame_rate) ||
            !ReadOptional(params, "track_buffer", tracker_config.track_buffer) ||
            !ReadOptional(params, "track_thresh", tracker_config.track_thresh) ||
            !ReadOptional(params, "high_thresh", tracker_config.high_thresh) ||
            !ReadOptional(params, "match_thresh", tracker_config.match_thresh))
        {
            Shutdown();
            return kInvalidConfig;
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

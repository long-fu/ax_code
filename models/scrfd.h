#pragma once

#include <string>
#include <vector>

#include "engine.h"

// void Det10g::ApplyDefaults() {
//   config_.model_file = "model/det_10g.axmodel";
//   config_.model_type = "det_10g";
//   config_.inputs = {1, 3, 640, 640};
//   config_.prob_threshold = 0.5f;
//   config_.nms_threshold = 0.4f;
//   config_.strides = {8, 16, 32};
//   config_.labels = {"face"};
//   config_.num_anchors = {2, 2, 2};
//   config_.anchors.clear();
// }

struct ScrfdConfig : public EngineConfig
{
    std::string config_path;
    std::string model_file = "model/det_10g.axmodel";
    std::string model_type = "scrfd";
    std::vector<int> inputs = {1, 3, 640, 640};
    float prob_threshold = 0.6f;
    float nms_threshold = 0.4f;
    std::vector<int> num_anchors = {2, 2, 2};
    std::vector<int> strides = {8, 16, 32};
    std::vector<std::string> labels = {"face"};
    std::string ModelFile() const override
    {
        return model_file;
    }
};

class Scrfd : public Engine
{
public:
    explicit Scrfd(const ScrfdConfig& config)
        : Engine(config), config_(config){

                          };
    ~Scrfd() override;

    int Postprocess(int pic_width, int pic_height,
                    std::vector<detection::Object>& objects) override;

    Scrfd(const Scrfd&) = delete;
    Scrfd& operator=(const Scrfd&) = delete;

private:
    struct CenterKey
    {
        int h, w, stride;
        bool operator==(const CenterKey& o) const
        {
            return h == o.h && w == o.w && stride == o.stride;
        }
    };
    struct CenterKeyHash
    {
        size_t operator()(const CenterKey& k) const
        {
            return (static_cast<size_t>(k.h) * 1315423911u) ^ (static_cast<size_t>(k.w) << 1) ^ static_cast<size_t>(k.stride);
        }
    };
    ScrfdConfig config_;
    std::unordered_map<CenterKey, std::vector<float>, CenterKeyHash> centerCache_;
};

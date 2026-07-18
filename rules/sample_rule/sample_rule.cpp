// rules/sample_rule/sample_rule.cpp
#include "BoxRule.hpp"
#include "Logger.h"
#include <cmath>
#include <utility>
#include <stdexcept>

static bool pointInPolygon(const std::vector<std::pair<float, float>>& poly, float x, float y) {
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        float dy = poly[j].second - poly[i].second;
        // Skip horizontal edges (dy == 0) explicitly to avoid division by zero
        if (dy == 0.0f) continue;

        if (((poly[i].second > y) != (poly[j].second > y)) &&
            (x < (poly[j].first - poly[i].first) * (y - poly[i].second) / dy
                     + poly[i].first)) {
            inside = !inside;
        }
    }
    return inside;
}

class SampleRule : public BoxRule {
public:
    int Init(const std::string& config) override {
        // Clear any previous state to support re-init
        region_.clear();

        // Parse region: [[100,100],[300,100],[300,300],[100,300]]
        size_t pos = 0;
        while ((pos = config.find("[[", pos)) != std::string::npos) {
            size_t end = config.find("]]", pos + 2);
            if (end == std::string::npos) break;
            std::string pair_str = config.substr(pos + 2, end - pos - 2);
            size_t comma = pair_str.find(",");
            if (comma != std::string::npos) {
                try {
                    float x = std::stof(pair_str.substr(0, comma));
                    float y = std::stof(pair_str.substr(comma + 1));
                    region_.emplace_back(x, y);
                } catch (const std::exception& e) {
                    LOG_ERROR("SampleRule failed to parse coord: {}", e.what());
                    return -1;
                }
            }
            pos = end + 2;
        }
        LOG_INFO("SampleRule initialized with {} region points", region_.size());
        return 0;
    }

    int Process(const std::vector<DetectionObject>& objects,
                std::vector<bool>& results) override {
        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& obj = objects[i];
            float cx = obj.rect.x + obj.rect.width / 2.0f;
            float cy = obj.rect.y + obj.rect.height / 2.0f;
            results[i] = pointInPolygon(region_, cx, cy);
        }
        return 0;
    }

    int Destroy() override {
        region_.clear();
        return 0;
    }

private:
    std::vector<std::pair<float, float>> region_;
};

REGISTER_RULE(SampleRule)

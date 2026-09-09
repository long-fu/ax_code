#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "business_plugin.h"
#include "detection_types.h"
#include "host_services.h"
#include "image_data.h"

using Json = nlohmann::json;

class FacePlugin : public BusinessPlugin {
public:
    FacePlugin() = default;
    ~FacePlugin() override;

    const char* Name() const override;
    uint32_t ApiVersion() const override;
    int Init(HostServices* host, const PluginConfig& cfg) override;
    int OnFrame(const ImageData& frame,
                const std::vector<detection::Object>& objects) override;
    void Shutdown() override;

private:
    struct TrackPending {
        detection::Object last_face;
        ImageData last_frame;
        bool feat_done = false;
        bool identify_inflight = false;
        int last_seen_frame = 0;
    };

    struct IdentifyItem {
        int track_id = -1;
        detection::Object face;
        std::vector<float> feat;
        ImageData face_img;
    };

    struct IdentifyResult {
        int track_id = -1;
        bool ok = false;
    };

    void PostIdentifyResult(int track_id, bool ok);
    void DrainIdentifyResults();
    void RollbackInflight(const std::vector<int>& track_ids);
    void RunIdentifyAndPush(std::vector<IdentifyItem> batch, ImageData frame,
                            const std::string& capture_msg_id,
                            const std::string& cur_time,
                            std::int64_t cur_created_at);

    static std::string PayloadString(const Json& payload, const char* key,
                                     const std::string& def);
    static Json MakeStrangerBoxJson(const detection::Object& face);
    static std::string MakeStrangerAlertDescription(
        int track_id, const detection::Object& face);
    static std::string MakeStrangerAlertDescription(
        const std::vector<std::pair<int, detection::Object>>& strangers);

    HostServices* host_ = nullptr;
    std::string collection_ = "face_embeddings";
    float frontal_score_thresh_ = 0.55f;

    std::unordered_map<int, TrackPending> track_pending_;
    int frame_seq_ = 0;

    std::mutex identify_results_mutex_;
    std::vector<IdentifyResult> identify_results_;

    static constexpr int kTrackPruneFrames = 60;
    static constexpr int kTrackInflightMaxFrames = 1500;
};

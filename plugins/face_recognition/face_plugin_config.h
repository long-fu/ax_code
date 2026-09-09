#pragma once

#include <string>

struct FacePluginConfig {
    float frontal_score_thresh = 0.55f;
    std::string qdrant_collection = "face_embeddings";
};

bool ParseFacePluginConfig(const std::string& params_yaml,
                           FacePluginConfig& config, std::string& error);

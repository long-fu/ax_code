#include "engine_factory.h"

#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "logger.h"
#include "scrfd.h"
#include "yolov5.h"

namespace engine_factory {
namespace {

template <typename T>
bool ReadOptional(const YAML::Node& node, const char* key, T& out) {
  if (!node[key]) {
    return false;
  }
  out = node[key].as<T>();
  return true;
}

bool ReadIntVector(const YAML::Node& node, const char* key,
                   std::vector<int>& out) {
  if (!node[key] || !node[key].IsSequence()) {
    return false;
  }
  out.clear();
  for (const auto& item : node[key]) {
    out.push_back(item.as<int>());
  }
  return true;
}

bool ReadStringVector(const YAML::Node& node, const char* key,
                      std::vector<std::string>& out) {
  if (!node[key] || !node[key].IsSequence()) {
    return false;
  }
  out.clear();
  for (const auto& item : node[key]) {
    out.push_back(item.as<std::string>());
  }
  return true;
}

bool ReadAnchors(const YAML::Node& node,
                 std::vector<std::vector<float>>& out) {
  if (!node["anchors"] || !node["anchors"].IsSequence()) {
    return false;
  }
  out.clear();
  for (const auto& pair : node["anchors"]) {
    if (!pair.IsSequence() || pair.size() != 2) {
      return false;
    }
    out.push_back({pair[0].as<float>(), pair[1].as<float>()});
  }
  return !out.empty();
}

template <typename ConfigT>
void ApplyCommon(ConfigT& cfg, const YAML::Node& root,
                 const std::string& path) {
  cfg.config_path = path;
  cfg.model_file = root["model_file"].as<std::string>();
  cfg.model_type = root["model_type"].as<std::string>();

  ReadIntVector(root, "inputs", cfg.inputs);
  ReadOptional(root, "prob_threshold", cfg.prob_threshold);
  ReadOptional(root, "nms_threshold", cfg.nms_threshold);
  ReadIntVector(root, "strides", cfg.strides);
  ReadIntVector(root, "num_anchors", cfg.num_anchors);
  ReadStringVector(root, "labels", cfg.labels);
}

}  // namespace

std::unique_ptr<Engine> CreateEngine(const std::string& config_path) {
  if (config_path.empty()) {
    LOG_ERROR("engine_factory: empty config path");
    return nullptr;
  }

  YAML::Node root;
  try {
    root = YAML::LoadFile(config_path);
  } catch (const YAML::Exception& e) {
    LOG_ERROR("engine_factory: load {} failed: {}", config_path, e.what());
    return nullptr;
  }

  if (!root || !root.IsMap()) {
    LOG_ERROR("engine_factory: root is not a map in {}", config_path);
    return nullptr;
  }
  if (!root["model_type"] || !root["model_file"]) {
    LOG_ERROR("engine_factory: model_type/model_file required in {}",
              config_path);
    return nullptr;
  }

  const std::string model_type = root["model_type"].as<std::string>();

  try {
    if (model_type == "yolov5") {
      Yolov5Config cfg;
      ApplyCommon(cfg, root, config_path);
      ReadAnchors(root, cfg.anchors);
      LOG_INFO("engine_factory: create Yolov5 from {} model_file={}",
               config_path, cfg.model_file);
      return std::make_unique<Yolov5>(cfg);
    }

    if (model_type == "scrfd") {
      ScrfdConfig cfg;
      ApplyCommon(cfg, root, config_path);
      LOG_INFO("engine_factory: create Scrfd from {} model_file={}",
               config_path, cfg.model_file);
      return std::make_unique<Scrfd>(cfg);
    }
  } catch (const YAML::Exception& e) {
    LOG_ERROR("engine_factory: parse {} failed: {}", config_path, e.what());
    return nullptr;
  }

  LOG_ERROR("engine_factory: unknown model_type '{}' in {}", model_type,
            config_path);
  return nullptr;
}

}  // namespace engine_factory

#pragma once

#include <memory>
#include <string>

#include "engine.h"

namespace engine_factory {

// Load YAML config and construct Yolov5 / Scrfd by model_type.
// Returns nullptr on failure (file missing, parse error, unknown type).
std::unique_ptr<Engine> CreateEngine(const std::string& config_path);

}  // namespace engine_factory

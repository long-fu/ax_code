#include "face_plugin_config.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace
{

template <typename T>
bool ReadOptional(const YAML::Node& params, const char* field, T& value,
                  std::string& error)
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
    catch (const YAML::Exception& exception)
    {
        error = std::string(field) + ": " + exception.what();
        return false;
    }
}

}  // namespace

bool ParseFacePluginConfig(const std::string& params_yaml,
                           FacePluginConfig& config, std::string& error)
{
    config = FacePluginConfig{};
    error.clear();
    try
    {
        const YAML::Node params = YAML::Load(params_yaml);
        if (!params || params.IsNull())
        {
            return true;
        }
        if (!params.IsMap())
        {
            error = "params_yaml: expected a map";
            return false;
        }
        if (!ReadOptional(params, "frontal_score_thresh",
                          config.frontal_score_thresh, error))
        {
            return false;
        }
        if (!(config.frontal_score_thresh >= 0.0f &&
              config.frontal_score_thresh <= 1.0f))
        {
            error = "frontal_score_thresh: expected a value in [0, 1]";
            return false;
        }
        if (!ReadOptional(params, "qdrant_collection",
                          config.qdrant_collection, error))
        {
            return false;
        }
        if (config.qdrant_collection.empty())
        {
            error = "qdrant_collection: must not be empty";
            return false;
        }
        return true;
    }
    catch (const YAML::Exception& exception)
    {
        error = std::string("params_yaml: ") + exception.what();
        return false;
    }
}

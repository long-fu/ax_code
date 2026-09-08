#include "scene_config.h"

#include <yaml-cpp/yaml.h>

#include <linux/limits.h>
#include <unistd.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>

namespace plugin
{
    namespace
    {

        namespace fs = std::filesystem;

        bool Fail(const std::string& yaml_path,
                  const std::string& field,
                  const std::string& detail,
                  std::string& error)
        {
            error = yaml_path + ": field '" + field + "' " + detail;
            return false;
        }

        fs::path ResolvePath(const fs::path& base, const std::string& path)
        {
            return fs::absolute(base / fs::path(path)).lexically_normal();
        }

        void WarnUnknownFields(const YAML::Node& node,
                               const std::unordered_set<std::string>& allowed,
                               const std::string& yaml_path,
                               const std::string& field)
        {
            if (!node.IsMap())
            {
                return;
            }
            for (const auto& entry : node)
            {
                if (!entry.first.IsScalar())
                {
                    std::clog << "WARNING: " << yaml_path << ": field '" << field
                              << "' contains a non-string key\n";
                    continue;
                }
                const std::string key = entry.first.Scalar();
                if (allowed.find(key) == allowed.end())
                {
                    std::clog << "WARNING: " << yaml_path << ": unknown field '"
                              << (field.empty() ? key : field + "." + key) << "'\n";
                }
            }
        }

        bool ReadRequiredString(const YAML::Node& node,
                                const std::string& key,
                                const std::string& field,
                                const std::string& yaml_path,
                                std::string& value,
                                std::string& error)
        {
            const YAML::Node item = node[key];
            if (!item.IsDefined() || !item.IsScalar())
            {
                return Fail(yaml_path, field, "must be a non-empty string", error);
            }
            try
            {
                value = item.as<std::string>();
            }
            catch (const YAML::Exception& exception)
            {
                return Fail(yaml_path, field,
                            "must be a non-empty string (" + std::string(exception.what()) + ")",
                            error);
            }
            if (value.empty())
            {
                return Fail(yaml_path, field, "must be a non-empty string", error);
            }
            return true;
        }

        bool ReadApiVersion(const YAML::Node& node,
                            const std::string& field,
                            const std::string& yaml_path,
                            uint32_t& value,
                            std::string& error)
        {
            const YAML::Node item = node["api_version"];
            if (!item.IsDefined() || !item.IsScalar())
            {
                return Fail(yaml_path, field, "must be a positive integer", error);
            }
            try
            {
                value = item.as<uint32_t>();
            }
            catch (const YAML::Exception& exception)
            {
                return Fail(yaml_path, field,
                            "must be a positive integer (" + std::string(exception.what()) + ")",
                            error);
            }
            if (value == 0)
            {
                return Fail(yaml_path, field, "must be a positive integer", error);
            }
            return true;
        }

        bool EmitParams(const YAML::Node& node,
                        const std::string& field,
                        const std::string& yaml_path,
                        std::string& params_yaml,
                        std::string& error)
        {
            const YAML::Node params = node["params"];
            if (!params.IsDefined())
            {
                params_yaml.clear();
                return true;
            }
            if (!params.IsMap())
            {
                return Fail(yaml_path, field, "must be a map", error);
            }
            YAML::Emitter emitter;
            emitter << params;
            if (!emitter.good())
            {
                return Fail(yaml_path, field,
                            "could not be serialized: " + emitter.GetLastError(), error);
            }
            params_yaml = emitter.c_str();
            return true;
        }

        bool ParseComponent(const YAML::Node& node,
                            const fs::path& config_dir,
                            const std::string& field,
                            const std::string& yaml_path,
                            bool allow_disabled,
                            ComponentConfig& config,
                            std::string& error)
        {
            if (!node.IsMap())
            {
                return Fail(yaml_path, field, "must be a map", error);
            }
            WarnUnknownFields(node,
                              {"name", "library", "params", "api_version",
                               "enabled"},
                              yaml_path, field);

            config = ComponentConfig{};
            config.config_dir = config_dir.string();

            const YAML::Node enabled = node["enabled"];
            if (enabled.IsDefined())
            {
                if (!enabled.IsScalar())
                {
                    return Fail(yaml_path, field + ".enabled", "must be a boolean",
                                error);
                }
                try
                {
                    config.enabled = enabled.as<bool>();
                }
                catch (const YAML::Exception& exception)
                {
                    return Fail(yaml_path, field + ".enabled",
                                "must be a boolean (" + std::string(exception.what()) + ")",
                                error);
                }
            }

            const bool required = !allow_disabled || config.enabled;
            const YAML::Node name = node["name"];
            if (required)
            {
                if (!ReadRequiredString(node, "name", field + ".name", yaml_path,
                                        config.name, error))
                {
                    return false;
                }
            }
            else if (name.IsDefined())
            {
                if (!ReadRequiredString(node, "name", field + ".name", yaml_path,
                                        config.name, error))
                {
                    return false;
                }
            }

            const YAML::Node library = node["library"];
            std::string library_path;
            if (required)
            {
                if (!ReadRequiredString(node, "library", field + ".library", yaml_path,
                                        library_path, error))
                {
                    return false;
                }
            }
            else if (library.IsDefined())
            {
                if (!ReadRequiredString(node, "library", field + ".library", yaml_path,
                                        library_path, error))
                {
                    return false;
                }
            }
            if (!library_path.empty())
            {
                config.library_path = ResolvePath(config_dir, library_path).string();
            }

            const YAML::Node api_version = node["api_version"];
            if (required)
            {
                if (!ReadApiVersion(node, field + ".api_version", yaml_path,
                                    config.api_version, error))
                {
                    return false;
                }
            }
            else if (api_version.IsDefined())
            {
                if (!ReadApiVersion(node, field + ".api_version", yaml_path,
                                    config.api_version, error))
                {
                    return false;
                }
            }

            return EmitParams(node, field + ".params", yaml_path, config.params_yaml,
                              error);
        }

        bool LoadYaml(const std::string& yaml_path,
                      YAML::Node& root,
                      std::string& error)
        {
            try
            {
                root = YAML::LoadFile(yaml_path);
            }
            catch (const YAML::Exception& exception)
            {
                error = yaml_path + ": YAML parse failed: " + exception.what();
                return false;
            }
            catch (const std::exception& exception)
            {
                error = yaml_path + ": could not be read: " + exception.what();
                return false;
            }
            if (!root.IsMap())
            {
                return Fail(yaml_path, "root", "must be a map", error);
            }
            return true;
        }

    } // namespace

    std::string DefaultAppConfigPath()
    {
        char executable[PATH_MAX];
        const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
        if (length <= 0)
        {
            return fs::absolute("config.yaml").lexically_normal().string();
        }
        executable[length] = '\0';
        return (fs::path(executable).parent_path() / "config.yaml").string();
    }

    bool LoadSceneConfigPath(const std::string& app_config_path,
                             std::string& scene_config_path,
                             std::string& error)
    {
        scene_config_path.clear();
        error.clear();
        YAML::Node root;
        if (!LoadYaml(app_config_path, root, error))
        {
            return false;
        }

        const YAML::Node pipeline = root["pipeline"];
        if (!pipeline.IsDefined() || !pipeline.IsMap())
        {
            return Fail(app_config_path, "pipeline", "must be a map", error);
        }
        std::string configured_path;
        if (!ReadRequiredString(pipeline, "scene_config", "pipeline.scene_config",
                                app_config_path, configured_path, error))
        {
            return false;
        }

        const fs::path app_path = fs::absolute(app_config_path).lexically_normal();
        scene_config_path = ResolvePath(app_path.parent_path(), configured_path).string();
        return true;
    }

    bool LoadSceneConfig(const std::string& scene_config_path,
                         SceneConfig& config,
                         std::string& error)
    {
        error.clear();
        YAML::Node root;
        if (!LoadYaml(scene_config_path, root, error))
        {
            return false;
        }
        WarnUnknownFields(root, {"version", "scene", "postprocessors"},
                          scene_config_path, "");

        const YAML::Node version = root["version"];
        uint32_t parsed_version = 0;
        if (!version.IsDefined() || !version.IsScalar())
        {
            return Fail(scene_config_path, "version", "must equal 1", error);
        }
        try
        {
            parsed_version = version.as<uint32_t>();
        }
        catch (const YAML::Exception& exception)
        {
            return Fail(scene_config_path, "version",
                        "must equal 1 (" + std::string(exception.what()) + ")",
                        error);
        }
        if (parsed_version != 1)
        {
            return Fail(scene_config_path, "version", "must equal 1", error);
        }

        const fs::path config_path = fs::absolute(scene_config_path).lexically_normal();
        const fs::path config_dir = config_path.parent_path();
        SceneConfig parsed;
        const YAML::Node scene = root["scene"];
        if (!scene.IsDefined() || !ParseComponent(scene, config_dir, "scene", scene_config_path, false, parsed.scene, error))
        {
            if (!scene.IsDefined())
            {
                return Fail(scene_config_path, "scene", "must be a map", error);
            }
            return false;
        }

        const YAML::Node postprocessors = root["postprocessors"];
        if (postprocessors.IsDefined())
        {
            if (!postprocessors.IsSequence())
            {
                return Fail(scene_config_path, "postprocessors", "must be a sequence",
                            error);
            }
            parsed.postprocessors.reserve(postprocessors.size());
            for (std::size_t index = 0; index < postprocessors.size(); ++index)
            {
                ComponentConfig processor;
                const std::string field = "postprocessors[" + std::to_string(index) + "]";
                if (!ParseComponent(postprocessors[index], config_dir, field,
                                    scene_config_path, true, processor, error))
                {
                    return false;
                }
                parsed.postprocessors.push_back(std::move(processor));
            }
        }

        config = std::move(parsed);
        return true;
    }

} // namespace plugin

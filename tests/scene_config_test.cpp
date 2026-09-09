#include "scene_config.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

    class TempDir
    {
    public:
        TempDir()
            : path_(std::filesystem::temp_directory_path() / ("ax_core_scene_config_test_" + std::to_string(getpid())))
        {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            std::filesystem::create_directories(path_);
        }

        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }

        const std::filesystem::path& path() const
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    bool WriteFile(const std::filesystem::path& path, const std::string& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);
        output << contents;
        return output.good();
    }

    std::string Resolved(const std::filesystem::path& path)
    {
        return std::filesystem::absolute(path).lexically_normal().string();
    }

    void Expect(bool condition, const std::string& message, int& failures)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    void ExpectErrorContains(const std::filesystem::path& path,
                             const std::string& yaml,
                             const std::string& field,
                             int& failures)
    {
        Expect(WriteFile(path, yaml), "write invalid config for " + field,
               failures);
        plugin::SceneConfig config;
        std::string error;
        Expect(!plugin::LoadSceneConfig(path.string(), config, error),
               "reject invalid field " + field, failures);
        Expect(error.find(path.string()) != std::string::npos,
               "error includes YAML path for " + field, failures);
        Expect(error.find(field) != std::string::npos,
               "error names field " + field + ": " + error, failures);
    }

} // namespace

int main()
{
    int failures = 0;
    TempDir temp;
    const auto app_config = temp.path() / "app" / "config.yaml";
    const auto scene_config = temp.path() / "scenes" / "face.yaml";

    Expect(WriteFile(app_config,
                     "pipeline:\n"
                     "  scene_config: ../scenes/face.yaml\n"
                     "  rtsp_input: redacted\n"),
           "write app config", failures);

    std::string loaded_scene_path;
    std::string error;
    Expect(plugin::LoadSceneConfigPath(app_config.string(), loaded_scene_path,
                                       error),
           "load pipeline.scene_config: " + error, failures);
    Expect(loaded_scene_path == Resolved(scene_config),
           "scene path resolves relative to app config", failures);

    Expect(WriteFile(
               scene_config,
               "version: 1\n"
               "unknown_root: accepted\n"
               "scene:\n"
               "  name: face_recognition\n"
               "  library: ./plugins/../plugins/libface_recognition.so\n"
               "  api_version: 1\n"
               "  params:\n"
               "    frontal_score_thresh: 0.55\n"
               "    qdrant_collection: face_embeddings\n"
               "postprocessors:\n"
               "  - name: tracker\n"
               "    enabled: true\n"
               "    library: ../postprocessors/libtracker.so\n"
               "    api_version: 1\n"
               "    params:\n"
               "      algorithm: bytetrack\n"
               "      track_labels: [face]\n"
               "      plugin_owned_option:\n"
               "        nested: true\n"
               "  - name: optional_processor\n"
               "    enabled: false\n"
               "    library: ./missing/liboptional.so\n"),
           "write valid scene config", failures);

    plugin::SceneConfig config;
    error.clear();
    Expect(plugin::LoadSceneConfig(scene_config.string(), config, error),
           "load valid scene config: " + error, failures);
    Expect(config.scene.name == "face_recognition", "scene name", failures);
    Expect(config.scene.api_version == 1, "scene API version", failures);
    Expect(config.scene.library_path == Resolved(scene_config.parent_path() / "plugins/libface_recognition.so"),
           "scene library resolves relative to scene config", failures);
    Expect(config.scene.config_dir == Resolved(scene_config.parent_path()),
           "scene config directory is absolute", failures);
    Expect(config.postprocessors.size() == 2, "both processors preserved",
           failures);
    if (config.postprocessors.size() == 2)
    {
        const auto& tracker = config.postprocessors[0];
        Expect(tracker.library_path == Resolved(scene_config.parent_path() / "../postprocessors/libtracker.so"),
               "processor library resolves relative to scene config", failures);
        Expect(tracker.config_dir == Resolved(scene_config.parent_path()),
               "processor config directory is absolute", failures);
        const YAML::Node params = YAML::Load(tracker.params_yaml);
        Expect(params["track_labels"] && params["track_labels"].IsSequence() && params["track_labels"].size() == 1 && params["track_labels"][0].as<std::string>() == "face",
               "serialized params preserve track_labels", failures);
        Expect(params["plugin_owned_option"] && params["plugin_owned_option"]["nested"].as<bool>(),
               "processor-specific params stay opaque", failures);

        const auto& disabled = config.postprocessors[1];
        Expect(!disabled.enabled, "disabled processor state is preserved",
               failures);
        Expect(disabled.api_version == 0,
               "disabled processor may omit API version", failures);
        Expect(disabled.library_path == Resolved(scene_config.parent_path() / "missing/liboptional.so"),
               "missing disabled library need not exist", failures);
    }

    const auto invalid = temp.path() / "invalid" / "scene.yaml";
    ExpectErrorContains(invalid,
                        "version: 1\nscene:\n  name: missing_library\n"
                        "  api_version: 1\n",
                        "scene.library", failures);
    ExpectErrorContains(invalid,
                        "version: 1\nscene:\n  name: valid\n"
                        "  library: libscene.so\n  api_version: 1\n"
                        "postprocessors:\n  - name: tracker\n"
                        "    library: libtracker.so\n",
                        "postprocessors[0].api_version", failures);
    ExpectErrorContains(invalid,
                        "version: 2\nscene:\n  name: valid\n"
                        "  library: libscene.so\n  api_version: 1\n",
                        "version", failures);
    ExpectErrorContains(invalid,
                        "version: 1\nscene:\n  name: valid\n"
                        "  library: libscene.so\n  api_version: 1\n"
                        "  params: invalid_scalar\n",
                        "scene.params", failures);

    const auto invalid_app = temp.path() / "invalid_app.yaml";
    Expect(WriteFile(invalid_app, "pipeline: []\n"),
           "write invalid app config", failures);
    loaded_scene_path.clear();
    error.clear();
    Expect(!plugin::LoadSceneConfigPath(invalid_app.string(), loaded_scene_path,
                                        error),
           "reject non-map pipeline", failures);
    Expect(error.find(invalid_app.string()) != std::string::npos && error.find("pipeline") != std::string::npos,
           "app config error includes path and pipeline field", failures);

    std::error_code ec;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", ec);
    Expect(!ec, "read test executable path", failures);
    if (!ec)
    {
        Expect(plugin::DefaultAppConfigPath() == (executable.parent_path() / "config.yaml").string(),
               "default config is beside executable", failures);
    }

    if (failures != 0)
    {
        std::cerr << "scene_config_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "scene_config_test: PASS\n";
    return 0;
}

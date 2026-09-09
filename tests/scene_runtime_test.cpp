#include "scene_runtime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{

class TempDir
{
public:
    TempDir()
        : path_(std::filesystem::temp_directory_path() /
                ("ax_core_scene_runtime_test_" + std::to_string(getpid())))
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }
    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void Expect(bool condition, const std::string& message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

plugin::ComponentConfig Processor(const std::string& name,
                                  const char* library,
                                  const std::filesystem::path& lifecycle)
{
    plugin::ComponentConfig config;
    config.name = name;
    config.library_path = library;
    config.params_yaml = "lifecycle_file: " + lifecycle.string() + "\n";
    config.api_version = kPostProcessorApiVersion;
    return config;
}

plugin::ComponentConfig Business(const std::filesystem::path& lifecycle,
                                 bool fail_init = false)
{
    plugin::ComponentConfig config;
    config.name = "scene";
    config.library_path = BUSINESS_PLUGIN_OK_PATH;
    config.config_dir = "/scene/config";
    config.params_yaml = "lifecycle_file: " + lifecycle.string() +
                         "\nlog_frames: true\n";
    if (fail_init)
    {
        config.params_yaml += "fail_init: true\n";
    }
    config.api_version = kBusinessPluginApiVersion;
    return config;
}

void TestProcessStages(const TempDir& temp, int& failures)
{
    const auto lifecycle = temp.path() / "process.txt";
    plugin::SceneConfig config;
    config.postprocessors = {
        Processor("set-track", POSTPROCESSOR_SET_TRACK_PATH, lifecycle)};
    config.scene = Business(lifecycle);

    HostServices host;
    plugin::SceneRuntime runtime;
    Expect(runtime.Init(&host, config) == 0, "runtime initializes", failures);

    ImageData frame{};
    std::vector<detection::Object> objects(2);
    objects[0].track_id = 999;
    objects[1].track_id = 1000;
    auto result = runtime.Process(frame, objects, 1);
    Expect(result.Ok() && result.stage == plugin::SceneProcessStage::kOk,
           "successful chain and business return kOk", failures);
    Expect(objects[0].track_id == 42,
           "runtime resets stale track id before processor sets it", failures);
    Expect(objects[1].track_id == -1,
           "runtime resets every object's stale track id", failures);

    runtime.Shutdown();
    runtime.Shutdown();
    Expect(ReadFile(lifecycle) ==
               "processor-init:set-track\n"
               "init\n"
               "processor-frame:set-track\n"
               "frame\n"
               "shutdown\n"
               "destroy\n"
               "dlclose\n"
               "processor-shutdown:set-track\n",
           "shutdown unloads business once before reverse processor chain",
           failures);
}

void TestPostprocessorFailureSkipsBusiness(const TempDir& temp, int& failures)
{
    const auto lifecycle = temp.path() / "failure.txt";
    plugin::SceneConfig config;
    config.postprocessors = {
        Processor("failure", POSTPROCESSOR_FAIL_PATH, lifecycle)};
    config.scene = Business(lifecycle);

    HostServices host;
    plugin::SceneRuntime runtime;
    Expect(runtime.Init(&host, config) == 0,
           "runtime with failing processor initializes", failures);
    std::vector<detection::Object> objects(1);
    objects[0].track_id = 99;
    const auto result = runtime.Process(ImageData{}, objects, 2);
    Expect(result.stage == plugin::SceneProcessStage::kPostProcessor &&
               result.code == -77,
           "processor failure returns its stage and code", failures);
    Expect(objects[0].track_id == -1,
           "runtime resets every track id before a failing chain", failures);
    Expect(ReadFile(lifecycle) == "init\n",
           "processor failure does not call business plugin", failures);
    runtime.Shutdown();
}

void TestBusinessFailureStage(const TempDir& temp, int& failures)
{
    const auto lifecycle = temp.path() / "business-failure.txt";
    plugin::SceneConfig config;
    config.postprocessors = {
        Processor("set-track", POSTPROCESSOR_SET_TRACK_PATH, lifecycle)};
    config.scene = Business(lifecycle);

    HostServices host;
    plugin::SceneRuntime runtime;
    Expect(runtime.Init(&host, config) == 0,
           "runtime for business failure initializes", failures);
    std::vector<detection::Object> objects;
    const auto result = runtime.Process(ImageData{}, objects, 3);
    Expect(result.stage == plugin::SceneProcessStage::kBusinessPlugin &&
               result.code == -42 && !result.Ok(),
           "business failure returns its stage and code", failures);
    Expect(ReadFile(lifecycle) ==
               "processor-init:set-track\n"
               "init\n"
               "processor-frame:set-track\n"
               "frame\n",
           "business runs only after the processor chain succeeds", failures);
    runtime.Shutdown();
}

void TestStartupRollbackAndRepeatedShutdown(const TempDir& temp, int& failures)
{
    const auto lifecycle = temp.path() / "rollback.txt";
    plugin::SceneConfig config;
    config.postprocessors = {
        Processor("first", POSTPROCESSOR_SET_TRACK_PATH, lifecycle),
        Processor("second", POSTPROCESSOR_SET_TRACK_PATH, lifecycle)};
    config.scene = Business(lifecycle, true);

    HostServices host;
    plugin::SceneRuntime runtime;
    Expect(runtime.Init(&host, config) == -66,
           "business initialization failure is returned", failures);
    runtime.Shutdown();
    runtime.Shutdown();
    Expect(ReadFile(lifecycle) ==
               "processor-init:first\n"
               "processor-init:second\n"
               "init\n"
               "destroy\n"
               "dlclose\n"
               "processor-shutdown:second\n"
               "processor-shutdown:first\n",
           "startup failure rolls back processors in reverse exactly once",
           failures);
}

} // namespace

int main()
{
    int failures = 0;
    TempDir temp;
    TestProcessStages(temp, failures);
    TestPostprocessorFailureSkipsBusiness(temp, failures);
    TestBusinessFailureStage(temp, failures);
    TestStartupRollbackAndRepeatedShutdown(temp, failures);
    if (failures != 0)
    {
        std::cerr << "scene_runtime_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "scene_runtime_test: PASS\n";
    return 0;
}

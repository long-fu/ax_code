#include "plugin_loader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

extern "C" void AxTestResetAsyncTask();
extern "C" bool AxTestWaitAsyncTaskStarted(int timeout_ms);
extern "C" void AxTestReleaseAsyncTask();

namespace
{

void Expect(bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

plugin::ComponentConfig Config(const char* name, const char* library,
                               uint32_t version = kBusinessPluginApiVersion)
{
    plugin::ComponentConfig config;
    config.name = name;
    config.library_path = library;
    config.config_dir = "/scene/config";
    config.params_yaml = "answer: 42\n";
    config.api_version = version;
    return config;
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main()
{
    int failures = 0;
    HostServices host;
    plugin::PluginManager manager;

    Expect(manager.OnFrame(ImageData{}, {}) != 0,
           "OnFrame must reject calls before a plugin is loaded", failures);

    auto ok = Config("scene", BUSINESS_PLUGIN_OK_PATH);
    const auto lifecycle = std::filesystem::temp_directory_path() /
                           "business_plugin_loader_lifecycle.txt";
    std::filesystem::remove(lifecycle);
    ok.params_yaml = "lifecycle_file: " + lifecycle.string() + "\n";
    Expect(manager.Load(&host, ok) == 0, "load valid business plugin", failures);
    Expect(manager.Loaded(), "manager reports loaded plugin", failures);

    detection::Object object;
    object.track_id = 42;
    Expect(manager.OnFrame(ImageData{}, {object}) == 0,
           "OnFrame delegates objects to loaded plugin", failures);
    object.track_id = 41;
    Expect(manager.OnFrame(ImageData{}, {object}) == -42,
           "OnFrame returns plugin failure", failures);

    manager.Unload();
    manager.Unload();
    Expect(!manager.Loaded(), "repeated unload remains unloaded", failures);
    Expect(ReadFile(lifecycle) == "init\nshutdown\ndestroy\ndlclose\n",
           "unload shuts down, destroys, and closes exactly once", failures);

    auto missing = Config("missing-scene", "/definitely/missing/business.so");
    Expect(manager.Load(&host, missing) != 0, "missing library fails", failures);
    Expect(!manager.Loaded(), "missing library leaves manager unloaded", failures);

    auto no_factory = Config("no-factory-scene", BUSINESS_PLUGIN_NO_FACTORY_PATH);
    Expect(manager.Load(&host, no_factory) != 0, "missing factory fails", failures);

    auto null_factory = Config("null-factory-scene", BUSINESS_PLUGIN_NULL_FACTORY_PATH);
    Expect(manager.Load(&host, null_factory) != 0, "null factory fails", failures);

    auto bad_version = Config("bad-version-scene", BUSINESS_PLUGIN_BAD_VERSION_PATH);
    Expect(manager.Load(&host, bad_version) != 0, "ABI mismatch fails", failures);

    auto init_failure = Config("scene", BUSINESS_PLUGIN_OK_PATH);
    init_failure.params_yaml = "fail_init: true\n";
    Expect(manager.Load(&host, init_failure) == -66, "Init failure is returned", failures);
    Expect(!manager.Loaded(), "Init failure leaves manager unloaded", failures);

    const std::filesystem::path empty_name_lifecycle =
        BUSINESS_EMPTY_NAME_LIFECYCLE;
    std::filesystem::remove(empty_name_lifecycle);
    auto empty_name = Config("configured-empty", BUSINESS_PLUGIN_EMPTY_NAME_PATH);
    Expect(manager.Load(&host, empty_name) != 0,
           "empty plugin name fails before Init", failures);
    Expect(ReadFile(empty_name_lifecycle) == "destroy\ndlclose\n",
           "empty plugin name is destroyed and closed without Init or Shutdown",
           failures);

    const std::filesystem::path mismatched_name_lifecycle =
        BUSINESS_MISMATCHED_NAME_LIFECYCLE;
    std::filesystem::remove(mismatched_name_lifecycle);
    auto mismatched_name =
        Config("configured-scene", BUSINESS_PLUGIN_MISMATCHED_NAME_PATH);
    Expect(manager.Load(&host, mismatched_name) != 0,
           "mismatched plugin name fails before Init", failures);
    Expect(ReadFile(mismatched_name_lifecycle) == "destroy\ndlclose\n",
           "mismatched plugin name is destroyed and closed without Init or Shutdown",
           failures);

    const std::filesystem::path partial_lifecycle =
        BUSINESS_PARTIAL_INIT_LIFECYCLE;
    std::filesystem::remove(partial_lifecycle);
    AxTestResetAsyncTask();
    auto partial =
        Config("partial-init-scene", BUSINESS_PLUGIN_PARTIAL_INIT_PATH);
    int partial_result = 0;
    std::thread loader([&] { partial_result = manager.Load(&host, partial); });
    const bool task_started = AxTestWaitAsyncTaskStarted(2000);
    Expect(task_started, "failed Init async task starts deterministically", failures);
    if (task_started)
    {
        Expect(ReadFile(partial_lifecycle) ==
                   "init-allocated\ninit-failed\n",
               "WaitQuiesce blocks Shutdown while the Init task is in flight",
               failures);
    }
    AxTestReleaseAsyncTask();
    loader.join();
    Expect(partial_result == -66, "partial Init failure status is returned",
           failures);
    Expect(ReadFile(partial_lifecycle) ==
               "init-allocated\ninit-failed\nshutdown-with-state\ndestroy\ndlclose\n",
           "failed Init waits, shuts down partial state, destroys, then closes",
           failures);

    if (failures != 0)
    {
        return 1;
    }
    std::cout << "business_plugin_loader_test: PASS\n";
    return 0;
}

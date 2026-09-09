#include "postprocessor_chain.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

namespace
{

class TempDir
{
public:
    TempDir()
        : path_(std::filesystem::temp_directory_path() /
                ("ax_core_postprocessor_chain_test_" +
                 std::to_string(getpid())))
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

void Expect(bool condition, const std::string& message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

plugin::ComponentConfig Config(const std::string& name,
                               const std::string& library_path,
                               const std::string& params_yaml = {},
                               uint32_t api_version =
                                   kPostProcessorApiVersion,
                               bool enabled = true)
{
    plugin::ComponentConfig config;
    config.name = name;
    config.library_path = library_path;
    config.params_yaml = params_yaml;
    config.api_version = api_version;
    config.enabled = enabled;
    return config;
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

plugin::PostProcessContext Context(ImageData& frame,
                                   std::vector<detection::Object>& objects)
{
    return {frame, objects, 42, std::chrono::steady_clock::now()};
}

void TestExecutionOrder(int& failures)
{
    plugin::PostProcessorChain chain;
    const std::vector<plugin::ComponentConfig> configs = {
        Config("append-one", POSTPROCESSOR_APPEND_PATH, "digit: 1\n"),
        Config("append-two", POSTPROCESSOR_APPEND_PATH, "digit: 2\n")};

    Expect(chain.Load(configs) == 0, "load ordered append processors",
           failures);
    Expect(chain.Size() == 2, "chain contains two enabled processors",
           failures);

    ImageData frame{};
    std::vector<detection::Object> objects(1);
    objects[0].track_id = -9;
    auto context = Context(frame, objects);
    Expect(chain.Process(context) == 0, "ordered chain succeeds", failures);
    Expect(objects[0].track_id == 12,
           "append processors run in configuration order and normalize a negative base",
           failures);

    chain.Unload();
    chain.Unload();
    Expect(chain.Size() == 0, "Unload is idempotent", failures);
}

void TestProcessStopsAtFirstFailure(int& failures)
{
    plugin::PostProcessorChain chain;
    const std::vector<plugin::ComponentConfig> configs = {
        Config("before-failure", POSTPROCESSOR_APPEND_PATH, "digit: 1\n"),
        Config("failure", POSTPROCESSOR_FAIL_PATH),
        Config("after-failure", POSTPROCESSOR_APPEND_PATH, "digit: 2\n")};

    Expect(chain.Load(configs) == 0, "load chain containing process failure",
           failures);
    ImageData frame{};
    std::vector<detection::Object> objects(1);
    auto context = Context(frame, objects);
    std::ostringstream process_stderr;
    std::streambuf* original_stderr = std::cerr.rdbuf(process_stderr.rdbuf());
    const int process_result = chain.Process(context);
    std::cerr.rdbuf(original_stderr);
    Expect(process_result == -77,
           "Process returns the failing processor status", failures);
    Expect(objects[0].track_id == 1,
           "processors after the first failure do not run", failures);
    Expect(process_stderr.str().empty(),
           "Process failure does not write an unbounded per-frame log",
           failures);
    Expect(chain.LastError().find("component='failure'") != std::string::npos &&
               chain.LastError().find("stage='process'") !=
                   std::string::npos &&
               chain.LastError().find("status -77") != std::string::npos,
           "Process exposes processor identity, stage, and status to caller",
           failures);
}

void TestDisabledProcessorIsSkipped(int& failures)
{
    plugin::PostProcessorChain chain;
    const std::vector<plugin::ComponentConfig> configs = {
        Config("disabled", "/missing/disabled.so", {}, 0, false),
        Config("enabled", POSTPROCESSOR_APPEND_PATH, "digit: 7\n")};

    Expect(chain.Load(configs) == 0,
           "disabled processor does not require a loadable library", failures);
    Expect(chain.Size() == 1, "disabled processor is absent from chain",
           failures);
}

void TestLoadFailures(int& failures)
{
    plugin::PostProcessorChain chain;
    Expect(chain.Load({Config("missing", "/missing/postprocessor.so")}) != 0,
           "missing shared object fails Load", failures);
    Expect(chain.Size() == 0, "missing library leaves an empty chain",
           failures);

    Expect(chain.Load({Config("no-factory", POSTPROCESSOR_NO_FACTORY_PATH)}) !=
               0,
           "missing factory symbols fail Load", failures);
    Expect(chain.Size() == 0, "missing symbols leave an empty chain",
           failures);

    Expect(chain.Load(
               {Config("bad-version", POSTPROCESSOR_BAD_VERSION_PATH)}) != 0,
           "processor API version 999 fails Load", failures);
    Expect(chain.Size() == 0, "bad instance version leaves an empty chain",
           failures);

    Expect(chain.Load({Config("config-version", POSTPROCESSOR_APPEND_PATH,
                              "digit: 1\n", 999)}) != 0,
           "configuration API version mismatch fails Load", failures);
    Expect(chain.Size() == 0, "bad config version leaves an empty chain",
           failures);

    Expect(chain.Load({Config("init-failure", POSTPROCESSOR_FAIL_PATH,
                              "fail_init:true\n")}) == -66,
           "Init failure status is returned from Load", failures);
    Expect(chain.Size() == 0, "Init failure leaves an empty chain", failures);
}

void TestNullFactoryFailsLoad(int& failures)
{
    plugin::PostProcessorChain chain;
    Expect(chain.Load(
               {Config("null-factory", POSTPROCESSOR_NULL_FACTORY_PATH)}) != 0,
           "null CreatePostProcessor result fails Load", failures);
    Expect(chain.Size() == 0, "null factory leaves an empty chain", failures);
    Expect(chain.LastError().find("component='null-factory'") !=
                   std::string::npos &&
               chain.LastError().find("stage='create'") != std::string::npos &&
               chain.LastError().find("factory returned null") !=
                   std::string::npos,
           "null factory failure exposes useful startup context", failures);
}

void TestPartialLoadRollsBackInReverseOrder(const TempDir& temp,
                                            int& failures)
{
    const auto lifecycle_file = temp.path() / "lifecycle.txt";
    const std::string lifecycle =
        "lifecycle_file: " + lifecycle_file.string() + "\n";
    plugin::PostProcessorChain chain;
    const std::vector<plugin::ComponentConfig> configs = {
        Config("first", POSTPROCESSOR_APPEND_PATH,
               "digit: 1\n" + lifecycle),
        Config("second", POSTPROCESSOR_APPEND_PATH,
               "digit: 2\n" + lifecycle),
        Config("rejecting", POSTPROCESSOR_FAIL_PATH,
               "fail_init:true\n")};

    Expect(chain.Load(configs) == -66,
           "partial Load returns the failing Init status", failures);
    Expect(chain.Size() == 0, "partial Load rolls back every loaded processor",
           failures);
    Expect(ReadFile(lifecycle_file) ==
               "shutdown:second\n"
               "destroy:second\n"
               "shutdown:first\n"
               "destroy:first\n",
           "rollback shuts down and destroys initialized processors in reverse order",
           failures);
}

void TestFailedInitCleansPartialProcessorState(int& failures)
{
    const std::filesystem::path lifecycle =
        POSTPROCESSOR_PARTIAL_INIT_LIFECYCLE;
    std::filesystem::remove(lifecycle);
    plugin::PostProcessorChain chain;
    Expect(chain.Load({Config("partial-init-processor",
                              POSTPROCESSOR_PARTIAL_INIT_PATH)}) == -66,
           "partial processor Init failure status is returned", failures);
    Expect(ReadFile(lifecycle) ==
               "init-allocated\nshutdown-with-state\ndestroy\ndlclose\n",
           "failed processor Init shuts down partial state before destroy and close",
           failures);
}

} // namespace

int main()
{
    int failures = 0;
    TempDir temp;

    TestExecutionOrder(failures);
    TestProcessStopsAtFirstFailure(failures);
    TestDisabledProcessorIsSkipped(failures);
    TestLoadFailures(failures);
    TestNullFactoryFailsLoad(failures);
    TestPartialLoadRollsBackInReverseOrder(temp, failures);
    TestFailedInitCleansPartialProcessorState(failures);

    if (failures != 0)
    {
        std::cerr << "postprocessor_chain_test: " << failures
                  << " failure(s)\n";
        return 1;
    }
    std::cout << "postprocessor_chain_test: PASS\n";
    return 0;
}

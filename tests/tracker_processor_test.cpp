#include "postprocessor_chain.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

constexpr const char* kValidParams = R"yaml(
algorithm: bytetrack
track_labels: [face]
frame_rate: 25
track_buffer: 30
track_thresh: 0.5
high_thresh: 0.6
match_thresh: 0.8
)yaml";

void Expect(bool condition, const std::string& message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

plugin::ComponentConfig Config(const std::string& params)
{
    plugin::ComponentConfig config;
    config.name = "tracker";
    config.library_path = TRACKER_POSTPROCESSOR_PATH;
    config.params_yaml = params;
    config.api_version = kPostProcessorApiVersion;
    return config;
}

detection::Object Object(const std::string& label_name, float x)
{
    detection::Object object{};
    object.rect = cv::Rect_<float>(x, 20.0f, 40.0f, 40.0f);
    object.label = label_name == "face" ? 91 : 7;
    object.label_name = label_name;
    object.prob = 0.95f;
    object.track_id = 1234;
    return object;
}

int Process(plugin::PostProcessorChain& chain,
            std::vector<detection::Object>& objects, uint64_t frame_seq)
{
    ImageData frame{};
    plugin::PostProcessContext context{
        frame, objects, frame_seq, std::chrono::steady_clock::now()};
    return chain.Process(context);
}

void TestFiltersByNameResetsAndKeepsStableId(int& failures)
{
    plugin::PostProcessorChain chain;
    Expect(chain.Load({Config(kValidParams)}) == 0,
           "valid tracker configuration loads", failures);

    std::vector<detection::Object> first = {Object("face", 10.0f),
                                             Object("smoke", 100.0f)};
    Expect(Process(chain, first, 1) == 0, "first frame processes", failures);
    Expect(first[0].track_id > 0, "configured label receives a track ID",
           failures);
    Expect(first[1].track_id == -1,
           "unconfigured label is reset and remains untracked", failures);
    const int first_id = first[0].track_id;

    std::vector<detection::Object> second = {Object("face", 12.0f)};
    Expect(Process(chain, second, 2) == 0, "nearby frame processes",
           failures);
    Expect(second[0].track_id == first_id,
           "nearby detection keeps the same track ID", failures);
}

void TestIdsAreUniqueAcrossLabels(int& failures)
{
    const std::string params = R"yaml(
algorithm: bytetrack
track_labels: [face, person]
frame_rate: 25
track_buffer: 30
track_thresh: 0.5
high_thresh: 0.6
match_thresh: 0.8
)yaml";
    plugin::PostProcessorChain chain;
    Expect(chain.Load({Config(params)}) == 0,
           "multi-label tracker configuration loads", failures);
    std::vector<detection::Object> objects = {Object("face", 10.0f),
                                               Object("person", 10.0f)};
    Expect(Process(chain, objects, 1) == 0, "multi-label frame processes",
           failures);
    Expect(objects[0].track_id > 0 && objects[1].track_id > 0,
           "both configured labels receive IDs", failures);
    Expect(objects[0].track_id != objects[1].track_id,
           "same-frame IDs do not collide across labels", failures);
}

void ExpectInvalid(const std::string& params, const std::string& field,
                   const std::string& expected_detail,
                   int& failures)
{
    plugin::PostProcessorChain chain;
    std::ostringstream startup_stderr;
    std::streambuf* original_stderr = std::cerr.rdbuf(startup_stderr.rdbuf());
    const int result = chain.Load({Config(params)});
    std::cerr.rdbuf(original_stderr);
    Expect(result != 0,
           "invalid " + field + " fails Load", failures);
    Expect(chain.LastError().find("component='tracker'") != std::string::npos &&
               chain.LastError().find("stage='init'") != std::string::npos,
           "invalid " + field + " is reported as tracker init failure",
           failures);
    Expect(startup_stderr.str().find(expected_detail) != std::string::npos,
           "invalid " + field + " reports its specific startup reason",
           failures);
}

void TestStrictValidation(int& failures)
{
    ExpectInvalid("algorithm: sort\ntrack_labels: [face]\n", "algorithm",
                  "algorithm", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: []\n", "track_labels",
                  "track_labels", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: ['']\n",
                  "empty label name", "label names", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [1]\n",
                  "numeric label ID", "label name", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [' 1 ']\n",
                  "whitespace numeric label", "label name", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [1.0]\n",
                  "floating-point label", "label name", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [1e2]\n",
                  "scientific label", "label name", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [0x10]\n",
                  "hex label", "label name", failures);
    const std::vector<std::string> yaml_numeric_labels = {
        "0b101", "+0b101", "-0b101", "0o17", "+0o17", "-0o17",
        "1_000", "0b10_01", "0o1_7", "0x1_0", ".inf", "+.inf",
        "-.inf", ".nan", ".INF", ".NaN"};
    for (const auto& numeric_label : yaml_numeric_labels)
    {
        ExpectInvalid("algorithm: bytetrack\ntrack_labels: ['" +
                          numeric_label + "']\n",
                      "YAML numeric label " + numeric_label, "label name",
                      failures);
    }
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [face]\nframe_rate: 0\n",
                  "frame_rate", "frame_rate", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [face]\ntrack_buffer: 0\n",
                  "track_buffer", "track_buffer", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [face]\ntrack_thresh: -0.1\n",
                  "track_thresh", "track_thresh", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [face]\nhigh_thresh: 1.1\n",
                  "high_thresh", "high_thresh", failures);
    ExpectInvalid("algorithm: bytetrack\ntrack_labels: [face]\nmatch_thresh: 1.1\n",
                  "match_thresh", "match_thresh", failures);

    const std::vector<std::pair<std::string, std::string>> malformed = {
        {"algorithm: []\ntrack_labels: [face]\n", "algorithm"},
        {"algorithm:\ntrack_labels: [face]\n", "algorithm"},
        {"algorithm: bytetrack\ntrack_labels: [[]]\n", "track_labels item"},
        {"algorithm: bytetrack\ntrack_labels: [face]\nframe_rate: []\n",
         "frame_rate"},
        {"algorithm: bytetrack\ntrack_labels: [face]\ntrack_buffer:\n",
         "track_buffer"},
        {"algorithm: bytetrack\ntrack_labels: [face]\ntrack_thresh: []\n",
         "track_thresh"},
        {"algorithm: bytetrack\ntrack_labels: [face]\nhigh_thresh:\n",
         "high_thresh"},
        {"algorithm: bytetrack\ntrack_labels: [face]\nmatch_thresh: nope\n",
         "match_thresh"}};
    for (const auto& item : malformed)
    {
        ExpectInvalid(item.first, item.second, item.second, failures);
    }
}

void TestDuplicateKeysFail(int& failures)
{
    ExpectInvalid(
        "algorithm: bytetrack\nalgorithm: bytetrack\ntrack_labels: [face]\n",
        "duplicate algorithm", "duplicate key 'algorithm'", failures);
    ExpectInvalid(
        "algorithm: bytetrack\ntrack_labels: [face]\nframe_rate: 25\nframe_rate: 30\n",
        "duplicate frame_rate", "duplicate key 'frame_rate'", failures);
    ExpectInvalid(
        "algorithm: bytetrack\ntrack_labels: [face]\ntrack_labels: [person]\n",
        "duplicate track_labels", "duplicate key 'track_labels'", failures);
    ExpectInvalid(
        "algorithm: bytetrack\ntrack_labels: [face]\nmatch_thresh: 0.7\nmatch_thresh: 0.8\n",
        "duplicate match_thresh", "duplicate key 'match_thresh'", failures);
}

void TestDigitNamesAndUnknownFields(int& failures)
{
    plugin::PostProcessorChain chain;
    const std::string params =
        "algorithm: bytetrack\ntrack_labels: [person2, 人脸]\nunknown_option: true\n";
    std::ostringstream startup_stderr;
    std::streambuf* original_stderr = std::cerr.rdbuf(startup_stderr.rdbuf());
    const int result = chain.Load({Config(params)});
    std::cerr.rdbuf(original_stderr);
    Expect(result == 0,
           "label names containing digits and non-ASCII names remain valid",
           failures);
    Expect(startup_stderr.str().find("unknown tracker parameter 'unknown_option'") !=
               std::string::npos,
           "unknown tracker parameter emits a focused warning", failures);
}

} // namespace

int main()
{
    int failures = 0;
    TestFiltersByNameResetsAndKeepsStableId(failures);
    TestIdsAreUniqueAcrossLabels(failures);
    TestStrictValidation(failures);
    TestDuplicateKeysFail(failures);
    TestDigitNamesAndUnknownFields(failures);
    if (failures != 0)
    {
        std::cerr << "tracker_processor_test: " << failures
                  << " failure(s)\n";
        return 1;
    }
    std::cout << "tracker_processor_test: PASS\n";
    return 0;
}

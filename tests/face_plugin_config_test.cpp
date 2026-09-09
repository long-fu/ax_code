#include "face_plugin_config.h"

#include <iostream>
#include <string>

namespace
{

void Expect(bool condition, const std::string& message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void ExpectInvalid(const std::string& yaml, const std::string& field,
                   int& failures)
{
    FacePluginConfig config;
    std::string error;
    Expect(!ParseFacePluginConfig(yaml, config, error),
           field + " invalid value is rejected", failures);
    Expect(error.find(field) != std::string::npos,
           field + " error names the field", failures);
}

}  // namespace

int main()
{
    int failures = 0;

    FacePluginConfig config;
    std::string error;
    Expect(ParseFacePluginConfig("", config, error),
           "empty params use defaults", failures);
    Expect(config.frontal_score_thresh == 0.55f,
           "default frontal_score_thresh", failures);
    Expect(config.qdrant_collection == "face_embeddings",
           "default qdrant_collection", failures);

    Expect(ParseFacePluginConfig(
               "frontal_score_thresh: 0.75\n"
               "qdrant_collection: visitors\n",
               config, error),
           "valid params parse", failures);
    Expect(config.frontal_score_thresh == 0.75f,
           "configured frontal_score_thresh", failures);
    Expect(config.qdrant_collection == "visitors",
           "configured qdrant_collection", failures);

    ExpectInvalid("frontal_score_thresh: nope\n", "frontal_score_thresh",
                  failures);
    ExpectInvalid("frontal_score_thresh: []\n", "frontal_score_thresh",
                  failures);
    ExpectInvalid("frontal_score_thresh: -0.01\n", "frontal_score_thresh",
                  failures);
    ExpectInvalid("frontal_score_thresh: 1.01\n", "frontal_score_thresh",
                  failures);
    ExpectInvalid("frontal_score_thresh: .nan\n", "frontal_score_thresh",
                  failures);
    ExpectInvalid("qdrant_collection: []\n", "qdrant_collection", failures);
    ExpectInvalid("qdrant_collection: ''\n", "qdrant_collection", failures);

    if (failures != 0)
    {
        std::cerr << "face_plugin_config_test: " << failures
                  << " failure(s)\n";
        return 1;
    }
    std::cout << "face_plugin_config_test: PASS\n";
    return 0;
}

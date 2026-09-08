#include "postprocessor.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{

class FailProcessor final : public PostProcessor
{
public:
    const char* Name() const override
    {
        return "fail";
    }

    uint32_t ApiVersion() const override
    {
        return kPostProcessorApiVersion;
    }

    int Init(const plugin::ComponentConfig& config) override
    {
        std::string normalized = config.params_yaml;
        normalized.erase(
            std::remove_if(normalized.begin(), normalized.end(),
                           [](unsigned char c) { return std::isspace(c); }),
            normalized.end());
        return normalized.find("fail_init:true") != std::string::npos ? -66
                                                                      : 0;
    }

    int Process(plugin::PostProcessContext&) override
    {
        return -77;
    }

    void Shutdown() override
    {
    }
};

} // namespace

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return new FailProcessor();
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor* processor)
{
    delete processor;
}

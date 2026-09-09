#include "postprocessor.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace
{

std::string ValueFor(const std::string& yaml, const std::string& key)
{
    const std::string prefix = key + ":";
    const size_t begin = yaml.find(prefix);
    if (begin == std::string::npos)
    {
        return {};
    }
    const size_t value_begin = begin + prefix.size();
    const size_t end = yaml.find('\n', value_begin);
    std::string value = yaml.substr(value_begin, end - value_begin);
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                             [](unsigned char c) {
                                                 return !std::isspace(c);
                                             }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char c) {
                                 return !std::isspace(c);
                             }).base(),
                value.end());
    return value;
}

class SetTrackProcessor final : public PostProcessor
{
public:
    const char* Name() const override { return name_.c_str(); }
    uint32_t ApiVersion() const override { return kPostProcessorApiVersion; }

    int Init(const plugin::ComponentConfig& config) override
    {
        name_ = config.name;
        lifecycle_file_ = ValueFor(config.params_yaml, "lifecycle_file");
        Append("processor-init:" + name_);
        return 0;
    }

    int Process(plugin::PostProcessContext& context) override
    {
        Append("processor-frame:" + name_);
        if (!context.objects.empty())
        {
            context.objects.front().track_id = 42;
        }
        return 0;
    }

    void Shutdown() override { Append("processor-shutdown:" + name_); }

private:
    void Append(const std::string& value)
    {
        if (!lifecycle_file_.empty())
        {
            std::ofstream output(lifecycle_file_, std::ios::app);
            output << value << '\n';
        }
    }

    std::string name_;
    std::string lifecycle_file_;
};

} // namespace

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return new SetTrackProcessor();
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor* processor)
{
    delete processor;
}

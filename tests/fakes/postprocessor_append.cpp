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

class AppendProcessor final : public PostProcessor
{
public:
    ~AppendProcessor() override
    {
        AppendLifecycle("destroy:");
    }

    const char* Name() const override
    {
        return name_.c_str();
    }

    uint32_t ApiVersion() const override
    {
        return kPostProcessorApiVersion;
    }

    int Init(const plugin::ComponentConfig& config) override
    {
        name_ = config.name;
        lifecycle_file_ = ValueFor(config.params_yaml, "lifecycle_file");
        const std::string digit = ValueFor(config.params_yaml, "digit");
        if (digit.size() != 1 || digit[0] < '0' || digit[0] > '9')
        {
            return -1;
        }
        digit_ = digit[0] - '0';
        return 0;
    }

    int Process(plugin::PostProcessContext& context) override
    {
        for (auto& object : context.objects)
        {
            const int base = object.track_id < 0 ? 0 : object.track_id;
            object.track_id = base * 10 + digit_;
        }
        return 0;
    }

    void Shutdown() override
    {
        AppendLifecycle("shutdown:");
    }

private:
    void AppendLifecycle(const char* phase)
    {
        if (lifecycle_file_.empty())
        {
            return;
        }
        std::ofstream output(lifecycle_file_, std::ios::app);
        output << phase << name_ << '\n';
    }

    std::string name_;
    std::string lifecycle_file_;
    int digit_ = 0;
};

} // namespace

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return new AppendProcessor();
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor* processor)
{
    delete processor;
}

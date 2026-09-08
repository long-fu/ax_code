#include "postprocessor.h"

namespace
{

class BadVersionProcessor final : public PostProcessor
{
public:
    const char* Name() const override
    {
        return "bad-version";
    }

    uint32_t ApiVersion() const override
    {
        return 999;
    }

    int Init(const plugin::ComponentConfig&) override
    {
        return 0;
    }

    int Process(plugin::PostProcessContext&) override
    {
        return 0;
    }

    void Shutdown() override
    {
    }
};

} // namespace

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return new BadVersionProcessor();
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor* processor)
{
    delete processor;
}

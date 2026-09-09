#include "postprocessor.h"

#include <fstream>
#include <memory>

namespace
{

void Append(const char* phase)
{
    std::ofstream(POSTPROCESSOR_PARTIAL_INIT_LIFECYCLE, std::ios::app)
        << phase << '\n';
}

class PartialInitProcessor final : public PostProcessor
{
public:
    const char* Name() const override { return "partial-init-processor"; }
    uint32_t ApiVersion() const override { return kPostProcessorApiVersion; }
    int Init(const plugin::ComponentConfig&) override
    {
        state_ = std::make_unique<int>(42);
        Append("init-allocated");
        return -66;
    }
    int Process(plugin::PostProcessContext&) override { return 0; }
    void Shutdown() override
    {
        Append(state_ ? "shutdown-with-state" : "shutdown-without-state");
        state_.reset();
    }

private:
    std::unique_ptr<int> state_;
};

} // namespace

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return new PartialInitProcessor();
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor* processor)
{
    Append("destroy");
    delete processor;
}

__attribute__((destructor)) static void OnLibraryUnload()
{
    Append("dlclose");
}

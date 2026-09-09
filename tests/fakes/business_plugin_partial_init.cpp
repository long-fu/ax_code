#include "business_plugin.h"
#include "host_services.h"

#include <fstream>
#include <memory>

extern "C" void AxTestBlockAsyncTask();

namespace
{

void Append(const char* phase)
{
    std::ofstream(BUSINESS_PARTIAL_INIT_LIFECYCLE, std::ios::app)
        << phase << '\n';
}

class PartialInitPlugin final : public BusinessPlugin
{
public:
    const char* Name() const override { return "partial-init-scene"; }
    uint32_t ApiVersion() const override { return kBusinessPluginApiVersion; }

    int Init(HostServices* host, const PluginConfig&) override
    {
        state_ = std::make_unique<int>(42);
        Append("init-allocated");
        if (!host->SubmitAsync(Name(), [] { AxTestBlockAsyncTask(); }))
        {
            return -68;
        }
        Append("init-failed");
        return -66;
    }

    int OnFrame(const ImageData&,
                const std::vector<detection::Object>&) override { return 0; }

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
BusinessPlugin* CreatePlugin()
{
    return new PartialInitPlugin();
}

extern "C" __attribute__((visibility("default")))
void DestroyPlugin(BusinessPlugin* plugin)
{
    Append("destroy");
    delete plugin;
}

__attribute__((destructor)) static void OnLibraryUnload()
{
    Append("dlclose");
}

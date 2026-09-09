#include "business_plugin.h"

namespace
{

class BadVersionBusinessPlugin final : public BusinessPlugin
{
public:
    const char* Name() const override { return "business-bad-version"; }
    uint32_t ApiVersion() const override { return 999; }
    int Init(HostServices*, const PluginConfig&) override { return 0; }
    int OnFrame(const ImageData&,
                const std::vector<detection::Object>&) override { return 0; }
    void Shutdown() override {}
};

} // namespace

extern "C" __attribute__((visibility("default")))
BusinessPlugin* CreatePlugin()
{
    return new BadVersionBusinessPlugin();
}

extern "C" __attribute__((visibility("default")))
void DestroyPlugin(BusinessPlugin* plugin)
{
    delete plugin;
}

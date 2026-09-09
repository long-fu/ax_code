#include "business_plugin.h"

#include <fstream>

namespace
{

class MismatchedNamePlugin final : public BusinessPlugin
{
public:
    const char* Name() const override { return "actual-scene"; }
    uint32_t ApiVersion() const override { return kBusinessPluginApiVersion; }
    int Init(HostServices*, const PluginConfig&) override
    {
        std::ofstream(BUSINESS_MISMATCHED_NAME_LIFECYCLE, std::ios::app)
            << "init\n";
        return 0;
    }
    int OnFrame(const ImageData&,
                const std::vector<detection::Object>&) override { return 0; }
    void Shutdown() override
    {
        std::ofstream(BUSINESS_MISMATCHED_NAME_LIFECYCLE, std::ios::app)
            << "shutdown\n";
    }
};

} // namespace

extern "C" __attribute__((visibility("default")))
BusinessPlugin* CreatePlugin()
{
    return new MismatchedNamePlugin();
}

extern "C" __attribute__((visibility("default")))
void DestroyPlugin(BusinessPlugin* plugin)
{
    std::ofstream(BUSINESS_MISMATCHED_NAME_LIFECYCLE, std::ios::app)
        << "destroy\n";
    delete plugin;
}

__attribute__((destructor)) static void OnLibraryUnload()
{
    std::ofstream(BUSINESS_MISMATCHED_NAME_LIFECYCLE, std::ios::app)
        << "dlclose\n";
}

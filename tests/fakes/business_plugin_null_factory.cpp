#include "business_plugin.h"

extern "C" __attribute__((visibility("default")))
BusinessPlugin* CreatePlugin()
{
    return nullptr;
}

extern "C" __attribute__((visibility("default")))
void DestroyPlugin(BusinessPlugin*)
{
}

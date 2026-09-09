#include "postprocessor.h"

extern "C" __attribute__((visibility("default")))
PostProcessor* CreatePostProcessor()
{
    return nullptr;
}

extern "C" __attribute__((visibility("default")))
void DestroyPostProcessor(PostProcessor*)
{
}

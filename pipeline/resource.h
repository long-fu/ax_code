#ifndef PIPELINE_RESOURCE_H
#define PIPELINE_RESOURCE_H
#pragma once

#include <cstdint>
#include <string>
#include "error.h"

namespace pipeline {

class Resource {
public:
    Resource();

    /**
     * @brief Create a Resource with device and config
     * @param [in] device_id: device id
     * @param [in] config_path: config file path
     */
    Resource(int32_t device_id, const std::string& config_path);

    ~Resource();

    TaskError Init();
    void Release();

private:
    bool is_released_;
    int32_t device_id_;
    std::string config_path_;
};

} // namespace pipeline
#endif  // PIPELINE_RESOURCE_H

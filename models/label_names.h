#pragma once

#include <string>
#include <vector>

#include "detection_types.h"

namespace models
{

bool AssignLabelNames(const std::vector<std::string>& labels,
                      std::vector<detection::Object>& objects,
                      std::string& error);

} // namespace models

#pragma once

#include <chrono>
#include <ctime>
#include <algorithm>
#include "uuid/uuid.h"
#include <cstring>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace my_utils
{
    inline std::time_t GetUnixSeconds()
    {
        auto now = std::chrono::system_clock::now();
        return std::chrono::system_clock::to_time_t(now);
    }

    inline std::string GenerateUuid()
    {
        uuid_t uuid;
        char uuid_str[37];

        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuid_str);

        std::string result(uuid_str);
        result.erase(std::remove(result.begin(), result.end(), '-'), result.end());

        return result;
    }

    inline std::string GetCurrentTimeYmdHMS()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

        return oss.str();
    }

} // namespace my_utils

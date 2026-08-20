#pragma once

#include <chrono>
#include <ctime>
#include <algorithm>
#include "uuid/uuid.h"
#include <string.h>
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace my_utils
{
    std::time_t GetUnixSeconds()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        return t;

        // std::tm* utc = std::gmtime(&t);
        // std::cout << "UTC: "
        //           << utc->tm_year + 1900 << "-"
        //           << utc->tm_mon + 1 << "-"
        //           << utc->tm_mday << " "
        //           << utc->tm_hour << ":"
        //           << utc->tm_min << ":"
        //           << utc->tm_sec << std::endl;
    }

    std::string GenerateUuid()
    {
        uuid_t uuid;
        char uuid_str[37];

        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuid_str);

        std::string result(uuid_str);
        result.erase(std::remove(result.begin(), result.end(), '-'), result.end());

        return result;
    }

    std::string GetCurrentTimeYmdHMS()
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
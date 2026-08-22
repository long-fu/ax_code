#pragma once

#include <chrono>
#include <ctime>
#include <cstdint>
#include <algorithm>
#include "uuid/uuid.h"
#include <cstring>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace my_utils
{
    // UTC epoch milliseconds (timezone +00:00).
    inline std::int64_t GetUnixMilliseconds()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();
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

    // ISO-8601 UTC, e.g. 2026-08-21T08:33:00Z
    inline std::string GetCurrentTimeIso8601Utc()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

} // namespace my_utils

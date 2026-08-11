#pragma once

#include <cstdint>
#include <string>
#include <vector>


typedef unsigned char uchar;

namespace utilities
{
    bool FileExist(const std::string& path);
    

    bool ReadFile(const std::string& path, std::vector<char>& data);
    

    bool DumpFile(const std::string& path, std::vector<uint8_t> data);
    

    bool DumpFile(const std::string& path, char* data, int size);
    

    bool ReadFile(const char* fn, std::vector<uchar>& data);

} // namespace utilities

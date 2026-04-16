#ifndef __FILE_H__
#define __FILE_H__

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

typedef unsigned char uchar;

namespace utilities
{
    bool file_exist(const std::string& path);
    

    bool read_file(const std::string& path, std::vector<char>& data);
    

    bool dump_file(const std::string& path, std::vector<uint8_t> data);
    

    bool dump_file(const std::string& path, char* data, int size);
    

    bool read_file(const char* fn, std::vector<uchar>& data);

} // namespace utilities


#endif

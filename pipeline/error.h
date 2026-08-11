#ifndef PIPELINE_ERROR_H
#define PIPELINE_ERROR_H
#pragma once

// Pipeline log macros delegate to spdlog-based LOG_* macros
#include "logger.h"

#define PIPELINE_LOG_ERROR(...)   LOG_ERROR(__VA_ARGS__)
#define PIPELINE_LOG_INFO(...)    LOG_INFO(__VA_ARGS__)
#define PIPELINE_LOG_WARNING(...) LOG_WARN(__VA_ARGS__)
#define PIPELINE_LOG_DEBUG(...)   LOG_DEBUG(__VA_ARGS__)

namespace pipeline {

typedef int TaskError;

// Base errors
const int kOk = 0;
const int kGeneralError = 1;
const int kInvalidArgs = 2;
const int kCreateThread = 6;
const int kCreateStream = 7;
const int kAppInit = 9;
const int kDestInvalid = 10;
const int kInitedAlready = 11;
const int kEnqueue = 12;
const int kWriteFile = 13;
const int kThreadAbnormal = 14;
const int kStartThread = 15;
const int kAddThread = 16;

// Memory errors
const int kMalloc = 101;
const int kMallocDevice = 102;

// File errors
const int kAccessFile = 201;
const int kInvalidFile = 202;
const int kOpenFile = 203;

} // namespace pipeline
#endif

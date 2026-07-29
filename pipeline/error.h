#ifndef PIPELINE_ERROR_H
#define PIPELINE_ERROR_H
#pragma once

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

/**
 * @brief Write error level log to host log
 * @param [in] fmt: the input format string
 * @return none
 */
#define PIPELINE_LOG_ERROR(fmt, ...) \
    do {fprintf(stdout, "[ERROR]  " fmt "\n", ##__VA_ARGS__);}while (0)

/**
 * @brief Write info level log to host log
 * @param [in] fmt: the input format string
 * @return none
 */
#define PIPELINE_LOG_INFO(fmt, ...) \
    do {fprintf(stdout, "[INFO]  " fmt "\n", ##__VA_ARGS__);}while (0)

/**
 * @brief Write warning level log to host log
 * @param [in] fmt: the input format string
 * @return none
 */
#define PIPELINE_LOG_WARNING(fmt, ...) \
    do {fprintf(stdout, "[WARNING]  " fmt "\n", ##__VA_ARGS__);}while (0)

/**
 * @brief Write debug level log to host log
 * @param [in] fmt: the input format string
 * @return none
 */
#define PIPELINE_LOG_DEBUG(fmt, ...) \
    do { fprintf(stdout, "[DEBUG]  " fmt "\n", ##__VA_ARGS__);}while (0)

} // namespace pipeline
#endif

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <string>
#include <map>

#define MACRO_BLACK "\033[1;30;30m"
#define MACRO_RED "\033[1;30;31m"
#define MACRO_GREEN "\033[1;30;32m"
#define MACRO_YELLOW "\033[1;30;33m"
#define MACRO_BLUE "\033[1;30;34m"
#define MACRO_PURPLE "\033[1;30;35m"
#define MACRO_WHITE "\033[1;30;37m"
#define MACRO_END "\033[0m"


/**
 * @brief Write acl error level log to host log
 * @param [in]: fmt: the input format string
 * @return none
 */
#define LOG_ERROR(fmt, ...)                                             \
    do                                                                  \
    {                                                                   \
        fprintf(stdout, MACRO_RED "[ERROR]  " fmt "\n", ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Write acl info level log to host log
 * @param [in]: fmt: the input format string
 * @return none
 */
#define LOG_INFO(fmt, ...)                                               \
    do                                                                   \
    {                                                                    \
        fprintf(stdout, MACRO_PURPLE "[INFO]  " fmt "\n", ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Write acl warining level log to host log
 * @param [in]: fmt: the input format string
 * @return none
 */
#define LOG_WARNING(fmt, ...)                                                \
    do                                                                       \
    {                                                                        \
        fprintf(stdout, MACRO_YELLOW "[WARNING]  " fmt "\n", ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Write acl debug level log to host log
 * @param [in]: fmt: the input format string
 * @return none
 */
#define LOG_DEBUG(fmt, ...)                                              \
    do                                                                   \
    {                                                                    \
        fprintf(stdout, MACRO_BLUE "[DEBUG]  " fmt "\n", ##__VA_ARGS__); \
    } while (0)

/**
 * @brief define variable record time &&
          set start time
 * @param [X]: function name
 * @return X_START X_END
 */
#define TIME_START(X)                                  \
    auto X##_START = std::chrono::steady_clock::now(), \
         X##_END = X##_START

/**
 * @brief set end time
 * @param [X]: function name
 * @return none
 */
#define TIME_END(X) \
    X##_END = std::chrono::steady_clock::now()

/**
 * @brief calculate time by nanosecond
 * @param [X]: function name
 * @return none
 */
#define TIME_NSEC(X) \
    std::chrono::duration_cast<std::chrono::nanoseconds>(X##_END - X##_START).count()

/**
 * @brief show time by nanosecond
 * @param [X]: function name
 * @return none
 */
#define TIME_NSEC_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_NSEC(X) \
         << " ns " << std::endl

/**
 * @brief calculate time and show by microsecond
 * @param [X]: variable name
 * @return none
 */
#define TIME_USEC(X) \
    std::chrono::duration_cast<std::chrono::microseconds>(X##_END - X##_START).count()

/**
 * @brief show time by microsecond
 * @param [X]: function name
 * @return none
 */
#define TIME_USEC_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_USEC(X) \
         << " us " << std::endl

/**
 * @brief calculate time and show by millisecond
 * @param [X]: variable name
 * @return none
 */
#define TIME_MSEC(X) \
    std::chrono::duration_cast<std::chrono::milliseconds>(X##_END - X##_START).count()

/**
 * @brief show time by millisecond
 * @param [X]: function name
 * @return none
 */
#define TIME_MSEC_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_MSEC(X) \
         << " ms " << std::endl

/**
 * @brief calculate time and show by second
 * @param [X]: variable name
 * @return none
 */
#define TIME_SEC(X) \
    std::chrono::duration_cast<std::chrono::seconds>(X##_END - X##_START).count()

/**
 * @brief show time by second
 * @param [X]: function name
 * @return none
 */
#define TIME_SEC_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_SEC(X) \
         << " s " << std::endl

/**
 * @brief calculate time and show by minute
 * @param [X]: variable name
 * @return none
 */
#define TIME_MINUTE(X) \
    std::chrono::duration_cast<std::chrono::minutes>(X##_END - X##_START).count()

/**
 * @brief show time by minute
 * @param [X]: function name
 * @return none
 */
#define TIME_MINUTE_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_MINUTE(X) \
         << " min " << std::endl

/**
 * @brief calculate time and show by hour
 * @param [X]: variable name
 * @return none
 */
#define TIME_HOUR(X) \
    std::chrono::duration_cast<std::chrono::hours>(X##_END - X##_START).count()

/**
 * @brief show time by hour
 * @param [X]: function name
 * @return none
 */
#define TIME_HOUR_SHOW(X)                               \
    std::cout << "Func " << #X << " cost : " << TIME_HOUR(X) \
         << " h " << std::endl    
#endif
#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h> // 必须包含以使用异步日志
#include <memory>
#include <string>
#include <chrono>
#include <csignal>
#include <vector>

// 确保启用异步日志支持
#ifndef SPDLOG_ENABLE_ASYNC_LOGGER
#define SPDLOG_ENABLE_ASYNC_LOGGER
#endif

class Logger {
public:
    // 获取单例 logger 实例
    static std::shared_ptr<spdlog::logger>& instance();

    // 初始化日志系统
    // @param log_filename: 日志文件名
    // @param max_log_size_mb: 单个日志文件最大大小 (MB)
    // @param max_files: 保留的最大日志文件数
    // @param level: 初始日志级别
    // @param enable_crash_handler: 是否启用崩溃捕获
    static void init(const std::string& log_filename = "ax_core.log",
                     size_t max_log_size_mb = 10,
                     int max_files = 5,
                     spdlog::level::level_enum level = spdlog::level::info,
                     bool enable_crash_handler = true);

    // 设置全局日志级别
    static void set_level(spdlog::level::level_enum level);

    // 获取当前日志级别
    static spdlog::level::level_enum get_level();

    // 手动刷新所有日志缓冲区
    static void flush();

    // 手动触发崩溃捕获（用于测试）
    static void trigger_crash_capture(int signal_num);

private:
    Logger() = default;
    ~Logger() = default;

    // 信号处理函数
    static void signal_handler(int signum);
    
    // 安装信号处理器
    static void install_crash_handler();

    static std::shared_ptr<spdlog::logger> g_logger;
    static bool g_initialized;
    static bool g_crash_handler_installed;
    static struct sigaction g_old_sig_action[]; 
};

// 便捷宏定义 (兼容 spdlog v1.x)
#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::critical, __VA_ARGS__)

// 带标签的日志宏
#define LOG_TAGGED_TRACE(tag, ...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::trace, "[{}] {}", tag, fmt::format(__VA_ARGS__))
#define LOG_TAGGED_DEBUG(tag, ...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::debug, "[{}] {}", tag, fmt::format(__VA_ARGS__))
#define LOG_TAGGED_INFO(tag, ...)  SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::info, "[{}] {}", tag, fmt::format(__VA_ARGS__))
#define LOG_TAGGED_WARN(tag, ...)  SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::warn, "[{}] {}", tag, fmt::format(__VA_ARGS__))
#define LOG_TAGGED_ERROR(tag, ...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::err, "[{}] {}", tag, fmt::format(__VA_ARGS__))
#define LOG_TAGGED_CRITICAL(tag, ...) SPDLOG_LOGGER_CALL(Logger::instance().get(), spdlog::level::critical, "[{}] {}", tag, fmt::format(__VA_ARGS__))


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
             
#endif // LOGGER_H

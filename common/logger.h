#pragma once

#include <csignal>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>


#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

// ============ 日志配置结构体 ============
struct LogConfig {
    std::string log_dir = "logs";              // 日志目录
    std::string base_filename = "app.log";      // 基础文件名
    spdlog::level::level_enum level = spdlog::level::info; // 默认级别
    bool console_output = true;                  // 是否输出到控制台
    bool async_mode = true;                      // 是否异步
    bool rotate_by_size = true;                  // true: 按大小滚动, false: 按天滚动
    size_t max_file_size = 50 * 1024 * 1024;     // 单文件最大 50MB
    size_t max_files = 10;                        // 最多保留文件数
    size_t async_queue_size = 8192;               // 异步队列大小(需为2的幂)
    size_t async_thread_count = 1;                // 异步后台线程数
    int flush_every_seconds = 3;                  // 定时 flush 间隔
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [tid:%t] %v";
};

// ============ 全局日志管理器(单例) ============
class LogManager {
public:
    static LogManager& Instance() {
        static LogManager instance;
        return instance;
    }

    // 初始化,程序启动时调用一次
    void Init(const LogConfig& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;
        config_ = cfg;

        if (cfg.async_mode) {
            spdlog::init_thread_pool(cfg.async_queue_size, cfg.async_thread_count);
        }

        // 全局定时 flush(独立于每个 logger 的 flush_on 级别触发)
        spdlog::flush_every(std::chrono::seconds(cfg.flush_every_seconds));

        // 创建默认 logger
        default_logger_ = CreateLoggerInternal("default");
        spdlog::set_default_logger(default_logger_);

        initialized_ = true;
    }

    // 获取/创建一个具名 logger(用于按模块分类,例如 "net", "db", "biz")
    std::shared_ptr<spdlog::logger> Get(const std::string& name = "default") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            // 兜底:未显式 Init 时使用默认配置初始化,避免直接崩溃
            mutex_.unlock();
            Init(LogConfig{});
            mutex_.lock();
        }
        auto it = loggers_.find(name);
        if (it != loggers_.end()) {
            return it->second;
        }
        auto logger = CreateLoggerInternal(name);
        return logger;
    }

    // 动态调整某个 logger(或全部)的级别,可在运行时通过管理接口调用
    void SetLevel(spdlog::level::level_enum level, const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        if (name.empty()) {
            for (auto& [n, logger] : loggers_) {
                logger->set_level(level);
            }
        } else {
            auto it = loggers_.find(name);
            if (it != loggers_.end()) {
                it->second->set_level(level);
            }
        }
    }

    // 程序退出前调用,确保异步队列落盘
    void Shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, logger] : loggers_) {
            logger->flush();
        }
        spdlog::shutdown();
        initialized_ = false;
        loggers_.clear();
    }

private:
    LogManager() = default;
    ~LogManager() { if (initialized_) Shutdown(); }
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::shared_ptr<spdlog::logger> CreateLoggerInternal(const std::string& name) {
        std::vector<spdlog::sink_ptr> sinks;

        if (config_.console_output) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(config_.pattern);
            sinks.push_back(console_sink);
        }

        std::string filepath = config_.log_dir + "/" + name + "_" + config_.base_filename;
        spdlog::sink_ptr file_sink;
        if (config_.rotate_by_size) {
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filepath, config_.max_file_size, config_.max_files);
        } else {
            file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                filepath, 0, 0); // 每天 0点0分 切分
        }
        file_sink->set_pattern(config_.pattern);
        sinks.push_back(file_sink);

        std::shared_ptr<spdlog::logger> logger;
        if (config_.async_mode) {
            logger = std::make_shared<spdlog::async_logger>(
                name, sinks.begin(), sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block); // block: 不丢日志; overrun_oldest: 允许丢
        } else {
            logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        }

        logger->set_level(config_.level);
        logger->flush_on(spdlog::level::err); // error 及以上立即 flush
        spdlog::register_logger(logger);
        loggers_[name] = logger;
        return logger;
    }

    std::mutex mutex_;
    bool initialized_ = false;
    LogConfig config_;
    std::shared_ptr<spdlog::logger> default_logger_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
};

// ============ 便捷宏(自动获取默认 logger,可选带模块名) ============
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(LogManager::Instance().Get(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(LogManager::Instance().Get(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(LogManager::Instance().Get(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(LogManager::Instance().Get(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(LogManager::Instance().Get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(LogManager::Instance().Get(), __VA_ARGS__)

// 指定模块 logger 的版本,例如: LOG_MOD_INFO("net", "connect to {}", ip);
#define LOG_MOD_TRACE(mod, ...)    SPDLOG_LOGGER_TRACE(LogManager::Instance().Get(mod), __VA_ARGS__)
#define LOG_MOD_DEBUG(mod, ...)    SPDLOG_LOGGER_DEBUG(LogManager::Instance().Get(mod), __VA_ARGS__)
#define LOG_MOD_INFO(mod, ...)     SPDLOG_LOGGER_INFO(LogManager::Instance().Get(mod), __VA_ARGS__)
#define LOG_MOD_WARN(mod, ...)     SPDLOG_LOGGER_WARN(LogManager::Instance().Get(mod), __VA_ARGS__)
#define LOG_MOD_ERROR(mod, ...)    SPDLOG_LOGGER_ERROR(LogManager::Instance().Get(mod), __VA_ARGS__)
#define LOG_MOD_CRITICAL(mod, ...) SPDLOG_LOGGER_CRITICAL(LogManager::Instance().Get(mod), __VA_ARGS__)

// ============================================================
//  性能计时宏
// ============================================================
#define TIME_START(X)                                          \
    auto X##_START = std::chrono::steady_clock::now();         \
    auto X##_END = X##_START

#define TIME_END(X)  X##_END = std::chrono::steady_clock::now()

#define TIME_USEC(X) \
    std::chrono::duration_cast<std::chrono::microseconds>( \
        X##_END - X##_START).count()

#if 0
#define TIME_USEC_SHOW(X)                               \
    do {                                                \
        std::cout << "Func " << #X << " cost : "        \
                  << TIME_USEC(X) << " us" << std::endl; \
    } while (0)
#endif

#define TIME_USEC_SHOW(X)                               \
        LOG_INFO("Func {} cost : {} us",#X, TIME_USEC(X))

#define TIME_MSEC(X) \
    std::chrono::duration_cast<std::chrono::milliseconds>( \
        X##_END - X##_START).count()

#define TIME_MSEC_SHOW(X)                               \
    do {                                                \
        std::cout << "Func " << #X << " cost : "        \
                  << TIME_MSEC(X) << " ms" << std::endl; \
    } while (0)

#define TIME_SEC(X) \
    std::chrono::duration_cast<std::chrono::seconds>( \
        X##_END - X##_START).count()

#define TIME_SEC_SHOW(X)                                \
    do {                                                \
        std::cout << "Func " << #X << " cost : "        \
                  << TIME_SEC(X) << " s" << std::endl;   \
    } while (0)

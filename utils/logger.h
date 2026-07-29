#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <csignal>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

// ============================================================
//  日志配置
// ============================================================
struct LogConfig {
    std::string log_file = "logs/app.log";
    std::string logger_name = "app";
    spdlog::level::level_enum level = spdlog::level::info;
    std::size_t max_file_size = 50 * 1024 * 1024;  // 50 MB
    std::size_t max_files = 5;
    bool color_console = true;
    bool install_crash_handler = true;
};

// ============================================================
//  Crash handler（signal-safe，写 stderr + 日志文件）
// ============================================================
static int g_crash_log_fd = -1;  // set by InitLogger

inline void InstallCrashHandler() {
    auto handler = +[](int sig) {
        // Build signal number string (signal-safe)
        char buf[32];
        int len = 0;
        buf[len++] = '\n';
        buf[len++] = '[';
        const char* fatal = "FATAL";
        while (*fatal) buf[len++] = *fatal++;
        buf[len++] = ']';
        buf[len++] = ' ';
        const char* sigstr = "signal ";
        while (*sigstr) buf[len++] = *sigstr++;
        if (sig >= 10) buf[len++] = '0' + (sig / 10);
        buf[len++] = '0' + (sig % 10);
        const char* tail = " caught, terminating\n";
        while (*tail) buf[len++] = *tail++;

        write(STDERR_FILENO, buf, len);
        if (g_crash_log_fd != -1) {
            write(g_crash_log_fd, buf, len);
        }
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    };
    std::signal(SIGSEGV, handler);
    std::signal(SIGABRT, handler);
    std::signal(SIGFPE, handler);
    std::signal(SIGILL, handler);
    std::signal(SIGBUS, handler);
    std::signal(SIGTERM, handler);
}

// ============================================================
//  初始化（仅需调用一次）
// ============================================================
inline void InitLogger(const LogConfig& cfg) {
    std::vector<spdlog::sink_ptr> sinks;

    if (cfg.color_console) {
        auto console =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_level(cfg.level);
        sinks.push_back(console);
    }

    auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        cfg.log_file, cfg.max_file_size, cfg.max_files);
    file->set_level(spdlog::level::trace);
    sinks.push_back(file);

    auto logger = std::make_shared<spdlog::logger>(
        cfg.logger_name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");

    // Open crash log fd (signal-safe write bypasses spdlog)
    if (g_crash_log_fd == -1) {
        g_crash_log_fd = open(cfg.log_file.c_str(),
                              O_WRONLY | O_CREAT | O_APPEND, 0644);
    }

    if (cfg.install_crash_handler) {
        InstallCrashHandler();
    }
}

inline void InitLogger(const std::string& log_file = "logs/app.log",
                       spdlog::level::level_enum level = spdlog::level::info) {
    LogConfig cfg;
    cfg.log_file = log_file;
    cfg.level = level;
    InitLogger(cfg);
}

// ============================================================
//  日志宏 — 直接映射 spdlog，零封装开销
// ============================================================
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(spdlog::default_logger_raw(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(spdlog::default_logger_raw(), __VA_ARGS__)

#define LOG_FLUSH()    spdlog::default_logger_raw()->flush()
#define LOG_SHUTDOWN() spdlog::shutdown()

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

#define TIME_USEC_SHOW(X)                               \
    do {                                                \
        std::cout << "Func " << #X << " cost : "        \
                  << TIME_USEC(X) << " us" << std::endl; \
    } while (0)

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

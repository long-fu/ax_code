#pragma once

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ============================================================
//  宏接口（推荐对外使用）
// ============================================================

#define LOG_INIT(logFile, logLevel) Logger::instance().init(logFile, logLevel)

#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(Logger::instance().logger(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(Logger::instance().logger(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(Logger::instance().logger(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(Logger::instance().logger(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(Logger::instance().logger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(Logger::instance().logger(), __VA_ARGS__)


#define LOG_FLUSH()     Logger::instance().flush()
#define LOG_SHUTDOWN()  Logger::instance().shutdown()

// ============================================================
//  Logger 单例类
// ============================================================
class Logger {
 public:
  static Logger& instance() {
    static Logger inst;
    return inst;
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  struct Config {
    std::string log_file = "logs/app.log";
    std::string logger_name = "app";
    spdlog::level::level_enum level = spdlog::level::info;
    std::size_t max_file_size = 50 * 1024 * 1024;  // 50 MB
    std::size_t max_files = 5;
    std::size_t async_queue_size = 8192;
    bool color_console = true;
    bool install_crash_handler = true;
  };

  void init(const std::string& log_file = "logs/app.log",
            spdlog::level::level_enum level = spdlog::level::info) {
    Config cfg;
    cfg.log_file = log_file;
    cfg.level = level;
    init(cfg);
  }

  void init(const Config& cfg) {
    spdlog::init_thread_pool(cfg.async_queue_size, 1);

    std::vector<spdlog::sink_ptr> sinks;

    if (cfg.color_console) {
      auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console->set_level(cfg.level);
      sinks.push_back(console);
    }

    auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        cfg.log_file, cfg.max_file_size, cfg.max_files);
    file->set_level(spdlog::level::trace);
    sinks.push_back(file);

    logger_ = std::make_shared<spdlog::async_logger>(
        cfg.logger_name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger_->set_level(spdlog::level::trace);
    logger_->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger_);
    spdlog::set_pattern("[%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");

    if (cfg.install_crash_handler) {
      installSignalHandlers();
    }

    logger_->info("Logger initialized. file={} level={}", cfg.log_file,
                  spdlog::level::to_string_view(cfg.level));
  }

  std::shared_ptr<spdlog::logger>& logger() { return logger_; }

  void flush() {
    if (logger_) logger_->flush();
  }

  void shutdown() {
    std::call_once(m_shutdown_flag_, [this]() {
      if (logger_) {
        logger_->info("Logger shutting down.");
        logger_->flush();
      }
      spdlog::shutdown();
    });
  }

 private:
  Logger() = default;
  ~Logger() { shutdown(); }

  std::shared_ptr<spdlog::logger> logger_;
  std::once_flag m_shutdown_flag_;
  mutable std::mutex m_shutdown_mutex_;

  static void signalHandler(int sig) {
    const char* name = "UNKNOWN";
    switch (sig) {
      case SIGSEGV:
        name = "SIGSEGV";
        break;
      case SIGABRT:
        name = "SIGABRT";
        break;
      case SIGFPE:
        name = "SIGFPE";
        break;
      case SIGILL:
        name = "SIGILL";
        break;
      case SIGBUS:
        name = "SIGBUS";
        break;
      case SIGTERM:
        name = "SIGTERM";
        break;
    }

    auto& logger_inst = instance();
    {
      std::lock_guard<std::mutex> lock(logger_inst.m_shutdown_mutex_);
      if (logger_inst.logger_) {
        logger_inst.logger_->critical("======== CRASH: signal {} ({}) ========", sig, name);
        logger_inst.logger_->flush();
      }
    }
    // Use try-catch in case thread pool is already gone
    try {
      spdlog::shutdown();
    } catch (...) {
      // Thread pool already destroyed, ignore
    }

    std::signal(sig, SIG_DFL);
    std::raise(sig);
  }

  static void installSignalHandlers() {
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGBUS, signalHandler);
    std::signal(SIGTERM, signalHandler);
  }
};

/**
 * @brief 性能计时宏
 */
#define TIME_START(X)                                  \
  auto X##_START = std::chrono::steady_clock::now(),   \
       X##_END = X##_START

#define TIME_END(X) X##_END = std::chrono::steady_clock::now()

#define TIME_USEC(X) \
  std::chrono::duration_cast<std::chrono::microseconds>(X##_END - X##_START).count()

#define TIME_USEC_SHOW(X)                                         \
  std::cout << "Func " << #X << " cost : " << TIME_USEC(X)        \
            << " us " << std::endl

#define TIME_MSEC(X) \
  std::chrono::duration_cast<std::chrono::milliseconds>(X##_END - X##_START).count()

#define TIME_MSEC_SHOW(X)                                         \
  std::cout << "Func " << #X << " cost : " << TIME_MSEC(X)        \
            << " ms " << std::endl

#define TIME_SEC(X) \
  std::chrono::duration_cast<std::chrono::seconds>(X##_END - X##_START).count()

#define TIME_SEC_SHOW(X)                                          \
  std::cout << "Func " << #X << " cost : " << TIME_SEC(X)         \
            << " s " << std::endl

#define TIME_MINUTE(X) \
  std::chrono::duration_cast<std::chrono::minutes>(X##_END - X##_START).count()

#define TIME_MINUTE_SHOW(X)                                       \
  std::cout << "Func " << #X << " cost : " << TIME_MINUTE(X)      \
            << " min " << std::endl

#define TIME_HOUR(X) \
  std::chrono::duration_cast<std::chrono::hours>(X##_END - X##_START).count()

#define TIME_HOUR_SHOW(X)                                         \
  std::cout << "Func " << #X << " cost : " << TIME_HOUR(X)        \
            << " h " << std::endl

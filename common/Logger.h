#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>

#include <memory>
#include <string>
#include <vector>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <cstdlib>
#include <chrono>


// Logger.h  
#pragma once  
  
#include <spdlog/spdlog.h>  
#include <spdlog/async.h>  
#include <spdlog/sinks/stdout_color_sinks.h>  
#include <spdlog/sinks/rotating_file_sink.h>  
#include <memory>  
#include <string>  
#include <csignal>  
#include <cstdlib>  
  
// ============================================================  
//  宏接口（推荐对外使用）  
// ============================================================  


#define LOG_INIT(logFile, logLevel) \  
    Logger::instance().init(logFile, logLevel)  
  
#define LOG_TRACE(...)    Logger::instance().logger()->trace(__VA_ARGS__)  
#define LOG_DEBUG(...)    Logger::instance().logger()->debug(__VA_ARGS__)  
#define LOG_INFO(...)     Logger::instance().logger()->info(__VA_ARGS__)  
#define LOG_WARN(...)     Logger::instance().logger()->warn(__VA_ARGS__)  
#define LOG_ERROR(...)    Logger::instance().logger()->error(__VA_ARGS__)  
#define LOG_CRITICAL(...) Logger::instance().logger()->critical(__VA_ARGS__)  
  
// 带源码位置（文件名:行号）  
#define LOG_INFO_LOC(...)     SPDLOG_LOGGER_INFO(Logger::instance().logger(), __VA_ARGS__)  
#define LOG_ERROR_LOC(...)    SPDLOG_LOGGER_ERROR(Logger::instance().logger(), __VA_ARGS__)  
  
#define LOG_FLUSH()  Logger::instance().flush()  
#define LOG_SHUTDOWN() Logger::instance().shutdown()  
  
// ============================================================  
//  Logger 单例类  
// ============================================================  
class Logger {  
public:  
    // Meyer's Singleton：线程安全，C++11 保证  
    static Logger& instance() {  
        static Logger inst;  
        return inst;  
    }  
  
    // 禁止拷贝/移动  
    Logger(const Logger&)            = delete;  
    Logger& operator=(const Logger&) = delete;  
    Logger(Logger&&)                 = delete;  
    Logger& operator=(Logger&&)      = delete;  
  
    struct Config {  
        std::string logFile    = "logs/app.log";  
        std::string loggerName = "app";  
        spdlog::level::level_enum level = spdlog::level::info;  
        std::size_t maxFileSize  = 50 * 1024 * 1024; // 50 MB  
        std::size_t maxFiles     = 5;  
        std::size_t asyncQueueSize = 8192;  
        bool        colorConsole   = true;  
        bool        installCrashHandler = true;  
    };  
  
    void init(const std::string& logFile = "logs/app.log",  
              spdlog::level::level_enum level = spdlog::level::info) {  
        Config cfg;  
        cfg.logFile = logFile;  
        cfg.level   = level;  
        init(cfg);  
    }  
  
    void init(const Config& cfg) {  
        // 异步线程池  
        spdlog::init_thread_pool(cfg.asyncQueueSize, 1);  
  
        std::vector<spdlog::sink_ptr> sinks;  
  
        // 控制台 sink  
        if (cfg.colorConsole) {  
            auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();  
            console->set_level(cfg.level);  
            sinks.push_back(console);  
        }  
  
        // 滚动文件 sink  
        auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(  
            cfg.logFile, cfg.maxFileSize, cfg.maxFiles);  
        file->set_level(spdlog::level::trace); // 文件记录全量  
        sinks.push_back(file);  
  
        logger_ = std::make_shared<spdlog::async_logger>(  
            cfg.loggerName,  
            sinks.begin(), sinks.end(),  
            spdlog::thread_pool(),  
            spdlog::async_overflow_policy::block);  
  
        logger_->set_level(spdlog::level::trace);  
        logger_->flush_on(spdlog::level::warn); // warn+ 立即 flush  
  
        spdlog::set_default_logger(logger_);  
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [tid:%t] %v");  
  
        if (cfg.installCrashHandler) {  
            installSignalHandlers();  
        }  
  
        logger_->info("Logger initialized. file={} level={}",  
                      cfg.logFile, spdlog::level::to_string_view(cfg.level));  
    }  
  
    std::shared_ptr<spdlog::logger>& logger() {  
        return logger_;  
    }  
  
    void flush() {  
        if (logger_) logger_->flush();  
    }  
  
    void shutdown() {  
        if (logger_) {  
            logger_->info("Logger shutting down.");  
            logger_->flush();  
        }  
        spdlog::shutdown();  
    }  
  
private:  
    Logger() = default;  
    ~Logger() { shutdown(); }  
  
    std::shared_ptr<spdlog::logger> logger_;  
  
    // ----------------------------------------------------------  
    //  崩溃信号处理  
    // ----------------------------------------------------------  
    static void signalHandler(int sig) {  
        const char* name = "UNKNOWN";  
        switch (sig) {  
            case SIGSEGV: name = "SIGSEGV"; break;  
            case SIGABRT: name = "SIGABRT"; break;  
            case SIGFPE:  name = "SIGFPE";  break;  
            case SIGILL:  name = "SIGILL";  break;  
            case SIGBUS:  name = "SIGBUS";  break;  
            case SIGTERM: name = "SIGTERM"; break;  
        }  
  
        // 用 critical 记录，立即 flush  
        if (auto& l = instance().logger_) {  
            l->critical("======== CRASH: signal {} ({}) ========", sig, name);  
            l->flush();  
        }  
        spdlog::shutdown();  
  
        // 恢复默认行为，产生 core dump  
        std::signal(sig, SIG_DFL);  
        std::raise(sig);  
    }  
  
    static void installSignalHandlers() {  
        std::signal(SIGSEGV, signalHandler);  
        std::signal(SIGABRT, signalHandler);  
        std::signal(SIGFPE,  signalHandler);  
        std::signal(SIGILL,  signalHandler);  
        std::signal(SIGBUS,  signalHandler);  
        std::signal(SIGTERM, signalHandler);  
    }  
};  

/**
 * @brief 性能计时宏
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
 
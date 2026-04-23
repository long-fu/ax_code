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

/**
 * @brief 日志级别枚举
 */
enum class LogLevel
{
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    critical = 5
};

/**
 * @brief SPDLog工具类封装
 */
class Logger
{
public:
    /**
     * @brief 获取单例实例
     */
    static Logger &GetInstance()
    {
        static Logger instance;
        return instance;
    }

    /**
     * @brief 初始化日志系统
     * @param log_path 日志目录路径
     * @param max_size_mb 单个日志文件最大大小(MB)
     * @param max_files 保留的日志文件数量
     * @param enable_console 是否输出到控制台
     * @return 初始化是否成功
     */
    bool Init(const std::string &log_path = "./logs",
              size_t max_size_mb = 10,
              int max_files = 5,
              bool enable_console = true)
    {
        if (m_initialized)
        {
            return true;
        }

        // 创建日志目录
        mkdir(log_path.c_str(), 0755);

        try
        {
            std::vector<spdlog::sink_ptr> sinks;

            // 文件日志sink (按大小轮转)
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_path + "/ax_core.log",
                max_size_mb * 1024 * 1024,
                max_files);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);

            // 控制台日志sink (带颜色)
            if (enable_console)
            {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(spdlog::level::trace);
                sinks.push_back(console_sink);
            }

            // 创建异步logger
            spdlog::init_thread_pool(8192, 1);
            auto tp = spdlog::thread_pool();

            m_logger = std::make_shared<spdlog::async_logger>(
                "ax_core",
                sinks.begin(),
                sinks.end(),
                tp,
                spdlog::async_overflow_policy::block);

            // 设置日志级别
            m_logger->set_level(spdlog::level::trace);

            // 设置日志格式: [时间] [级别] [线程ID] 消息
            m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");

            // 注册logger
            spdlog::register_logger(m_logger);

            // 安装崩溃捕获
            InstallCrashHandler();

            m_initialized = true;
            Info("Logger initialized - path: {}, max_size: {}MB, max_files: {}",
                 log_path, max_size_mb, max_files);
            return true;
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            fprintf(stderr, "Logger init failed: %s\n", ex.what());
            return false;
        }
    }

    /**
     * @brief 设置日志级别
     */
    void SetLevel(LogLevel level)
    {
        if (m_logger)
        {
            m_logger->set_level(static_cast<spdlog::level::level_enum>(level));
        }
    }

    /**
     * @brief 刷新日志缓冲区
     */
    void Flush()
    {
        if (m_logger)
        {
            m_logger->flush();
        }
    }

    /**
     * @brief 关闭日志系统
     */
    void Shutdown()
    {
        if (m_initialized)
        {
            Flush();
            spdlog::drop_all();
            m_initialized = false;
        }
    }

    /**
     * @brief 获取spdlog指针
     */
    std::shared_ptr<spdlog::logger> GetSpdlog() const
    {
        return m_logger;
    }

    // 日志输出方法
    void Trace(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->trace("{}", buf);
    }

    void Debug(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->debug("{}", buf);
    }

    void Info(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->info("{}", buf);
    }

    void Warn(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->warn("{}", buf);
    }

    void Error(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->error("{}", buf);
    }

    void Critical(const char *fmt, ...)
    {
        if (!m_logger)
            return;
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_logger->critical("{}", buf);
    }

private:
    Logger() : m_initialized(false) {}
    ~Logger()
    {
        Shutdown();
    }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /**
     * @brief 安装崩溃信号处理器
     */
    void InstallCrashHandler()
    {
        std::signal(SIGSEGV, SignalHandler);
        std::signal(SIGABRT, SignalHandler);
        std::signal(SIGFPE, SignalHandler);
        std::signal(SIGILL, SignalHandler);

        // C++异常终止器
        std::set_terminate([]()
                           {
            auto logger = spdlog::get("ax_core");
            if (logger)
            {
                logger->critical("[CRASH] Unhandled C++ exception!");
                PrintStackTrace();
            }
            std::abort(); });
    }

    static void SignalHandler(int signum)
    {
        const char *msg = nullptr;
        switch (signum)
        {
        case SIGSEGV:
            msg = "Segmentation Fault";
            break;
        case SIGABRT:
            msg = "Abort Signal";
            break;
        case SIGFPE:
            msg = "Floating Point Exception";
            break;
        case SIGILL:
            msg = "Illegal Instruction";
            break;
        default:
            msg = "Unknown Signal";
            break;
        }

        auto logger = spdlog::get("ax_core");
        if (logger)
        {
            logger->critical("[CRASH] Signal {}: {}", signum, msg);
            PrintStackTrace();
        }

        std::signal(signum, SIG_DFL);
        raise(signum);
    }

    static void PrintStackTrace()
    {
        const int MAX_FRAMES = 64;
        void *buffer[MAX_FRAMES];

        int frames = backtrace(buffer, MAX_FRAMES);
        if (frames == 0)
            return;

        char **symbols = backtrace_symbols(buffer, frames);
        if (!symbols)
            return;

        auto logger = spdlog::get("ax_core");

        for (int i = 0; i < frames; ++i)
        {
            std::string symbol(symbols[i]);
            size_t start = symbol.find('(');
            size_t end = symbol.find('+');

            std::string demangled = symbol;
            if (start != std::string::npos && end != std::string::npos && end > start + 1)
            {
                std::string func = symbol.substr(start + 1, end - start - 1);
                int status = 0;
                char *dem = abi::__cxa_demangle(func.c_str(), nullptr, nullptr, &status);
                if (status == 0 && dem)
                {
                    demangled = dem;
                    free(dem);
                }
            }

            if (logger)
                logger->trace("  #{} {}", i, demangled);
        }
        free(symbols);
    }

    std::shared_ptr<spdlog::async_logger> m_logger;
    bool m_initialized;
};

// 便捷宏定义
#define LOG_TRACE(fmt, ...) Logger::GetInstance().Trace(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::GetInstance().Debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) Logger::GetInstance().Info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::GetInstance().Warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::GetInstance().Error(fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) Logger::GetInstance().Critical(fmt, ##__VA_ARGS__)

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
 
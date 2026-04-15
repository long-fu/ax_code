#include "Logger.h"
#include <spdlog/logger.h>
#include <spdlog/common.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <cstring>
#include <signal.h>
#include <sys/time.h>
#include <sstream>
#include <atomic>

namespace fs = std::filesystem;

std::shared_ptr<spdlog::logger> Logger::g_logger = nullptr;
bool Logger::g_initialized = false;
bool Logger::g_crash_handler_installed = false;
struct sigaction Logger::g_old_sig_action[6] = {0}; 

std::shared_ptr<spdlog::logger>& Logger::instance() {
    if (!g_initialized) {
        init();
    }
    return g_logger;
}

void Logger::init(const std::string& log_filename,
                  size_t max_log_size_mb,
                  int max_files,
                  spdlog::level::level_enum level,
                  bool enable_crash_handler) {
    if (g_initialized) {
        spdlog::warn("Logger already initialized, ignoring subsequent init calls.");
        return;
    }

    try {
        fs::create_directories("logs");
        std::string log_path = "logs/" + log_filename;

        // 1. 初始化全局异步线程池 (spdlog v1.17+ 推荐方式)
        // 参数: 队列大小 (8192), 线程数 (CPU核心数或固定值如4)
        spdlog::init_thread_pool(8192, 4);

        // 2. 创建控制台 sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        // 3. 创建文件 rotating sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path,
            max_log_size_mb * 1024 * 1024,
            max_files
        );
        file_sink->set_level(level);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        
        // 4. 创建异步 logger (自动使用全局线程池)
        // 注意：在 v1.17 中，如果调用了 init_thread_pool，这里不需要再传 thread_pool 参数
        g_logger = std::make_shared<spdlog::async_logger>(
            "ax_core_logger",
            sinks.begin(),
            sinks.end(),
            spdlog::async_overflow_policy::block
        );

        // 5. 设置全局配置
        spdlog::set_default_logger(g_logger);
        spdlog::set_level(level);
        
        // 6. 启用自动刷新
        spdlog::flush_on(level);
        spdlog::flush_every(std::chrono::seconds(5));

        g_initialized = true;
        spdlog::info("Logger initialized successfully. Log file: {}", log_path);

        // 7. 安装崩溃处理器
        if (enable_crash_handler && !g_crash_handler_installed) {
            install_crash_handler();
        }

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        // 降级为同步 stderr logger
        g_logger = spdlog::stderr_color_mt("fallback_logger");
        g_initialized = true;
    }
}

void Logger::install_crash_handler() {
    const int signals[] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS, SIGSYS};
    const char* signal_names[] = {"SIGSEGV", "SIGABRT", "SIGILL", "SIGFPE", "SIGBUS", "SIGSYS"};

    for (int i = 0; i < 6; ++i) {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        
        if (sigaction(signals[i], &sa, &g_old_sig_action[i]) == 0) {
            // 可以在调试模式下打印，生产环境建议注释掉以免干扰日志
            // spdlog::info("Installed crash handler for {}: {}", signal_names[i], signals[i]);
        } else {
            spdlog::warn("Failed to install handler for {}: {}", signal_names[i], signals[i]);
        }
    }
    g_crash_handler_installed = true;
}

void Logger::signal_handler(int signum) {
    // 信号处理函数必须是异步安全的
    // 避免调用 C++ STL 非异步安全函数 (如 std::cout, std::string 等)
    
    const char* msg = "\n\n=== CRASH DETECTED ===\n";
    write(STDERR_FILENO, msg, strlen(msg));
    
    // 尝试写入堆栈信息到 stderr
    void* array[100];
    int size = backtrace(array, 100);
    
    fprintf(stderr, "Backtrace (%d frames):\n", size);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    // 如果 spdlog 已初始化，尝试强制刷新并关闭
    if (g_initialized && g_logger) {
        try {
            // 停止异步线程池，防止新日志进入
            spdlog::shutdown(); 
            
            // 再次尝试写入文件（此时 spdlog 可能已转为同步模式或已关闭）
            // 由于 spdlog 内部状态不确定，最安全的是依赖上面的 stderr 输出
        } catch (...) {
            // 忽略异常
        }
    }

    // 恢复默认行为或直接退出
    _exit(1);
}

void Logger::trigger_crash_capture(int signal_num) {
    signal_handler(signal_num);
}

void Logger::set_level(spdlog::level::level_enum level) {
    if (g_logger) {
        g_logger->set_level(level);
        spdlog::set_level(level);
    }
}

spdlog::level::level_enum Logger::get_level() {
    return g_logger ? g_logger->level() : spdlog::level::info;
}

void Logger::flush() {
    if (g_logger) {
        g_logger->flush();
    }
    // spdlog::flush_all();
}

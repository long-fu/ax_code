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
#include <memory>

namespace fs = std::filesystem;

std::shared_ptr<spdlog::logger> Logger::g_logger = nullptr;
bool Logger::g_initialized = false;
bool Logger::g_crash_handler_installed = false;
struct sigaction Logger::g_old_sig_action[6] = {0}; // 保存 SIGSEGV, SIGABRT 等

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

        // 创建控制台 sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        // 创建文件 rotating sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path,
            max_log_size_mb * 1024 * 1024,
            max_files
        );
        file_sink->set_level(level);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        
        auto thread_pool = std::make_shared<spdlog::thread_pool>(8, 1 << 20);
        
        g_logger = std::make_shared<spdlog::async_logger>(
            "ax_core_logger",
            sinks.begin(),
            sinks.end(),
            thread_pool,
            spdlog::async_overflow_policy::block
        );

        spdlog::set_default_logger(g_logger);
        spdlog::set_level(level);
        spdlog::flush_on(level);
        spdlog::flush_every(std::chrono::seconds(5));

        g_initialized = true;
        spdlog::info("Logger initialized successfully. Log file: {}", log_path);

        // 安装崩溃处理器
        if (enable_crash_handler && !g_crash_handler_installed) {
            install_crash_handler();
        }

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
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
            spdlog::info("Installed crash handler for {}: {}", signal_names[i], signals[i]);
        } else {
            spdlog::warn("Failed to install handler for {}: {}", signal_names[i], signals[i]);
        }
    }
    g_crash_handler_installed = true;
}

void Logger::signal_handler(int signum) {
    // 注意：在信号处理函数中只能调用异步安全的函数
    // 这里我们尝试直接写文件，因为 spdlog 的 async 机制可能不安全
    
    const char* msg = "=== CRASH DETECTED ===\n";
    write(STDERR_FILENO, msg, strlen(msg));
    
    // 尝试写入日志文件（如果已初始化）
    if (g_initialized && g_logger) {
        // 由于 spdlog 是异步的，我们需要强制刷新并等待
        // 但为了安全，我们只记录最基本信息
        try {
            g_logger->flush();
            
            // 生成堆栈信息
            void* array[100];
            int size = backtrace(array, 100);
            
            // 将堆栈写入 stderr 以便查看
            fprintf(stderr, "Backtrace (%d frames):\n", size);
            backtrace_symbols_fd(array, size, STDERR_FILENO);
            
            // 尝试解析符号名称（简单版）
            char** symbols = backtrace_symbols(array, size);
            if (symbols) {
                for (int i = 0; i < size; i++) {
                    // 尝试 demangle C++ 符号
                    int status;
                    char* demangled = abi::__cxa_demangle(symbols[i], NULL, NULL, &status);
                    if (status == 0 && demangled) {
                        fprintf(stderr, "%d: %s\n", i, demangled);
                        free(demangled);
                    } else {
                        fprintf(stderr, "%d: %s\n", i, symbols[i]);
                    }
                }
                free(symbols);
            }
            
            // 再次强制刷新
            g_logger->flush();
            spdlog::shutdown(); // 停止异步线程
            
        } catch (...) {
            // 忽略异常，防止二次崩溃
        }
    }

    // 恢复原来的信号处理函数（如果需要）
    // 这里直接终止程序
    _exit(1);
}

std::string Logger::generate_stack_trace() {
    void* array[100];
    int size = backtrace(array, 100);
    char** symbols = backtrace_symbols(array, size);
    
    std::ostringstream oss;
    oss << "Stack trace (" << size << " frames):\n";
    
    if (symbols) {
        for (int i = 0; i < size; i++) {
            int status;
            char* demangled = abi::__cxa_demangle(symbols[i], NULL, NULL, &status);
            if (status == 0 && demangled) {
                oss << "  #" << i << ": " << demangled << "\n";
                free(demangled);
            } else {
                oss << "  #" << i << ": " << symbols[i] << "\n";
            }
        }
        free(symbols);
    }
    
    return oss.str();
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
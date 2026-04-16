// #include "Logger.h"
// #include <iostream>
// #include <unistd.h>
// #include <sys/stat.h>
// #include <execinfo.h>
// #include <cxxabi.h>
// #include <cstdlib>
// #include <cstring>
// #include <spdlog/async_logger.h>




// // std::weak_ptr<spdlog::logger> Logger::s


// // --- CrashHandler 实现 ---
// void CrashHandler::install() {
//     // 注册信号处理函数
//     // SIGSEGV: 段错误 (空指针解引用等)
//     // SIGABRT: 中止 (assert 失败或 abort())
//     // SIGFPE: 浮点异常 (除零)
//     // SIGILL: 非法指令
//     signal(SIGSEGV, signal_handler);
//     signal(SIGABRT, signal_handler);
//     signal(SIGFPE, signal_handler);
//     signal(SIGILL, signal_handler);
//     // 注册 C++ 未处理异常终止器
//     std::set_terminate([]() {
//         try {
//             auto logger = spdlog::get("app_logger");
//             if (logger) {
//                 logger->critical("【CRASH】检测到未处理的 C++ 异常！");
//                 print_stack_trace();
//             } else {
//                 // 如果 logger 还没初始化成功，直接打印到 stderr
//                 std::cerr << "【CRASH】未处理异常，但 Logger 未初始化。\n";
//                 print_stack_trace();
//             }
//         } catch (...) {
//             // 防止在崩溃处理中再次抛出异常
//         }
//         std::abort();
//     });
// }
// void CrashHandler::signal_handler(int signum) {
//     const char* msg = nullptr;
//     switch (signum) {
//         case SIGSEGV: msg = "Segmentation Fault (内存访问违规)"; break;
//         case SIGABRT: msg = "Abort Signal (通常由 assert 触发)"; break;
//         case SIGFPE:  msg = "Floating Point Exception (除零等)"; break;
//         case SIGILL:  msg = "Illegal Instruction"; break;
//         default:      msg = "Unknown Signal"; break;
//     }
//     auto logger = spdlog::get("app_logger");
//     if (logger) {
//         logger->critical("【CRASH】收到致命信号 {}: {}", signum, msg);
//         print_stack_trace();
//     } else {
//         std::cerr << "【CRASH】收到致命信号 " << signum << ": " << msg << "\n";
//         print_stack_trace();
//     }
//     // 恢复默认行为并重新发送信号，以便系统生成 Core Dump
//     signal(signum, SIG_DFL);
//     raise(signum);
// }
// void CrashHandler::print_stack_trace() {
//     const int MAX_FRAMES = 64;
//     void* buffer[MAX_FRAMES];
    
//     // 获取调用栈
//     int frames = backtrace(buffer, MAX_FRAMES);
//     if (frames == 0) return;
//     // 获取符号字符串
//     char** symbols = backtrace_symbols(buffer, frames);
//     if (symbols == nullptr) return;
//     auto logger = spdlog::get("app_logger");
//     std::ostream* out_stream = nullptr;
    
//     // 尝试写入日志，如果 logger 不可用则回退到 stderr
//     if (logger) {
//         logger->critical("【STACK TRACE】开始打印堆栈 (共 {} 帧):", frames);
//         out_stream = &std::cout; // 这里我们手动控制输出格式，或者直接用 logger
//     } else {
//         std::cerr << "【STACK TRACE】开始打印堆栈 (共 " << frames << " 帧):\n";
//     }
//     for (int i = 0; i < frames; ++i) {
//         std::string symbol(symbols[i]);
        
//         // 尝试解析函数名 (格式通常为: ./binary(function+0xoffset) [0xaddress])
//         size_t start = symbol.find('(');
//         size_t end = symbol.find('+');
        
//         std::string demangled_name = symbol;
//         bool has_func = false;
//         if (start != std::string::npos && end != std::string::npos && end > start + 1) {
//             std::string func_name = symbol.substr(start + 1, end - start - 1);
            
//             int status = 0;
//             char* demangled = abi::__cxa_demangle(func_name.c_str(), nullptr, nullptr, &status);
            
//             if (status == 0 && demangled) {
//                 demangled_name = demangled;
//                 free(demangled);
//                 has_func = true;
//             }
//         }
//         if (has_func) {
//             if (logger) logger->critical("  #{} {}", i, demangled_name);
//             else std::cerr << "  #" << i << " " << demangled_name << "\n";
//         } else {
//             if (logger) logger->critical("  #{} {}", i, symbol);
//             else std::cerr << "  #" << i << " " << symbol << "\n";
//         }
//     }
//     free(symbols);
// }
// // --- Logger 实现 ---
// std::shared_ptr<Logger> Logger::instance() {
//     static std::shared_ptr<Logger> inst = std::make_shared<Logger>();
//     return inst;
// }
// bool Logger::init(const std::string& log_path, size_t max_size_mb, int max_files, bool enable_console) {
//     if (m_initialized) {
//         spdlog::warn("Logger 已初始化，跳过重复初始化");
//         return true;
//     }
//     // 创建日志目录
//     mkdir(log_path.c_str(), 0755);
//     try {
//         std::vector<spdlog::sink_ptr> sinks;
//         // 1. 文件日志 (按大小轮转 Rotating File Sink)
//         // spdlog 1.17 支持 rotating_file_sink_mt
//         auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
//             log_path + "/app.log", 
//             max_size_mb * 1024 * 1024, 
//             max_files
//         );
//         sinks.push_back(file_sink);
//         // 2. 控制台日志 (带颜色)
//         if (enable_console) {
//             auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
//             console_sink->set_level(spdlog::level::trace);
//             sinks.push_back(console_sink);
//         }
//         // 创建异步 Logger (Thread Pool 自动管理)
//         // 注意：spdlog 1.17 推荐使用 async_logger
//         // auto thread_pool = spdlog::details::thread_pool();
//         spdlog::init_thread_pool(8192,1);
//         auto thread_pool = spdlog::thread_pool();
        
//         // std::weak_ptr<details::thread_pool> tp = std::make_sha
//         m_logger = std::make_shared<spdlog::async_logger>(
//             "app_logger", 
//             sinks.begin(), 
//             sinks.end(), 
//             thread_pool,
//             spdlog::async_overflow_policy::block
//         );
//         // 设置全局级别和格式
//         m_logger->set_level(spdlog::level::trace);
//         // 格式：[时间] [级别] [线程ID] 消息
//         m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] %v");
//         // 注册为 spdlog 默认实例
//         spdlog::register_logger(m_logger);
//         // 安装崩溃捕获
//         CrashHandler::install();
//         m_initialized = true;
//         spdlog::info("Logger 初始化成功 -> Path: {}, MaxSize: {}MB", log_path, max_size_mb);
//         return true;
//     } catch (const spdlog::spdlog_ex& ex) {
//         std::cerr << "Logger 初始化失败: " << ex.what() << std::endl;
//         return false;
//     }
// }
// std::shared_ptr<spdlog::logger> Logger::get_spdlog() const {
//     return m_logger;
// }
// void Logger::shutdown() {
//     if (!m_initialized) return;
//     spdlog::drop_all(); // 停止所有 logger 并刷新缓冲区
//     m_initialized = false;
// }
// Logger::~Logger() {
//     shutdown();
// }

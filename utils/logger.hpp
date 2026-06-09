// #pragma once
// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/rotating_file_sink.h>
// #include <spdlog/sinks/stdout_color_sinks.h>
// #include <spdlog/async.h>
// #include <memory>
// #include <string>
// #include <vector>

// // 全局 logger，程序启动时调用 InitLogger() 一次，之后直接用宏访问。
// //
// // 用法：
// //   NLOG_INFO("started on {}", addr);
// //   NLOG_WARN("device offline: {}", id);
// //   NLOG_ERROR("parse failed: {}", e.what());

// #define LOG_NAME "tracker"

// #define NLOG_TRACE(...)    spdlog::get(LOG_NAME)->trace(__VA_ARGS__)
// #define NLOG_DEBUG(...)    spdlog::get(LOG_NAME)->debug(__VA_ARGS__)
// #define NLOG_INFO(...)     spdlog::get(LOG_NAME)->info(__VA_ARGS__)
// #define NLOG_WARN(...)     spdlog::get(LOG_NAME)->warn(__VA_ARGS__)
// #define NLOG_ERROR(...)    spdlog::get(LOG_NAME)->error(__VA_ARGS__)
// #define NLOG_CRITICAL(...) spdlog::get(LOG_NAME)->critical(__VA_ARGS__)

// inline void InitLogger(const std::string& log_file = LOG_NAME".log",
//                        spdlog::level::level_enum level = spdlog::level::info) {
//     // ── sinks ──────────────────────────────────────────────────────────────
//     // 1. 彩色终端输出
//     auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
//     console_sink->set_level(level);

//     // 2. 滚动文件：单文件最大 10MB，保留 5 个
//     auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
//         log_file, 10 * 1024 * 1024, 5);
//     file_sink->set_level(level);

//     // ── logger ─────────────────────────────────────────────────────────────
//     auto logger = std::make_shared<spdlog::logger>(
//         LOG_NAME,
//         spdlog::sinks_init_list{console_sink, file_sink});

//     logger->set_level(level);
//     // 格式：[2026-05-14 10:23:01.123] [node_exporter] [info] message
//     logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
//     // 错误级别立即 flush，其他批量 flush（每 3 秒）
//     logger->flush_on(spdlog::level::err);
//     spdlog::flush_every(std::chrono::seconds(3));

//     spdlog::register_logger(logger);
//     spdlog::set_default_logger(logger);
// }

#include "pch.h"
#include "Logger.h"
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

void Logger::Init(const std::string& appName, Option option)
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    auto logDir = std::filesystem::path(buf).parent_path() / "log";
    std::filesystem::create_directories(logDir);

    auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    std::shared_ptr<spdlog::sinks::sink> fileSink;

    if (option == Option::Own)
    {
        // Each process writes to its own log/<appName>.log (rotating, 5MB x3)
        auto path = logDir / (appName + ".log");
        fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            path.string(), 1024 * 1024 * 5, 3);
    }
    else
    {
        // All processes append to the same log/<appName>.log
        auto path = logDir / (appName + ".log");
        fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path.string(), false); // false = append, not truncate
    }

    auto logger = std::make_shared<spdlog::logger>(appName,
        spdlog::sinks_init_list{ msvcSink, fileSink });

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::debug);

    spdlog::set_default_logger(logger);
}

void Logger::Share()
{
    // Called by a loaded DLL: nothing to do here because Get() always
    // returns common.dll's own default logger directly.
    // Kept for explicit documentation at DLL_PROCESS_ATTACH call sites.
}

void Logger::SetLevel(spdlog::level::level_enum level)
{
    spdlog::default_logger()->set_level(level);
}

void Logger::Flush()
{
    spdlog::default_logger()->flush();
}

std::shared_ptr<spdlog::logger> Logger::Get()
{
    return spdlog::default_logger();
}

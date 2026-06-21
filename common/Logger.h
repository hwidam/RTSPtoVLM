#pragma once

#ifdef COMMON_EXPORTS
#define COMMON_API __declspec(dllexport)
#else
#define COMMON_API __declspec(dllimport)
#endif

#include <spdlog/spdlog.h>

class Logger
{
public:
    Logger() = delete;

    // Option::Own   - each project writes to its own  log/<appName>.log
    // Option::Shared - all projects append to one shared log/<sharedName>.log
    enum class Option { Own, Shared };

    static COMMON_API void Init(const std::string& appName, Option option = Option::Own);

    // Call at DLL_PROCESS_ATTACH in loaded DLLs to adopt the EXE's logger
    static COMMON_API void Share();

    static COMMON_API void SetLevel(spdlog::level::level_enum level);
    static COMMON_API void Flush();

    // Get() is exported — so the templates below always route through
    // common.dll's registry, not the caller's private one
    static COMMON_API std::shared_ptr<spdlog::logger> Get();

    template<typename... Args>
    static void Trace(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->info(fmt, std::forward<Args>(args)...);
        Get()->flush();
    }

    template<typename... Args>
    static void Warn(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Critical(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Get()->critical(fmt, std::forward<Args>(args)...);
    }
};

#pragma once

#ifdef COMMON_EXPORTS
#define COMMON_API __declspec(dllexport)
#else
#define COMMON_API __declspec(dllimport)
#endif

#include <string>
#include <vector>
#include <sstream>

struct RtspConfig
{
    std::string host;
    int         port     = 554;
    std::string username;
    std::string password;
    std::string path;       // e.g. /live/stream1

    std::string BuildUri() const
    {
        std::ostringstream ss;
        ss << "rtsp://";
        if (!username.empty())
            ss << username << ":" << password << "@";
        ss << host << ":" << port << path;
        return ss.str();
    }
};

class IniHandler
{
public:
    IniHandler() = delete;

    // Loads config.ini from the exe directory (or a custom path).
    // Returns false if the file cannot be opened.
    static COMMON_API bool Load(const std::string& iniPath = "");

    static COMMON_API const std::vector<RtspConfig>& GetRtspList();
};

#include "pch.h"
#include "iniHandler.h"
#include <simpleini/SimpleIni.h>
#include <filesystem>

static std::vector<RtspConfig> s_rtspList;

static std::filesystem::path ResolveConfigPath()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path exePath(buf);

    // <exe dir>/config/
    auto configDir = exePath.parent_path() / "config";
    std::filesystem::create_directories(configDir);

    // VA.exe -> VA_Config.ini
    std::string stem = exePath.stem().string();
    return configDir / (stem + "_Config.ini");
}

static void CreateDefaultIni(const std::filesystem::path& path)
{
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetValue("Settings", "RtspCount", "1");
    ini.SetValue("RTSP_1",   "Host",      "192.168.0.1");
    ini.SetValue("RTSP_1",   "Port",      "554");
    ini.SetValue("RTSP_1",   "Username",  "admin");
    ini.SetValue("RTSP_1",   "Password",  "");
    ini.SetValue("RTSP_1",   "Path",      "/live/stream1");
    ini.SaveFile(path.string().c_str());
}

bool IniHandler::Load(const std::string& iniPath)
{
    std::filesystem::path path;
    if (iniPath.empty())
    {
        path = ResolveConfigPath();
        if (!std::filesystem::exists(path))
            CreateDefaultIni(path);
    }
    else
    {
        path = iniPath;
    }

    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadFile(path.string().c_str()) < SI_OK)
        return false;

    s_rtspList.clear();

    int count = 0;
    const char* val = ini.GetValue("Settings", "RtspCount", "0");
    try { count = std::stoi(val); } catch (...) { count = 0; }

    for (int i = 1; i <= count; ++i)
    {
        std::string section = "RTSP_" + std::to_string(i);

        RtspConfig cfg;
        cfg.host     = ini.GetValue(section.c_str(), "Host",     "");
        cfg.username = ini.GetValue(section.c_str(), "Username", "");
        cfg.password = ini.GetValue(section.c_str(), "Password", "");
        cfg.path     = ini.GetValue(section.c_str(), "Path",     "/");

        const char* portStr = ini.GetValue(section.c_str(), "Port", "554");
        try { cfg.port = std::stoi(portStr); } catch (...) { cfg.port = 554; }

        if (!cfg.host.empty())
            s_rtspList.push_back(cfg);
    }

    return true;
}

const std::vector<RtspConfig>& IniHandler::GetRtspList()
{
    return s_rtspList;
}

#include "pch.h"
#include "RLPathFinder.h"

namespace RLPathFinder {

static std::filesystem::path ReadRegString(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD size = sizeof(buf);
    DWORD type = REG_SZ;
    LSTATUS r = RegQueryValueExW(hKey, value, nullptr, &type, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if (r != ERROR_SUCCESS) return {};
    return std::filesystem::path(buf);
}

static bool IsRLRoot(const std::filesystem::path& p) {
    return !p.empty() &&
           std::filesystem::exists(p / "TAGame" / "CookedPCConsole");
}

std::filesystem::path FindRocketLeaguePath() {
    {
        auto steamPath = ReadRegString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
        if (!steamPath.empty()) {
            auto rl = steamPath / "steamapps" / "common" / "rocketleague";
            if (IsRLRoot(rl)) return rl;
        }
    }
    {
        auto epicPath = ReadRegString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Rocket League",
            L"InstallLocation");
        if (IsRLRoot(epicPath)) return epicPath;
    }
    {
        auto epicPath = ReadRegString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\EpicGames\\Unreal Engine\\rocketleague",
            L"InstallLocation");
        if (IsRLRoot(epicPath)) return epicPath;
    }
    for (auto& p : {
        std::filesystem::path("C:/Program Files/Epic Games/rocketleague"),
        std::filesystem::path("C:/Program Files (x86)/Steam/steamapps/common/rocketleague"),
        std::filesystem::path("D:/Program Files (x86)/Steam/steamapps/common/rocketleague"),
        std::filesystem::path("D:/SteamLibrary/steamapps/common/rocketleague"),
        std::filesystem::path("C:/SteamLibrary/steamapps/common/rocketleague"),
    }) {
        if (IsRLRoot(p)) return p;
    }
    return {};
}

} // namespace RLPathFinder

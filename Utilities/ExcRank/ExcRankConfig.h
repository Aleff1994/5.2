#pragma once
#include "Util.h"
#include "ExcRankCore.h"
#include <memory>
#include <mutex>
#include <windows.h>
namespace ExcRank
{
// Each server keeps one validated immutable snapshot. Bad edits retain the old one.
class Configuration
{
    std::mutex mutex;
    DWORD checked = 0;
    ULONGLONG stamp = 0;
    std::shared_ptr<const Config> active;
    unsigned revision = 0;

  public:
    Configuration()
    {
        Config c;
        c.enabled = false;
        active = std::make_shared<const Config>(c);
    }
    std::shared_ptr<const Config> Snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex);
        DWORD now = GetTickCount();
        if (checked && now - checked < 1000)
            return active;
        checked = now;
        WIN32_FILE_ATTRIBUTE_DATA data{};
        const char *path = "..\\Data\\Item\\ExcRank.txt";
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
            return active;
        ULARGE_INTEGER time;
        time.HighPart = data.ftLastWriteTime.dwHighDateTime;
        time.LowPart = data.ftLastWriteTime.dwLowDateTime;
        if (time.QuadPart == stamp)
            return active;
        stamp = time.QuadPart;
        Config candidate;
        std::string error;
        if (!Load(path, candidate, error))
        {
            LogAdd(LOG_RED, "[ExcRank] Config rejected; previous policy retained: %s", error.c_str());
            return active;
        }
        active = std::make_shared<const Config>(candidate);
        ++revision;
        LogAdd(LOG_GREEN, "[ExcRank] Configuration loaded: max=%u, weights=%u/%u/%u", candidate.maxOptions,
               candidate.weight[0], candidate.weight[1], candidate.weight[2]);
        return active;
    }
    unsigned Revision()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return revision;
    }
};
inline Configuration &ConfigurationStore()
{
    static Configuration state;
    return state;
}
inline std::shared_ptr<const Config> Policy()
{
    return ConfigurationStore().Snapshot();
}
} // namespace ExcRank

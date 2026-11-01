#pragma once
#include "../../Utilities/ExcRank/ExcRankCacheCore.h"
#include "ExcRankChannel.h"
namespace ExcRankCache
{
struct LiveBackend
{
    std::uint32_t Now() const
    {
        return GetTickCount();
    }
    bool Enabled() const
    {
        return ExcRankChannel::Get().Enabled();
    }
    std::uint32_t Send(ExcRankProtocol::Message message)
    {
        return ExcRankChannel::Get().Send(message);
    }
    bool Take(std::uint32_t token, ExcRankProtocol::Message &reply)
    {
        return ExcRankChannel::Get().Take(token, reply);
    }
};
inline Cache<LiveBackend> &Get()
{
    static LiveBackend backend;
    static Cache<LiveBackend> cache(backend);
    return cache;
}
} // namespace ExcRankCache

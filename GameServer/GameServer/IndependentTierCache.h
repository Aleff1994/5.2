#pragma once
#include "IndependentTierChannel.h"
#include "../../Utilities/IndependentTierCacheCore.h"
namespace IndependentTierCache {
struct LiveBackend {
 std::uint32_t Now() const { return GetTickCount(); }
 bool Enabled() const { return IndependentTierChannel::Get().Enabled(); }
 std::uint32_t Send(IndependentTierProtocol::Message message) { return IndependentTierChannel::Get().Send(message); }
 bool Take(std::uint32_t token,IndependentTierProtocol::Message& reply) { return IndependentTierChannel::Get().Take(token,reply); }
};
inline Cache<LiveBackend>& Get() { static LiveBackend backend;static Cache<LiveBackend> cache(backend);return cache; }
}

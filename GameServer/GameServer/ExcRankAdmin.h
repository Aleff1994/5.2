#pragma once
#include "../../Utilities/ExcRank/ExcRankRandom.h"
#include "ExcRankRuntime.h"
#include "NativeExcellentAdmin.h"
#include "Notice.h"
#include <vector>
namespace ExcRankAdmin
{
struct Pending
{
    std::uint32_t token = 0, serial = 0;
    unsigned item = 0;
    int user = -1;
    std::uintptr_t socket = 0;
    DWORD stamp = 0;
    char account[11]{};
};
inline std::vector<Pending> &PendingRequests()
{
    static std::vector<Pending> list;
    return list;
}
inline bool Assign(LPOBJ object, unsigned slot, const ExcRank::ItemOptions &data)
{
    if (!object || slot >= INVENTORY_SIZE || PendingRequests().size() >= 64)
        return false;
    auto &item = object->Inventory[slot];
    auto cfg = ExcRank::Policy();
    if (!IndependentTierControl::Enabled() || !cfg->enabled || !cfg->sources[5].enabled || !item.IsItem() ||
        !item.m_Serial || (cfg->requireExcellent && !(item.m_NewOption & 63)) ||
        !ExcRank::ValidatePolicy(data, item.m_Index, *cfg))
        return false;
    for (auto option : data.slots)
        if (!(cfg->sources[5].rankMask & (1u << (static_cast<unsigned>(option.rank) - 1))))
            return false;
    ExcRankProtocol::Message message;
    message.operation = ExcRankProtocol::Operation::Create;
    ExcRankProtocol::Entry entry;
    entry.serial = item.m_Serial;
    entry.itemId = static_cast<std::uint16_t>(item.m_Index);
    entry.metadata = data;
    ExcRank::SecureRandom random;
    if (!ExcRank::GenerateExclusive(*cfg, item.m_Index, random, entry.metadata))
        return false;
    message.entries.push_back(entry);
    const auto token = ExcRankChannel::Get().Send(message);
    if (!token)
        return false;
    Pending p;
    p.token = token;
    p.serial = item.m_Serial;
    p.item = item.m_Index;
    p.user = object->Index;
    p.socket = static_cast<std::uintptr_t>(object->Socket);
    p.stamp = GetTickCount();
    memcpy(p.account, object->Account, 11);
    PendingRequests().push_back(p);
    return true;
}
inline void Pump()
{
    NativeExcellentAdmin::Pump();
    auto &list = PendingRequests();
    for (auto i = list.begin(); i != list.end();)
    {
        ExcRankProtocol::Message response;
        bool ready = ExcRankChannel::Get().Take(i->token, response);
        bool timeout = GetTickCount() - i->stamp >= 30000;
        if (!ready && !timeout)
        {
            ++i;
            continue;
        }
        bool saved = ready && !response.entries.empty() &&
                     (response.entries[0].status == ExcRankProtocol::Status::Created ||
                      response.entries[0].status == ExcRankProtocol::Status::Existing);
        if (saved)
        {
            auto &cache = ExcRankCache::Get();
            {
                std::lock_guard<std::mutex> lock(cache.mutex);
                cache.records.erase(i->serial);
            }
            cache.Request(i->serial, i->item);
        }
        if (OBJECT_USER_RANGE(i->user) && gObjIsConnectedGP(i->user) &&
            i->socket == static_cast<std::uintptr_t>(gObj[i->user].Socket) &&
            !memcmp(i->account, gObj[i->user].Account, 11))
            gNotice.GCNoticeSend(
                i->user, 1, 0, 0, 0, 0, 0,
                saved ? "ExcRank: bloco salvo; adicionais serao atualizados."
                      : "ExcRank: nao alterado (conflito, dados invalidos ou persistencia indisponivel).");
        LogAdd(saved ? LOG_GREEN : LOG_RED, "[ExcRankAdmin] serial=%lu user=%d saved=%d",
               static_cast<unsigned long>(i->serial), i->user, saved ? 1 : 0);
        i = list.erase(i);
    }
}
} // namespace ExcRankAdmin

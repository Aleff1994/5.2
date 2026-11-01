#pragma once
#include "../../Utilities/ExcRank/ExcRankConfig.h"
#include "ExcRankStore.h"
#include "IndependentTierCreation.h"
namespace ExcRankCreation
{
inline bool Prepare(std::uint32_t serial, unsigned item, unsigned map, unsigned excellentMask,
                    ExcRank::Origin source = ExcRank::Origin::Drop, std::uint32_t copySerial = 0)
{
    const auto cfg = ExcRank::Policy();
    if (copySerial)
    {
        ExcRankStore::Record original;
        const auto read = ExcRankStore::Read(gQueryManager, copySerial, original);
        if (read == ExcRankStore::Status::Missing)
            return true;
        if (read != ExcRankStore::Status::Found)
            return false;
        if (original.metadata.slots.empty())
            return true;
        if (original.originalItemId != static_cast<int>(item) ||
            !ExcRank::ValidateStructure(original.metadata))
            return true;
        const auto written = ExcRankStore::Create(gQueryManager, serial, item, original.metadata);
        return written == ExcRankStore::Status::Created || written == ExcRankStore::Status::Existing;
    }
    if (!IndependentTierControl::Enabled() || !cfg->enabled)
        return true;
    if (static_cast<unsigned>(source) >= 6)
        return false;
    // Existing special inventory/crafting routes are opt-in, never guessed as drops.
    if (map >= 0xE0 && source == ExcRank::Origin::Drop)
        return true;
    auto mapped = cfg->mapOrigins.find(map);
    if (source == ExcRank::Origin::Drop && mapped != cfg->mapOrigins.end())
        source = mapped->second;
    if (cfg->requireExcellent && !(excellentMask & 63))
        return true;
    bool eligible = false;
    for (unsigned id = 1; id <= 12; ++id)
        eligible |= cfg->Allows(item, id);
    if (!eligible)
        return true;
    const auto &rule = cfg->sources[static_cast<unsigned>(source)];
    if (!rule.enabled)
        return true;
    ExcRankStore::Record existing;
    const auto read = ExcRankStore::Read(gQueryManager, serial, existing);
    if (read == ExcRankStore::Status::Found)
        return existing.originalItemId == static_cast<int>(item);
    if (read != ExcRankStore::Status::Missing)
        return false;
    IndependentTierDropCreation::Random rng;
    ExcRank::ItemOptions generated;
    if (!ExcRank::Generate(*cfg, item, source, rng, generated))
        return false;
    if (generated.slots.empty())
        return true;
    const auto written = ExcRankStore::Create(gQueryManager, serial, item, generated);
    if (written == ExcRankStore::Status::Created || written == ExcRankStore::Status::Existing)
        return true;
    if (written == ExcRankStore::Status::Conflict)
        return ExcRankStore::Read(gQueryManager, serial, existing) == ExcRankStore::Status::Found &&
               existing.originalItemId == static_cast<int>(item);
    return false;
}
} // namespace ExcRankCreation

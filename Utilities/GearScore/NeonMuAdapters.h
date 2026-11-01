#pragma once
#include "ItemGearScore.h"
#include "../IndependentItemTiers.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace NeonGear {
// Compatibility proposal, NOT a DB conversion: Legacy=Normal, C=Uncommon,
// B=Rare, A=Legendary, S=Epic. Metadata remains stored exactly as before.
inline Rank RankFromExisting(IndependentItemTiers::Grade grade) noexcept {
    return static_cast<Rank>(static_cast<unsigned>(grade));
}
template<class NativeItem>
ItemInput Capture(NativeItem& item, const IndependentItemTiers::Metadata* metadata, bool ready) {
    ItemInput result;
    if(!item.IsItem()) { result.equippedUsable=false; return result; }
    result.section = item.m_Index / 512;
    result.index = item.m_Index % 512;
    result.level = item.m_Level;
    result.lifeOption = item.m_Option3;
    // PackedSetOption high nibble is independent PvP tier in this project!
    result.ancient = (item.m_SetOption & 15) != 0;
    result.excellentMask = static_cast<unsigned>(item.m_NewOption) & 63;
    result.equippedUsable = item.m_IsValidItem != 0 && item.m_Durability > 0;
    result.ranksReady = ready && metadata && metadata->Valid();
    if(result.ranksReady)
        for(unsigned bit=0; bit<6; ++bit)
            result.ranks[bit] = RankFromExisting(metadata->Get(IndependentItemTiers::Family::Excellent,
                                                             static_cast<std::uint16_t>(bit)));
    return result;
}
// Lookup signature: bool(NativeItem&, Metadata&). Must only consult existing cache here.
// Request async misses separately and queue CharacterCalcAttribute on authoritative completion.
template<class NativeObject, class Lookup>
ArmorResult CaptureArmor(const ItemGearScore& policy, NativeObject& object, Lookup lookup, bool enabled) {
    std::array<ItemInput,5> armor{};
    for(unsigned n=0; n<5; ++n) {
        auto& item = object.Inventory[n+2];
        IndependentItemTiers::Metadata metadata;
        const bool ready = enabled && item.IsItem() && lookup(item, metadata);
        armor[n] = Capture(item, &metadata, ready);
    }
    return policy.Armor(armor, enabled);
}
// Run AFTER all native Defense/MaxLife/AddLife/PhysiSpeed/MagicSpeed calculations,
// BEFORE existing Life clamp and GCLifeSend. Invoke ONCE on freshly reset stats.
// Existing native speed caps / client synchronization still apply afterwards.
template<class NativeObject>
void ApplyArmorToFreshNativeStats(NativeObject& object, const ArmorResult& armor) {
    const auto addPercent = [](int value, unsigned percent) {
        if(!percent || value<=0)return value;
        const auto positive = (std::max)(value, 0);
        return static_cast<int>((std::min)(std::int64_t{1000000000},
            std::int64_t{positive} + std::int64_t{positive} * percent / 100));
    };
    object.Defense = addPercent(object.Defense, armor.bonus.defensePercent);
    object.PhysiSpeed = addPercent(object.PhysiSpeed, armor.bonus.speedPercent);
    object.MagicSpeed = addPercent(object.MagicSpeed, armor.bonus.speedPercent);
    if(armor.bonus.hpPercent) {
        const double maximum = (std::max)(0.0, static_cast<double>(object.MaxLife) + object.AddLife);
        const auto extraHP = static_cast<std::int64_t>(maximum * armor.bonus.hpPercent / 100.0);
        object.AddLife = static_cast<int>((std::min)(std::int64_t{1000000000},
            static_cast<std::int64_t>(object.AddLife) + extraHP));
    }
    // Native CharacterCalcAttribute already clamps Life; never heal here.
}

// Client adapter: bounded insertion into the existing TextList arrays.
// Pass localized format strings (gTextClien.Select) and the project's color constants.
// base/grade originate from the existing v8 server reply, never from local configuration.
template<std::size_t Rows, std::size_t Columns, class Color, class Bold>
bool AppendTooltip(char (&text)[Rows][Columns], Color (&colors)[Rows], Bold (&bold)[Rows], int& line,
                   std::uint32_t base, std::uint32_t grade, const char* baseFormat,
                   const char* gradeFormat, const char* totalFormat,
                   Color baseColor, Color gradeColor, Color totalColor) {
    static_assert(Columns >= 64, "Tooltip row too narrow");
    if(line < 0 || static_cast<std::size_t>(line) > Rows || Rows-static_cast<std::size_t>(line)<3 ||
       base>1000000 || grade>1000000) return false;
    char buffer[3][Columns]{};
    const unsigned values[] = {base, grade, base+grade};
    const char* formats[] = {baseFormat, gradeFormat, totalFormat};
    const Color palette[] = {baseColor, gradeColor, totalColor};
    for(unsigned n=0; n<3; ++n) {
        if(!formats[n]) return false;
        const int length = std::snprintf(buffer[n], Columns, formats[n], values[n]);
        if(length<0 || static_cast<std::size_t>(length)>=Columns) return false;
    }
    for(unsigned n=0; n<3; ++n) {
        std::memcpy(text[line], buffer[n], Columns);
        colors[line]=palette[n]; bold[line]=true; ++line;
    }
    return true;
}
} // namespace NeonGear

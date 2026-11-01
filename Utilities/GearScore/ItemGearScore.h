#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace NeonGear {
// Standalone C++17 policy. No global state, disk I/O in combat, item mutation or ownership of CItem.
enum class Rank : unsigned { Normal, Uncommon, Rare, Legendary, Epic };
enum class HarmonyEffect : unsigned {
    AttackSuccessPvP, DefenseSuccessPvP, DecreaseSDRate,
    IgnoreSDRate, ResistIgnoreSDRate, DamageReduction, Count
};
constexpr unsigned HarmonyCount = static_cast<unsigned>(HarmonyEffect::Count);
struct Score {
    bool valid = false;
    std::uint32_t base = 0, grade = 0;
    std::uint32_t refinePoints = 0, rankPoints = 0, lifePoints = 0;
    std::uint32_t Total() const noexcept { return base + grade; }
};
struct ItemInput {
    int section = -1, index = -1, level = 0, lifeOption = 0;
    bool ancient = false, equippedUsable = true, ranksReady = true;
    unsigned excellentMask = 0;
    std::array<Rank, 6> ranks{};
};
struct SetRule {
    std::uint32_t minimumGrade = 0;
    unsigned defensePercent = 0, hpPercent = 0, speedPercent = 0;
};
struct ArmorResult {
    std::uint32_t grade = 0;
    unsigned pieces = 0;
    bool ready = true;
    SetRule bonus{};
};
struct Config {
    // refine[n] is the TOTAL contribution at +n, not a per-step contribution.
    std::array<std::uint32_t, 16> refine{};
    std::array<std::uint32_t, 5> rankPoints{};
    std::uint32_t refineBasePercentPerLevel = 0, lifePointsPerStep = 0;
    std::uint32_t harmonyReferenceBase = 100, harmonyMaximumPercent = 400;
    std::array<int, HarmonyCount> harmonyCaps{{1000, 1000, 40, 40, 60, 40}};
    SetRule setCaps{0, 30, 20, 15};
    bool requireFivePieces = true;
    std::map<unsigned, std::uint32_t> items;
    std::vector<SetRule> sets;
};
// Strict and transactional: any malformed/duplicate/unknown entry leaves output untouched.
bool LoadConfig(std::istream& input, Config& output, std::string& error);
bool LoadConfigFile(const std::string& path, Config& output, std::string& error);

struct Stats {
    std::uint32_t defense = 0, maximumHP = 0, physicalSpeed = 0, magicSpeed = 0;
    std::uint32_t currentHP = 0;
};
struct HarmonyTotals {
    std::array<std::int64_t, HarmonyCount> original{}, scaled{};
    std::array<bool, HarmonyCount> touched{};
};

class ItemGearScore {
public:
    // Construct only with a successfully loaded configuration; thereafter immutable.
    explicit ItemGearScore(Config config);
    Score Evaluate(const ItemInput& item, bool commonEnabled) const noexcept;
    // Array order is Helm, Armor, Pants, Gloves, Boots (GS inventory 2..6).
    ArmorResult Armor(const std::array<ItemInput, 5>& items, bool commonEnabled) const noexcept;
    // Pure transform of FRESH native stats, once per CharacterCalcAttribute.
    Stats ApplySetBonus(const Stats& native, const ArmorResult& armor) const noexcept;
    int ScaleHarmonyValue(int raw, HarmonyEffect effect, const Score& item, bool commonEnabled) const noexcept;
    void AddHarmony(HarmonyTotals& totals, HarmonyEffect effect, int raw,
                    const Score& item, bool commonEnabled) const noexcept;
    // Replace only the Harmony portion, preserving other native sources. PvM always returns native.
    int EffectivePvPValue(int nativeTotal, HarmonyEffect effect, const HarmonyTotals& totals,
                          bool isPlayerVersusPlayer, bool commonEnabled) const noexcept;
    const Config& Settings() const noexcept { return config_; }
private:
    Config config_;
};
// Native type/option identifiers from this project's JewelOfHarmonyOption.h.
bool ClassifyHarmony(int nativeType, int nativeOption, HarmonyEffect& effect) noexcept;
} // namespace NeonGear

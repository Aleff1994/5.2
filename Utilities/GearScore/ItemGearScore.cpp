#include "ItemGearScore.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace NeonGear {
namespace {
constexpr std::uint32_t ScoreLimit = 1000000;
std::uint32_t ClampScore(std::uint64_t value) noexcept {
    return static_cast<std::uint32_t>((std::min)(value, std::uint64_t{ScoreLimit}));
}
std::uint64_t RoundedRatio(std::uint64_t value, std::uint64_t numerator, std::uint64_t denominator) noexcept {
    return (value * numerator + denominator / 2) / denominator;
}
unsigned EffectIndex(HarmonyEffect effect) noexcept { return static_cast<unsigned>(effect); }
std::uint32_t Boost(std::uint32_t value, unsigned percent) noexcept {
    return static_cast<std::uint32_t>((std::min)(std::uint64_t{1000000000},
        std::uint64_t{value} + std::uint64_t{value} * percent / 100));
}
bool Validate(const Config& c) {
    if(c.items.empty() || c.harmonyReferenceBase == 0 || c.harmonyReferenceBase > ScoreLimit ||
       c.harmonyMaximumPercent < 100 || c.harmonyMaximumPercent > 1000 ||
       c.refineBasePercentPerLevel > 1000 || c.lifePointsPerStep > ScoreLimit ||
       c.setCaps.defensePercent > 100 || c.setCaps.hpPercent > 100 || c.setCaps.speedPercent > 100)
        return false;
    if(c.refine[0] != 0) return false;
    for(unsigned n=0; n<c.refine.size(); ++n)
        if(c.refine[n] > ScoreLimit || (n && c.refine[n] < c.refine[n-1])) return false;
    for(unsigned n=0; n<c.rankPoints.size(); ++n)
        if(c.rankPoints[n] > ScoreLimit || (n && c.rankPoints[n] < c.rankPoints[n-1])) return false;
    for(const auto& pair : c.items)
        if(pair.first >= 16*512 || !pair.second || pair.second > 400000) return false;
    for(unsigned n=0; n<HarmonyCount; ++n)
        if(c.harmonyCaps[n] < 0 || c.harmonyCaps[n] > (n<2 ? 100000 : 100)) return false;
    std::uint32_t previous = 0;
    SetRule previousRule{};
    for(const auto& rule : c.sets) {
        if(!rule.minimumGrade || rule.minimumGrade <= previous || rule.minimumGrade > 5*ScoreLimit ||
           rule.defensePercent > c.setCaps.defensePercent || rule.hpPercent > c.setCaps.hpPercent ||
           rule.speedPercent > c.setCaps.speedPercent || rule.defensePercent < previousRule.defensePercent ||
           rule.hpPercent < previousRule.hpPercent || rule.speedPercent < previousRule.speedPercent) return false;
        previous = rule.minimumGrade; previousRule = rule;
    }
    return true;
}
}

bool LoadConfig(std::istream& input, Config& output, std::string& error) {
    Config candidate;
    std::set<std::string> singleton;
    std::set<unsigned> thresholds;
    std::string line;
    unsigned lineNumber = 0;
    const auto fail = [&](const std::string& message) {
        error = "line " + std::to_string(lineNumber) + ": " + message; return false;
    };
    while(std::getline(input, line)) {
        ++lineNumber;
        if(line.size() > 4096 || lineNumber > 20000) return fail("configuration limit exceeded");
        if(lineNumber == 1 && line.compare(0,3,"\xEF\xBB\xBF") == 0) line.erase(0,3);
        line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
        std::istringstream row(line);
        std::string key;
        if(!(row >> key)) continue;
        std::vector<std::uint32_t> values;
        std::string token;
        while(row >> token) {
            if(token.empty() || token.size()>10 || token.find_first_not_of("0123456789")!=std::string::npos)
                return fail("expected nonnegative decimal integer");
            const auto number = std::stoull(token);
            if(number > 10000000) return fail("integer out of range");
            values.push_back(static_cast<std::uint32_t>(number));
        }
        if(key != "item" && key != "set" && !singleton.insert(key).second)
            return fail("duplicate directive: " + key);
        const auto count = [&](std::size_t expected) { return values.size() == expected; };
        if(key == "item" && count(3)) {
            if(values[0]>=16 || values[1]>=512 || !values[2] || values[2]>400000)
                return fail("invalid item section/index/base");
            if(!candidate.items.emplace(values[0]*512+values[1], values[2]).second)
                return fail("duplicate item ID");
        } else if(key == "refine" && count(16)) {
            std::copy(values.begin(), values.end(), candidate.refine.begin());
        } else if(key == "rank_points" && count(5)) {
            std::copy(values.begin(), values.end(), candidate.rankPoints.begin());
        } else if(key == "refine_base_percent" && count(1)) {
            candidate.refineBasePercentPerLevel = values[0];
        } else if(key == "life_points" && count(1)) {
            candidate.lifePointsPerStep = values[0];
        } else if(key == "harmony_reference" && count(1)) {
            candidate.harmonyReferenceBase = values[0];
        } else if(key == "harmony_max_percent" && count(1)) {
            candidate.harmonyMaximumPercent = values[0];
        } else if(key == "harmony_caps" && count(HarmonyCount)) {
            for(unsigned n=0; n<HarmonyCount; ++n) candidate.harmonyCaps[n] = static_cast<int>(values[n]);
        } else if(key == "set_caps" && count(3)) {
            candidate.setCaps = {0, values[0], values[1], values[2]};
        } else if(key == "require_five" && count(1) && values[0] <= 1) {
            candidate.requireFivePieces = values[0] != 0;
        } else if(key == "set" && count(4)) {
            if(!thresholds.insert(values[0]).second) return fail("duplicate set threshold");
            candidate.sets.push_back({values[0], values[1], values[2], values[3]});
        } else return fail("unknown directive or incorrect value count: " + key);
    }
    if(input.bad()) return fail("read failure");
    for(const char* key : {"refine", "rank_points", "refine_base_percent", "life_points", "harmony_reference",
                          "harmony_max_percent", "harmony_caps", "set_caps", "require_five"})
        if(!singleton.count(key)) return fail(std::string("missing directive: ") + key);
    std::sort(candidate.sets.begin(), candidate.sets.end(),
        [](const SetRule& a, const SetRule& b) { return a.minimumGrade < b.minimumGrade; });
    if(!Validate(candidate)) return fail("invalid range, ordering or cap");
    output = std::move(candidate); error.clear(); return true;
}
bool LoadConfigFile(const std::string& path, Config& output, std::string& error) {
    std::ifstream input(path);
    if(!input) { error = "cannot open configuration: " + path; return false; }
    return LoadConfig(input, output, error);
}
ItemGearScore::ItemGearScore(Config config) : config_(std::move(config)) {
    if(!Validate(config_)) throw std::invalid_argument("Invalid ItemGearScore configuration");
}
Score ItemGearScore::Evaluate(const ItemInput& item, bool enabled) const noexcept {
    if(!enabled || item.section < 0 || item.section >= 16 || item.index < 0 || item.index >= 512 ||
       item.level < 0 || item.level > 15 || item.lifeOption < 0 || item.lifeOption > 7 ||
       item.excellentMask > 63 || (item.excellentMask && !item.ranksReady)) return {};
    auto it = config_.items.find(static_cast<unsigned>(item.section*512 + item.index));
    if(it == config_.items.end()) return {}; // No invented default or random tier.
    Score result;
    result.base = ClampScore(item.ancient ? RoundedRatio(it->second, 250, 100) : it->second);
    result.refinePoints = ClampScore(std::uint64_t{config_.refine[item.level]} +
        RoundedRatio(result.base, config_.refineBasePercentPerLevel * item.level, 100));
    std::uint64_t rankPoints = 0;
    for(unsigned bit=0; bit<6; ++bit) if(item.excellentMask & (1u << bit)) {
        const auto rank = static_cast<unsigned>(item.ranks[bit]);
        if(rank >= config_.rankPoints.size()) return {};
        rankPoints += config_.rankPoints[rank];
    }
    result.rankPoints = (std::min)(ClampScore(rankPoints), ScoreLimit-result.refinePoints);
    result.lifePoints = (std::min)(ClampScore(std::uint64_t{config_.lifePointsPerStep} * item.lifeOption),
                                  ScoreLimit-result.refinePoints-result.rankPoints);
    result.grade = result.refinePoints+result.rankPoints+result.lifePoints;
    result.valid = true; return result;
}
ArmorResult ItemGearScore::Armor(const std::array<ItemInput,5>& items, bool enabled) const noexcept {
    ArmorResult result;
    if(!enabled) return result;
    for(unsigned slot=0; slot<items.size(); ++slot) {
        const auto& item = items[slot];
        if(!item.equippedUsable || item.section != static_cast<int>(7+slot)) continue;
        if(item.excellentMask && !item.ranksReady) { result.ready=false; continue; }
        const Score score = Evaluate(item, true);
        if(!score.valid) continue;
        ++result.pieces; result.grade += score.grade;
    }
    // Never grant a temporarily lower/partial set while metadata is pending.
    if(!result.ready || (config_.requireFivePieces && result.pieces != 5)) return result;
    for(const auto& rule : config_.sets)
        if(result.grade >= rule.minimumGrade) result.bonus = rule;
    return result; // Highest tier wins; thresholds do not stack.
}
Stats ItemGearScore::ApplySetBonus(const Stats& native, const ArmorResult& armor) const noexcept {
    Stats output = native;
    output.defense = Boost(native.defense, armor.bonus.defensePercent);
    output.maximumHP = Boost(native.maximumHP, armor.bonus.hpPercent);
    output.physicalSpeed = Boost(native.physicalSpeed, armor.bonus.speedPercent);
    output.magicSpeed = Boost(native.magicSpeed, armor.bonus.speedPercent);
    output.currentHP = (std::min)(native.currentHP, output.maximumHP); // No free healing on equip.
    return output;
}
int ItemGearScore::ScaleHarmonyValue(int raw, HarmonyEffect effect, const Score& item, bool enabled) const noexcept {
    const unsigned n = EffectIndex(effect);
    if(!enabled || !item.valid || raw <= 0 || n >= HarmonyCount) return raw;
    const auto numerator = (std::min)(std::uint64_t{item.base} * 100,
        std::uint64_t{config_.harmonyReferenceBase} * config_.harmonyMaximumPercent);
    const auto scaled = RoundedRatio(static_cast<unsigned>(raw), numerator,
                                    std::uint64_t{config_.harmonyReferenceBase} * 100);
    return static_cast<int>((std::min)(scaled, static_cast<std::uint64_t>(config_.harmonyCaps[n])));
}
void ItemGearScore::AddHarmony(HarmonyTotals& totals, HarmonyEffect effect, int raw,
                              const Score& item, bool enabled) const noexcept {
    const unsigned n = EffectIndex(effect);
    if(n >= HarmonyCount || raw <= 0) return;
    totals.original[n] += raw;
    totals.scaled[n] += ScaleHarmonyValue(raw, effect, item, enabled);
    totals.touched[n] = totals.touched[n] || (enabled && item.valid);
}
int ItemGearScore::EffectivePvPValue(int native, HarmonyEffect effect, const HarmonyTotals& totals,
                                    bool pvp, bool enabled) const noexcept {
    const unsigned n = EffectIndex(effect);
    if(!pvp || !enabled || n >= HarmonyCount || !totals.touched[n]) return native;
    const auto effectiveHarmony = (std::min)(totals.scaled[n], std::int64_t{config_.harmonyCaps[n]});
    const auto result = std::int64_t{native} - totals.original[n] + effectiveHarmony;
    return static_cast<int>((std::max)(std::int64_t{0},
        (std::min)(result, std::int64_t{(std::numeric_limits<int>::max)()})));
}
bool ClassifyHarmony(int type, int option, HarmonyEffect& effect) noexcept {
    if((type==1 && option==8) || (type==2 && option==7)) effect=HarmonyEffect::AttackSuccessPvP;
    else if(type==3 && option==6) effect=HarmonyEffect::DefenseSuccessPvP;
    else if((type==1 && option==9) || (type==2 && option==6)) effect=HarmonyEffect::DecreaseSDRate;
    else if((type==1 && option==10) || (type==2 && option==8)) effect=HarmonyEffect::IgnoreSDRate;
    else if(type==3 && option==8) effect=HarmonyEffect::ResistIgnoreSDRate;
    else if(type==3 && option==7) effect=HarmonyEffect::DamageReduction;
    else return false;
    return true;
}
} // namespace NeonGear

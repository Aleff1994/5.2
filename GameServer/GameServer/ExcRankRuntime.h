#include "../../Utilities/IndependentExcellentTables.h"
#pragma once
#include "../../Utilities/ExcRank/ExcRankConfig.h"
#include "ExcRankCache.h"
#include "IndependentTierCache.h"
#include "User.h"
#include "Util.h"
#include <array>
namespace ExcRankRuntime
{
struct State
{
    bool valid = false, pending = false;
    std::uintptr_t socket = 0;
    char account[11]{}, name[11]{};
    ExcRank::Totals totals;
    unsigned displayed[13]={};
    unsigned physical = 0, magical = 0, physicalRank = 0, magicalRank = 0, attackStyle = 0, critStyle = 0,
             excellentStyle = 0;
    unsigned appliedExclusive[4] = {};
    int nativeCritical = 0, nativeExcellent = 0;
    unsigned ranksByOption[13] = {};
    std::shared_ptr<const ExcRank::Config> policy;
};
inline std::array<State, MAX_OBJECT_USER> &States()
{
    static std::array<State, MAX_OBJECT_USER> s;
    return s;
}
inline State *Get(LPOBJ p, bool fresh = false)
{
    if (!p || p->Type != OBJECT_USER || !OBJECT_USER_RANGE(p->Index))
        return nullptr;
    auto &s = States()[p->Index - OBJECT_START_USER];
    if (!s.valid || s.socket != static_cast<std::uintptr_t>(p->Socket) || memcmp(s.account, p->Account, 11) ||
        memcmp(s.name, p->Name, 11))
    {
        s = State{};
        if (!fresh)
            return nullptr;
        s.valid = true;
        s.socket = static_cast<std::uintptr_t>(p->Socket);
        memcpy(s.account, p->Account, 11);
        memcpy(s.name, p->Name, 11);
    }
    return &s;
}
inline void Reset(int index)
{
    if (OBJECT_USER_RANGE(index))
        States()[index - OBJECT_START_USER] = State{};
}
inline bool Enabled()
{
    return IndependentTierControl::Enabled() && ExcRank::Policy()->enabled;
}
inline bool GradeEnabled()
{
    static const bool value =
        GetPrivateProfileIntA("GameServerInfo", "ExcRankGradeEnable", 0, IndependentTierControl::Path()) == 1;
    return value;
}
inline void ApplyExclusive(LPOBJ p)
{
    const auto cfg = ExcRank::Policy();
    if (!p || !IndependentTierControl::Enabled() || !cfg->enabled)
        return;
    unsigned totals[4] = {};
    const int slots[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 237, 238, 239, 240, 241, 242};
    for (int slot : slots)
    {
        auto &item = p->Inventory[slot];
        if (!item.IsItem() || !item.m_IsValidItem || !(item.m_Durability > 0) || !(item.m_NewOption & 63) ||
            cfg->Class(item.m_Index) == ExcRank::ItemClass::None)
            continue;
        ExcRank::ItemOptions data;
        if (ExcRankCache::Get().Lookup(item.m_Serial, item.m_Index, data) != ExcRankCache::State::Ready)
        {
            ExcRankCache::Get().Request(item.m_Serial, item.m_Index);
            continue;
        }
        for (auto attribute : data.attributes)
            if (attribute.id >= 1 && attribute.id <= 4)
                totals[attribute.id - 1] =
                    (std::min)(cfg->sanityCap, totals[attribute.id - 1] + attribute.value);
    }
    if (auto *state = Get(p, true))
        for (unsigned n = 0; n < 4; ++n)
            state->appliedExclusive[n] = totals[n];
    p->AddStrength = static_cast<int>(
        (std::min)(static_cast<long long>(INT32_MAX), static_cast<long long>(p->AddStrength) + totals[0]));
    p->AddDexterity = static_cast<int>(
        (std::min)(static_cast<long long>(INT32_MAX), static_cast<long long>(p->AddDexterity) + totals[1]));
    p->AddVitality = static_cast<int>(
        (std::min)(static_cast<long long>(INT32_MAX), static_cast<long long>(p->AddVitality) + totals[2]));
    p->AddEnergy = static_cast<int>(
        (std::min)(static_cast<long long>(INT32_MAX), static_cast<long long>(p->AddEnergy) + totals[3]));
}
inline bool HasHP(LPOBJ p)
{
    auto *s = Get(p);
    return s && s->totals.value[7] != 0;
}
inline unsigned Add(unsigned a, unsigned b, unsigned cap)
{
    return static_cast<unsigned>(
        (std::min)(static_cast<std::uint64_t>(cap), static_cast<std::uint64_t>(a) + b));
}
inline ExcRank::Totals Collect(LPOBJ p, const ExcRank::Config &cfg, unsigned &physical, unsigned &magical,
                               bool *pending = nullptr)
{
    ExcRank::Totals t;
    physical = magical = 0;
    const int slots[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 237, 238, 239, 240, 241, 242};
    bool magicWeapon = cfg.Class(p->Inventory[0].m_Index) == ExcRank::ItemClass::Magic ||
                       cfg.Class(p->Inventory[1].m_Index) == ExcRank::ItemClass::Magic;
    for (int slot : slots)
    {
        auto &item = p->Inventory[slot];
        if (!item.IsItem() || !item.m_IsValidItem || !(item.m_Durability > 0) || !item.m_Serial)
            continue;
        ExcRank::ItemOptions options;
        auto state = ExcRankCache::Get().Lookup(item.m_Serial, item.m_Index, options);
        if (state != ExcRankCache::State::Ready)
        {
            if (state != ExcRankCache::State::Legacy)
            {
                if (pending)
                    *pending = true;
                ExcRankCache::Get().Request(item.m_Serial, item.m_Index);
            }
            continue;
        }
        IndependentItemTiers::Metadata native;
        if(IndependentTierCache::Get().Lookup(item.m_Serial,item.m_Index,native)==IndependentTierCache::State::Ready && !native.options.empty())continue;
        ExcRank::Aggregate(t, options, item.m_Index, cfg);
        for (auto opt : options.slots)
            if (opt.option == 3 && cfg.Allows(item.m_Index, 3))
            {
                auto category = cfg.Class(item.m_Index);
                unsigned value = cfg.Value(opt);
                if (category == ExcRank::ItemClass::Magic ||
                    (category == ExcRank::ItemClass::Pendant && magicWeapon))
                    magical = Add(magical, value, cfg.sanityCap);
                else
                    physical = Add(physical, value, cfg.sanityCap);
            }
    }
    return t;
}
inline void Apply(LPOBJ p)
{
    auto *s = Get(p, true);
    if (!s)
        return;
    s->totals = {};
    s->physical = s->magical = 0;
    s->policy = ExcRank::Policy();
    if (!IndependentTierControl::Enabled() || !s->policy->enabled)
        return;
    const auto &cfg = *s->policy;
    s->pending = false;
    s->totals = Collect(p, cfg, s->physical, s->magical, &s->pending);
    const auto &v = s->totals.value;
    for(unsigned n=0;n<13;++n)s->displayed[n]=v[n];
    const int nativeReduction=p->DamageReduction[DAMAGE_REDUCTION_EXCELLENT_ITEM],nativeReflect=p->DamageReflect,nativePhysSpeed=p->PhysiSpeed;
    const int nativeCritDisplay=ExcRank::CritChance(p->CriticalDamageRate,0,0),nativeExcDisplay=ExcRank::ExcellentChance(p->ExcellentDamageRate,0,0);
    for (auto &rank : s->ranksByOption)
        rank = 0;
    s->physicalRank = s->magicalRank = 0;
    const int rankSlots[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 237, 238, 239, 240, 241, 242};
    const bool magicWeapon = cfg.Class(p->Inventory[0].m_Index) == ExcRank::ItemClass::Magic ||
                             cfg.Class(p->Inventory[1].m_Index) == ExcRank::ItemClass::Magic;
    for (int slot : rankSlots)
    {
        auto &item = p->Inventory[slot];
        if (!item.IsItem() || !item.m_IsValidItem || !(item.m_Durability > 0))
            continue;
        ExcRank::ItemOptions data;
        if (ExcRankCache::Get().Lookup(item.m_Serial, item.m_Index, data) != ExcRankCache::State::Ready)
            continue;
        for (auto option : data.slots)
            if (cfg.Allows(item.m_Index, option.option) && cfg.Value(option))
            {
                unsigned rank = static_cast<unsigned>(option.rank);
                s->ranksByOption[option.option] = (std::max)(s->ranksByOption[option.option], rank);
                if (option.option == 3)
                {
                    const auto category = cfg.Class(item.m_Index);
                    auto &target = category == ExcRank::ItemClass::Magic ||
                                           (category == ExcRank::ItemClass::Pendant && magicWeapon)
                                       ? s->magicalRank
                                       : s->physicalRank;
                    target = (std::max)(target, rank);
                }
            }
    }
    s->nativeCritical = p->CriticalDamageRate;
    s->nativeExcellent = p->ExcellentDamageRate;

    // Classical HP and ranked HP use the same native MaxLife, not each other.
    p->AddLife = static_cast<float>(
        (std::min)(static_cast<double>(INT32_MAX) - (std::max)(0.0, static_cast<double>(p->MaxLife)),
                   static_cast<double>(p->AddLife) + p->MaxLife * v[7] / 100.0));
    p->AddShield = static_cast<int>(
        (std::min)(static_cast<double>(INT32_MAX) - (std::max)(0, p->MaxShield),
                   static_cast<double>(p->AddShield) + p->MaxShield * static_cast<double>(v[8]) / 100.0));
    p->ExcellentDamageRate = static_cast<int>(Add((std::max)(0, p->ExcellentDamageRate), v[1], 100000));
    p->CriticalDamageRate = static_cast<int>(Add((std::max)(0, p->CriticalDamageRate), v[6], 100000));
    p->ResistCriticalDamageRate =
        static_cast<int>(Add((std::max)(0, p->ResistCriticalDamageRate), v[11], cfg.sanityCap));
    if (v[9])
        p->DamageReduction[DAMAGE_REDUCTION_EXCELLENT_ITEM] = static_cast<int>(
            Add((std::max)(0, p->DamageReduction[DAMAGE_REDUCTION_EXCELLENT_ITEM]), v[9], cfg.reductionCap));
    if (v[10])
        p->DamageReflect = static_cast<int>(Add((std::max)(0, p->DamageReflect), v[10], cfg.reflectCap));
    if (v[4])
    {
        unsigned cap = cfg.attackSpeed[(std::min)(6u, static_cast<unsigned>(p->Class))];
        p->PhysiSpeed = ExcRank::ClampAttackSpeed(p->PhysiSpeed, v[4], cap);
        p->MagicSpeed = ExcRank::ClampAttackSpeed(p->MagicSpeed, v[4], cap);
    }
    s->displayed[1]=static_cast<unsigned>((std::max)(0,ExcRank::ExcellentChance(p->ExcellentDamageRate,0,0)-nativeExcDisplay));
    s->displayed[6]=static_cast<unsigned>((std::max)(0,ExcRank::CritChance(p->CriticalDamageRate,0,0)-nativeCritDisplay));
    s->displayed[9]=static_cast<unsigned>((std::max)(0,p->DamageReduction[DAMAGE_REDUCTION_EXCELLENT_ITEM]-nativeReduction));
    s->displayed[10]=static_cast<unsigned>((std::max)(0,p->DamageReflect-nativeReflect));
    s->displayed[4]=static_cast<unsigned>((std::max)(0,p->PhysiSpeed-nativePhysSpeed));
    p->MoneyAmountDropRate = static_cast<int>(Add((std::max)(0, p->MoneyAmountDropRate), v[12], 100000));
}
inline void BeginAttack(LPOBJ p)
{
    auto *s = Get(p);
    if (s)
        s->attackStyle = s->critStyle = s->excellentStyle = 0;
}
inline unsigned ExtraChance(LPOBJ p, unsigned option)
{
    auto *s = Get(p);
    return s && option < 13 ? s->totals.value[option] : 0;
}
inline bool RollChance(LPOBJ p, unsigned option, unsigned roll, int base, int bonus, int resistance,
                       int nativeOverride = -1)
{
    const bool success = roll < static_cast<unsigned>(ExcRank::CritChance(base, bonus, resistance));
    auto *s = Get(p);
    if (success && s && s->totals.value[option] &&
        roll >=
            static_cast<unsigned>(ExcRank::CritChance(
                nativeOverride >= 0 ? nativeOverride : (option == 1 ? s->nativeExcellent : s->nativeCritical),
                bonus, resistance)))
    {
        if (option == 1)
            s->excellentStyle = s->ranksByOption[1];
        else
            s->critStyle = s->ranksByOption[6];
    }
    return success;
}
inline int DamageIncrease(LPOBJ p, int damage, bool magic)
{
    auto *s = Get(p);
    if (!s || !IndependentTierControl::Enabled())
        return damage;
    const int changed = ExcRank::Scale(damage, magic ? s->magical : s->physical);
    if (changed > damage)
        s->attackStyle = (std::max)(s->attackStyle, magic ? s->magicalRank : s->physicalRank);
    return changed;
}
inline int MonsterDamage(LPOBJ p, int damage, bool excluded)
{
    auto *s = Get(p);
    if (excluded || !IndependentTierControl::Enabled())
        return damage;
    damage=IndependentExcellentTables::ScaleMonsterDamage(damage,p->TierMonsterDamagePercent,false);
    if(!s)return damage;
    const int changed = ExcRank::Scale(damage, s->totals.value[2]);
    if (changed > damage)
        s->attackStyle = (std::max)(s->attackStyle, s->ranksByOption[2]);
    return changed;
}
inline unsigned DamageStyle(LPOBJ p, unsigned type, int damage, int shield)
{
    auto *s = Get(p);
    if (!s || !Enabled() || (damage <= 0 && shield <= 0))
        return 0;
    switch (type & 15)
    {
    case 4:
        return s->displayed[10] ? s->ranksByOption[10] : 0;
    case 2:
        return (std::max)(s->attackStyle, s->excellentStyle);
    case 3:
        return (std::max)(s->attackStyle, s->critStyle);
    case 0:
    case 1:
        return s->attackStyle;
    default:
        return 0;
    }
}
inline int ReflectPercent(LPOBJ p, int native)
{
    auto *s = Get(p);
    return s && s->totals.value[10] && s->policy ? (std::min)(native, static_cast<int>(s->policy->reflectCap))
                                                 : native;
}
struct MonsterState
{
    std::uint64_t epoch = 0;
    bool heal = false, zen = false;
};
inline std::array<MonsterState, MAX_OBJECT> &Monsters()
{
    static std::array<MonsterState, MAX_OBJECT> s;
    return s;
}
inline void Spawn(int index)
{
    if (OBJECT_RANGE(index))
    {
        auto &s = Monsters()[index];
        ++s.epoch;
        s.heal = s.zen = false;
    }
}
inline bool Claim(LPOBJ victim, bool heal)
{
    if (!victim || victim->Type != OBJECT_MONSTER || !OBJECT_RANGE(victim->Index) ||
        OBJECT_RANGE(victim->SummonIndex))
        return false;
    auto &s = Monsters()[victim->Index];
    bool &flag = heal ? s.heal : s.zen;
    if (flag)
        return false;
    flag = true;
    return true;
}
inline int LifeOnKill(LPOBJ p, LPOBJ victim)
{
    auto *s = Get(p);
    return s && s->totals.value[5] && Claim(victim, true)
               ? ExcRank::LifeOnKillAmount(p->MaxLife + p->AddLife, p->Life, s->totals.value[5],
                                           victim->Level)
               : 0;
}
inline bool AllowZen(LPOBJ p, LPOBJ victim)
{
    auto *s = Get(p);
    return !s || !s->totals.value[12] || Claim(victim, false);
}
} // namespace ExcRankRuntime

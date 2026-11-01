#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ExcRank
{
enum class Rank : std::uint8_t
{
    None = 0,
    Rare = 1,
    Legendary = 2,
    Epic = 3
};
enum class ItemClass : unsigned
{
    None,
    Physical,
    Magic,
    Pendant,
    Armor,
    Shield,
    Ring,
    Wing1,
    Wing2,
    Cape
};
enum class Origin : unsigned
{
    Drop,
    Boss,
    Event,
    NPC,
    Reward,
    Admin,
    Count
};
struct Slot
{
    std::uint8_t option = 0;
    Rank rank = Rank::None;
    Slot()
    {
    }
    Slot(unsigned id, Rank r) : option(static_cast<std::uint8_t>(id)), rank(r)
    {
    }
};
struct ExclusiveAttribute
{
    std::uint8_t id = 0;
    std::uint16_t value = 0;
};
struct ItemOptions
{
    std::array<ExclusiveAttribute, 2> attributes{};
    std::vector<Slot> slots;
    bool Valid() const;
};
inline bool ValidateStructure(const ItemOptions &options)
{
    if (options.slots.size() > 4)
        return false;
    const auto &a = options.attributes;
    if (a[0].id || a[1].id || a[0].value || a[1].value)
    {
        if (a[0].id < 1 || a[0].id > 4 || a[1].id < 1 || a[1].id > 4 || a[0].id == a[1].id || !a[0].value ||
            a[0].value > 1000 || !a[1].value || a[1].value > 1000)
            return false;
    }
    unsigned seen = 0;
    for (const auto &s : options.slots)
    {
        unsigned r = static_cast<unsigned>(s.rank);
        if (s.option < 1 || s.option > 12 || r < 1 || r > 3 || (seen & (1u << s.option)))
            return false;
        seen |= 1u << s.option;
    }
    return true;
}
inline bool ItemOptions::Valid() const
{
    return ValidateStructure(*this);
}
inline std::uint8_t CRC8(const std::uint8_t *p, std::size_t size)
{
    std::uint8_t c = 0;
    for (std::size_t n = 0; n < size; ++n)
    {
        c ^= p[n];
        for (unsigned b = 0; b < 8; ++b)
            c = static_cast<std::uint8_t>((c & 128) ? (c << 1) ^ 7 : c << 1);
    }
    return c;
}
inline bool Encode(const ItemOptions &data, std::vector<std::uint8_t> &out)
{
    if (!data.Valid())
        return false;
    const bool attributes = data.attributes[0].id != 0;
    std::vector<std::uint8_t> b{static_cast<std::uint8_t>(attributes ? 2 : 1),
                                static_cast<std::uint8_t>(data.slots.size())};
    for (auto s : data.slots)
    {
        b.push_back(s.option);
        b.push_back(static_cast<std::uint8_t>(s.rank));
    }
    if (attributes)
        for (auto attribute : data.attributes)
        {
            b.push_back(attribute.id);
            b.push_back(static_cast<std::uint8_t>(attribute.value));
            b.push_back(static_cast<std::uint8_t>(attribute.value >> 8));
        }
    b.push_back(CRC8(b.data(), b.size()));
    out.swap(b);
    return true;
}
inline bool Decode(const std::uint8_t *p, std::size_t size, ItemOptions &out)
{
    if (!p || size < 3 || (p[0] != 1 && p[0] != 2) || p[1] > 4 || size != (p[0] == 2 ? 9u : 3u) + 2u * p[1] ||
        CRC8(p, size - 1) != p[size - 1])
        return false;
    ItemOptions d;
    for (unsigned n = 0; n < p[1]; ++n)
        d.slots.push_back(Slot(p[2 + n * 2], static_cast<Rank>(p[3 + n * 2])));
    if (p[0] == 2)
        for (unsigned n = 0; n < 2; ++n)
        {
            const auto pos = 2 + 2u * p[1] + 3 * n;
            d.attributes[n].id = p[pos];
            d.attributes[n].value = static_cast<std::uint16_t>(p[pos + 1] | (p[pos + 2] << 8));
        }
    if (!d.Valid())
        return false;
    out = d;
    return true;
}
inline std::string ToHex(const std::vector<std::uint8_t> &b)
{
    const char *h = "0123456789ABCDEF";
    std::string s;
    for (auto v : b)
    {
        s += h[v >> 4];
        s += h[v & 15];
    }
    return s;
}
inline bool FromHex(const std::string &s, std::vector<std::uint8_t> &out)
{
    if (s.size() % 2 || s.size() > 34)
        return false;
    std::vector<std::uint8_t> b;
    for (std::size_t n = 0; n < s.size(); n += 2)
    {
        unsigned v = 0;
        for (unsigned k = 0; k < 2; ++k)
        {
            char c = s[n + k];
            unsigned d = c >= '0' && c <= '9'   ? c - '0'
                         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                         : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                                : 99;
            if (d > 15)
                return false;
            v = v * 16 + d;
        }
        b.push_back(static_cast<std::uint8_t>(v));
    }
    out.swap(b);
    return true;
}
struct SourceRule
{
    bool enabled = false;
    unsigned chance = 100, rankMask = 7;
};
struct Config
{
    bool enabled = true, requireExcellent = true;
    unsigned maxOptions = 3, weight[3] = {70, 25, 5};
    bool rankEnabled[3] = {true, true, true};
    bool optionEnabled[13];
    unsigned value[13][3];
    unsigned classMask[10];
    SourceRule sources[6];
    unsigned reductionCap = 60, reflectCap = 50, sanityCap = 1000,
             attackSpeed[7] = {1000, 1000, 1000, 1000, 1000, 1000, 1000};
    std::map<unsigned, unsigned> baseScores;
    unsigned ancientBasePercent = 250, classicPoints = 5, lifeScorePoints = 3;
    unsigned rankScore[3] = {20, 35, 50};
    unsigned armorMin = 20, armorMax = 40, weaponMin = 75, weaponMax = 150, armorStep = 20, weaponStep = 25;
    unsigned exclusiveTierBase = 50, exclusiveTierStep = 25;
    std::map<unsigned, ItemClass> itemClasses;
    std::map<unsigned, bool> allowedItems;
    std::map<unsigned, Origin> mapOrigins;
    std::map<unsigned, bool> bossMonsters;
    std::vector<std::pair<unsigned, unsigned>> exclusive;
    Config()
    {
        for (unsigned i = 0; i < 13; ++i)
        {
            optionEnabled[i] = i != 0;
            for (unsigned r = 0; r < 3; ++r)
                value[i][r] = 0;
        }
        const unsigned table[12][3] = {{7, 8, 10}, {7, 8, 10}, {7, 8, 10}, {28, 32, 40},
                                       {1, 2, 3},  {7, 8, 10}, {3, 4, 5},  {3, 4, 5},
                                       {3, 4, 5},  {3, 4, 5},  {7, 8, 10}, {21, 24, 30}};
        for (unsigned i = 1; i <= 12; ++i)
            for (unsigned r = 0; r < 3; ++r)
                value[i][r] = table[i - 1][r];
        for (auto &mask : classMask)
            mask = 0;
        classMask[1] = classMask[2] = classMask[3] = 0x3F;
        classMask[4] = classMask[5] = classMask[6] = 0xFC0;
        sources[5].enabled = true;
        exclusive.push_back({1, 6});
    }
    ItemClass Class(unsigned id) const
    {
        auto i = itemClasses.find(id);
        if (i != itemClasses.end())
            return i->second;
        unsigned group = id / 512, index = id % 512;
        if (group <= 4)
            return ItemClass::Physical;
        if (group == 5)
            return ItemClass::Magic;
        if (group == 6)
            return ItemClass::Shield;
        if (group >= 7 && group <= 11)
            return ItemClass::Armor;
        if (group == 13)
        {
            if (index == 12 || index == 13 || (index >= 25 && index <= 28))
                return ItemClass::Pendant;
            if (index == 8 || index == 9 || index == 21 || index == 22 || index == 23 || index == 24)
                return ItemClass::Ring;
        }
        return ItemClass::None;
    }
    bool Allows(unsigned item, unsigned option) const
    {
        auto a = allowedItems.find(item);
        if (a != allowedItems.end() && !a->second)
            return false;
        if (item > 8191 || option < 1 || option > 12 || !optionEnabled[option])
            return false;
        const unsigned category = static_cast<unsigned>(Class(item));
        return category < 10 && (classMask[category] & (1u << (option - 1))) != 0;
    }
    unsigned Value(Slot s) const
    {
        unsigned rank = static_cast<unsigned>(s.rank);
        return s.option >= 1 && s.option <= 12 && rank >= 1 && rank <= 3 && rankEnabled[rank - 1] &&
                       optionEnabled[s.option]
                   ? value[s.option][rank - 1]
                   : 0;
    }
};
inline bool ValidatePolicy(const ItemOptions &data, unsigned item, const Config &cfg, bool generating = true)
{
    if (!data.Valid() || (generating && data.slots.size() > cfg.maxOptions))
        return false;
    for (const auto &s : data.slots)
        if (!cfg.Allows(item, s.option) || !cfg.Value(s))
            return false;
    for (const auto &e : cfg.exclusive)
    {
        bool a = false, b = false;
        for (auto s : data.slots)
        {
            a |= s.option == e.first;
            b |= s.option == e.second;
        }
        if (a && b)
            return false;
    }
    return true;
}
template <class Random>
bool GenerateExclusive(const Config &cfg, unsigned item, Random &rng, ItemOptions &out, bool nativeExcellent = false)
{
    if ((!nativeExcellent && out.slots.empty()) || out.attributes[0].id)
        return true;
    const auto category = cfg.Class(item);
    if (category == ItemClass::None)
        return true;
    const auto base = cfg.baseScores.find(item);
    const unsigned score = base == cfg.baseScores.end() ? cfg.exclusiveTierBase : base->second;
    const unsigned tier = (std::min)(
        3u, score > cfg.exclusiveTierBase ? (score - cfg.exclusiveTierBase) / cfg.exclusiveTierStep : 0u);
    const bool weapon =
        category == ItemClass::Physical || category == ItemClass::Magic || category == ItemClass::Pendant;
    unsigned low = (weapon ? cfg.weaponMin : cfg.armorMin) + tier * (weapon ? cfg.weaponStep : cfg.armorStep);
    unsigned high =
        (weapon ? cfg.weaponMax : cfg.armorMax) + 2 * tier * (weapon ? cfg.weaponStep : cfg.armorStep);
    low = (std::min)(1000u, low);
    high = (std::min)(1000u, high);
    std::vector<unsigned> pool{1, 2, 3, 4};
    auto result = out;
    for (unsigned n = 0; n < 2; ++n)
    {
        std::uint32_t roll = 0;
        if (!rng.Draw(static_cast<unsigned>(pool.size()), roll) || roll >= pool.size())
            return false;
        result.attributes[n].id = static_cast<std::uint8_t>(pool[roll]);
        pool.erase(pool.begin() + roll);
        if (!rng.Draw(high - low + 1, roll) || roll > high - low)
            return false;
        result.attributes[n].value = static_cast<std::uint16_t>(low + roll);
    }
    out = result;
    return true;
}
struct Score
{
    bool valid = false;
    unsigned base = 0, grade = 0;
};
inline Score EvaluateScore(const Config &cfg, unsigned id, unsigned level, unsigned life,
                           unsigned excellentMask, bool ancient, const ItemOptions &extra, bool gradeEnabled)
{
    Score out;
    auto base = cfg.baseScores.find(id);
    if (base == cfg.baseScores.end())
        return out;
    out.valid = true;
    out.base = static_cast<unsigned>(
        (std::min)(std::uint64_t(1000000),
                   std::uint64_t(base->second) * (ancient ? cfg.ancientBasePercent : 100) / 100));
    if (!gradeEnabled)
        return out;
    const unsigned refine[] = {0, 2, 5, 9, 14, 20, 27, 35, 44, 54, 65, 77, 90, 104, 119, 135};
    unsigned grade = refine[(std::min)(15u, level)] + (std::min)(7u, life) * cfg.lifeScorePoints;
    for (unsigned bit = 0; bit < 6; ++bit)
        if (excellentMask & (1u << bit))
            grade += cfg.classicPoints;
    for (auto option : extra.slots)
        if (cfg.Allows(id, option.option) && cfg.Value(option))
            grade += cfg.rankScore[static_cast<unsigned>(option.rank) - 1];
    out.grade = (std::min)(1000000u, grade);
    return out;
}
template <class Random>
bool Generate(const Config &cfg, unsigned item, Origin origin, Random &rng, ItemOptions &out,
              Rank forced = Rank::None)
{
    out = {};
    unsigned source = static_cast<unsigned>(origin);
    if (!cfg.enabled || source >= 6 || !cfg.sources[source].enabled)
        return true;
    std::uint32_t roll = 0;
    if (!rng.Draw(100, roll) || roll >= 100)
        return false;
    if (roll >= cfg.sources[source].chance)
        return true;
    std::vector<unsigned> pool;
    for (unsigned i = 1; i <= 12; ++i)
        if (cfg.Allows(item, i))
            pool.push_back(i);
    while (!pool.empty() && out.slots.size() < cfg.maxOptions)
    {
        if (!rng.Draw(static_cast<std::uint32_t>(pool.size()), roll) || roll >= pool.size())
            return false;
        unsigned id = pool[roll];
        pool.erase(pool.begin() + roll);
        bool conflict = false;
        for (const auto &e : cfg.exclusive)
            for (auto s : out.slots)
                if ((id == e.first && s.option == e.second) || (id == e.second && s.option == e.first))
                    conflict = true;
        if (conflict)
            continue;
        unsigned r = static_cast<unsigned>(forced);
        if (!r)
        {
            unsigned total = 0;
            for (unsigned k = 0; k < 3; ++k)
                if (cfg.rankEnabled[k] && (cfg.sources[source].rankMask & (1u << k)))
                    total += cfg.weight[k];
            if (!total)
                return true;
            if (!rng.Draw(total, roll) || roll >= total)
                return false;
            for (unsigned k = 0; k < 3; ++k)
                if (cfg.rankEnabled[k] && (cfg.sources[source].rankMask & (1u << k)))
                {
                    if (roll < cfg.weight[k])
                    {
                        r = k + 1;
                        break;
                    }
                    roll -= cfg.weight[k];
                }
        }
        if (r < 1 || r > 3 || !cfg.rankEnabled[r - 1] || !(cfg.sources[source].rankMask & (1u << (r - 1))))
            return false;
        out.slots.push_back(Slot(id, static_cast<Rank>(r)));
    }
    return ValidatePolicy(out, item, cfg) && GenerateExclusive(cfg, item, rng, out);
}
struct Totals
{
    std::array<unsigned, 13> value{};
    unsigned pieces = 0, ranks[3] = {};
};
inline void Aggregate(Totals &out, const ItemOptions &options, unsigned item, const Config &cfg)
{
    if (!options.Valid())
        return;
    bool used = false;
    for (auto s : options.slots)
    {
        if (!cfg.Allows(item, s.option))
            continue;
        unsigned v = cfg.Value(s);
        if (!v)
            continue;
        out.value[s.option] = (std::min)(cfg.sanityCap, out.value[s.option] + v);
        ++out.ranks[static_cast<unsigned>(s.rank) - 1];
        used = true;
    }
    if (used)
        ++out.pieces;
}
inline int Scale(int v, unsigned percent)
{
    if (v <= 0)
        return v;
    std::int64_t n = static_cast<std::int64_t>(v) * (100ull + percent) / 100;
    return static_cast<int>((std::min)(n, static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
}
inline int AdjustOutgoingDamage(int damage, const Totals &t, bool monster)
{
    damage = Scale(damage, t.value[3]);
    return monster ? Scale(damage, t.value[2]) : damage;
}
inline int ReduceIncomingDamage(int damage, unsigned total, unsigned cap = 60)
{
    return damage <= 0 ? damage
                       : (std::max)(1, static_cast<int>(static_cast<std::int64_t>(damage) *
                                                        (100 - (std::min)(total, cap)) / 100));
}
inline int ReflectAmount(int damage, unsigned total, bool reflected = false, bool environmental = false,
                         unsigned cap = 50)
{
    if (damage <= 0 || reflected || environmental)
        return 0;
    const auto amount = static_cast<std::int64_t>(damage) * (std::min)(total, cap) / 100;
    return static_cast<int>((std::min)(amount, static_cast<std::int64_t>(INT32_MAX)));
}
inline int CritChance(int base, int bonus, int resistance)
{
    const std::int64_t n = static_cast<std::int64_t>(base) + bonus - resistance;
    return static_cast<int>((std::max)(std::int64_t(0), (std::min)(std::int64_t(100), n)));
}
inline int ExcellentChance(int base, int bonus, int resistance)
{
    return CritChance(base, bonus, resistance);
}
inline int ClampAttackSpeed(int native, unsigned bonus, unsigned maximum)
{
    return static_cast<int>(
        (std::max)(std::int64_t(0), (std::min)(static_cast<std::int64_t>(INT32_MAX),
                                               (std::min)(static_cast<std::int64_t>(maximum),
                                                          static_cast<std::int64_t>(native) + bonus))));
}
inline int ScaleMax(int base, unsigned percent)
{
    return Scale(base, percent);
}
inline int LifeOnKillAmount(double maxHP, double life, unsigned percent, int monsterLevel)
{
    if (!percent || !std::isfinite(maxHP) || !std::isfinite(life) || maxHP <= 0)
        return 0;
    double value = maxHP * percent / 100.0 + (std::max)(0, monsterLevel);
    return static_cast<int>(
        (std::max)(0.0, (std::min)((std::min)(value, maxHP - life), static_cast<double>(INT32_MAX))));
}
inline int ZenWithBonus(int native, unsigned percent)
{
    return Scale(native, percent);
}
class KillRewardGuard
{
    std::map<std::uint64_t, std::uint64_t> kills;

  public:
    bool Claim(std::uint64_t key, std::uint64_t now)
    {
        auto i = kills.find(key);
        if (i != kills.end() && now - i->second < 60000)
            return false;
        if (kills.size() > 8192)
            for (auto j = kills.begin(); j != kills.end();)
                if (now - j->second >= 60000)
                    j = kills.erase(j);
                else
                    ++j;
        kills[key] = now;
        return true;
    }
};
inline const char *RankName(Rank r)
{
    return r == Rank::Rare ? "RARE" : r == Rank::Legendary ? "LEGENDARY" : r == Rank::Epic ? "EPIC" : "";
}
inline unsigned RankColorRGB(Rank r)
{
    return r == Rank::Rare        ? 0x4DA3FF
           : r == Rank::Legendary ? 0xFFA630
           : r == Rank::Epic      ? 0xC06BFF
                                  : 0xFFFFFF;
}
inline std::string FormatLine(unsigned id, Rank r, unsigned value)
{
    const char *names[] = {"",
                           "Excellent damage chance",
                           "Damage to monsters",
                           "Damage increase",
                           "Attack speed",
                           "Life on monster kill",
                           "Critical damage chance",
                           "Maximum life",
                           "Maximum SD",
                           "Damage decrease",
                           "Reflect damage",
                           "Critical resistance",
                           "Zen obtained"};
    if (id < 1 || id > 12)
        return "";
    std::ostringstream s;
    s << "[ " << RankName(r) << " ] " << names[id] << " +" << value << (id == 4 ? "" : "%");
    if (id == 5)
        s << " + monster level";
    return s.str();
}

inline std::string Trim(std::string s)
{
    const auto a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
inline bool Number(const std::string &s, unsigned &out)
{
    if (s.empty() || s[0] == '-')
        return false;
    char *end = nullptr;
    unsigned long n = std::strtoul(s.c_str(), &end, 0);
    if (!end || *end || n > 1000000)
        return false;
    out = static_cast<unsigned>(n);
    return true;
}
inline bool Load(const std::string &path, Config &result, std::string &error)
{
    std::ifstream f(path.c_str());
    if (!f)
    {
        error = "cannot open " + path;
        return false;
    }
    Config c;
    std::string section, line;
    unsigned number = 0;
    while (std::getline(f, line))
    {
        ++number;
        line = Trim(line.substr(0, line.find(';')));
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            if (section == "Exclusive")
                c.exclusive.clear();
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            error = "expected key=value at line " + std::to_string(number);
            return false;
        }
        std::string key = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        unsigned n = 0;
        if (!Number(v, n))
        {
            error = "invalid integer at line " + std::to_string(number);
            return false;
        }
        bool known = true;
        if ((key == "Enabled" || key == "RequireExcellent" || key == "RareEnabled" ||
             key == "LegendaryEnabled" || key == "EpicEnabled") &&
            n > 1)
        {
            error = "boolean must be 0 or 1";
            return false;
        }
        if (section == "System")
        {
            if (key == "Enabled")
                c.enabled = n == 1;
            else if (key == "RequireExcellent")
                c.requireExcellent = n == 1;
            else if (key == "MaxOptions")
                c.maxOptions = n;
            else if (key == "SanityCap")
                c.sanityCap = n;
            else if (key == "DamageDecreaseCap")
                c.reductionCap = n;
            else if (key == "ReflectCap")
                c.reflectCap = n;
            else
                known = false;
        }
        else if (section == "Ranks")
        {
            const char *names[] = {"Rare", "Legendary", "Epic"};
            known = false;
            for (unsigned k = 0; k < 3; ++k)
            {
                if (key == names[k])
                {
                    c.weight[k] = n;
                    known = true;
                }
                if (key == std::string(names[k]) + "Enabled")
                {
                    c.rankEnabled[k] = n == 1;
                    known = true;
                }
            }
        }
        else if (section.compare(0, 6, "Option") == 0)
        {
            unsigned id = 0;
            if (!Number(section.substr(6), id) || id < 1 || id > 12)
                known = false;
            else if (key == "Enabled")
                c.optionEnabled[id] = n == 1;
            else if (key == "Rare")
                c.value[id][0] = n;
            else if (key == "Legendary")
                c.value[id][1] = n;
            else if (key == "Epic")
                c.value[id][2] = n;
            else
                known = false;
        }
        else if (section.compare(0, 6, "Source") == 0)
        {
            unsigned id = 0;
            if (!Number(section.substr(6), id) || id >= 6)
                known = false;
            else if (key == "Enabled")
                c.sources[id].enabled = n == 1;
            else if (key == "Chance")
                c.sources[id].chance = n;
            else if (key == "RankMask")
                c.sources[id].rankMask = n;
            else
                known = false;
        }
        else if (section == "BaseScore")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 8191 || !n || n > 400000)
                known = false;
            else
                c.baseScores[id] = n;
        }
        else if (section == "Score")
        {
            if (key == "AncientBasePercent" && n >= 100 && n <= 1000)
                c.ancientBasePercent = n;
            else if (key == "ClassicOptionPoints" && n <= 1000)
                c.classicPoints = n;
            else if (key == "LifePoints" && n <= 1000)
                c.lifeScorePoints = n;
            else if (key == "RarePoints" && n <= 1000)
                c.rankScore[0] = n;
            else if (key == "LegendaryPoints" && n <= 1000)
                c.rankScore[1] = n;
            else if (key == "EpicPoints" && n <= 1000)
                c.rankScore[2] = n;
            else
                known = false;
        }
        else if (section == "ExclusiveBonus")
        {
            if (n > 500)
                known = false;
            else if (key == "ArmorMin")
                c.armorMin = n;
            else if (key == "ArmorMax")
                c.armorMax = n;
            else if (key == "WeaponMin")
                c.weaponMin = n;
            else if (key == "WeaponMax")
                c.weaponMax = n;
            else if (key == "ArmorStep")
                c.armorStep = n;
            else if (key == "WeaponStep")
                c.weaponStep = n;
            else if (key == "TierBase")
                c.exclusiveTierBase = n;
            else if (key == "TierStep" && n)
                c.exclusiveTierStep = n;
            else
                known = false;
        }
        else if (section == "ItemClass")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 8191 || n > 9)
                known = false;
            else
                c.itemClasses[id] = static_cast<ItemClass>(n);
        }
        else if (section == "AllowedItems")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 8191 || n > 1)
                known = false;
            else
                c.allowedItems[id] = n != 0;
        }
        else if (section == "ClassOptions")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 9 || n > 4095)
                known = false;
            else
                c.classMask[id] = n;
        }
        else if (section == "MapOrigin")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 255 || n >= 6)
                known = false;
            else
                c.mapOrigins[id] = static_cast<Origin>(n);
        }
        else if (section == "BossMonsters")
        {
            unsigned id = 0;
            if (!Number(key, id) || id > 65535 || n > 1)
                known = false;
            else
                c.bossMonsters[id] = n != 0;
        }
        else if (section == "AttackSpeed")
        {
            unsigned id = 0;
            if (!Number(key, id) || id >= 7 || n > 65535 || !n)
                known = false;
            else
                c.attackSpeed[id] = n;
        }
        else if (section == "Exclusive")
        {
            unsigned id = 0;
            if (!Number(key, id) || id < 1 || id > 12 || n < 1 || n > 12 || n == id)
                known = false;
            else
                c.exclusive.push_back({id, n});
        }
        else
            known = false;
        if (!known)
        {
            error = "unknown/invalid setting " + section + "/" + key;
            return false;
        }
    }
    if (c.maxOptions < 1 || c.maxOptions > 4 || c.sanityCap < 1 || c.sanityCap > 1000 ||
        c.reductionCap > 60 || c.reflectCap > 50)
    {
        error = "invalid option count or combat caps";
        return false;
    }
    if (!c.armorMin || c.armorMax < c.armorMin || !c.weaponMin || c.weaponMax < c.weaponMin)
    {
        error = "invalid exclusive ranges";
        return false;
    }
    unsigned total = 0;
    for (unsigned r = 0; r < 3; ++r)
        if (c.rankEnabled[r])
            total += c.weight[r];
    if (!total || total > 1000000)
    {
        error = "invalid rank weights";
        return false;
    }
    for (unsigned id = 1; id <= 12; ++id)
        for (unsigned r = 0; r < 3; ++r)
            if (c.value[id][r] > 1000 || (c.optionEnabled[id] && c.rankEnabled[r] && !c.value[id][r]))
            {
                error = "option value exceeds sanity cap";
                return false;
            }
    for (auto s : c.sources)
        if (s.chance > 100 || s.rankMask > 7)
        {
            error = "invalid source rule";
            return false;
        }
    result = c;
    error.clear();
    return true;
}
} // namespace ExcRank

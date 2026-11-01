#pragma once
#include "ExcRankCore.h"
namespace ExcRankOrigin
{
inline ExcRank::Origin &Current()
{
    static thread_local ExcRank::Origin value = ExcRank::Origin::Drop;
    return value;
}
class Scoped
{
    ExcRank::Origin old;

  public:
    explicit Scoped(ExcRank::Origin value) : old(Current())
    {
        Current() = value;
    }
    ~Scoped()
    {
        Current() = old;
    }
    Scoped(const Scoped &) = delete;
    Scoped &operator=(const Scoped &) = delete;
};
// Dedicated authenticated GS->DS trailer; no native item field is repurposed.
constexpr unsigned TrailerSize = 7;
inline void Write(std::uint8_t *bytes, ExcRank::Origin source, std::uint32_t copySerial)
{
    bytes[0] = 0xE7;
    bytes[1] = 1;
    bytes[2] = static_cast<std::uint8_t>(source);
    for (unsigned i = 0; i < 4; ++i)
        bytes[3 + i] = static_cast<std::uint8_t>(copySerial >> (8 * i));
}
inline bool Read(const std::uint8_t *bytes, std::size_t size, ExcRank::Origin &source, std::uint32_t &copy)
{
    if (!bytes || size != TrailerSize || bytes[0] != 0xE7 || bytes[1] != 1 || bytes[2] >= 6)
        return false;
    source = static_cast<ExcRank::Origin>(bytes[2]);
    copy = 0;
    for (unsigned i = 0; i < 4; ++i)
        copy |= static_cast<std::uint32_t>(bytes[3 + i]) << (8 * i);
    return true;
}
} // namespace ExcRankOrigin

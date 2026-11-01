#pragma once
#include <cstdint>
#include <cstddef>
namespace RankDamageVisual
{
constexpr unsigned Tag = 0xE8, Marker = 0xC00;
inline unsigned Read(const std::uint8_t *p, std::size_t size, std::size_t offset)
{
    return p && offset <= size && size - offset >= 2 && p[offset] == Tag && p[offset + 1] >= 1 &&
                   p[offset + 1] <= 3
               ? p[offset + 1]
               : 0;
}
inline unsigned WithRank(unsigned type, unsigned rank)
{
    return (type & ~Marker) | ((rank >= 1 && rank <= 3 ? rank : 0) << 10);
}
inline unsigned FromType(unsigned type)
{
    return (type >> 10) & 3;
}
inline void Tint(unsigned rank, float *color)
{
    if (!color)
        return;
    const unsigned rgb = rank == 1 ? 0x4DA3FF : rank == 2 ? 0xFFA630 : rank == 3 ? 0xC06BFF : 0;
    if (!rgb)
        return;
    color[0] = ((rgb >> 16) & 255) / 255.f;
    color[1] = ((rgb >> 8) & 255) / 255.f;
    color[2] = (rgb & 255) / 255.f;
}
} // namespace RankDamageVisual

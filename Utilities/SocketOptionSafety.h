#pragma once
#include <cstddef>
namespace SocketOptionSafety {
template<std::size_t N>
bool CountCategory(int (&counts)[N], int category) {
    if(category < 1 || static_cast<std::size_t>(category) > N)return false;
    ++counts[category-1];
    return true;
}
inline bool ValidSeed(unsigned seed, unsigned count) { return seed < count; }
inline bool ValidSphere(unsigned level, unsigned count) { return level >= 1 && level <= count; }
}

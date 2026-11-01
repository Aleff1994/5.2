#pragma once
#include <cstddef>
namespace BoundedPacketText {
// Both wire field and destination are bounded, even without a wire terminator.
template<std::size_t Destination, std::size_t Source>
inline void Copy(char (&destination)[Destination], const char (&source)[Source]) {
    static_assert(Destination > 0, "Text destination must hold a terminator");
    std::size_t i = 0;
    while (i < Source && i + 1 < Destination && source[i] != 0) {
        destination[i] = source[i];
        ++i;
    }
    destination[i] = 0;
}
}

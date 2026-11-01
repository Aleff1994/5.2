#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#pragma comment(lib, "bcrypt.lib")
namespace ExcRank
{
struct SecureRandom
{
    bool Draw(std::uint32_t total, std::uint32_t &out)
    {
        if (!total)
            return false;
        const auto threshold = (std::uint32_t(0) - total) % total;
        for (unsigned n = 0; n < 64; ++n)
        {
            std::uint32_t value = 0;
            if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value),
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
                return false;
            if (value >= threshold)
            {
                out = value % total;
                return true;
            }
        }
        return false;
    }
};
} // namespace ExcRank

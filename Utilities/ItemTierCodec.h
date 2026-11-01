#pragma once
// Version-1 transport proposal. The low nibble remains the Ancient option.
// This codec does not alter ItemOptionEx (cash-shop/380 flags).
// Do not enable producers until all item transfer consumers use this codec.
namespace ItemTierCodec
{
    inline unsigned char Level(unsigned char packed)
    {
        const unsigned char value = packed >> 4;
        return value <= 10 ? value : 0;
    }
    inline unsigned char Ancient(unsigned char packed) { return packed & 15; }
    inline unsigned char Pack(unsigned char ancient, unsigned char level)
    {
        return (ancient & 15) | ((level <= 10 ? level : 0) << 4);
    }
}

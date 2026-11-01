#pragma once
namespace ExcellentRankStyle {
constexpr int Rare=1001,Legendary=1002,Epic=1003;
inline bool ResolveRGB(int color,unsigned& r,unsigned& g,unsigned& b) {
 switch(color) {
 case Rare:r=0x4D;g=0xA3;b=0xFF;return true;
 case Legendary:r=0xFF;g=0xA6;b=0x30;return true;
 case Epic:r=0xC0;g=0x6B;b=0xFF;return true;
 default:return false;
 }
}
}

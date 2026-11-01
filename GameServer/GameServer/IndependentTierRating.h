#pragma once
#include "GearScoreRuntime.h"
namespace IndependentTierRating {
inline NeonGear::Score Evaluate(CItem* item,const IndependentItemTiers::Metadata& metadata,bool ready) {
 return item ? GearScoreRuntime::Evaluate(*item,metadata,ready) : NeonGear::Score{};
}
}

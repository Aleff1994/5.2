#pragma once
#include "ItemGearScore.h"
#include <atomic>
#include <memory>
#include <utility>

namespace NeonGear {
// Load on server start or explicit admin reload, never inside an attack/tooltip loop.
// Readers keep one immutable snapshot throughout a character recalculation.
class GearScoreService {
public:
    bool Load(const std::string& path, std::string& error) {
        Config config;
        if(!LoadConfigFile(path, config, error)) return false;
        std::shared_ptr<const ItemGearScore> replacement =
            std::make_shared<const ItemGearScore>(std::move(config));
        std::atomic_store(&policy_, std::move(replacement));
        return true;
    }
    std::shared_ptr<const ItemGearScore> Snapshot() const noexcept {
        return std::atomic_load(&policy_);
    }
private:
    std::shared_ptr<const ItemGearScore> policy_;
};
} // namespace NeonGear

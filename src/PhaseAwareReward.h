#pragma once

#include <RLGymCPP/Gamestates/GameState.h>
#include <RLGymCPP/Rewards/Reward.h>
#include "CurriculumManager.h"

#include <memory>
#include <vector>

namespace MyGL {

using RLGC::Reward;

class PhaseAwareReward : public Reward {
public:
    struct Entry {
        Entry(std::unique_ptr<Reward> reward, float weight);
        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;
        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

        std::unique_ptr<Reward> reward;
        float weight;
    };

    PhaseAwareReward(CurriculumManager& manager, std::vector<Entry> skillRewards, std::vector<Entry> tacticalRewards);

    std::vector<float> GetAllRewards(const RLGC::GameState& state, bool isFinal) override;

private:
    std::vector<float> Evaluate(const std::vector<Entry>& entries, const RLGC::GameState& state, bool isFinal) const;

    std::vector<Entry> skillEntries;
    std::vector<Entry> tacticalEntries;
    CurriculumManager& manager;
};

}

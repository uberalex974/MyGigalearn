#include "PhaseAwareReward.h"

namespace MyGL {

PhaseAwareReward::Entry::Entry(std::unique_ptr<Reward> reward, float weight) :
    reward(std::move(reward)), weight(weight) { }

PhaseAwareReward::PhaseAwareReward(CurriculumManager& manager, std::vector<Entry> skillRewards, std::vector<Entry> tacticalRewards) :
    manager(manager),
    skillEntries(std::move(skillRewards)),
    tacticalEntries(std::move(tacticalRewards)) {
}

std::vector<float> PhaseAwareReward::Evaluate(const std::vector<Entry>& entries, const RLGC::GameState& state, bool isFinal) const {
    std::vector<float> result(state.players.size(), 0.f);
    for (const auto& entry : entries) {
        if (!entry.reward)
            continue;

        auto rewards = entry.reward->GetAllRewards(state, isFinal);
        for (size_t i = 0; i < result.size() && i < rewards.size(); ++i) {
            result[i] += rewards[i] * entry.weight;
        }
    }
    return result;
}

std::vector<float> PhaseAwareReward::GetAllRewards(const RLGC::GameState& state, bool isFinal) {
    switch (manager.CurrentPhase()) {
        case CurriculumPhase::Arena:
            return Evaluate(tacticalEntries, state, isFinal);
        default:
            return Evaluate(skillEntries, state, isFinal);
    }
}

}

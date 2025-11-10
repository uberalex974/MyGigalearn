#include "CurriculumManager.h"

namespace MyGL {

CurriculumManager& CurriculumManager::Instance() {
    static CurriculumManager instance;
    return instance;
}

void CurriculumManager::Configure(const CurriculumParameters& params_) {
    parameters = params_;
    phase = params_.initialPhase;
    autoSwitchEnabled = params_.autoSwitch;
    accumulators.clear();
    logCounter = 0;
}

void CurriculumManager::SetPhaseChangeCallback(std::function<void(CurriculumPhase)> callback) {
    phaseChangeCallback = std::move(callback);
}

CurriculumPhase CurriculumManager::CurrentPhase() const {
    return phase;
}

bool CurriculumManager::UseScenarioCurriculum() const {
    return phase == CurriculumPhase::Dojo;
}

void CurriculumManager::ObserveScenarioReward(const std::string& scenario, double reward) {
    auto& entry = accumulators[scenario];
    entry.first += reward;
    ++entry.second;
}

std::optional<std::unordered_map<std::string, double>> CurriculumManager::CollectScenarioAverages() {
    if (parameters.logInterval <= 0)
        return std::nullopt;

    if (++logCounter < static_cast<std::uint64_t>(parameters.logInterval))
        return std::nullopt;

    std::unordered_map<std::string, double> averages;
    for (auto& pair : accumulators) {
        auto& scenario = pair.first;
        auto sum = pair.second.first;
        auto count = pair.second.second;
        if (count == 0)
            continue;
        averages[scenario] = sum / static_cast<double>(count);
    }

    TryAutoSwitch(accumulators);
    accumulators.clear();
    logCounter = 0;

    return averages;
}

void CurriculumManager::ForcePhase(CurriculumPhase newPhase) {
    if (phase == newPhase)
        return;
    phase = newPhase;
    if (phaseChangeCallback)
        phaseChangeCallback(phase);
}

void CurriculumManager::TryAutoSwitch(const std::unordered_map<std::string, std::pair<double, std::uint64_t>>& averages) {
    if (!autoSwitchEnabled || phase != CurriculumPhase::Dojo)
        return;

    for (const auto& pair : averages) {
        auto count = pair.second.second;
        if (count < parameters.autoSwitchMinSamples)
            continue;
        double avg = pair.second.first / static_cast<double>(count);
        if (avg >= parameters.autoSwitchThreshold) {
            ForcePhase(CurriculumPhase::Arena);
            break;
        }
    }
}

double CurriculumManager::AutoSwitchThreshold() const {
    return parameters.autoSwitchThreshold;
}

}

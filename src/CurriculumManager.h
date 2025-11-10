#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace MyGL {

enum class CurriculumPhase {
    Dojo,
    Arena
};

struct CurriculumParameters {
    CurriculumPhase initialPhase = CurriculumPhase::Dojo;
    bool autoSwitch = false;
    int logInterval = 512;
    double autoSwitchThreshold = 120.0;
    std::uint64_t autoSwitchMinSamples = 256;
};

class CurriculumManager {
public:
    static CurriculumManager& Instance();

    void Configure(const CurriculumParameters& params);
    void SetPhaseChangeCallback(std::function<void(CurriculumPhase)> callback);
    CurriculumPhase CurrentPhase() const;
    bool UseScenarioCurriculum() const;
    void ObserveScenarioReward(const std::string& scenario, double reward);
    std::optional<std::unordered_map<std::string, double>> CollectScenarioAverages();
    void ForcePhase(CurriculumPhase phase);
    double AutoSwitchThreshold() const;

private:
    void TryAutoSwitch(const std::unordered_map<std::string, std::pair<double, std::uint64_t>>& averages);

    CurriculumParameters parameters;
    CurriculumPhase phase = CurriculumPhase::Dojo;
    std::function<void(CurriculumPhase)> phaseChangeCallback;
    std::unordered_map<std::string, std::pair<double, std::uint64_t>> accumulators;
    std::uint64_t logCounter = 0;
    bool autoSwitchEnabled = false;
};

}

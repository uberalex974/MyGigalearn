#include <GigaLearnCPP/Learner.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include <RLGymCPP/OBSBuilders/AdvancedObs.h>
#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/Tactical1v1Reward.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>

#include "CurriculumManager.h"
#include "PhaseAwareReward.h"
#include "ScenarioGenerator.h"

#include <cstdlib> // pour rand()

using namespace GGL;   // GigaLearn
using namespace RLGC;  // RLGymCPP
using namespace MyGL;

namespace {
    struct CurriculumConfig {
        std::string trainingPhase = "auto";
        std::string gameMode = "scenario";
        std::string rewardFunction = "skill_based";
        std::filesystem::path loadPretrainedModelPath = {};
        int numGames = 256;
        int scenarioRewardLogInterval = 512;
        bool autoSwitch = true;
        double autoSwitchThreshold = 120.0;
        int autoSwitchMinSamples = 256;
    };

    CurriculumConfig LoadCurriculumConfig(const std::filesystem::path& path) {
        CurriculumConfig config;
        if (!std::filesystem::exists(path))
            return config;

        std::ifstream file(path);
        if (!file) {
            std::cerr << "[ExampleMain] Failed to read config at " << path << "\n";
            return config;
        }

        try {
            auto json = nlohmann::json::parse(file);
            config.trainingPhase = json.value("training_phase", config.trainingPhase);
            config.gameMode = json.value("game_mode", config.gameMode);
            config.rewardFunction = json.value("reward_function", config.rewardFunction);
            config.numGames = json.value("num_games", config.numGames);
            config.scenarioRewardLogInterval = json.value("scenario_reward_log_interval", config.scenarioRewardLogInterval);
            std::string loadPath = json.value("load_pretrained_model_path", std::string{});
            if (!loadPath.empty())
                config.loadPretrainedModelPath = std::filesystem::path(loadPath);
            config.autoSwitch = json.contains("auto_switch") ? json.value("auto_switch", config.autoSwitch) : (config.trainingPhase == "auto");
            config.autoSwitchThreshold = json.value("auto_switch_threshold", config.autoSwitchThreshold);
            config.autoSwitchMinSamples = json.value("auto_switch_min_samples", config.autoSwitchMinSamples);
        } catch (std::exception& e) {
            std::cerr << "[ExampleMain] Failed to parse config: " << e.what() << "\n";
        }

        return config;
    }

    std::vector<PhaseAwareReward::Entry> BuildSkillRewards() {
        std::vector<PhaseAwareReward::Entry> entries;
        entries.emplace_back(std::make_unique<AirReward>(), 0.25f);
        entries.emplace_back(std::make_unique<FaceBallReward>(), 0.25f);
        entries.emplace_back(std::make_unique<VelocityPlayerToBallReward>(), 4.f);
        entries.emplace_back(std::make_unique<StrongTouchReward>(20, 100), 60);
        entries.emplace_back(std::make_unique<ZeroSumReward>(new VelocityBallToGoalReward(), 1), 2.0f);
        entries.emplace_back(std::make_unique<PickupBoostReward>(), 10.f);
        entries.emplace_back(std::make_unique<SaveBoostReward>(), 0.2f);
        entries.emplace_back(std::make_unique<ZeroSumReward>(new BumpReward(), 0.5f), 20);
        entries.emplace_back(std::make_unique<ZeroSumReward>(new DemoReward(), 0.5f), 80);
        entries.emplace_back(std::make_unique<GoalReward>(), 150);
        return entries;
    }

    std::vector<PhaseAwareReward::Entry> BuildTacticalRewards() {
        std::vector<PhaseAwareReward::Entry> entries;
        entries.emplace_back(std::make_unique<Tactical1v1Reward>(), 1.f);
        return entries;
    }
}

// Create the RLGymCPP environment for each of our games
EnvCreateResult EnvCreateFunc(int index) {
    auto reward = new PhaseAwareReward(
        MyGL::CurriculumManager::Instance(),
        BuildSkillRewards(),
        BuildTacticalRewards()
    );
    std::vector<WeightedReward> rewards = { { reward, 1.f } };
    std::vector<TerminalCondition*> terminalConditions = {
        new NoTouchCondition(10),
        new GoalScoreCondition()
    };

    // Make the arena
    int playersPerTeam = 1;
    auto arena = Arena::Create(GameMode::SOCCAR);
    for (int i = 0; i < playersPerTeam; i++) {
        arena->AddCar(Team::BLUE);
        arena->AddCar(Team::ORANGE);
    }

    EnvCreateResult result = {};
    result.actionParser = new DefaultAction();
    result.obsBuilder = new AdvancedObs();
    result.stateSetter = new KickoffState();
    result.terminalConditions = terminalConditions;
    result.rewards = rewards;

    result.arena = arena;

    return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {
    auto& manager = MyGL::CurriculumManager::Instance();
    if (learner->envSet) {
        auto& envState = learner->envSet->state;
        for (int arenaIdx = 0; arenaIdx < states.size(); ++arenaIdx) {
            std::string scenarioKey = states[arenaIdx].scenarioName.empty()
                ? "Default"
                : states[arenaIdx].scenarioName;

            int playerStart = envState.arenaPlayerStartIdx[arenaIdx];
            double totalReward = 0.0;
            for (int playerIdx = 0; playerIdx < states[arenaIdx].players.size(); ++playerIdx) {
                totalReward += envState.rewards[playerStart + playerIdx];
            }

            manager.ObserveScenarioReward(scenarioKey, totalReward);
        }

        if (auto averages = manager.CollectScenarioAverages()) {
            double bestAverage = std::numeric_limits<double>::lowest();
            for (const auto& entry : *averages) {
                report["Reward/" + entry.first] = entry.second;
                bestAverage = std::max(bestAverage, entry.second);
            }
            report["Curriculum/Phase"] = manager.CurrentPhase() == MyGL::CurriculumPhase::Arena;
            if (bestAverage > std::numeric_limits<double>::lowest()) {
                double threshold = manager.AutoSwitchThreshold();
                if (threshold > 0.0) {
                    double progress = std::min(bestAverage / threshold, 1.0);
                    report["Curriculum/AutoSwitchProgress"] = progress;
                    report["Curriculum/AutoSwitchTarget"] = threshold;
                }
            }
        }
    }

    // To prevent expensive metrics from eating at performance, we will only run them on 1/4th of steps
    // This doesn't really matter unless you have expensive metrics (which this example doesn't)
    bool doExpensiveMetrics = (rand() % 4) == 0;

    // Add our metrics
    for (auto& state : states) {
        if (doExpensiveMetrics) {
            for (auto& player : state.players) {
                report.AddAvg("Player/In Air Ratio", !player.isOnGround);
                report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
                report.AddAvg("Player/Demoed Ratio", player.isDemoed);

                report.AddAvg("Player/Speed", player.vel.Length());
                Vec dirToBall = (state.ball.pos - player.pos).Normalized();
                report.AddAvg("Player/Speed Towards Ball", RS_MAX(0, player.vel.Dot(dirToBall)));

                report.AddAvg("Player/Boost", player.boost);

                if (player.ballTouchedStep)
                    report.AddAvg("Player/Touch Height", state.ball.pos.z);
            }
        }

        if (state.goalScored)
            report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
    }
}

int main(int argc, char* argv[]) {
    // Initialize RocketSim with collision meshes
    RocketSim::Init("C:\\Giga\\GigaLearnCPP-Leak\\collision_meshes");

    // Make configuration for the learner
    LearnerConfig cfg = {};

#if defined(GGL_PROJECT_ROOT_SOURCE_DIR)
    std::filesystem::path projectRoot = std::filesystem::path(GGL_PROJECT_ROOT_SOURCE_DIR);
#else
    std::filesystem::path projectRoot = std::filesystem::current_path();
#endif

    auto trainingConfigPath = projectRoot / "config" / "training_config.json";
    auto curriculum = LoadCurriculumConfig(trainingConfigPath);

    cfg.numGames = curriculum.numGames;
    cfg.scenarioRewardLogInterval = curriculum.scenarioRewardLogInterval;
    cfg.loadPretrainedModelPath = curriculum.loadPretrainedModelPath;

    MyGL::CurriculumParameters params = {};
    params.initialPhase = (curriculum.trainingPhase == "arena")
        ? MyGL::CurriculumPhase::Arena
        : MyGL::CurriculumPhase::Dojo;
    params.autoSwitch = curriculum.autoSwitch;
    params.logInterval = curriculum.scenarioRewardLogInterval;
    params.autoSwitchThreshold = curriculum.autoSwitchThreshold;
    params.autoSwitchMinSamples = curriculum.autoSwitchMinSamples;

    auto& manager = MyGL::CurriculumManager::Instance();
    manager.Configure(params);
    manager.SetPhaseChangeCallback([](MyGL::CurriculumPhase phase) {
        std::cout << "[Curriculum] Switched to phase " << (phase == MyGL::CurriculumPhase::Arena ? "Arena" : "Dojo") << "\n";
    });

    bool wantCurriculum = (curriculum.trainingPhase != "arena") || (curriculum.gameMode == "scenario");
    std::shared_ptr<MyGL::ScenarioGenerator> scenarioGenerator;
    if (wantCurriculum) {
        auto scenarioCsvPath = projectRoot / "src" / "scenarios.csv";
        scenarioGenerator = std::make_shared<MyGL::ScenarioGenerator>(scenarioCsvPath);
        if (!scenarioGenerator->HasScenarios()) {
            std::cerr << "[ExampleMain] ScenarioGenerator did not find scenarios at " << scenarioCsvPath << "\n";
        }
    }

    if (scenarioGenerator) {
        cfg.scenarioProvider = [scenarioGenerator](int /*arenaIdx*/) -> std::optional<MyGL::Scenario> {
            if (!MyGL::CurriculumManager::Instance().UseScenarioCurriculum())
                return std::nullopt;
            if (!scenarioGenerator->HasScenarios())
                return std::nullopt;
            return scenarioGenerator->GenerateRandomScenario();
        };
    } else {
        cfg.scenarioProvider = nullptr;
    }

    std::cout << "[ExampleMain] Training phase: " << curriculum.trainingPhase
        << ", game mode: " << curriculum.gameMode
        << ", reward function: " << curriculum.rewardFunction << "\n";

    cfg.deviceType = LearnerDeviceType::GPU_CUDA;

    cfg.tickSkip = 8;
    cfg.actionDelay = cfg.tickSkip - 1; // Normal value in other RLGym frameworks

    // Leave this empty to use a random seed each run
    // The random seed can have a strong effect on the outcome of a run
    cfg.randomSeed = 123;

    int tsPerItr = 50'000;
    cfg.ppo.tsPerItr = tsPerItr;
    cfg.ppo.batchSize = tsPerItr;
    cfg.ppo.miniBatchSize = 50'000; // Lower this if too much VRAM is being allocated

    // Using 2 epochs seems pretty optimal when comparing time training to skill
    // Perhaps 1 or 3 is better for you, test and find out!
    cfg.ppo.epochs = 1;

    // This scales differently than "ent_coef" in other frameworks
    // This is the scale for normalized entropy, which means you won't have to change it if you add more actions
    cfg.ppo.entropyScale = 0.035f;

    // Rate of reward decay
    // Starting low tends to work out
    cfg.ppo.gaeGamma = 0.99;

    // Good learning rate to start
    cfg.ppo.policyLR = 1.5e-4;
    cfg.ppo.criticLR = 1.5e-4;

    cfg.ppo.sharedHead.layerSizes = { 256, 256 };
    cfg.ppo.policy.layerSizes = { 256, 256, 256 };
    cfg.ppo.critic.layerSizes = { 256, 256, 256 };

    auto optim = ModelOptimType::ADAM;
    cfg.ppo.policy.optimType = optim;
    cfg.ppo.critic.optimType = optim;
    cfg.ppo.sharedHead.optimType = optim;

    auto activation = ModelActivationType::RELU;
    cfg.ppo.policy.activationType = activation;
    cfg.ppo.critic.activationType = activation;
    cfg.ppo.sharedHead.activationType = activation;

    bool addLayerNorm = true;
    cfg.ppo.policy.addLayerNorm = addLayerNorm;
    cfg.ppo.critic.addLayerNorm = addLayerNorm;
    cfg.ppo.sharedHead.addLayerNorm = addLayerNorm;

    cfg.sendMetrics = true; // Send metrics
    cfg.renderMode = false; // Don't render

    // Make the learner with the environment creation function and the config we just made
    Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);

    // Start learning!
    learner->Start();

    return EXIT_SUCCESS;
}

#include "ScenarioGenerator.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace MyGL {

	ScenarioGenerator::ScenarioGenerator(const std::filesystem::path& csvPath, unsigned seed) :
		rng(seed),
		sourcePath(csvPath)
	{
		LoadFromFile(sourcePath);
	}

	bool ScenarioGenerator::HasScenarios() const noexcept {
		return !baseScenarios.empty();
	}

	Scenario ScenarioGenerator::GenerateRandomScenario() {
		if (baseScenarios.empty())
			return {};

		std::uniform_int_distribution<size_t> indexDist(0, baseScenarios.size() - 1);
		Scenario scenario = baseScenarios[indexDist(rng)];

		auto jitter = [this](float base, float variance) {
			std::uniform_real_distribution<float> dist(-variance, variance);
			return base + dist(rng);
		};

		auto& state = scenario.initial_state;
		state.ballPosX = jitter(state.ballPosX, 50.f);
		state.ballPosY = jitter(state.ballPosY, 50.f);
		state.ballPosZ = jitter(state.ballPosZ, 15.f);

		state.ballVelX += jitter(0.f, 200.f);
		state.ballVelY += jitter(0.f, 200.f);
		state.ballVelZ += jitter(0.f, 200.f);

		state.carPosX = jitter(state.carPosX, 80.f);
		state.carPosY = jitter(state.carPosY, 80.f);
		state.carPosZ = std::max(0.f, jitter(state.carPosZ, 30.f));

		state.carVelX += jitter(0.f, 150.f);
		state.carVelY += jitter(0.f, 150.f);
		state.carVelZ += jitter(0.f, 120.f);

		state.carYaw = jitter(state.carYaw, 0.25f);
		state.carPitch = jitter(state.carPitch, 0.12f);
		state.carRoll = jitter(state.carRoll, 0.12f);

		float boostWithNoise = jitter(state.carBoost, 6.f);
		state.carBoost = std::clamp(boostWithNoise, 0.f, 100.f);

		return scenario;
	}

	void ScenarioGenerator::Reload(const std::filesystem::path& csvPath) {
		sourcePath = csvPath;
		LoadFromFile(sourcePath);
	}

	void ScenarioGenerator::LoadFromFile(const std::filesystem::path& csvPath) {
		baseScenarios.clear();
		auto actualPath = ResolvePath(csvPath);
		if (actualPath.empty()) {
			std::cerr << "[ScenarioGenerator] Could not locate " << csvPath << "\n";
			return;
		}

		std::ifstream file(actualPath);
		if (!file) {
			std::cerr << "[ScenarioGenerator] Failed to open " << actualPath << "\n";
			return;
		}

		std::string line;
		std::getline(file, line); // header

		while (std::getline(file, line)) {
			if (line.empty() || line[0] == '#')
				continue;

			auto tokens = SplitCSV(line);
			if (tokens.size() < 17)
				continue;

			Scenario scenario;
			scenario.name = Trim(tokens[0]);

			auto& state = scenario.initial_state;
			state.ballPosX = ParseFloatOr(tokens[1], 0.f);
			state.ballPosY = ParseFloatOr(tokens[2], 0.f);
			state.ballPosZ = ParseFloatOr(tokens[3], 92.f);

			state.ballVelX = ParseFloatOr(tokens[4], 0.f);
			state.ballVelY = ParseFloatOr(tokens[5], 0.f);
			state.ballVelZ = ParseFloatOr(tokens[6], 0.f);

			state.carPosX = ParseFloatOr(tokens[7], 0.f);
			state.carPosY = ParseFloatOr(tokens[8], 0.f);
			state.carPosZ = ParseFloatOr(tokens[9], 17.f);

			state.carVelX = ParseFloatOr(tokens[10], 0.f);
			state.carVelY = ParseFloatOr(tokens[11], 0.f);
			state.carVelZ = ParseFloatOr(tokens[12], 0.f);

			state.carYaw = ParseFloatOr(tokens[13], 0.f);
			state.carPitch = ParseFloatOr(tokens[14], 0.f);
			state.carRoll = ParseFloatOr(tokens[15], 0.f);

			state.carBoost = std::clamp(ParseFloatOr(tokens[16], 100.f), 0.f, 100.f);

			baseScenarios.push_back(scenario);
		}

		if (baseScenarios.empty()) {
			std::cerr << "[ScenarioGenerator] No valid scenarios loaded from " << actualPath << "\n";
		}
	}

	std::string ScenarioGenerator::Trim(const std::string& token) {
		const auto notSpace = [](int ch) { return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n'; };

		size_t start = 0;
		while (start < token.size() && !notSpace(token[start]))
			++start;

		size_t end = token.size();
		while (end > start && !notSpace(token[end - 1]))
			--end;

		return token.substr(start, end - start);
	}

	std::vector<std::string> ScenarioGenerator::SplitCSV(const std::string& line) {
		std::vector<std::string> tokens;
		std::istringstream stream(line);
		std::string token;

		while (std::getline(stream, token, ',')) {
			tokens.push_back(Trim(token));
		}

		return tokens;
	}

	float ScenarioGenerator::ParseFloatOr(const std::string& token, float fallback) {
		try {
			return std::stof(token);
		} catch (...) {
			return fallback;
		}
	}

	std::filesystem::path ScenarioGenerator::ResolvePath(const std::filesystem::path& requested) const {
		if (!requested.empty() && std::filesystem::exists(requested))
			return requested;

		std::filesystem::path current = std::filesystem::current_path();
		for (int depth = 0; depth < 6; depth++) {
			std::filesystem::path candidate = current / requested;
			if (std::filesystem::exists(candidate))
				return candidate;
			if (current == current.root_path())
				break;
			current = current.parent_path();
		}

		return {};
	}
}

#pragma once

#include "Scenario.h"

#include <filesystem>
#include <iosfwd>
#include <random>
#include <string>
#include <vector>

namespace MyGL {

	class ScenarioGenerator {
	public:
		explicit ScenarioGenerator(const std::filesystem::path& csvPath = "scenarios.csv", unsigned seed = std::random_device{}());

		[[nodiscard]] bool HasScenarios() const noexcept;
		[[nodiscard]] Scenario GenerateRandomScenario();
		void Reload(const std::filesystem::path& csvPath);

	private:
		void LoadFromFile(const std::filesystem::path& csvPath);
		static std::string Trim(const std::string& token);
		static std::vector<std::string> SplitCSV(const std::string& line);
		static float ParseFloatOr(const std::string& token, float fallback);
		std::filesystem::path ResolvePath(const std::filesystem::path& requested) const;

	private:
		std::vector<Scenario> baseScenarios;
		std::mt19937 rng;
		std::filesystem::path sourcePath;
	};
}

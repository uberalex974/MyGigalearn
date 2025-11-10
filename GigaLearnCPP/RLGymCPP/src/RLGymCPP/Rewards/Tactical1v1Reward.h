#pragma once

#include "Reward.h"

namespace RLGC {

	class Tactical1v1Reward : public Reward {
	public:
		float goalReward = 120.f;
		float concedePenalty = -90.f;
		float saveBonus = 12.f;
		float proximityScale = 0.04f;

		float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			float reward = 0.f;

			if (player.eventState.goal)
				reward += goalReward;

			if (state.goalScored && !player.eventState.goal)
				reward += concedePenalty;

			if (player.eventState.save)
				reward += saveBonus;

			float distance = (player.pos - state.ball.pos).Length();
			float proximity = 1.f - RS_MIN(distance / 3600.f, 1.f);
			reward += proximityScale * proximity;

			return reward;
		}
	};
}

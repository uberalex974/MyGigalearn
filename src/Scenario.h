#pragma once

#include <string>

namespace MyGL
{
    // Représente un état simplifié du jeu pour un seul bot + la balle
    struct GameState
    {
        // Balle - position 3D
        float ballPosX{};
        float ballPosY{};
        float ballPosZ{};

        // Balle - vélocité 3D
        float ballVelX{};
        float ballVelY{};
        float ballVelZ{};

        // Voiture - position 3D
        float carPosX{};
        float carPosY{};
        float carPosZ{};

        // Voiture - vélocité 3D
        float carVelX{};
        float carVelY{};
        float carVelZ{};

        // Voiture - orientation (yaw, pitch, roll)
        float carYaw{};
        float carPitch{};
        float carRoll{};

        // Boost de la voiture
        float carBoost{};
    };

    // Définition d’un scénario d’entraînement
    struct Scenario
    {
        std::string name;
        GameState   initial_state;
    };
} // namespace MyGL

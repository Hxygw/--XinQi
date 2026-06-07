#pragma once

#include <cstdint>
#include "../XinQiCore/XinQiCore.h"

// ============================================================
// XinQi AI — Pure Monte Carlo Tree Search
// No neural network, no evaluation function.
// Uses UCB1 for tree policy, random playout for rollout.
// ============================================================

// ----- Move types -----
constexpr int8_t MOVE_PLACE = 0;
constexpr int8_t MOVE_SHIFT = 1;

// ----- Move representation -----
struct AIMove {
    int8_t type;
    int8_t x, y, z;       // Place: target; Shift: source
    int8_t tx, ty, tz;    // Shift: target; Place: unused
};

// Convenient "no move found" sentinel
constexpr AIMove AI_MOVE_NONE = { -1, -1, -1, -1, -1, -1, -1 };

// ----- MCTS configuration -----
struct AIConfig {
    int32_t simulations;    // number of playouts per decision (default 2000)
    double   explorationC;  // UCB1 constant (default 1.414 = sqrt(2))
};

constexpr AIConfig AI_CONFIG_DEFAULT = { 2000, 1.414 };

// ----- Public API -----
// Think for the current player and return the best move.
// gs is not modified.
AIMove AI_Think(const GameState* gs, const AIConfig* cfg);

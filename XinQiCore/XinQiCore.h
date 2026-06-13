#pragma once

#include <cstdint>

// ============================================================
// XinQi (芯棋) Core Engine — Public API
// N x N x N 3D board game (N >= 3)
//
// Memory layout:
//   board[0 .. N^3-1]  — current cells
//   accessed via idx = (x * N + y) * N + z
// ============================================================

// ----- Cell states -----
constexpr int8_t CELL_EMPTY           = 0;
constexpr int8_t CELL_BLACK           = 1;
constexpr int8_t CELL_WHITE           = 2;
constexpr int8_t CELL_BLACK_VACANCY   = 3;  // 黑方内芯空位
constexpr int8_t CELL_WHITE_VACANCY   = 4;  // 白方内芯空位

constexpr int8_t CELL_COUNT = 5;
constexpr int8_t COLOR_NONE  = 0;
constexpr int8_t COLOR_BLACK = 1;
constexpr int8_t COLOR_WHITE = 2;

// ----- Result codes -----
// >= 1 = win, 0 = ok, < 0 = error
constexpr int8_t RESULT_OK              = 0;
constexpr int8_t WIN_CLEAR_BOARD        = 1;  // 清台终局
constexpr int8_t WIN_CORE_INVASION      = 2;  // 内芯侵入
constexpr int8_t WIN_NO_LEGAL_MOVE      = 3;  // 轮到的一方无合法操作 → 该方获胜
constexpr int8_t ERR_OCCUPIED           = -1;
constexpr int8_t ERR_SUICIDE            = -2;
constexpr int8_t ERR_SUPERKO            = -3;
constexpr int8_t ERR_FIRST_MOVE_CENTER  = -4;
constexpr int8_t ERR_NOT_CORE           = -5;
constexpr int8_t ERR_NOT_ADJACENT       = -6;
constexpr int8_t ERR_CORE_VACANCY       = -7;

constexpr int8_t MAX_BOARD_SIZE = 13;

// ============================================================
// GameState — complete game snapshot, 40 bytes + board data
// ============================================================
struct GameState {
    int8_t  size;          // N (3 .. MAX_BOARD_SIZE)
    int8_t  current;       // current player (COLOR_BLACK / COLOR_WHITE)
    int32_t moveCount;
    uint64_t hash;         // Zobrist hash of current board
    uint64_t prevHash;     // hash from one move ago (for super-KO)
    int32_t lastCaptureCount; // stones captured by the most recent move (0 if none)
    bool    allowShift;         // false = disable Shift entirely (for simplified training)
    bool    allowInvasionWin;   // false = disable Core Invasion victory (for simplified training)
    // board[0 .. N^3-1] follows in memory (int8_t flat array)
};

// ----- Lifecycle -----
GameState* XinQi_Create(int8_t size);
void       XinQi_Destroy(GameState* gs);
GameState* XinQi_Clone(const GameState* gs);

// ----- Accessors -----
int8_t       XinQi_GetCell(const GameState* gs, int8_t x, int8_t y, int8_t z);
int32_t      XinQi_CellCount(const GameState* gs);
const int8_t* XinQi_BoardData(const GameState* gs);
int8_t*       XinQi_BoardData(GameState* gs);

// ----- Queries -----
bool XinQi_IsCore(const GameState* gs, int8_t x, int8_t y, int8_t z);
bool XinQi_HasAnyCore(const GameState* gs, int8_t color);
bool XinQi_HasAnyLegalMove(const GameState* gs);

// ----- Pre-check (no state change) -----
int8_t XinQi_CheckPlace(const GameState* gs, int8_t x, int8_t y, int8_t z);
int8_t XinQi_CheckShift(const GameState* gs,
                        int8_t fx, int8_t fy, int8_t fz,
                        int8_t tx, int8_t ty, int8_t tz);

// ----- Moves -----
int8_t XinQi_Place(GameState* gs, int8_t x, int8_t y, int8_t z);
int8_t XinQi_Shift(GameState* gs,
                   int8_t fx, int8_t fy, int8_t fz,
                   int8_t tx, int8_t ty, int8_t tz);

// ----- Runtime flags (for simplified training) -----
void XinQi_SetAllowShift(GameState* gs, bool allow);
bool XinQi_GetAllowShift(const GameState* gs);
void XinQi_SetAllowInvasionWin(GameState* gs, bool allow);
bool XinQi_GetAllowInvasionWin(const GameState* gs);

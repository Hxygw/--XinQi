#include "XinQiAI.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>

static inline int8_t opponent(int8_t color) {
    return (color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
}

// ============================================================
// 快速 PRNG (xorshift64*)
// ============================================================
static uint64_t g_rng = 0xDEADBEEFCAFEBABEULL;

static inline uint64_t xorshift64() {
    uint64_t x = g_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng = x * 0x2545F4914F6CDD1DULL;
    return g_rng;
}

static inline int32_t rand_range(int32_t lo, int32_t hi) {
    uint64_t range = (uint64_t)(hi - lo);
    return (int32_t)(lo + (int)(xorshift64() % range));
}

// ============================================================
// 快速自杀预检 — O(1) 常量
//
// 若在 X/Y/Z 三个截面中每个截面都至少有一个空格/空位邻接，
// 则落子后必有气 → 不可能自杀，跳过完整 CheckPlace。
// ============================================================
static bool quick_check_place(const GameState* gs, int8_t x, int8_t y, int8_t z) {
    int8_t N = gs->size;
    int8_t color = gs->current;

    // 先手第一步禁天元
    if (gs->moveCount == 0 && color == COLOR_BLACK) {
        int8_t lo = (N - 1) / 2, hi = N / 2;
        if (x >= lo && x <= hi && y >= lo && y <= hi && z >= lo && z <= hi)
            return false;
    }

    // 三截面快速气检查 (4-邻域在二维切片中至少一个空位)
    // X 截面 (YZ 平面)
    bool ok = false;
    for (int d = 0; d < 4; ++d) {
        int ny = y + (d == 0 ? 1 : d == 1 ? -1 : 0);
        int nz = z + (d == 2 ? 1 : d == 3 ? -1 : 0);
        if (ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
        int8_t c = XinQi_GetCell(gs, x, (int8_t)ny, (int8_t)nz);
        if (c == CELL_EMPTY || c == CELL_BLACK_VACANCY || c == CELL_WHITE_VACANCY)
        { ok = true; break; }
    }
    if (!ok) return false;

    // Y 截面 (XZ 平面)
    ok = false;
    for (int d = 0; d < 4; ++d) {
        int nx = x + (d == 0 ? 1 : d == 1 ? -1 : 0);
        int nz = z + (d == 2 ? 1 : d == 3 ? -1 : 0);
        if (nx < 0 || nx >= N || nz < 0 || nz >= N) continue;
        int8_t c = XinQi_GetCell(gs, (int8_t)nx, y, (int8_t)nz);
        if (c == CELL_EMPTY || c == CELL_BLACK_VACANCY || c == CELL_WHITE_VACANCY)
        { ok = true; break; }
    }
    if (!ok) return false;

    // Z 截面 (XY 平面)
    for (int d = 0; d < 4; ++d) {
        int nx = x + (d == 0 ? 1 : d == 1 ? -1 : 0);
        int ny = y + (d == 2 ? 1 : d == 3 ? -1 : 0);
        if (nx < 0 || nx >= N || ny < 0 || ny >= N) continue;
        int8_t c = XinQi_GetCell(gs, (int8_t)nx, (int8_t)ny, z);
        if (c == CELL_EMPTY || c == CELL_BLACK_VACANCY || c == CELL_WHITE_VACANCY)
            return true; // 三个截面都有气 → 不可能自杀
    }
    return false;
}

// ============================================================
// MCTS 节点池
// ============================================================
constexpr int32_t MAX_NODES = 200000;

struct MCTSNode {
    int32_t parent;
    int32_t firstChild;
    int32_t nextSibling;
    int32_t visits;
    int32_t wins;
    int8_t  player;
    AIMove  move;
};

static MCTSNode g_nodes[MAX_NODES];
static int32_t g_nodeCount;

static int32_t alloc_node(int32_t parent, int8_t player, AIMove move) {
    if (g_nodeCount >= MAX_NODES) return -1;
    int32_t id = g_nodeCount++;
    MCTSNode& n = g_nodes[id];
    n.parent = parent;
    n.firstChild = -1;
    n.nextSibling = -1;
    n.visits = 0;
    n.wins = 0;
    n.player = player;
    n.move = move;
    return id;
}

// ============================================================
// 着法生成（优化版）
// ============================================================

// 使用 quick_check 作预过滤，避免大部分完整 CheckPlace 调用
static int gen_place_moves(const GameState* gs, AIMove* out, int maxOut) {
    int8_t N = gs->size;
    int32_t total = N * N * N;
    const int8_t* board = XinQi_BoardData(gs);
    int8_t color = gs->current;
    int count = 0;

    for (int32_t idx = 0; idx < total && count < maxOut; ++idx) {
        int8_t cell = board[idx];
        bool canOccupy = (cell == CELL_EMPTY) ||
            (color == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
            (color == COLOR_WHITE && cell == CELL_BLACK_VACANCY);
        if (!canOccupy) continue;

        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);

        // 快速预检
        if (!quick_check_place(gs, x, y, z)) {
            // 快速预检不过 → 走完整检查
            int8_t r = XinQi_CheckPlace(gs, x, y, z);
            if (r != RESULT_OK) continue;
        }

        out[count].type = MOVE_PLACE;
        out[count].x = x; out[count].y = y; out[count].z = z;
        out[count].tx = out[count].ty = out[count].tz = -1;
        ++count;
    }
    return count;
}

static int gen_shift_moves(const GameState* gs, AIMove* out, int maxOut) {
    int8_t N = gs->size;
    int32_t total = N * N * N;
    const int8_t* board = XinQi_BoardData(gs);
    int8_t color = gs->current;
    int count = 0;

    int dx6[6] = { 1,-1, 0, 0, 0, 0 };
    int dy6[6] = { 0, 0, 1,-1, 0, 0 };
    int dz6[6] = { 0, 0, 0, 0, 1,-1 };

    for (int32_t idx = 0; idx < total && count < maxOut; ++idx) {
        if (board[idx] != color) continue;
        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);
        if (!XinQi_IsCore(gs, x, y, z)) continue;

        for (int d = 0; d < 6; ++d) {
            int8_t nx = (int8_t)(x + dx6[d]);
            int8_t ny = (int8_t)(y + dy6[d]);
            int8_t nz = (int8_t)(z + dz6[d]);

            if (XinQi_GetCell(gs, nx, ny, nz) != CELL_EMPTY) continue;

            int8_t r = XinQi_CheckShift(gs, x, y, z, nx, ny, nz);
            if (r == RESULT_OK) {
                out[count].type  = MOVE_SHIFT;
                out[count].x = x; out[count].y = y; out[count].z = z;
                out[count].tx = nx; out[count].ty = ny; out[count].tz = nz;
                ++count;
            }
        }
    }
    return count;
}

// ============================================================
// 快速随机走一步（不生成全部合法着法）
// ============================================================
static bool fast_random_play_step(GameState* gs) {
    int8_t N = gs->size;
    int32_t total = N * N * N;
    int8_t color = gs->current;

    int dx6[6] = { 1,-1, 0, 0, 0, 0 };
    int dy6[6] = { 0, 0, 1,-1, 0, 0 };
    int dz6[6] = { 0, 0, 0, 0, 1,-1 };

    // Phase 1: 随机尝试落子 (最多 80 次试探)
    for (int t = 0; t < 80; ++t) {
        int32_t idx = rand_range(0, total);
        int8_t cell = XinQi_GetCell(gs, (int8_t)(idx/(N*N)), (int8_t)((idx/N)%N), (int8_t)(idx%N));
        bool canOccupy = (cell == CELL_EMPTY) ||
            (color == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
            (color == COLOR_WHITE && cell == CELL_BLACK_VACANCY);
        if (!canOccupy) continue;

        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);

        // 快速预检
        if (!quick_check_place(gs, x, y, z)) continue;

        int8_t r = XinQi_Place(gs, x, y, z);
        if (r >= 0) return r == RESULT_OK;
        // r < 0 → 非法，棋盘不变，继续试
    }

    // Phase 2: 随机尝试挪子
    int32_t startIdx = rand_range(0, total);
    for (int t = 0; t < total; ++t) {
        int32_t idx = (startIdx + t) % total;
        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);
        if (XinQi_GetCell(gs, x, y, z) != color) continue;
        if (!XinQi_IsCore(gs, x, y, z)) continue;

        int d = rand_range(0, 6);
        int8_t nx = (int8_t)(x + dx6[d]);
        int8_t ny = (int8_t)(y + dy6[d]);
        int8_t nz = (int8_t)(z + dz6[d]);
        if (XinQi_GetCell(gs, nx, ny, nz) != CELL_EMPTY) continue;

        // 快速预检：核心检查是挪子是否合法
        // 先用 CheckShift 检查
        if (XinQi_CheckShift(gs, x, y, z, nx, ny, nz) != RESULT_OK) continue;

        int8_t r = XinQi_Shift(gs, x, y, z, nx, ny, nz);
        if (r >= 0) return r == RESULT_OK;
    }

    return false;
}

// ============================================================
// 展开节点
// ============================================================
static void expand_node(int32_t nodeId, GameState* gs) {
    MCTSNode& node = g_nodes[nodeId];
    if (node.firstChild != -1) return;

    AIMove moves[512];
    int nPlace = gen_place_moves(gs, moves, 256);
    int nShift = gen_shift_moves(gs, moves + nPlace, 256);
    int total = nPlace + nShift;
    if (total == 0) return;

    for (int i = 0; i < total; ++i) {
        int8_t nextPlayer = opponent(gs->current);
        int32_t childId = alloc_node(nodeId, nextPlayer, moves[i]);
        if (childId == -1) break;
        g_nodes[childId].nextSibling = node.firstChild;
        node.firstChild = childId;
    }
}

// ============================================================
// UCB1
// ============================================================
static int32_t ucb1_select(int32_t nodeId) {
    MCTSNode& parent = g_nodes[nodeId];
    double parentLog = log((double)parent.visits);
    double C = 1.414;

    int32_t bestId = -1;
    double bestVal = -1e308;

    for (int32_t childId = parent.firstChild; childId != -1; childId = g_nodes[childId].nextSibling) {
        MCTSNode& child = g_nodes[childId];
        if (child.visits == 0) return childId;
        double val = (double)child.wins / child.visits + C * sqrt(parentLog / child.visits);
        if (val > bestVal) { bestVal = val; bestId = childId; }
    }
    return bestId;
}

// ============================================================
// 执行着法
// ============================================================
static int apply_move(GameState* gs, const AIMove& m) {
    if (m.type == MOVE_PLACE)
        return XinQi_Place(gs, m.x, m.y, m.z);
    else
        return XinQi_Shift(gs, m.x, m.y, m.z, m.tx, m.ty, m.tz);
}

// ============================================================
// 公共 API：MCTS 主循环
// ============================================================

AIMove AI_Think(const GameState* gs, const AIConfig* cfg) {
    if (!gs || !cfg) return AI_MOVE_NONE;
    if (!XinQi_HasAnyLegalMove(gs)) return AI_MOVE_NONE;

    GameState* rootState = XinQi_Clone(gs);
    if (!rootState) return AI_MOVE_NONE;

    g_nodeCount = 0;
    int32_t rootId = alloc_node(-1, gs->current, AI_MOVE_NONE);
    if (rootId == -1) { XinQi_Destroy(rootState); return AI_MOVE_NONE; }

    for (int32_t sim = 0; sim < cfg->simulations; ++sim) {
        GameState* walk = XinQi_Clone(rootState);
        if (!walk) break;

        int32_t nodeId = rootId;

        // --- Selection ---
        while (true) {
            MCTSNode& node = g_nodes[nodeId];
            if (node.firstChild == -1) break;
            int32_t nextId = ucb1_select(nodeId);
            if (nextId == -1) break;
            int8_t r = apply_move(walk, g_nodes[nextId].move);
            if (r < 0) break;
            if (r > 0) { nodeId = nextId; break; }
            nodeId = nextId;
        }

        // --- Expansion ---
        if (g_nodes[nodeId].firstChild == -1) {
            bool terminal = !XinQi_HasAnyLegalMove(walk);
            if (!terminal) {
                expand_node(nodeId, walk);
                if (g_nodes[nodeId].firstChild != -1) {
                    int32_t fc = g_nodes[nodeId].firstChild;
                    if (apply_move(walk, g_nodes[fc].move) >= 0)
                        nodeId = fc;
                }
            }
        }

        // --- Rollout (快速随机仿真) ---
        while (true) {
            if (!XinQi_HasAnyLegalMove(walk)) break;
            if (!fast_random_play_step(walk)) break;
        }

        // --- Backpropagation ---
        int8_t winner = opponent(walk->current);
        int32_t backId = nodeId;
        while (backId != -1) {
            MCTSNode& n = g_nodes[backId];
            n.visits++;
            if (winner == n.player) n.wins++;
            backId = n.parent;
        }

        XinQi_Destroy(walk);
    }

    int32_t bestChild = -1, bestVisits = -1;
    for (int32_t childId = g_nodes[rootId].firstChild; childId != -1; childId = g_nodes[childId].nextSibling) {
        if (g_nodes[childId].visits > bestVisits) {
            bestVisits = g_nodes[childId].visits;
            bestChild = childId;
        }
    }

    AIMove result = AI_MOVE_NONE;
    if (bestChild != -1) result = g_nodes[bestChild].move;

    XinQi_Destroy(rootState);
    return result;
}

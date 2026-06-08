#define XINQITRAIN_EXPORTS
#include "train_api.h"
#include "../XinQiCore/XinQiCore.h"
#include "../XinQiAI/XinQiAI.h"
#include <cstring>
#include <cmath>

// ============================================================
// 生命周期 — 直接透传 XinQiCore
// ============================================================

void* Train_Create(int boardSize) {
    return XinQi_Create((int8_t)boardSize);
}

void Train_Destroy(void* gs) {
    XinQi_Destroy((GameState*)gs);
}

void* Train_Clone(void* gs) {
    return XinQi_Clone((const GameState*)gs);
}

// ============================================================
// 棋盘查询
// ============================================================

int Train_GetBoardSize(void* gs) {
    return ((GameState*)gs)->size;
}

int Train_GetCurrentPlayer(void* gs) {
    return ((GameState*)gs)->current;
}

int Train_GetMoveCount(void* gs) {
    return ((GameState*)gs)->moveCount;
}

// 终局判定：当前玩家无合法着法即结束
int Train_IsTerminal(void* gs) {
    return !XinQi_HasAnyLegalMove((const GameState*)gs);
}

// 胜者推断：无合法操作时 current 方获胜（无棋可走 = 对方被彻底封锁）
int Train_GetWinner(void* gs) {
    GameState* g = (GameState*)gs;
    if (!XinQi_HasAnyLegalMove((const GameState*)gs)) {
        return g->current;  // 无棋可走的一方获胜
    }
    return 0;
}

int Train_GetBoard(void* gs, int8_t* out, int maxLen) {
    const GameState* g = (const GameState*)gs;
    int32_t total = XinQi_CellCount(g);
    if (maxLen < total) return -1;
    memcpy(out, XinQi_BoardData(g), total);
    return total;
}

// ============================================================
// 着法
// ============================================================

int Train_Place(void* gs, int x, int y, int z) {
    return XinQi_Place((GameState*)gs, (int8_t)x, (int8_t)y, (int8_t)z);
}

int Train_Shift(void* gs, int fx, int fy, int fz, int tx, int ty, int tz) {
    return XinQi_Shift((GameState*)gs, (int8_t)fx, (int8_t)fy, (int8_t)fz,
                       (int8_t)tx, (int8_t)ty, (int8_t)tz);
}

// ============================================================
// 合法着法枚举
// ============================================================

int Train_GetLegalPlaces(void* gs, int32_t* out, int maxOut) {
    int8_t N = ((GameState*)gs)->size;
    int total = N * N * N;
    const int8_t* board = XinQi_BoardData((const GameState*)gs);
    int8_t color = ((GameState*)gs)->current;
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

        int8_t r = XinQi_CheckPlace((const GameState*)gs, x, y, z);
        if (r == RESULT_OK || r > 0) {
            out[count++] = idx;
        }
    }
    return count;
}

// Shift 着法编码: code = (fx*N^2 + fy*N + fz) * 6 + dir
// 解码: src = code / 6, dir = code % 6
int Train_GetLegalShifts(void* gs, int32_t* out, int maxOut) {
    int8_t N = ((GameState*)gs)->size;
    int total = N * N * N;
    const int8_t* board = XinQi_BoardData((const GameState*)gs);
    int8_t color = ((GameState*)gs)->current;
    int count = 0;

    int dx[6] = {1,-1,0,0,0,0};
    int dy[6] = {0,0,1,-1,0,0};
    int dz[6] = {0,0,0,0,1,-1};

    for (int32_t idx = 0; idx < total && count < maxOut; ++idx) {
        if (board[idx] != color) continue;
        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);
        if (!XinQi_IsCore((const GameState*)gs, x, y, z)) continue;

        for (int d = 0; d < 6; ++d) {
            int8_t nx = (int8_t)(x + dx[d]);
            int8_t ny = (int8_t)(y + dy[d]);
            int8_t nz = (int8_t)(z + dz[d]);

            if (XinQi_GetCell((const GameState*)gs, nx, ny, nz) != CELL_EMPTY) continue;
            int8_t r = XinQi_CheckShift((const GameState*)gs, x, y, z, nx, ny, nz);
            if (r == RESULT_OK || r > 0) {
                out[count++] = idx * 6 + d;
            }
        }
    }
    return count;
}

int Train_HasLegalMove(void* gs) {
    return XinQi_HasAnyLegalMove((const GameState*)gs) ? 1 : 0;
}

// ============================================================
// MCTS 策略（仅 Place 着法）
//
// 这是一个极简版 MCTS，专为训练数据生成设计：
//   - 只有一层树（根 → 合法 Place 着法）
//   - 均匀先验（没有策略网络）
//   - Rollout 用随机采样
//   - 返回访问次数归一化后的策略分布
//
// 替换为带网络先验的 AlphaZero 风格 MCTS 后，
// 只需将 uniform prior 换成网络输出的 P(s,a)。 ⚡
// ============================================================

int Train_MCTS_Policy(void* gs, float* policyOut, int policyLen, int simulations) {
    const GameState* g = (const GameState*)gs;
    int8_t N = g->size;
    int total = N * N * N;
    if (policyLen < total) return -1;

    memset(policyOut, 0, total * sizeof(float));
    if (!XinQi_HasAnyLegalMove(g)) return 0; // 终局

    // 收集合法 Place 着法
    int32_t placeMoves[512];
    int nPlace = Train_GetLegalPlaces(gs, placeMoves, 512);
    if (nPlace == 0) return 0;

    int* visitCounts = new int[total]();
    if (!visitCounts) return -1;

    // 子节点统计（uniform prior，等待网络替换）
    struct MCTSCtx { int visits, wins; };
    MCTSCtx* children = new MCTSCtx[nPlace]();

    double C = 1.414;                         // UCB1 探索常数
    const int MAX_SIMS = simulations < 100 ? 100 : simulations;

    for (int sim = 0; sim < MAX_SIMS; ++sim) {
        // ── Selection：UCB1 ──
        int bestIdx = -1;
        double bestVal = -1e308;
        for (int i = 0; i < nPlace; ++i) {
            if (children[i].visits == 0) { bestIdx = i; break; }
            double ucb = (double)children[i].wins / children[i].visits
                       + C * sqrt(log((double)(sim + 1)) / children[i].visits);
            if (ucb > bestVal) { bestVal = ucb; bestIdx = i; }
        }
        if (bestIdx < 0) break;

        // ── Expansion + Rollout ──
        GameState* simState = XinQi_Clone(g);
        if (!simState) break;

        int32_t idx = placeMoves[bestIdx];
        int8_t x = (int8_t)(idx / (N * N));
        int8_t y = (int8_t)((idx / N) % N);
        int8_t z = (int8_t)(idx % N);
        int8_t r = XinQi_Place(simState, x, y, z);

        int8_t winner = -1;
        if (r > 0 || !XinQi_HasAnyLegalMove(simState)) {
            // 落子即终局
            winner = (r > 0) ? simState->current
                             : ((simState->current == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK);
        } else {
            // Rollout：随机走到底
            int stepLimit = 200;
            int8_t sn = simState->size;
            int32_t stotal = sn * sn * sn;
            while (stepLimit-- > 0 && winner == -1) {
                if (!XinQi_HasAnyLegalMove(simState)) {
                    winner = (simState->current == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
                    break;
                }
                // 优先随机采样（~80 次试探命中率 >95%）
                int8_t scolor = simState->current;
                bool found = false;
                for (int t = 0; t < 80; ++t) {
                    int32_t ridx = (int32_t)(((uint64_t)rand() * stotal) / RAND_MAX);
                    if (ridx >= stotal) ridx = 0;
                    int8_t cell = XinQi_GetCell(simState,
                        (int8_t)(ridx / (sn * sn)),
                        (int8_t)((ridx / sn) % sn),
                        (int8_t)(ridx % sn));
                    bool can = (cell == CELL_EMPTY) ||
                        (scolor == COLOR_BLACK && cell == CELL_WHITE_VACANCY) ||
                        (scolor == COLOR_WHITE && cell == CELL_BLACK_VACANCY);
                    if (!can) continue;
                    int8_t sx = (int8_t)(ridx / (sn * sn));
                    int8_t sy = (int8_t)((ridx / sn) % sn);
                    int8_t sz = (int8_t)(ridx % sn);
                    int8_t rr = XinQi_Place(simState, sx, sy, sz);
                    if (rr >= 0) {
                        found = true;
                        if (rr > 0) { winner = simState->current; }
                        break;
                    }
                }
                if (found) continue;
                // 全扫描兜底（极少发生，仅当棋盘快满时）
                int32_t fallback[256];
                int nf = 0;
                for (int32_t ri = 0; ri < stotal && nf < 256; ++ri) {
                    int8_t rcell = XinQi_GetCell(simState,
                        (int8_t)(ri / (sn * sn)),
                        (int8_t)((ri / sn) % sn),
                        (int8_t)(ri % sn));
                    bool can = (rcell == CELL_EMPTY) ||
                        (scolor == COLOR_BLACK && rcell == CELL_WHITE_VACANCY) ||
                        (scolor == COLOR_WHITE && rcell == CELL_BLACK_VACANCY);
                    if (!can) continue;
                    if (XinQi_CheckPlace(simState,
                            (int8_t)(ri / (sn * sn)),
                            (int8_t)((ri / sn) % sn),
                            (int8_t)(ri % sn)) == RESULT_OK)
                        fallback[nf++] = ri;
                }
                if (nf == 0) {
                    if (!XinQi_HasAnyLegalMove(simState))
                        winner = (simState->current == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
                    break;
                }
                int pick = (int)(((uint64_t)rand() * nf) / RAND_MAX);
                if (pick >= nf) pick = nf - 1;
                int8_t rr = XinQi_Place(simState,
                    (int8_t)(fallback[pick] / (sn * sn)),
                    (int8_t)((fallback[pick] / sn) % sn),
                    (int8_t)(fallback[pick] % sn));
                if (rr > 0) winner = simState->current;
            }
        }

        // ── Backprop ──
        if (winner != -1) {
            bool movePlayerWon = (winner == ((GameState*)gs)->current);
            children[bestIdx].visits++;
            if (movePlayerWon) children[bestIdx].wins++;
            visitCounts[idx]++;
        }
        XinQi_Destroy(simState);
    }

    // ── 策略输出：访问次数 / 总访问数 ──
    float totalVisits = 0;
    for (int i = 0; i < total; ++i) totalVisits += (float)visitCounts[i];
    if (totalVisits > 0) {
        for (int i = 0; i < total; ++i)
            policyOut[i] = (float)visitCounts[i] / totalVisits;
    } else {
        float uniform = 1.0f / nPlace;
        for (int i = 0; i < nPlace; ++i)
            policyOut[placeMoves[i]] = uniform;
    }

    delete[] visitCounts;
    delete[] children;
    return 1;
}

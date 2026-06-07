#pragma once
#include <cstdint>

#ifdef XINQITRAIN_EXPORTS
#define TRAIN_API __declspec(dllexport)
#else
#define TRAIN_API __declspec(dllimport)
#endif

// ────────────────────────────────────────────────────────────────
// XinQi Training API — C 接口 DLL
//
// 设计目标:
//   让 Python (通过 ctypes) 无痛调用芯棋引擎做强化学习训练。
//   所有函数接收 void* GameState，不暴露 C++ 类型。
//   全部 /MT 编译，零运行时依赖（不需要 VC++ Redistributable）。
//
// 使用流程:
//   Create → (Place/Shift + GetBoard + MCTS_Policy) 循环 → Destroy
//
// Move 索引约定 (Place):
//   flat_idx = x * N^2 + y * N + z
//   策略网络输出层大小 = 棋盘总格数 = N^3
//
// Shift 的编码:
//   当前简化版 MCTS 只返回 Place 策略。
//   Shift 着法可通过 GetLegalShifts + 自行实现 MCTS 支持。
// ────────────────────────────────────────────────────────────────

extern "C" {

// ── 生命周期 ──
// 注意: GameState 内部包含 Zobrist 表（约 88KB），
//       Create 首次调用会初始化该表（一次性的 ~1ms）。
TRAIN_API void* Train_Create(int boardSize);
TRAIN_API void  Train_Destroy(void* gs);
TRAIN_API void* Train_Clone(void* gs);

// ── 棋盘查询 ──
TRAIN_API int Train_GetBoardSize(void* gs);
TRAIN_API int Train_GetCurrentPlayer(void* gs);  // 1=Black, 2=White
TRAIN_API int Train_GetMoveCount(void* gs);
TRAIN_API int Train_IsTerminal(void* gs);         // 当前玩家无合法着法
TRAIN_API int Train_GetWinner(void* gs);          // 0=none, 1=Black, 2=White

// 扁平棋盘数据 (int8_t 数组，长度 size^3)
// 行列: idx = x*N^2 + y*N + z
// 值:   0=空  1=黑  2=白  3=黑方内芯空位  4=白方内芯空位
TRAIN_API int Train_GetBoard(void* gs, int8_t* out, int maxLen);

// ── 着法 ──
TRAIN_API int Train_Place(void* gs, int x, int y, int z);  // 返回结果码
TRAIN_API int Train_Shift(void* gs, int fx, int fy, int fz,
                          int tx, int ty, int tz);

// ── 合法着法 ──
// Place 着法: 填充 flat_idx 数组 (x*N^2 + y*N + z)
TRAIN_API int Train_GetLegalPlaces(void* gs, int32_t* out, int maxOut);
// Shift 着法: 填充 packed code
//   code = (fx*N^2 + fy*N + fz) * 6 + dir
//   dir: 0=+x 1=-x 2=+y 3=-y 4=+z 5=-z
TRAIN_API int Train_GetLegalShifts(void* gs, int32_t* out, int maxOut);
TRAIN_API int Train_HasLegalMove(void* gs);  // 1=有, 0=无

// ── MCTS 策略 ──
// 对当前局面运行简化 MCTS，返回 Place 策略分布 (float[], 长度 N^3)。
// 策略只覆盖 Place 着法，不包含 Shift。
// simulations: 每步模拟次数，800 适合快速迭代，5000 适合高质量数据。
// 返回值: 1=成功, 0=终局, -1=缓冲区不足
//
// 性能参考 (5x5x5):
//   200 sims → ~0.1s
//   800 sims → ~0.5s
//   5000 sims → ~3s
TRAIN_API int Train_MCTS_Policy(void* gs, float* policyOut,
                                int policyLen, int simulations);

}

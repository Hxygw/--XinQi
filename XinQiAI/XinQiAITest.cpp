// ============================================================
// XinQiAI Test — 自对弈演示
// 编译（VS 2022 x64 环境）:
//   cl /EHsc /std:c++20 /utf-8 /I..\XinQiCore /I..\XinQiAI
//       XinQiAITest.cpp ..\XinQiCore\x64\Debug\XinQiCore.lib
//       ..\XinQiAI\x64\Debug\XinQiAI.lib
// ============================================================

#include "XinQiCore.h"
#include "XinQiAI.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

// 简易计时
#include <chrono>

static void print_board(const GameState* gs) {
    int8_t n = gs->size;
    printf("\n  棋盘 (N=%d, 第%d手, %s方)\n", n, gs->moveCount,
           gs->current == COLOR_BLACK ? "黑" : "白");

    for (int8_t z = 0; z < n; ++z) {
        printf("\n  Z=%d:", z);
        for (int8_t y = 0; y < n; ++y) {
            printf("\n  Y=%d: ", y);
            for (int8_t x = 0; x < n; ++x) {
                int8_t c = XinQi_GetCell(gs, x, y, z);
                char ch = '.';
                if (c == CELL_BLACK) ch = 'B';
                else if (c == CELL_WHITE) ch = 'W';
                else if (c == CELL_BLACK_VACANCY) ch = 'b';
                else if (c == CELL_WHITE_VACANCY) ch = 'w';
                printf("%c ", ch);
            }
        }
        printf("\n");
    }
    fflush(stdout);
}

static void print_move(const AIMove& m) {
    if (m.type == MOVE_PLACE) {
        printf("落子 (%d,%d,%d)", m.x, m.y, m.z);
    } else if (m.type == MOVE_SHIFT) {
        printf("挪子 (%d,%d,%d)->(%d,%d,%d)", m.x, m.y, m.z, m.tx, m.ty, m.tz);
    } else {
        printf("无着法");
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    printf("============================================\n");
    printf("XinQi AI — 纯 MCTS 自对弈演示\n");
    printf("============================================\n");

    // 棋盘大小和模拟次数
    int8_t boardSize = 5;   // 5×5×5
    int simCount = 800;      // 每手模拟次数

    printf("棋盘: %d×%d×%d  |  模拟: %d/手\n", boardSize, boardSize, boardSize, simCount);

    GameState* gs = XinQi_Create(boardSize);
    if (!gs) { printf("创建棋盘失败\n"); return 1; }

    AIConfig cfg;
    cfg.simulations = simCount;
    cfg.explorationC = 1.414;

    int moveLimit = 50; // 防止无限循环
    int turn = 0;

    using namespace std::chrono;

    while (turn < moveLimit) {
        printf("\n--- 第 %d 手 ---\n", turn + 1);
        print_board(gs);

        if (!XinQi_HasAnyLegalMove(gs)) {
            printf("当前玩家无合法着法！对方获胜。\n");
            break;
        }

        auto t0 = high_resolution_clock::now();
        AIMove move = AI_Think(gs, &cfg);
        auto t1 = high_resolution_clock::now();
        double ms = duration<double, std::milli>(t1 - t0).count();

        if (move.type == -1) {
            printf("AI 找不到着法。游戏结束。\n");
            break;
        }

        printf("AI (%s方) 选择: ", gs->current == COLOR_BLACK ? "黑" : "白");
        print_move(move);
        printf("  [%.0fms]\n", ms);

        int8_t result;
        if (move.type == MOVE_PLACE) {
            result = XinQi_Place(gs, move.x, move.y, move.z);
        } else {
            result = XinQi_Shift(gs, move.x, move.y, move.z, move.tx, move.ty, move.tz);
        }

        if (result > 0) {
            print_board(gs);
            const char* reason = "";
            if (result == WIN_CLEAR_BOARD) reason = "清台终局";
            else if (result == WIN_CORE_INVASION) reason = "内芯侵入";
            else if (result == WIN_NO_LEGAL_MOVE) reason = "对方无合法着法";
            printf("\n*** %s方 获胜！(%s) ***\n",
                   (gs->current == COLOR_BLACK) ? "白" : "黑", reason);
            break;
        }

        if (result < 0) {
            printf("*** 非法着法 (code=%d)！终止 ***\n", result);
            break;
        }

        turn++;
    }

    if (turn >= moveLimit) {
        printf("\n达到步数上限，平局。\n");
    }

    int blackScore = 0, whiteScore = 0;
    const int8_t* board = XinQi_BoardData(gs);
    int32_t total = XinQi_CellCount(gs);
    for (int32_t i = 0; i < total; ++i) {
        if (board[i] == CELL_BLACK) blackScore++;
        else if (board[i] == CELL_WHITE) whiteScore++;
    }
    printf("终局: 黑 %d 子, 白 %d 子\n", blackScore, whiteScore);

    XinQi_Destroy(gs);
    return 0;
}

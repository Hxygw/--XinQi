"""芯棋自对弈数据生成 — 为 AlphaZero 风格训练提供训练数据

用法:
    python selfplay.py                          # 100 盘 (默认)
    python selfplay.py --games 1000 --board 5   # 5x5x5, 1000 盘
    python selfplay.py --games 100 --sims 500   # 每手 500 次模拟

输出格式 (每盘一个 .npz 文件):
    data/selfplay_{game_id:06d}.npz

    key         shape       dtype    说明
    board       (T, N^3)    int8     每一步的原始棋盘 (0-4整数值)
    policy      (T, N^3)    float32  MCTS 策略分布 (访问次数归一化)
    result      (T,)        float32  终局值: 1.0=当前玩家赢, -1.0=输
    winner      scalar      int      全局胜者: 0=无, 1=黑, 2=白
    board_size  scalar      int      棋盘大小 (N)

    T = 本盘手数 (每盘不同), N = board_size

训练时加载所有 .npz 文件即可。典型做法:
    for file in data/*.npz:
        d = np.load(file)
        board = d["board"]     # 做 one-hot 编码后送网络
        policy = d["policy"]   # 监督损失: cross_entropy(policy_net, policy)
        result = d["result"]   # 价值损失: mse(value_net, result)
"""

import numpy as np
import os, sys, time, argparse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from xinqi_env import XinQiEnv


def play_one_game(board_size: int, mcts_sims: int) -> dict:
    """自对弈一盘，返回训练数据"""
    env = XinQiEnv(board_size)

    boards = []
    policies = []

    while not env.terminal and env.move_count < 500:
        # 记录当前局面
        boards.append(env.get_board().copy())

        # MCTS 策略（C++ 内执行，约 0.1-0.5s）
        pi = env.mcts_policy(mcts_sims)  # (N^3,) float32
        policies.append(pi)

        # 按策略采样走棋
        legal = env.get_legal_mask()
        probs = pi.copy()
        probs[~legal] = 0.0
        if probs.sum() == 0:
            break
        probs /= probs.sum()

        # 温度策略: 前 10 步鼓励探索，之后逐步确定化
        temp = 1.0 if env.move_count < 10 else 0.3
        noisy = np.power(probs, 1.0 / temp)
        noisy /= noisy.sum()
        action = np.random.choice(len(probs), p=noisy)

        r = env.step(action)
        # 温度采样偶尔会走到自杀点，重试即可
        if r < 0:
            legal_idx = np.where(legal)[0]
            if len(legal_idx) == 0:
                break
            action = np.random.choice(legal_idx)
            r = env.step(action)

    # 标注终局值
    T = len(boards)
    result = np.zeros(T, dtype=np.float32)
    if env.winner != 0:  # 有胜者
        for i in range(T):
            player_at_turn = 1 if i % 2 == 0 else 2  # 黑先
            result[i] = 1.0 if env.winner == player_at_turn else -1.0

    return {
        "board": np.stack(boards, axis=0),      # (T, N^3)
        "policy": np.stack(policies, axis=0),    # (T, N^3)
        "result": result,                        # (T,)
        "winner": env.winner,
        "moves": T,
    }


def main():
    parser = argparse.ArgumentParser(description="芯棋自对弈数据生成")
    parser.add_argument("--games", type=int, default=100, help="对弈盘数")
    parser.add_argument("--board", type=int, default=5, help="棋盘大小")
    parser.add_argument("--sims", type=int, default=800, help="MCTS 模拟次数")
    parser.add_argument("--out", type=str, default="data", help="输出目录")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    total_time = 0.0
    total_moves = 0

    for game_id in range(1, args.games + 1):
        t0 = time.time()
        data = play_one_game(args.board, args.sims)
        dt = time.time() - t0

        # 保存 (NPZ 自动压缩)
        np.savez_compressed(
            out_dir / f"selfplay_{game_id:06d}.npz",
            board=data["board"],
            policy=data["policy"],
            result=data["result"],
            winner=data["winner"],
            board_size=args.board,
        )

        total_time += dt
        total_moves += data["moves"]

        if game_id == 1 or game_id % 10 == 0:
            eta = (args.games - game_id) * (total_time / game_id)
            print(f"  [{game_id:5d}/{args.games}]  "
                  f"{data['moves']:3d}手  "
                  f"{dt:5.1f}s  "
                  f"均 {total_moves/game_id:.1f}手/盘  "
                  f"预计剩余 {eta:.0f}s", flush=True)

    print(f"\n完成: {args.games} 盘, 共 {total_moves} 手, "
          f"平均 {total_moves/args.games:.1f} 手/盘, "
          f"耗时 {total_time:.0f}s")


if __name__ == "__main__":
    main()

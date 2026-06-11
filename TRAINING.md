# XinQi AI Training Guide

> This document explains how to train machine learning models for XinQi (芯棋).
> The training interface is designed to be framework-agnostic — you can use PyTorch,
> TensorFlow, JAX, or any other framework of your choice.

---

## 中文

### 🧠 为什么在芯棋上训练 AI？

芯棋是一个**在三维格点上展开的原创吃子棋类**。它的规则不属于任何已知游戏的变体——这意味着在它上面训练的每一个模型，都是在探索一个**从未被 AI 征服过的博弈空间**。

三维空间中的 MCTS 遇到了传统二维棋类不会出现的稀疏信号问题；挪子机制引入了多模态动作空间的表征挑战；双重胜利路径迫使价值网络同时评估两种不同的终局条件。每一个环节都有开放的研究问题等着你去回答。

而且，5×5×5 棋盘（125 格，~100 分支因子）用 CPU 就能开始训练。你最需要的是一个好的想法，而不是一块 H100。

**芯棋等待它的第一个 AI。下面的文档帮你上路。**

### 概述

芯棋内置了一套 C++ → Python 的训练桥接。游戏引擎（`XinQiCore`）和 MCTS 搜索（`XinQiAI`）编译为
`XinQiTrain.dll`，通过 Python ctypes 暴露为 `XinQiEnv` 类。

**当前状态：训练管线骨架已就绪，AI 本身等待社区贡献。**

```
训练数据流：
  棋盘状态 → 编码 → 网络推理 → 策略/价值 → MCTS → 自对弈 → .npz 文件 → 训练循环
    ↑                                                                     ↓
    └────────────── 加载模型权重进行下一轮自对弈 ←──────────────────────────┘
```

### 环境准备

```bash
# 需要 Python 3.8+
pip install numpy

# 可选（训练神经网络时使用）
pip install torch  # 或 tensorflow, jax 等
```

`XinQiTrain.dll` 已预编译（Release，/MT 静态链接，零运行时依赖），位于 `XinQiTrain/` 目录下。

### Python API 参考

#### 创建环境

```python
from xinqi_env import XinQiEnv

env = XinQiEnv(board_size=5)  # 默认 5×5×5，支持 3–13
```

#### 棋盘数据

```python
# 原始棋盘（int8 数组，shape (N³,)）
#   0 = 空, 1 = 黑子, 2 = 白子, 3 = 黑方内芯空位, 4 = 白方内芯空位
board = env.get_board()

# CNN 编码（float32，shape (3, N, N, N)，channel-first）
#   通道 0: 己方棋子
#   通道 1: 对方棋子
#   通道 2: 己方内芯空位
encoded = env.get_board_encoded()
```

**设计说明：** 当前编码仅包含 Place 动作所需的信息。Shift 动作需要额外的特征通道
（如己方内芯位置编码），可以通过调用 `XinQi_IsCore` 为每个格子计算。
你完全可以设计自己的编码方案——`get_board()` 返回原始数据，`encoded` 只是示例。

#### 合法动作

```python
# Place 动作的合法索引（x*N² + y*N + z）
legal_places = env.get_legal_places()

# Place 动作的布尔掩码（shape (N³,)）
legal_mask = env.get_legal_mask()
```

Place 动作输出为 N³ 维的策略向量。

Shift 动作不包含在当前简化 MCTS 策略输出中。你可以通过以下方式添加：

```python
# 获取合法 Shift 动作（packed code）
#   code = src_flat_idx * 6 + dir
#   其中 dir: 0=+x, 1=-x, 2=+y, 3=-y, 4=+z, 5=-z
legal_shifts = env.get_legal_shifts()
```

#### 执行动作

```python
# Place: 传入平坦索引
result = env.step(flat_idx)

# Shift: 传入源位置索引和方向
result = env.step_shift(src_idx, dir_idx)

# 返回码
#   >= 1: 胜利（1=清台, 2=内芯侵入, 3=无合法操作）
#     0: 成功，游戏继续
#    < 0: 非法（-1=已有子, -2=自杀, -3=超级劫, -4=禁天元,
#                -5=非内芯, -6=不邻接, -7=己方空位）
```

#### MCTS 策略

```python
# 运行 MCTS，返回 Place 策略分布（float32，shape (N³,)）
policy = env.mcts_policy(simulations=800)

# 仅合法位置的访问次数被归一化到 [0,1]，非法位置为 0
```

当前 MCTS 使用均匀先验 + 随机 Rollout（纯随机仿真）。
替换为网络先验后，将 `uniform prior` 换成网络输出的 `P(s,a)` 即可。

#### 自对弈

`selfplay.py` 提供了一个完整的数据生成示例：

```bash
python selfplay.py --games 100 --board 5 --sims 800
```

每盘棋输出一个 `.npz` 文件：

| key | shape | dtype | 说明 |
|-----|-------|-------|------|
| `board` | (T, N³) | int8 | 每步原始棋盘 |
| `policy` | (T, N³) | float32 | MCTS 策略（访问次数归一化） |
| `result` | (T,) | float32 | 终局值（当前玩家视角：+1=胜, -1=负） |
| `winner` | scalar | int | 全局胜者（0=无, 1=黑, 2=白） |
| `board_size` | scalar | int | 棋盘大小 N |

#### 克隆环境

```python
clone = env.clone()  # 深拷贝，用于推演
```

### 训练建议（非约束性）

以下仅为方向性建议，你可以选择任何你觉得合适的训练方法：

- **小网络起步**：5×5×5 棋盘状态空间小，3 层轻量 Conv3d（< 10 万参数）足够
- **数据生成**：当前 MCTS 使用纯随机 Rollout，数据质量有限。改进 Rollout 启发式
  （如优先下在棋子附近）可以显著提升数据价值
- **策略表示**：Place 输出是 N³ 维；Shift 输出需要额外设计动作空间编码
- **价值目标**：`result` 字段提供终局值，可直接作为价值网络训练目标
- **迭代训练**：先用改进后的 MCTS 生成数据 → 训练小网络 → 用网络替换 MCTS 先验
  → 生成更高质量数据 → 循环
- **硬件要求**：5×5×5 在 CPU 上即可完成基础训练；GPU 主要用于加速卷积网络

### 已知限制

- `XinQiTrain.dll` 仅限 Windows（DLL 依赖）。Linux/Mac 需要将 `XinQiCore` 编译为 `.so`/`.dylib`
- 当前 MCTS 只输出 Place 策略，不包含 Shift。需要自行扩展 Shift 动作空间
- 自对弈脚本是单线程的。大规模数据生成需要自行实现并行

---

## English

### 🧠 Why Train on XinQi?

XinQi is an **original capture-based board game on a 3D lattice**. It is not a variant of any known game — every model trained on it explores a **game space never conquered by AI before**.

MCTS in 3D faces signal sparsity issues unseen in traditional 2D games. The Shift mechanism introduces multi-modal action space challenges. Dual win conditions force the value network to evaluate two fundamentally different endgame scenarios. Every component is an open research question.

And at 5×5×5 (125 cells, ~100 branching factor), you can start training on CPU. What you need most is a good idea, not an H100.

**XinQi is waiting for its first real AI. The docs below will get you started.**

### Overview

XinQi ships with a C++ → Python training bridge. The game engine (`XinQiCore`) and MCTS
search (`XinQiAI`) are compiled into `XinQiTrain.dll`, exposed to Python via ctypes
as the `XinQiEnv` class.

**Current state: The training pipeline skeleton is ready. The real AI is waiting for
the community to build it.**

### Setup

```bash
# Requires Python 3.8+
pip install numpy

# Optional (for neural network training)
pip install torch  # or tensorflow, jax, etc.
```

A pre-built `XinQiTrain.dll` (Release, /MT static link, zero runtime dependencies)
is located in the `XinQiTrain/` directory.

### Python API Reference

#### Creating the Environment

```python
from xinqi_env import XinQiEnv

env = XinQiEnv(board_size=5)  # default 5×5×5, supports 3–13
```

#### Board Data

```python
# Raw board (int8 array, shape (N³,))
#   0 = Empty, 1 = Black, 2 = White, 3 = Black vacancy, 4 = White vacancy
board = env.get_board()

# CNN encoding (float32, shape (3, N, N, N), channel-first)
#   Channel 0: own stones
#   Channel 1: opponent stones
#   Channel 2: own core vacancies
encoded = env.get_board_encoded()
```

**Note:** The built-in encoding covers only Place actions. For Shift actions, you may
need additional feature channels (e.g., inner core positions). You can compute these
by calling `XinQi_IsCore` per cell. You're also free to design your own encoding —
`get_board()` returns raw data, and the `encoded` version is just a convenience
starting point.

#### Legal Moves

```python
# Flat indices of legal Place moves (x*N² + y*N + z)
legal_places = env.get_legal_places()

# Boolean mask for Place moves (shape (N³,))
legal_mask = env.get_legal_mask()
```

Place policy output dimension is N³.

Shift moves are not included in the simplified MCTS policy output.
You can access them via:

```python
# Legal Shift moves (packed code)
#   code = src_flat_idx * 6 + dir
#   where dir: 0=+x, 1=-x, 2=+y, 3=-y, 4=+z, 5=-z
legal_shifts = env.get_legal_shifts()
```

#### Executing Moves

```python
# Place: pass a flat index
result = env.step(flat_idx)

# Shift: pass source index and direction
result = env.step_shift(src_idx, dir_idx)

# Return codes
#   >= 1: win (1=clear board, 2=core invasion, 3=no legal moves)
#     0: OK, game continues
#    < 0: illegal (-1=occupied, -2=suicide, -3=super-KO, -4=center ban,
#                  -5=not core, -6=not adjacent, -7=own vacancy)
```

#### MCTS Policy

```python
# Run MCTS, return Place policy distribution (float32, shape (N³,))
policy = env.mcts_policy(simulations=800)

# Visit counts are normalized to [0,1] over legal moves only.
```

The current MCTS uses uniform priors + random rollouts.
To replace it with a network-guided search, swap the uniform prior with the
network's `P(s,a)` output.

#### Self-Play

`selfplay.py` provides a complete data generation example:

```bash
python selfplay.py --games 100 --board 5 --sims 800
```

Each game produces a `.npz` file:

| key | shape | dtype | Description |
|-----|-------|-------|-------------|
| `board` | (T, N³) | int8 | Raw board at each step |
| `policy` | (T, N³) | float32 | MCTS policy (visit counts normalized) |
| `result` | (T,) | float32 | Terminal value (+1=win, -1=loss, from current player's perspective) |
| `winner` | scalar | int | Global winner (0=none, 1=Black, 2=White) |
| `board_size` | scalar | int | Board size N |

#### Cloning

```python
clone = env.clone()  # Deep copy for look-ahead
```

### Training Suggestions (Non-prescriptive)

These are directional suggestions, not constraints. Feel free to choose any approach:

- **Start small**: 5×5×5 has a small state space. A light 3-layer Conv3d (< 100K params)
  is sufficient to begin
- **Data quality**: The current MCTS uses purely random rollouts. Improving the rollout
  heuristic (e.g., preferring moves near existing stones) significantly boosts data quality
- **Policy representation**: Place outputs are N³-dimensional. Shift outputs require
  additional action space encoding
- **Value target**: The `result` field provides terminal values, usable directly as
  value network training targets
- **Iterative training**: Generate data with improved MCTS → train a small network →
  replace MCTS priors with network output → generate better data → repeat
- **Hardware**: 5×5×5 is trainable on CPU for basic experiments; GPU accelerates
  convolutional network training

### Known Limitations

- `XinQiTrain.dll` is Windows-only (DLL dependency). Linux/Mac users need to compile
  `XinQiCore` as `.so`/`.dylib`
- The current MCTS outputs Place policy only (no Shift). Extend the action space
  encoding for Shift moves
- The self-play script is single-threaded. Large-scale data generation requires
  parallelization

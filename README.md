<p align="center">
  <img src="开发文档/成品图/芯棋%20XinQi%20暗色系-中文-首页.png" alt="芯棋 XinQi" width="720">
</p>

<h1 align="center">芯棋 · XinQi</h1>

<p align="center">
  <strong>3D 立体围棋 — N×N×N 格点棋盘，三截面独立提子，内芯挪子机制</strong>
  <br>
  <em>3D Go — N×N×N lattice board, 3-section independent capture, core-shift mechanic</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C" alt="C++20">
  <img src="https://img.shields.io/badge/Svelte-5-orange" alt="Svelte 5">
  <img src="https://img.shields.io/badge/Three.js-r174-green" alt="Three.js">
  <img src="https://img.shields.io/badge/platform-Windows-lightgrey" alt="Platform">
</p>

<br>

---

## 🇨🇳 中文

### 什么是芯棋？

芯棋是一种在 **N×N×N 三维格点棋盘** 上进行的双人零和棋类游戏。它从围棋中汲取灵感，但拥有完全独立的规则体系——**三截面独立提子** 和 **内芯挪子** 机制创造了前所未有的攻防深度。

- 🎲 **N×N×N 棋盘** (默认 5×5×5，125 个格点)
- 🧊 **三截面提子** — 在 X/Y/Z 三个二维截面中各算各的气，任一截面无气即被提走
- ⚡ **内芯挪子** — 被完全包围的"内芯"棋子可被挪到相邻位置，原址留下永久"伤疤"（内芯空位）
- 🏆 **两种胜利方式** — 清台终局（对手无内芯）或 内芯侵入（占据对手内芯空位）
- 🌐 **中英双语界面** — 一键切换
- 🌗 **亮暗双主题**

> 完整规则：[中文](开发文档/设计/游戏规则.md) | [English](开发文档/设计/game-rules.md)

### 截图

| 首页 | 对局 | 内芯 | 剖面 |
|------|------|------|------|
| ![首页](开发文档/成品图/芯棋%20XinQi%20暗色系-中文-首页.png) | ![对局](开发文档/成品图/芯棋%20XinQi%20暗色系-中文-对局.png) | ![内芯](开发文档/成品图/芯棋%20XinQi%20暗色系-中文-内芯.png) | ![剖面](开发文档/成品图/芯棋%20XinQi%20暗色系-中文-剖面.png) |

| 亮色主题 | 英文界面 |
|----------|----------|
| ![亮色](开发文档/成品图/芯棋%20XinQi%20亮色系-中文-对局.png) | ![英文](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-对局.png) |

### 快速开始

**方式一：下载预编译包（推荐）**

从 [Releases](https://github.com/Hxygw/--XinQi/releases) 下载最新版 `XinQiServer-v0.1.0.zip`，解压后双击 `XinQiServer.exe`，浏览器打开 `http://localhost:8090`。

**方式二：从源码构建**

需要 Visual Studio 2022 (v145) + Node.js 18+。

```bash
# 1. 编译 C++ 后端
cd "D:\Program Files\VisualStudio\MSBuild\Current\Bin"
MSBuild "path\to\XinQiServer\XinQiServer.vcxproj" /p:Configuration=Release /p:Platform=x64

# 2. 构建前端
cd xinqi-frontend
npm install
npm run build
# 将 dist/ 内容复制到 XinQiServer/dist/

# 3. 运行
XinQiServer\x64\Release\XinQiServer.exe
# 浏览器打开 http://localhost:8090
```

### 项目结构

```
XinQi/
├── XinQiCore/          # 核心引擎 (C++20, 静态库)
│   ├── XinQiCore.h     # 纯 C 风格 API
│   └── XinQiCore.cpp   # 棋盘、落子、挪子、提子、内芯判定、Zobrist 哈希
├── XinQiAI/            # AI 引擎 (纯 MCTS, 静态库)
│   ├── XinQiAI.h
│   └── XinQiAI.cpp     # UCB1 + 随机仿真
├── XinQiServer/        # HTTP 服务器 (捆绑引擎+AI+前端)
│   ├── src/main.cpp
│   └── dist/           # 预构建的前端文件
├── XinQiTrain/         # 训练模块 (DLL + Python 绑定)
│   ├── train_api.h/cpp # C 接口导出
│   ├── xinqi_env.py    # Python ctypes 封装
│   ├── selfplay.py     # 自对弈数据生成
│   └── XinQiTrain.dll  # 预编译 DLL
└── xinqi-frontend/     # 前端 (Svelte 5 + Three.js)
    └── src/
        ├── App.svelte
        ├── components/  # Board3D, GameInfo, Modal
        └── lib/         # api, legality, boardUtils, types, i18n, sound
```

### 技术栈

| 模块 | 技术 |
|------|------|
| 核心引擎 | C++20, Zobrist Hashing |
| AI | 纯 MCTS, UCB1, 随机仿真 |
| HTTP 服务器 | C++20, [httplib](https://github.com/yhirose/cpp-httplib) (header-only) |
| 前端 | Svelte 5 + Three.js + TypeScript + Vite 6 |
| 训练桥接 | C++ DLL → Python ctypes → NumPy → PyTorch |
| 分发 | 单 exe + 静态前端文件，零外部依赖 |

### AI 训练（面向贡献者）

芯棋内置了一套完整的 AI 训练基础设施，让你可以在 5×5×5 棋盘上用 AlphaZero 风格方法训练神经网络。

**当前状态：训练管线骨架已搭建好，等待社区贡献真正的 AI。**

```
Python 侧可用功能：
├── XinQiEnv(board_size)     ← 创建棋盘环境
├── env.get_board()          ← 获取棋盘数据 (int8 array)
├── env.get_board_encoded()  ← 3 通道 CNN 编码 (己方/对方/空位)
├── env.mcts_policy(sims)    ← MCTS 策略分布 (float32)
├── env.step(action_idx)     ← 执行落子
├── env.step_shift(src, dir) ← 执行挪子
└── selfplay.py              ← 自对弈 → .npz 训练数据
```

**你可以做的贡献方向：**

| 难度 | 方向 | 工作量 |
|------|------|--------|
| ★☆☆ | 改进 MCTS Rollout 启发式 | ~50 行 C++ |
| ★★☆ | 手工局面评估函数 | ~150 行 C++ |
| ★★☆ | 小网络 + MCTS (AlphaZero 轻量版) | 3-5 天 |
| ★★★★ | 完整 AlphaZero | 2-4 周 |

**关键设计：** 当前 MCTS 使用纯随机仿真（可以认为它是"纯笨蛋"）。这意味着生成的训练数据质量有限，只能作为验证训练管线的基线。真正的 AI 需要从改进 Rollout 启发式，或直接替换为网络指导的 MCTS 开始。

> 训练数据格式为标准 `.npz`，详情见 `XinQiTrain/selfplay.py` 文件头注释。

---

## 🇬🇧 English

### What is XinQi?

XinQi (芯棋, "Core Chess") is a two-player zero-sum board game played on an **N×N×N 3D lattice board**. Inspired by Go, it features an entirely independent rule system with **3-section independent capture** and **core-shift mechanics** that create unprecedented strategic depth.

- 🎲 **N×N×N board** (default 5×5×5, 125 cells)
- 🧊 **3-section capture** — liberties are evaluated independently in X/Y/Z 2D slices; a group is captured if any slice has zero liberties
- ⚡ **Core Shift** — fully surrounded "inner core" stones can be shifted to adjacent positions, leaving permanent "scars" (core vacancies)
- 🏆 **Two win conditions** — Clear Board (opponent has no cores) or Core Invasion (occupy opponent's core vacancy)
- 🌐 **Bilingual UI** — Chinese/English toggle
- 🌗 **Dark/Light themes**

> Full rules: [中文](开发文档/设计/游戏规则.md) | [English](开发文档/设计/game-rules.md)

### Screenshots

| Home | Gameplay | Core | Cross-section |
|------|----------|------|---------------|
| ![Home](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-首页.png) | ![Game](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-对局.png) | ![Core](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-内芯.png) | ![Section](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-剖面.png) |

| Light Theme | 
|-------------|
| ![Light](开发文档/成品图/芯棋%20XinQi%20亮色系-英文-对局.png) |

### Quick Start

**Option 1: Download pre-built binary**

Download `XinQiServer-v0.1.0.zip` from [Releases](https://github.com/Hxygw/--XinQi/releases), unzip, double-click `XinQiServer.exe`, and open `http://localhost:8090` in your browser.

**Option 2: Build from source**

Requires Visual Studio 2022 (v145) + Node.js 18+.

```bash
# 1. Build C++ backend
cd "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin"
MSBuild "path\to\XinQiServer\XinQiServer.vcxproj" /p:Configuration=Release /p:Platform=x64

# 2. Build frontend
cd xinqi-frontend
npm install
npm run build
# Copy dist/ to XinQiServer/dist/

# 3. Run
XinQiServer\x64\Release\XinQiServer.exe
# Open http://localhost:8090
```

### Project Structure

```
XinQi/
├── XinQiCore/          # Core engine (C++20, static library)
├── XinQiAI/            # AI engine (pure MCTS, static library)
├── XinQiServer/        # HTTP server (bundles engine+AI+frontend)
├── XinQiTrain/         # Training bridge (DLL + Python bindings)
└── xinqi-frontend/     # Frontend (Svelte 5 + Three.js)
```

### Tech Stack

| Module | Technology |
|--------|-----------|
| Core Engine | C++20, Zobrist Hashing |
| AI | Pure MCTS, UCB1, Random Rollout |
| HTTP Server | C++20, [httplib](https://github.com/yhirose/cpp-httplib) (header-only) |
| Frontend | Svelte 5 + Three.js + TypeScript + Vite 6 |
| Training Bridge | C++ DLL → Python ctypes → NumPy → PyTorch |
| Distribution | Single exe + static frontend files, zero external deps |

### AI Training (for Contributors)

XinQi ships with a complete AI training scaffolding, enabling AlphaZero-style training on a 5×5×5 board.

**Current state: The training pipeline skeleton is ready. The real AI is waiting for the community.**

```
Python API overview:
├── XinQiEnv(board_size)         ← Create board environment
├── env.get_board()              ← Get raw board (int8 array)
├── env.get_board_encoded()      ← 3-channel CNN encoding
├── env.mcts_policy(sims)        ← MCTS policy distribution
├── env.step(action_idx)         ← Execute place
├── env.step_shift(src, dir)     ← Execute shift
└── selfplay.py                  ← Self-play → .npz training data
```

**Contribution ideas:**

| Difficulty | Direction | Effort |
|-----------|-----------|--------|
| ★☆☆ | Improve MCTS rollout heuristics | ~50 lines C++ |
| ★★☆ | Handcrafted evaluation function | ~150 lines C++ |
| ★★☆ | Small network + MCTS (light AlphaZero) | 3-5 days |
| ★★★★ | Full AlphaZero stack | 2-4 weeks |

**Design note:** The current MCTS uses purely random rollouts, meaning the generated training data is only suitable as a baseline for debugging the training pipeline. A real AI requires either improved rollout heuristics or a network-guided MCTS.

> Training data format is standard `.npz` — see `XinQiTrain/selfplay.py` header comments for details.

---

## License

[MIT](LICENSE) © 2026 XinQi Contributors

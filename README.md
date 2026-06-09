<p align="center">
  <img src="开发文档/成品图/芯棋%20XinQi%20亮色系-中文-首页.png" alt="芯棋 XinQi" width="720">
</p>

<h1 align="center">芯棋 · XinQi</h1>

<p align="center">
  <strong>全新三维棋类 — N×N×N 格点棋盘，三截面独立提子 + 内芯挪子</strong>
  <br>
  <em>一个崭新的棋类宇宙，不是变体，是原创。</em>
</p>

<p align="center">
  <code>规则简单 · 深度无穷</code>
  <br>
  <em>五分钟学会，一辈子下不完</em>
</p>

<p align="center">
  <table align="center">
    <tr>
      <td align="center"><b>🖥️ XinQiServer</b></td>
      <td align="center"><b>🌐 XinQiRoomServer</b></td>
    </tr>
    <tr>
      <td align="center">本地对弈 · AI 对战 · 棋谱回放<br>双击运行，浏览器即玩</td>
      <td align="center">多人在线 · 4 位房间码 · 好友秒入<br>手机电脑均可 · 局域网或 ngrok</td>
    </tr>
  </table>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C" alt="C++20">
  <img src="https://img.shields.io/badge/Svelte-5-orange" alt="Svelte 5">
  <img src="https://img.shields.io/badge/Three.js-r174-green" alt="Three.js">
  <img src="https://img.shields.io/badge/AI-MCTS-brightgreen" alt="AI">
  <img src="https://img.shields.io/badge/multiplayer-ready-%2344cc11" alt="Multiplayer">
  <img src="https://img.shields.io/badge/mobile-friendly-%2344cc11" alt="Mobile Friendly">
  <img src="https://img.shields.io/badge/platform-Windows-lightgrey" alt="Platform">
</p>

<br>

---

## 🇨🇳 中文

### 芯棋是什么？

芯棋不是围棋的三维变体——它是一套**从零设计的原创棋类规则**，在 **N×N×N 三维立体格点**上展开。灵感源于围棋的"气"的概念，但演化出了完全不同的攻防体系和胜利路径。

**一句话概括：在三维棋盘上，你的棋子可以被包围、被提走，也可以"破壳而出"。**

#### 四大特色

|  |  |  |
|---|---|---|
| 🎲 **三维棋盘** | 🤏 **规则两分钟** | 🧠 **AI 就绪** | 🌐 **在线对战** |
| 默认 5×5×5 立 方格点，N=3~13 可调 | 落子 + 挪子，两种操作，一种执着 | 内置 MCTS + Python 训练管线，社区驱动 | 房间服务器 + 4 位码入局，瓜熟蒂落 |
| 125 个格点构筑立体战场 | 无长考，上手即战 | 从 Rollout 启发式到 AlphaZero 皆可 | 桌面手机均可，同局域网或 ngrok 分享 |

#### 更多亮点

- 🧊 **三截面提子** — X/Y/Z 截面各自算气，任一截面无气即提，攻防思维迥异于平面棋类
- ⚡ **内芯挪子** — 被六面合围的"内芯"可以破壳移位，原址变成永久"伤疤"
- 🏆 **双重胜利** — 内芯侵入（踏入对手伤疤）或清台终局（对手无内芯），两种截然不同的战略路径
- 🌐 **中英双语** — 一键切换界面语言
- 🌗 **亮暗主题** — 护眼暗色 + 优雅亮色，随光而变

---

### 快速规则

棋盘是 **N×N×N 立体格点**（默认 5×5×5 = 125 格）。黑白轮流落子，黑先。

**每手两种操作（二选一）：**

| 操作 | 说明 |
|------|------|
| **落子** 🪨 | 在空格放己方棋子。不能自杀、不能立即恢复上一步局面、第一步禁止天元 |
| **挪子** 🌀 | 将一个**内芯**移到相邻格，原位变成永久"伤疤"（内芯空位）。挪子消耗一手棋 |

**内芯**：六个方向（±X/±Y/±Z）全部被己方或棋盘壁包围的棋子，是最核心的战略资源。

**提子**：落子/挪子后，在 X/Y/Z 三个截面中**各自独立**检查气。任一截面内对方连通块无气 → 整个连通块被提走。

**胜利条件（满足任一即赢）：**
1. **内芯侵入** 🏆 — 你占据对手的内芯空位（伤疤），立即获胜（主要胜利路径）
2. **清台终局** — 你触发吃子后，对手棋盘上不存在任何内芯，立即获胜
3. **无棋可走** — 轮到某方时该方无任何合法操作，该方直接获胜

> 完整规则：[中文](开发文档/设计/游戏规则.md) · [English](开发文档/设计/game-rules.md)

---

### 截图

| 首页（亮色） | 对局（亮色） | 内芯（亮色） | 剖面（亮色） |
|------|------|------|------|
| ![首页](开发文档/成品图/芯棋%20XinQi%20亮色系-中文-首页.png) | ![对局](开发文档/成品图/芯棋%20XinQi%20亮色系-中文-对局.png) | ![内芯](开发文档/成品图/芯棋%20XinQi%20亮色系-中文-内芯.png) | ![剖面](开发文档/成品图/芯棋%20XinQi%20亮色系-中文-剖面.png) |

| 暗色系 | 英文界面 |
|----------|----------|
| ![暗色](开发文档/成品图/芯棋%20XinQi%20暗色系-中文-对局.png) | ![英文](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-对局.png) |

---

### 快速开始

**方式一：下载预编译包（推荐）**

从 [Releases](https://github.com/Hxygw/--XinQi/releases) 下载最新版 `XinQiServer-v0.1.0.zip`，解压后双击 `XinQiServer.exe`，浏览器打开 `http://localhost:8090`。

> 内置两盘示例棋谱（**clearboard-20**、**invasion-76**），打开游戏后点击 **浏览棋谱** 即可观看回放。

**方式二：从源码构建**

需要 Visual Studio 2022 (v145) + Node.js 18+。

> ⚠️ MSBuild 可能在系统 PATH 中，也可能在 VS 安装目录下（如 `D:\Program Files\VisualStudio\MSBuild\Current\Bin\MSBuild.exe`）。如果找不到 `MSBuild` 命令，请使用完整路径。

```bash
# 1. 编译 C++ 后端
MSBuild XinQiServer\XinQiServer.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. 构建前端
cd xinqi-frontend
npm install
npm run build

# 3. 运行
XinQiServer\x64\Release\XinQiServer.exe
# 浏览器打开 http://localhost:8090
```

**方式三：联网对战**

芯棋附带独立房间服务器，支持多人在线对战。4 位房间号，好友秒入。

```bash
# 1. 编译房间服务器
MSBuild XinQiRoomServer\XinQiRoomServer.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. 构建房间前端
cd xinqi-room-frontend
npm install
npm run build

# 3. 运行
XinQiRoomServer\x64\Release\XinQiRoomServer.exe
# 浏览器打开 http://localhost:8090

# 4. 分享给朋友（同局域网或 ngrok）
ngrok http 8090
```

> 房主创建房间 → 4 位房间号 → 朋友输入房间号加入 → 准备就绪 → 对弈开始。
>
> 📱 **手机浏览器同样支持**：触屏操作首次点击预览合法性，再次同一格确认落子。同局域网或 ngrok 均可直连。

---

### 项目结构

```
XinQi/
├── XinQiCore/              # 核心引擎 (C++20, 静态库)
│   ├── XinQiCore.h         # 纯 C 风格 API
│   └── XinQiCore.cpp       # 棋盘、落子、挪子、提子、内芯判定、Zobrist 哈希
├── XinQiAI/                # AI 引擎 (纯 MCTS, 静态库)
│   ├── XinQiAI.h
│   └── XinQiAI.cpp         # UCB1 + 随机仿真
├── XinQiServer/            # HTTP 服务器 (捆绑引擎+AI+前端)
│   └── src/main.cpp
├── XinQiRoomServer/        # 房间服务器 (多人在线对战)
│   └── src/main.cpp
├── XinQiTrain/             # 训练模块 (DLL + Python 绑定)
│   ├── train_api.h/cpp     # C 接口导出
│   ├── xinqi_env.py        # Python ctypes 封装
│   ├── selfplay.py         # 自对弈 → .npz 训练数据
│   └── XinQiTrain.dll      # 预编译 DLL (Release, /MT)
├── xinqi-frontend/         # 主前端 (Svelte 5 + Three.js)
│   └── src/
└── xinqi-room-frontend/    # 房间前端 (Svelte 5 + Three.js，含房间+本地对战)
    └── src/
```

---

### 技术栈

| 模块 | 技术 |
|------|------|
| 核心引擎 | C++20, Zobrist Hashing |
| AI | 纯 MCTS (UCB1, 随机仿真) |
| HTTP 服务器 | C++20, [httplib](https://github.com/yhirose/cpp-httplib) (header-only) |
| 前端 | Svelte 5 + Three.js + TypeScript + Vite 6 |
| 训练桥接 | C++ DLL → Python ctypes → NumPy → PyTorch |
| 分发 | 单 exe + 静态前端文件，零外部依赖 |
| 联机 | 独立房间服务器，4 位房间码入局 |

---

### AI 训练（面向贡献者）

芯棋内置了一套完整的 AI 训练基础设施，让你可以在 5×5×5 棋盘上用 AlphaZero 风格方法训练神经网络。

**当前状态：训练管线骨架已搭建好，等待社区贡献真正的 AI。**

> 详细的 API 文档和训练指南见 [`TRAINING.md`](TRAINING.md)。

```python
# Python 侧一句话上手
from xinqi_env import XinQiEnv
env = XinQiEnv(board_size=5)
policy = env.mcts_policy(simulations=800)  # MCTS 策略分布
env.step(action_idx)                        # 执行落子
```

**你可以做的贡献方向：**

| 难度 | 方向 | 工作量 |
|------|------|--------|
| ★☆☆ | 改进 MCTS Rollout 启发式 | ~50 行 C++ |
| ★★☆ | 手工局面评估函数 | ~150 行 C++ |
| ★★☆ | 小网络 + MCTS (AlphaZero 轻量版) | 3‑5 天 |
| ★★★★ | 完整 AlphaZero | 2‑4 周 |

> 训练数据格式为标准 `.npz`，详情见 `XinQiTrain/selfplay.py` 文件头注释。

---

## 🇬🇧 English

### What is XinQi?

XinQi (芯棋, "Core Chess") is a **brand-new board game** played on an **N×N×N 3D lattice board**. It is not a Go variant — it's an original ruleset inspired by the concept of "liberties" from Go, evolved into a completely different strategic system with its own attack defense dynamics and win conditions.

**In one sentence: place stones in 3D space, capture by cutting off air in any dimension, and let your inner cores break free.**

<p align="center">
  <table align="center">
    <tr>
      <td align="center"><b>🖥️ XinQiServer</b></td>
      <td align="center"><b>🌐 XinQiRoomServer</b></td>
    </tr>
    <tr>
      <td align="center">Local PvP · AI play · Record replay<br>Double-click to run, play in browser</td>
      <td align="center">Online multiplayer · 4-digit room code<br>Desktop & mobile · LAN or ngrok</td>
    </tr>
  </table>
</p>

#### At a Glance

|  |  |  |
|---|---|---|
| 🎲 **3D Board** | 🤏 **Two-Minute Rules** | 🧠 **AI-Ready** | 🌐 **Online Multiplayer** |
| Default 5×5×5 lattice, N=3~13 adjustable | Two actions: Place & Shift. Simple to learn. | Built-in MCTS + Python training pipeline | Room server with 4-digit code, easy PvP |
| 125 cells in a cube | Deep strategy unfolds naturally | From rollout heuristics to full AlphaZero | Desktop & mobile, LAN or ngrok |

#### Highlights

- 🧊 **3-Section Capture** — Liberties evaluated independently in X/Y/Z slices. A group dies if ANY slice has zero liberties. Unprecedented tactical complexity.
- ⚡ **Core Shift** — Fully surrounded "inner core" stones break out by shifting to adjacent positions, leaving permanent scars.
- 🏆 **Dual Win Conditions** — Core Invasion (step into opponent's scar) or Clear Board (opponent has no cores left). Two entirely different strategic paths.
- 🌐 **Bilingual UI** — Chinese/English toggle.
- 🌗 **Dark/Light Themes** — Easy on the eyes, day or night.

### Quick Rules

The board is an **N×N×N 3D lattice** (default 5×5×5 = 125 cells). Black and White alternate, Black goes first.

**Two actions per turn (pick one):**

| Action | Description |
|--------|-------------|
| **Place** 🪨 | Put your stone on an empty cell. No suicide, no super-KO, first move cannot be center. |
| **Shift** 🌀 | Move an **inner core** to an adjacent cell, leaving a permanent scar (core vacancy). Costs one turn. |

**Inner Core**: A stone with all six directions (±X/±Y/±Z) occupied by friendly stones or the board boundary.

**Capture**: After each Place/Shift, liberties are checked **independently** in X, Y, and Z sections. If any section has zero liberties for an opponent's connected group, the entire group is removed.

**Win conditions (first to achieve wins):**
1. **Core Invasion** 🏆 — Occupy the opponent's core vacancy (scar). Game over.
2. **Clear Board** — After triggering a capture, opponent has zero inner cores remaining. Game over.
3. **No Legal Moves** — If a player has no legal Place or Shift on their turn, they win immediately.

> Full rules: [中文](开发文档/设计/游戏规则.md) · [English](开发文档/设计/game-rules.md)

### Screenshots

| Home (Light) | Gameplay (Light) | Core (Light) | Cross-section (Light) |
|------|----------|------|---------------|
| ![Home](开发文档/成品图/芯棋%20XinQi%20亮色系-英文-首页.png) | ![Game](开发文档/成品图/芯棋%20XinQi%20亮色系-英文-对局.png) | ![Core](开发文档/成品图/芯棋%20XinQi%20亮色系-英文-内芯.png) | ![Section](开发文档/成品图/芯棋%20XinQi%20亮色系-英文-剖面.png) |

| Dark Theme |
|-------------|
| ![Dark](开发文档/成品图/芯棋%20XinQi%20暗色系-英文-对局.png) |

### Quick Start

**Option 1: Download pre-built binary**

Download `XinQiServer-v0.1.0.zip` from [Releases](https://github.com/Hxygw/--XinQi/releases), unzip, double-click `XinQiServer.exe`, and open `http://localhost:8090` in your browser.

> Two sample games are included (**clearboard-20**, **invasion-76**). Click **Browse Records** after launching to watch replays.

**Option 2: Build from source**

Requires Visual Studio 2022 (v145) + Node.js 18+.

> ⚠️ MSBuild may not be in your system PATH. If you can't find it, use the full path (e.g. `D:\Program Files\VisualStudio\MSBuild\Current\Bin\MSBuild.exe`).

```bash
# 1. Build C++ backend
MSBuild XinQiServer\XinQiServer.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. Build frontend
cd xinqi-frontend
npm install
npm run build

# 3. Run
XinQiServer\x64\Release\XinQiServer.exe
# Open http://localhost:8090
```

**Option 3: Online Multiplayer**

XinQi includes a separate room server for online matches. 4-digit room code, invite a friend instantly.

```bash
# 1. Build room server
MSBuild XinQiRoomServer\XinQiRoomServer.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. Build room frontend
cd xinqi-room-frontend
npm install
npm run build

# 3. Run
XinQiRoomServer\x64\Release\XinQiRoomServer.exe
# Open http://localhost:8090

# 4. Share with friends (LAN or ngrok)
ngrok http 8090
```

> Host creates a room → 4-digit code → friend enters code to join → both ready → game starts.
>
> 📱 **Mobile friendly**: First tap previews legality, second tap confirms. Works on LAN or via ngrok.

### Project Structure

```
XinQi/
├── XinQiCore/              # Core engine (C++20, static library)
├── XinQiAI/                # AI engine (pure MCTS, static library)
├── XinQiServer/            # HTTP server (bundles engine+AI+frontend)
├── XinQiRoomServer/        # Room server (multiplayer online)
├── XinQiTrain/             # Training bridge (DLL + Python bindings)
├── xinqi-frontend/         # Main frontend (Svelte 5 + Three.js)
└── xinqi-room-frontend/    # Room frontend (Svelte 5 + Three.js)
```

### Tech Stack

| Module | Technology |
|--------|-----------|
| Core Engine | C++20, Zobrist Hashing |
| AI | Pure MCTS (UCB1, Random Rollout) |
| HTTP Server | C++20, [httplib](https://github.com/yhirose/cpp-httplib) (header-only) |
| Frontend | Svelte 5 + Three.js + TypeScript + Vite 6 |
| Training Bridge | C++ DLL → Python ctypes → NumPy → PyTorch |
| Distribution | Single exe + static frontend files, zero external deps |
| Multiplayer | Standalone room server, 4-digit room codes |

### AI Training

XinQi ships with a complete AI training scaffolding, enabling AlphaZero-style training on a 5×5×5 board.

**Current state: The training pipeline skeleton is ready. The real AI is waiting for the community.**

```python
# One-liner to get started
from xinqi_env import XinQiEnv
env = XinQiEnv(board_size=5)
policy = env.mcts_policy(simulations=800)
env.step(action_idx)
```

> Full API docs and training guide: [`TRAINING.md`](TRAINING.md)

**Contribution ideas:**

| Difficulty | Direction | Effort |
|-----------|-----------|--------|
| ★☆☆ | Improve MCTS rollout heuristics | ~50 lines C++ |
| ★★☆ | Handcrafted evaluation function | ~150 lines C++ |
| ★★☆ | Small network + MCTS (light AlphaZero) | 3‑5 days |
| ★★★★ | Full AlphaZero stack | 2‑4 weeks |

---

## License

[MIT](LICENSE) © 2026 XinQi Contributors

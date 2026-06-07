/**
 * REST API 客户端。
 *
 * 连接 XinQiServer (localhost:8090)。
 */

import type { GameState, PlayResponse, MoveStoneResponse } from "./types";

const BASE = "http://localhost:8090";

async function req<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}: ${await res.text()}`);
  }
  return res.json() as Promise<T>;
}

export interface RecordData {
  board_size: number;
  move_count: number;
  moves: { x: number; y: number; z: number; player: string; is_move: boolean }[];
  state_hex: string;
  winner: string;
  saved?: string;
}

export interface ReplayStep {
  board: number[];
  current_player: "Black" | "White";
  move_count: number;
  inner_core_count: { Black: number; White: number };
  vacancy_indices: number[];
  last_move?: { x: number; y: number; z: number };
  last_move_is_move?: boolean;
}

export interface ReplayResult {
  total: number;
  steps: ReplayStep[];
  broken: boolean;
  winner: "Black" | "White" | "None";
}

export const apiClient = {
  newGame(boardSize = 5): Promise<{ game_id: number; current_player: string; board_size: number }> {
    return req("/api/new_game", {
      method: "POST",
      body: JSON.stringify({ board_size: boardSize }),
    });
  },

  play(x: number, y: number, z: number): Promise<PlayResponse> {
    return req("/api/play", {
      method: "POST",
      body: JSON.stringify({ x, y, z }),
    });
  },

  /** 挪子 */
  moveStone(x: number, y: number, z: number, tx: number, ty: number, tz: number): Promise<MoveStoneResponse> {
    return req("/api/move_stone", {
      method: "POST",
      body: JSON.stringify({ x, y, z, target_x: tx, target_y: ty, target_z: tz }),
    });
  },

  getState(): Promise<GameState> {
    return req("/api/state");
  },

  /** 获取序列化局面 */
  async getSerializedState(): Promise<string> {
    const { state } = await req<{ state: string }>("/api/serialize");
    return state;
  },

  /** 保存棋谱 */
  async saveRecord(): Promise<void> {
    await req("/api/record/save", { method: "POST" });
  },

  /** 获取棋谱文件列表 */
  async listRecords(): Promise<string[]> {
    const { files } = await req<{ files: string[] }>("/api/record/list");
    return files;
  },

  /** 获取单份棋谱 */
  async getRecord(filename: string): Promise<RecordData> {
    return req<RecordData>(`/api/record/get?file=${encodeURIComponent(filename)}`);
  },

  /** 批量计算棋谱所有步的棋盘状态 */
  async replayRecord(record: RecordData): Promise<ReplayResult> {
    return req<ReplayResult>("/api/record/replay", {
      method: "POST",
      body: JSON.stringify({
        state_hex: record.state_hex,
        board_size: record.board_size,
        moves: record.moves,
      }),
    });
  },
};

// ── AI 客户端 ──────────────────────────────────────────────

export interface AIMoveResponse {
  x: number;
  y: number;
  z: number;
  type: number;
  target_x?: number;
  target_y?: number;
  target_z?: number;
  error?: string;
}

export interface AIStatusResponse {
  engine: string;
  version: number;
  max_simulations: number;
  default_simulations: number;
}

export const aiClient = {
  /** AI 落子 */
  move(simulations = 2000): Promise<AIMoveResponse> {
    return req("/api/ai/move", {
      method: "POST",
      body: JSON.stringify({ simulations }),
    });
  },

  /** AI 服务状态 */
  status(): Promise<AIStatusResponse> {
    return req("/api/ai/status");
  },
};

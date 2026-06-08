/**
 * 房间 API 客户端。
 *
 * 连接 XinQiRoomServer (localhost:8090)。
 * 仅包含房间相关接口。
 */

import type { PlayResponse, MoveStoneResponse, GameState } from "./types";

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

export interface CreateRoomResponse {
  room_code: string;
  player_id: string;
  role: "Black" | "White";
  board_size: number;
}

export interface JoinRoomResponse {
  player_id: string;
  role: "Black" | "White";
  board_size: number;
}

export interface RoomInfoResponse {
  room_code: string;
  host_id: string;
  guest_id: string | null;
  board_size: number;
  started: boolean;
  finished: boolean;
}

export const roomClient = {
  /** 创建房间 */
  createRoom(boardSize = 5): Promise<CreateRoomResponse> {
    return req("/api/room/create", {
      method: "POST",
      body: JSON.stringify({ board_size: boardSize }),
    });
  },

  /** 加入房间 */
  joinRoom(code: string): Promise<JoinRoomResponse> {
    return req("/api/room/join", {
      method: "POST",
      body: JSON.stringify({ room_code: code }),
    });
  },

  /** 开始游戏（房主发起） */
  startGame(code: string, playerId: string): Promise<{ success: boolean }> {
    return req(`/api/room/${code}/start`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId }),
    });
  },

  /** 落子 */
  play(code: string, playerId: string, x: number, y: number, z: number): Promise<PlayResponse> {
    return req(`/api/room/${code}/play`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId, x, y, z }),
    });
  },

  /** 挪子 */
  moveStone(code: string, playerId: string, x: number, y: number, z: number,
    tx: number, ty: number, tz: number): Promise<MoveStoneResponse> {
    return req(`/api/room/${code}/move_stone`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId, x, y, z, target_x: tx, target_y: ty, target_z: tz }),
    });
  },

  /** 获取房间游戏状态 */
  getState(code: string): Promise<GameState> {
    return req(`/api/room/${code}/state`);
  },

  /** 获取房间信息（含 opponent 信息和是否开始） */
  getInfo(code: string): Promise<RoomInfoResponse> {
    return req(`/api/room/${code}/info`);
  },
};

export type { PlayResponse, MoveStoneResponse, GameState };

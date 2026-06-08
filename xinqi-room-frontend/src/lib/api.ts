/**
 * 房间 API 客户端。
 *
 * 自动使用当前页面域名，支持 ngrok / localhost / 远程部署。
 */

import type { PlayResponse, MoveStoneResponse, GameState } from "./types";

const BASE = window.location.origin;

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
  board_size: number;
  has_guest: boolean;
  guest_ready: boolean;
  host_id: string;
  started: boolean;
  terminated: boolean;
  winner?: string;
  guest_id?: string;
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
  startGame(code: string, playerId: string, boardSize?: number): Promise<{ ok: boolean }> {
    return req(`/api/room/${code}/start`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId, board_size: boardSize }),
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

  /** 客人准备 */
  setReady(code: string, playerId: string): Promise<{ ok: boolean }> {
    return req(`/api/room/${code}/ready`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId }),
    });
  },

  /** 心跳（房主定期发送，保持房间活跃） */
  ping(code: string): Promise<{ ok: boolean }> {
    return req(`/api/room/${code}/ping`, {
      method: "POST",
      body: "{}",
    });
  },

  /** 离开房间（房主离开=关闭，客人离开=清空状态） */
  leaveRoom(code: string, playerId: string): Promise<{ ok: boolean; closed?: boolean }> {
    return req(`/api/room/${code}/leave`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId }),
    });
  },

  /** 房主重置游戏（返回房间） */
  resetGame(code: string, playerId: string): Promise<{ ok: boolean }> {
    return req(`/api/room/${code}/reset`, {
      method: "POST",
      body: JSON.stringify({ player_id: playerId }),
    });
  },
};

export type { PlayResponse, MoveStoneResponse, GameState };

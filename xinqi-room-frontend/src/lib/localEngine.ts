/**
 * 本地游戏引擎 — 纯前端执行落子/挪子/提子/终局判定。
 *
 * 使用 legality.ts 做合法性验证，执行时独立做提子和终局判定。
 * 避免网络延迟，适合本地PvP和教学。
 */

import { LegalityChecker } from "./legality";
import { findAllInnerCores, to1D, to3D } from "./boardUtils";

export interface PlaceResult {
  legal: boolean;
  captured: number[];
  terminal: boolean;
  winner?: "Black" | "White";
  error?: string;
}

export interface ShiftResult {
  legal: boolean;
  captured: number[];
  newVacancy: number;
  terminal: boolean;
  winner?: "Black" | "White";
  error?: string;
}

/** 三截面找吃：在 board 上找 opponent 连通块，无气则记入 captured */
function findAndCapture(
  board: Uint8Array,
  placedIdx: number,
  player: 1 | 2,
  N: number,
): number[] {
  const captured = new Set<number>();
  const opponent = player === 1 ? 2 : 1;
  const p = to3D(placedIdx, N);
  const dirs: [number, number, number][] = [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]];

  for (let axis = 0; axis < 3; axis++) {
    // 收集该截面的对手棋子
    const section: number[] = [];
    const visited = new Uint8Array(N * N * N);
    for (let i = 0; i < N * N * N; i++) {
      const q = to3D(i, N);
      const inSection = axis === 0 ? q.x === p.x : axis === 1 ? q.y === p.y : q.z === p.z;
      if (inSection) section.push(i);
    }
    for (const ci of section) {
      if (board[ci] === opponent && !visited[ci]) {
        const group: number[] = [];
        const queue = [ci];
        visited[ci] = 1;
        while (queue.length > 0) {
          const cur = queue.shift()!;
          group.push(cur);
          const pt = to3D(cur, N);
          for (const [dx, dy, dz] of dirs) {
            const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
            if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
            const ni = to1D(nx, ny, nz, N);
            if (!visited[ni] && board[ni] === opponent) {
              visited[ni] = 1;
              queue.push(ni);
            }
          }
        }
        // 检查气
        let hasLib = false;
        for (const gi of group) {
          const pt = to3D(gi, N);
          for (const [dx, dy, dz] of dirs) {
            const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
            if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
            const ni = to1D(nx, ny, nz, N);
            if (board[ni] === 0) { hasLib = true; break; }
          }
          if (hasLib) break;
        }
        if (!hasLib) for (const gi of group) captured.add(gi);
      }
    }
  }

  const result = [...captured];
  // 执行提子
  for (const ci of result) board[ci] = 0;
  return result;
}

/**
 * 本地执行落子。
 */
export function executePlace(
  board: Uint8Array,
  idx: number,
  player: 1 | 2,
  checker: LegalityChecker,
  historyHashes: Set<number>,
  moveCount: number,
  vacancyOwners: Map<number, "Black" | "White">,
): PlaceResult {
  const isFirst = moveCount === 0;
  const result = checker.checkMove(board, idx, player, historyHashes, isFirst);
  if (!result.legal) {
    return { legal: false, captured: [], terminal: false, error: result.reason };
  }

  // 执行落子前先记录该位置是否为对手空位（侵入检测）
  const opponentName = player === 1 ? "White" : "Black";
  const wasOpponentVacancy = vacancyOwners.get(idx) === opponentName;

  // 执行落子
  board[idx] = player;

  // 三截面找吃 + 提子
  const captures = findAndCapture(board, idx, player, checker.N);

  // 侵入获胜
  if (wasOpponentVacancy) {
    vacancyOwners.delete(idx);
    return { legal: true, captured: captures, terminal: true, winner: player === 1 ? "Black" : "White" };
  }

  // 清台：触发吃子后对方无内芯
  if (captures.length > 0) {
    const cores = findAllInnerCores(board, checker.N);
    const oppCores = player === 1 ? cores.white : cores.black;
    if (oppCores.length === 0) {
      return { legal: true, captured: captures, terminal: true, winner: player === 1 ? "Black" : "White" };
    }
  }

  // 无棋可走：当前玩家无合法操作则获胜
  if (!hasAnyLegalMove(board, player, checker, vacancyOwners)) {
    return { legal: true, captured: captures, terminal: true, winner: player === 1 ? "Black" : "White" };
  }

  return { legal: true, captured: captures, terminal: false };
}

/**
 * 本地执行挪子。
 */
export function executeShift(
  board: Uint8Array,
  sourceIdx: number,
  targetIdx: number,
  player: 1 | 2,
  checker: LegalityChecker,
  historyHashes: Set<number>,
  ownVacancies: Set<number>,
  vacancyOwners: Map<number, "Black" | "White">,
): ShiftResult {
  const result = checker.checkMoveStone(board, sourceIdx, targetIdx, player, ownVacancies, historyHashes);
  if (!result.legal) {
    return { legal: false, captured: [], newVacancy: -1, terminal: false, error: result.reason };
  }

  const opponentName = player === 1 ? "White" : "Black";
  const wasOpponentVacancy = vacancyOwners.get(targetIdx) === opponentName;

  // 执行挪子
  board[sourceIdx] = 0;
  board[targetIdx] = player;
  const newVacancy = sourceIdx;
  vacancyOwners.set(newVacancy, player === 1 ? "Black" : "White");

  // 三截面找吃 + 提子
  const captures = findAndCapture(board, targetIdx, player, checker.N);

  // 侵入获胜
  if (wasOpponentVacancy) {
    return { legal: true, captured: captures, newVacancy, terminal: true, winner: player === 1 ? "Black" : "White" };
  }

  // 清台
  if (captures.length > 0) {
    const cores = findAllInnerCores(board, checker.N);
    const oppCores = player === 1 ? cores.white : cores.black;
    if (oppCores.length === 0) {
      return { legal: true, captured: captures, newVacancy, terminal: true, winner: player === 1 ? "Black" : "White" };
    }
  }

  // 无棋可走
  if (!hasAnyLegalMove(board, player, checker, vacancyOwners)) {
    return { legal: true, captured: captures, newVacancy, terminal: true, winner: player === 1 ? "Black" : "White" };
  }

  return { legal: true, captured: captures, newVacancy, terminal: false };
}

/** 检查某方是否还有合法操作 */
function hasAnyLegalMove(
  board: Uint8Array,
  player: 1 | 2,
  checker: LegalityChecker,
  vacancyOwners: Map<number, "Black" | "White">,
): boolean {
  const N = checker.N;
  const total = N * N * N;
  const ownVacancies = new Set<number>();
  const ownerName = player === 1 ? "Black" : "White";
  for (const [idx, owner] of vacancyOwners) {
    if (owner === ownerName) ownVacancies.add(idx);
  }
  // 检查是否有合法落子
  for (let i = 0; i < total; i++) {
    if (board[i] !== 0) continue;
    const r = checker.checkMove(board, i, player, undefined, false);
    if (r.legal) return true;
  }
  // 检查是否有合法挪子（从任意内芯出发）
  for (let i = 0; i < total; i++) {
    if (board[i] !== player) continue;
    if (isInnerCoreFast(board, i, N)) {
      for (let j = 0; j < total; j++) {
        if (board[j] !== 0) continue;
        if (ownVacancies.has(j)) continue;
        const r = checker.checkMoveStone(board, i, j, player, ownVacancies, undefined);
        if (r.legal) return true;
      }
    }
  }
  return false;
}

/** 快速内芯判断 */
function isInnerCoreFast(board: Uint8Array, idx: number, N: number): boolean {
  const dirs: [number, number, number][] = [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]];
  const p = to3D(idx, N);
  for (const [dx, dy, dz] of dirs) {
    const nx = p.x + dx, ny = p.y + dy, nz = p.z + dz;
    if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
    const ni = to1D(nx, ny, nz, N);
    if (board[ni] !== player) return false;
  }
  return true;
}

/** 棋盘哈希（用于超级劫检测） */
export function boardHash(board: Uint8Array): number {
  let h = 5381;
  for (let i = 0; i < board.length; i++) {
    h = ((h << 5) + h + board[i]) | 0;
  }
  return h;
}

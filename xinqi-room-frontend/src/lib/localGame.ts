/**
 * 本地游戏执行引擎。
 *
 * 用于本地 PvP 模式：在无服务器的情况下执行落子、挪子、
 * 提子、自杀检测、终局判断。
 */

import { to3D, to1D, findAllInnerCores } from "./boardUtils";
import { boardHash } from "./legality";

// 三轴截面邻接方向
const SECTION_DIRS: Record<number, [number, number, number][]> = {
  0: [[0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]],
  1: [[1, 0, 0], [-1, 0, 0], [0, 0, 1], [0, 0, -1]],
  2: [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0]],
};

const DIRS6: [number, number, number][] = [
  [1, 0, 0], [-1, 0, 0],
  [0, 1, 0], [0, -1, 0],
  [0, 0, 1], [0, 0, -1],
];

function inBounds(x: number, y: number, z: number, N: number): boolean {
  return x >= 0 && x < N && y >= 0 && y < N && z >= 0 && z < N;
}

/** 在指定截面上 BFS 找连通块 */
function floodFill(board: Uint8Array, start: number, axis: number, color: number, N: number): number[] {
  const visited = new Set<number>();
  const queue = [start];
  visited.add(start);
  let head = 0;
  while (head < queue.length) {
    const cur = queue[head++];
    const pt = to3D(cur, N);
    for (const [dx, dy, dz] of SECTION_DIRS[axis]) {
      const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
      if (!inBounds(nx, ny, nz, N)) continue;
      const ni = to1D(nx, ny, nz, N);
      if (!visited.has(ni) && board[ni] === color) {
        visited.add(ni);
        queue.push(ni);
      }
    }
  }
  return [...visited];
}

/** 检查截面上的连通块是否有气 */
function hasLiberty(board: Uint8Array, group: number[], axis: number, N: number): boolean {
  for (const gi of group) {
    const pt = to3D(gi, N);
    for (const [dx, dy, dz] of SECTION_DIRS[axis]) {
      const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
      if (!inBounds(nx, ny, nz, N)) continue;
      const ni = to1D(nx, ny, nz, N);
      if (board[ni] === 0 || board[ni] === 3 || board[ni] === 4) return true;
    }
  }
  return false;
}

/** 找到所有应该被提走的棋子 */
function findCaptures(board: Uint8Array, idx: number, player: number, N: number): Set<number> {
  const opponent = player === 1 ? 2 : 1;
  const captures = new Set<number>();
  const seen = new Set<number>();

  for (let axis = 0; axis < 3; axis++) {
    const pt = to3D(idx, N);
    for (const [dx, dy, dz] of SECTION_DIRS[axis]) {
      const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
      if (!inBounds(nx, ny, nz, N)) continue;
      const ni = to1D(nx, ny, nz, N);
      if (board[ni] !== opponent || seen.has(ni)) continue;
      const group = floodFill(board, ni, axis, opponent, N);
      for (const gi of group) seen.add(gi);
      if (!hasLiberty(board, group, axis, N)) {
        for (const gi of group) captures.add(gi);
      }
    }
  }
  return captures;
}

/** 检查自杀 */
function checkSuicide(board: Uint8Array, idx: number, player: number, N: number): boolean {
  for (let axis = 0; axis < 3; axis++) {
    const group = floodFill(board, idx, axis, player, N);
    if (!hasLiberty(board, group, axis, N)) return true;
  }
  return false;
}

/** 检查是否中心格 */
function isCenterCellLocal(x: number, y: number, z: number, N: number): boolean {
  if (N % 2 === 1) {
    const c = (N - 1) / 2;
    return x === c && y === c && z === c;
  } else {
    const lo = N / 2 - 1, hi = N / 2;
    return x >= lo && x <= hi && y >= lo && y <= hi && z >= lo && z <= hi;
  }
}

function getConnectedBlock(board: Uint8Array, start: number, color: number, N: number): Set<number> {
  const block = new Set<number>();
  const queue = [start];
  block.add(start);
  let head = 0;
  while (head < queue.length) {
    const cur = queue[head++];
    const pt = to3D(cur, N);
    for (const [dx, dy, dz] of DIRS6) {
      const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
      if (!inBounds(nx, ny, nz, N)) continue;
      const ni = to1D(nx, ny, nz, N);
      if (!block.has(ni) && board[ni] === color) {
        block.add(ni);
        queue.push(ni);
      }
    }
  }
  return block;
}

/** 检查挪子目标是否与源连通块相邻 */
function isAdjacentToBlock(board: Uint8Array, sourceIdx: number, targetIdx: number, N: number): boolean {
  const color = board[sourceIdx];
  const block = getConnectedBlock(board, sourceIdx, color, N);
  for (const bi of block) {
    const pt = to3D(bi, N);
    for (const [dx, dy, dz] of DIRS6) {
      const nx = pt.x + dx, ny = pt.y + dy, nz = pt.z + dz;
      if (!inBounds(nx, ny, nz, N)) continue;
      const ni = to1D(nx, ny, nz, N);
      if (ni === targetIdx) return true;
    }
  }
  return false;
}

// ── 导出接口 ──

export interface LocalMoveResult {
  legal: boolean;
  captured: number[];
  terminal: boolean;
  winner?: "Black" | "White" | "Draw";
  error?: string;
}

/**
 * 本地执行落子。
 * @returns 执行结果，legal=false 时 board 不变
 */
export function executePlace(
  board: Uint8Array,
  idx: number,
  player: 1 | 2,
  N: number,
  historyHashes: Set<number>,
  moveCount: number,
): LocalMoveResult {
  // 空位检查
  if (board[idx] !== 0) return { legal: false, captured: [], terminal: false, error: "occupied" };

  // 检查是否是对方内芯空位（核心入侵）
  const isVacancy = board[idx] === 3 || board[idx] === 4;
  const vacancyOwner = board[idx] === 3 ? "Black" : board[idx] === 4 ? "White" : null;

  // 先手第一步禁天元
  if (moveCount === 0 && player === 1) {
    const pt = to3D(idx, N);
    if (isCenterCellLocal(pt.x, pt.y, pt.z, N)) {
      return { legal: false, captured: [], terminal: false, error: "first_move_center" };
    }
  }

  // 己方内芯空位不可回填
  if (vacancyOwner === (player === 1 ? "Black" : "White")) {
    return { legal: false, captured: [], terminal: false, error: "core_vacancy" };
  }

  // 落子：如果是对方内芯空位，将 board[idx] 置为 0 再落子
  const tempBoard = new Uint8Array(board);
  if (isVacancy) {
    tempBoard[idx] = player; // 直接覆盖空位（对方空位）
  } else {
    tempBoard[idx] = player;
  }

  // 找吃
  const captures = findCaptures(tempBoard, idx, player, N);
  for (const ci of captures) tempBoard[ci] = 0;

  // 自杀检测
  if (checkSuicide(tempBoard, idx, player, N)) {
    return { legal: false, captured: [], terminal: false, error: "suicide" };
  }

  // 超级劫检测
  const hash = boardHash(tempBoard);
  if (historyHashes.has(hash)) {
    return { legal: false, captured: [], terminal: false, error: "superko" };
  }

  // 执行：更新原棋盘
  board.set(tempBoard);

  // 终局检测：触发吃子后检查是否清台
  if (captures.size > 0) {
    const cores = findAllInnerCores(board, N);
    const opponentCores = player === 1 ? cores.white : cores.black;
    if (opponentCores.length === 0) {
      return { legal: true, captured: [...captures], terminal: true, winner: player === 1 ? "Black" : "White" };
    }
  }

  return { legal: true, captured: [...captures], terminal: false };
}

/**
 * 本地执行挪子。
 */
export function executeMoveStone(
  board: Uint8Array,
  sourceIdx: number,
  targetIdx: number,
  player: 1 | 2,
  N: number,
  historyHashes: Set<number>,
  ownVacancies: Set<number>,
): LocalMoveResult {
  // 源棋子属于当前玩家
  if (board[sourceIdx] !== player) {
    return { legal: false, captured: [], terminal: false, error: "not_inner_core" };
  }

  // 目标是空位
  if (board[targetIdx] !== 0) {
    return { legal: false, captured: [], terminal: false, error: "target_occupied" };
  }

  // 目标不是己方内芯空位
  if (ownVacancies.has(targetIdx)) {
    return { legal: false, captured: [], terminal: false, error: "own_vacancy" };
  }

  // 目标与源连通块相邻
  if (!isAdjacentToBlock(board, sourceIdx, targetIdx, N)) {
    return { legal: false, captured: [], terminal: false, error: "not_adjacent" };
  }

  // 模拟
  const tempBoard = new Uint8Array(board);
  tempBoard[sourceIdx] = 0;   // 移除源
  tempBoard[targetIdx] = player; // 落目标

  // 找吃
  const captures = findCaptures(tempBoard, targetIdx, player, N);
  for (const ci of captures) tempBoard[ci] = 0;

  // 自杀检测
  if (checkSuicide(tempBoard, targetIdx, player, N)) {
    return { legal: false, captured: [], terminal: false, error: "suicide" };
  }

  // 超级劫检测
  const hash = boardHash(tempBoard);
  if (historyHashes.has(hash)) {
    return { legal: false, captured: [], terminal: false, error: "superko" };
  }

  board.set(tempBoard);

  const cores = findAllInnerCores(board, N);
  const opponentCores = player === 1 ? cores.white : cores.black;
  if (opponentCores.length === 0) {
    return { legal: true, captured: [...captures], terminal: true, winner: player === 1 ? "Black" : "White" };
  }

  return { legal: true, captured: [...captures], terminal: false };
}

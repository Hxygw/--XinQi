/**
 * 本地游戏引擎 — 纯前端执行落子/挪子/提子/终局判定。
 *
 * 三截面独立提子：在 X/Y/Z 截面中各算各的气，任一截面无气即提。
 * 基于 legality.ts 做合法性验证。
 */

import { LegalityChecker } from "./legality";
import { findAllInnerCores, to1D, to3D, buildNeighbors } from "./boardUtils";
import type { NeighborTable } from "./boardUtils";

export interface PlaceResult {
  legal: boolean;
  captured: number[];
  terminal: boolean;
  winner?: "Black" | "White";
  error?: string;
  /** 终局类型：1=清台 2=侵入 3=无棋可走（仅 terminal=true 时有意义） */
  result_code?: number;
  /** 执行后的新棋盘（每次返回新对象，确保响应式） */
  board: Uint8Array;
}

export interface ShiftResult {
  legal: boolean;
  captured: number[];
  newVacancy: number;
  terminal: boolean;
  winner?: "Black" | "White";
  error?: string;
  /** 终局类型：1=清台 2=侵入 3=无棋可走（仅 terminal=true 时有意义） */
  result_code?: number;
  board: Uint8Array;
}

/**
 * 三截面独立找吃 + 提子。
 * 在 X/Y/Z 三个截面中各算各的气，任一截面内对手连通块无气即提。
 */
function findAndCapture(board: Uint8Array, playIdx: number, player: 1 | 2, N: number): number[] {
  const opponent = player === 1 ? 2 : 1;
  const captured = new Set<number>();
  const tbl = buildNeighbors(N);

  // 落子点的三个截面邻居
  const play3D = to3D(playIdx, N);
  const sectionCoords: number[][] = [];
  // X-section: fix x, all y,z
  const xs: number[] = [];
  for (let y = 0; y < N; y++) for (let z = 0; z < N; z++) xs.push(to1D(play3D.x, y, z, N));
  sectionCoords.push(xs);
  // Y-section: fix y, all x,z
  const ys: number[] = [];
  for (let x = 0; x < N; x++) for (let z = 0; z < N; z++) ys.push(to1D(x, play3D.y, z, N));
  sectionCoords.push(ys);
  // Z-section: fix z, all x,y
  const zs: number[] = [];
  for (let x = 0; x < N; x++) for (let y = 0; y < N; y++) zs.push(to1D(x, y, play3D.z, N));
  sectionCoords.push(zs);

  for (let axis = 0; axis < 3; axis++) {
    const section = sectionCoords[axis];
    const visited = new Uint8Array(N * N * N);
    const neighbors = tbl.neigh[axis];

    // 遍历该截面中所有对手棋子，找到连通块
    for (const ci of section) {
      if (board[ci] !== opponent || visited[ci]) continue;

      // 截面内 4 方向 BFS（不跨截面）
      const group: number[] = [];
      const bfsQueue = [ci];
      visited[ci] = 1;
      let bfsHead = 0;
      while (bfsHead < bfsQueue.length) {
        const cur = bfsQueue[bfsHead++];
        group.push(cur);
        for (let n = 0; n < 4; n++) {
          const ni = neighbors[cur * 4 + n];
          if (ni >= 0 && !visited[ni] && board[ni] === opponent) {
            // 确保 ni 也在同一截面中
            const np = to3D(ni, N);
            const inSection = axis === 0 ? np.x === play3D.x : axis === 1 ? np.y === play3D.y : np.z === play3D.z;
            if (inSection) {
              visited[ni] = 1;
              bfsQueue.push(ni);
            }
          }
        }
      }

      // 检查该连通块在截面内是否有气
      let hasLib = false;
      for (const gi of group) {
        for (let n = 0; n < 4; n++) {
          const ni = neighbors[gi * 4 + n];
          if (ni >= 0 && (board[ni] === 0 || board[ni] === 3 || board[ni] === 4)) {
            hasLib = true;
            break;
          }
        }
        if (hasLib) break;
      }

      if (!hasLib) {
        for (const gi of group) captured.add(gi);
      }
    }
  }

  const result = [...captured];
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
  // 0. 己方内芯空位不可落子（Core: ERR_CORE_VACANCY）
  const playerName = player === 1 ? "Black" : "White";
  const opponentName = player === 1 ? "White" : "Black";
  if (vacancyOwners.get(idx) === playerName) {
    return { legal: false, captured: [], terminal: false, error: "core_vacancy", board: new Uint8Array(board) };
  }

  const isFirst = moveCount === 0;
  const result = checker.checkMove(board, idx, player, historyHashes, isFirst);
  if (!result.legal) {
    return { legal: false, captured: [], terminal: false, error: result.reason, board: new Uint8Array(board) };
  }

  const wasOpponentVacancy = vacancyOwners.get(idx) === opponentName;

  const newBoard = new Uint8Array(board);
  newBoard[idx] = player;

  const captures = findAndCapture(newBoard, idx, player, checker.N);

  // 自杀复检：findAndCapture 可能与 checkMove 内部提子结果不同
  const suicideCheck = checker.checkPlaceSuicide(newBoard, idx, player);
  if (!suicideCheck.legal) {
    return { legal: false, captured: [], terminal: false, error: "suicide", board: new Uint8Array(board) };
  }

  // 侵入获胜
  if (wasOpponentVacancy) {
    vacancyOwners.delete(idx);
    return { legal: true, captured: captures, terminal: true, result_code: 2, winner: player === 1 ? "Black" : "White", board: newBoard };
  }

  // 清台：触发吃子后对方无内芯
  if (captures.length > 0) {
    const cores = findAllInnerCores(newBoard, checker.N);
    const oppCores = player === 1 ? cores.white : cores.black;
    if (oppCores.length === 0) {
      return { legal: true, captured: captures, terminal: true, result_code: 1, winner: player === 1 ? "Black" : "White", board: newBoard };
    }
  }

  // 无棋可走
  if (!hasAnyLegalMove(newBoard, player, checker, vacancyOwners)) {
    return { legal: true, captured: captures, terminal: true, result_code: 3, winner: player === 1 ? "Black" : "White", board: newBoard };
  }

  return { legal: true, captured: captures, terminal: false, board: newBoard };
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
    return { legal: false, captured: [], newVacancy: -1, terminal: false, error: result.reason, board: new Uint8Array(board) };
  }

  const opponentName = player === 1 ? "White" : "Black";
  const wasOpponentVacancy = vacancyOwners.get(targetIdx) === opponentName;

  const newBoard = new Uint8Array(board);
  newBoard[sourceIdx] = 0;
  newBoard[targetIdx] = player;
  const newVacancy = sourceIdx;
  vacancyOwners.set(newVacancy, player === 1 ? "Black" : "White");

  const captures = findAndCapture(newBoard, targetIdx, player, checker.N);

  // 自杀复检：findAndCapture 可能与 checkMoveStone 内部提子结果不同
  const suicideCheck = checker.checkPlaceSuicide(newBoard, targetIdx, player);
  if (!suicideCheck.legal) {
    return { legal: false, captured: [], newVacancy: -1, terminal: false, error: "suicide", board: new Uint8Array(board) };
  }

  if (wasOpponentVacancy) {
    return { legal: true, captured: captures, newVacancy, terminal: true, result_code: 2, winner: player === 1 ? "Black" : "White", board: newBoard };
  }

  if (captures.length > 0) {
    const cores = findAllInnerCores(newBoard, checker.N);
    const oppCores = player === 1 ? cores.white : cores.black;
    if (oppCores.length === 0) {
      return { legal: true, captured: captures, newVacancy, terminal: true, result_code: 1, winner: player === 1 ? "Black" : "White", board: newBoard };
    }
  }

  if (!hasAnyLegalMove(newBoard, player, checker, vacancyOwners)) {
    return { legal: true, captured: captures, newVacancy, terminal: true, result_code: 3, winner: player === 1 ? "Black" : "White", board: newBoard };
  }

  return { legal: true, captured: captures, newVacancy, terminal: false, board: newBoard };
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
  for (let i = 0; i < total; i++) {
    if (board[i] !== 0) continue;
    // 己方内芯空位不可填
    if (vacancyOwners.get(i) === ownerName) continue;
    if (checker.checkMove(board, i, player, undefined, false).legal) return true;
  }
  // 挪子：从内芯出发到空位
  const dirs: [number, number, number][] = [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]];
  for (let i = 0; i < total; i++) {
    if (board[i] !== player) continue;
    // 快速内芯检查
    let isCore = true;
    const p = to3D(i, N);
    for (const [dx, dy, dz] of dirs) {
      const nx = p.x + dx, ny = p.y + dy, nz = p.z + dz;
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
      const ni = to1D(nx, ny, nz, N);
      if (board[ni] !== player) { isCore = false; break; }
    }
    if (!isCore) continue;
    for (let j = 0; j < total; j++) {
      if (board[j] !== 0) continue;
      if (ownVacancies.has(j)) continue;
      if (checker.checkMoveStone(board, i, j, player, ownVacancies, undefined).legal) return true;
    }
  }
  return false;
}

/** 棋盘哈希 */
export function boardHash(board: Uint8Array): number {
  let h = 5381;
  for (let i = 0; i < board.length; i++) {
    h = ((h << 5) + h + board[i]) | 0;
  }
  return h;
}

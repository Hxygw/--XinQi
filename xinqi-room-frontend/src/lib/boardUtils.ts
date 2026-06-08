/**
 * 芯棋棋盘工具函数。
 * 内芯检测、连通块 BFS、邻接关系等。
 */

import { CellState } from "./types";

// ── 索引转换 ──────────────────────────────────────────

export function to3D(idx: number, N: number): { x: number; y: number; z: number } {
  const x = Math.floor(idx / (N * N));
  const r = idx % (N * N);
  const y = Math.floor(r / N);
  const z = r % N;
  return { x, y, z };
}

export function to1D(x: number, y: number, z: number, N: number): number {
  return x * N * N + y * N + z;
}

// ── 六个方向 ──────────────────────────────────────────

const DIRS: [number, number, number][] = [
  [1, 0, 0], [-1, 0, 0],
  [0, 1, 0], [0, -1, 0],
  [0, 0, 1], [0, 0, -1],
];

/** 6-邻接索引（3D），超出棋盘返回 -1 */
export function getNeighbors6(idx: number, N: number): number[] {
  const { x, y, z } = to3D(idx, N);
  const result: number[] = [];
  for (const [dx, dy, dz] of DIRS) {
    const nx = x + dx, ny = y + dy, nz = z + dz;
    if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
      result.push(to1D(nx, ny, nz, N));
    }
  }
  return result;
}

// ── 截面 4-邻接（用在截面规则中） ────────────────────

/** 三轴各自的截面 4-邻接（与 OrthoGO 一致） */
export type Axis = 0 | 1 | 2; // X, Y, Z

const SECTION_DIRS: { [axis: number]: [number, number, number][] } = {
  0: [[0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], // X: Y-Z 平面
  1: [[1, 0, 0], [-1, 0, 0], [0, 0, 1], [0, 0, -1]], // Y: X-Z 平面
  2: [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0]], // Z: X-Y 平面
};

export function getSectionNeighbors(idx: number, axis: Axis, N: number): number[] {
  const { x, y, z } = to3D(idx, N);
  const result: number[] = [];
  for (const [dx, dy, dz] of SECTION_DIRS[axis]) {
    const nx = x + dx, ny = y + dy, nz = z + dz;
    if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
      result.push(to1D(nx, ny, nz, N));
    }
  }
  return result;
}

// ── 截面 4-邻接预计算（性能优化，与 OrthoGO 相同） ──

export interface NeighborTable {
  neigh: Int32Array[]; // neigh[axis][idx*4 + i] 最多 4 个邻接
  N: number;
  total: number;
}

export function buildNeighbors(N: number): NeighborTable {
  const total = N * N * N;
  const neigh: Int32Array[] = [
    new Int32Array(total * 4),
    new Int32Array(total * 4),
    new Int32Array(total * 4),
  ];
  neigh[0].fill(-1);
  neigh[1].fill(-1);
  neigh[2].fill(-1);

  for (let x = 0; x < N; x++) {
    for (let y = 0; y < N; y++) {
      for (let z = 0; z < N; z++) {
        const idx = x * N * N + y * N + z;
        const base = idx * 4;

        // Axis X: 固定 x, Y-Z 平面邻接
        if (y > 0) neigh[0][base] = x * N * N + (y - 1) * N + z;
        if (y < N - 1) neigh[0][base + 1] = x * N * N + (y + 1) * N + z;
        if (z > 0) neigh[0][base + 2] = x * N * N + y * N + (z - 1);
        if (z < N - 1) neigh[0][base + 3] = x * N * N + y * N + (z + 1);

        // Axis Y: 固定 y, X-Z 平面邻接
        if (x > 0) neigh[1][base] = (x - 1) * N * N + y * N + z;
        if (x < N - 1) neigh[1][base + 1] = (x + 1) * N * N + y * N + z;
        if (z > 0) neigh[1][base + 2] = x * N * N + y * N + (z - 1);
        if (z < N - 1) neigh[1][base + 3] = x * N * N + y * N + (z + 1);

        // Axis Z: 固定 z, X-Y 平面邻接
        if (x > 0) neigh[2][base] = (x - 1) * N * N + y * N + z;
        if (x < N - 1) neigh[2][base + 1] = (x + 1) * N * N + y * N + z;
        if (y > 0) neigh[2][base + 2] = x * N * N + (y - 1) * N + z;
        if (y < N - 1) neigh[2][base + 3] = x * N * N + (y + 1) * N + z;
      }
    }
  }

  return { neigh, N, total };
}

// ── 3D 6-邻接 BFS 连通块 ──────────────────────────────

/** BFS 获取同色连通块的所有索引 */
export function getConnectedComponent(
  board: Uint8Array,
  start: number,
  N: number,
  visited?: Uint8Array,
  visitGen?: number
): { indices: number[]; visitedArray: Uint8Array; gen: number } {
  const ownVisited = !visited;
  const v = visited ?? new Uint8Array(N * N * N);
  const gen = visitGen ?? 1;

  const color = board[start];
  if (color === 0) return { indices: [], visitedArray: v, gen: ownVisited ? 0 : gen };

  const queue: number[] = [start];
  const indices: number[] = [];
  if (ownVisited) v[start] = gen;
  else v[start] = gen;

  let head = 0;
  while (head < queue.length) {
    const cur = queue[head++];
    indices.push(cur);
    for (const ni of getNeighbors6(cur, N)) {
      if (v[ni] !== gen && board[ni] === color) {
        v[ni] = gen;
        queue.push(ni);
      }
    }
  }

  return { indices, visitedArray: v, gen: ownVisited ? gen : gen };
}

// ── 内芯判定 ──────────────────────────────────────────

/**
 * 检查 idx 位置的棋子是否为内芯。
 *
 * 内芯定义：棋子在六方向（±X,±Y,±Z）上每个方向
 * 要么被己方棋子占据，要么碰到棋盘边界（边界视为友方）。
 */
export function isInnerCore(board: Uint8Array, idx: number, N: number): boolean {
  const color = board[idx];
  if (color === 0) return false;

  const { x, y, z } = to3D(idx, N);
  const dirs: [number, number, number][] = [
    [1, 0, 0], [-1, 0, 0],
    [0, 1, 0], [0, -1, 0],
    [0, 0, 1], [0, 0, -1],
  ];

  for (const [dx, dy, dz] of dirs) {
    const nx = x + dx, ny = y + dy, nz = z + dz;
    // 边界视为友方
    if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
    // 非边界必须被己方占据
    const ni = to1D(nx, ny, nz, N);
    if (board[ni] !== color) return false;
  }

  return true;
}

/**
 * 查找棋盘上所有内芯。
 * 返回双方内芯的索引列表。
 */
export function findAllInnerCores(
  board: Uint8Array,
  N: number
): { black: number[]; white: number[] } {
  const black: number[] = [];
  const white: number[] = [];
  const total = N * N * N;
  for (let i = 0; i < total; i++) {
    if (board[i] === 1 && isInnerCore(board, i, N)) black.push(i);
    else if (board[i] === 2 && isInnerCore(board, i, N)) white.push(i);
  }
  return { black, white };
}

/** 快速判断某格是否为内芯（基于预计算列表） */
export function isInnerCoreFast(idx: number, innerCoreSet: Set<number>): boolean {
  return innerCoreSet.has(idx);
}

/** 从内芯列表构建 Set */
export function buildInnerCoreSet(cores: number[]): Set<number> {
  return new Set(cores);
}

/**
 * 检查挪出 source 后，source 的连通块中是否还有内芯。
 * 用于判断挪子后连通块是否完全失去内芯。
 */
export function connectedBlockHasInnerCore(
  board: Uint8Array,
  sourceIdx: number,
  N: number,
  innerCoreSet: Set<number>
): boolean {
  const color = board[sourceIdx];
  if (color === 0) return false;

  const block = getConnectedComponent(board, sourceIdx, N);
  for (const idx of block.indices) {
    if (idx !== sourceIdx && innerCoreSet.has(idx)) return true;
  }
  return false;
}

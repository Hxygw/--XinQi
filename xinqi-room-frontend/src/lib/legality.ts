/**
 * TypeScript 版芯棋合法性检测。
 *
 * 镜像 C++ 截面规则逻辑，悬浮实时判断。
 * 包含：落子合法性（checkMove）和 挪子合法性（checkMoveStone）。
 *
 * 棋盘边界视为友方——边界不是气，不影响吃子/自杀判定。
 */

import { buildNeighbors, isInnerCore, getNeighbors6, to3D, to1D } from "./boardUtils";
import type { NeighborTable } from "./boardUtils";

// ── 类型 ──────────────────────────────────────────────────

export enum Axis {
  X = 0, // 固定 X，截面为 Y-Z 平面
  Y = 1, // 固定 Y，截面为 X-Z 平面
  Z = 2, // 固定 Z，截面为 X-Y 平面
}

export interface CheckResult {
  legal: boolean;
  reason?: "occupied" | "suicide" | "superko" | "first_move_center" | "not_inner_core" | "not_adjacent" | "own_vacancy";
  /** 导致自杀的截面轴列表 */
  fatalAxes?: Axis[];
}

export interface MoveStoneCheckResult {
  legal: boolean;
  reason?: "not_inner_core" | "target_occupied" | "own_vacancy" | "not_adjacent" | "suicide" | "superko";
  fatalAxes?: Axis[];
}

// ── 合法性检测器 ──────────────────────────────────────────

export class LegalityChecker {
  private tbl: NeighborTable;
  private genCounter = 0;
  private visited: Int32Array;
  private queue: Int32Array;

  constructor(N: number) {
    this.tbl = buildNeighbors(N);
    this.visited = new Int32Array(N * N * N);
    this.queue = new Int32Array(N * N * N);
  }

  reinit(N: number): void {
    this.tbl = buildNeighbors(N);
    this.visited = new Int32Array(N * N * N);
    this.queue = new Int32Array(N * N * N);
  }

  get N(): number { return this.tbl.N; }
  get total(): number { return this.tbl.total; }

  // ── 落子合法性检查 ─────────────────────────────────

  /**
   * 检查指定空点落子是否合法。
   * @param board 当前棋盘状态（平坦数组，0=空 1=黑 2=白）
   * @param idx 要落子的位置
   * @param player 当前玩家 (1=Black, 2=White)
   * @param historyHashes 历史哈希集合（可选，用于超级劫检查）
   * @param isFirstMove 是否为第一步（禁天元）
   */
  checkMove(
    board: Uint8Array,
    idx: number,
    player: number,
    historyHashes?: Set<number>,
    isFirstMove?: boolean,
  ): CheckResult {
    // 1. 空位检查（对手内芯空位视为可落子——核心入侵）
    const cell = board[idx];
    if (cell !== 0) {
      // 对手内芯空位：合法（由 caller 处理核心入侵逻辑）
      const oppVacancy = (player === 1 && cell === 4) || (player === 2 && cell === 3);
      if (!oppVacancy) {
        return { legal: false, reason: "occupied" };
      }
      // 对手空位视为空，继续后续检查（自杀/超级劫）
    }

    // 2. 先手第一步禁天元/最内层
    if (isFirstMove && player === 1) {
      const pt = to3D(idx, this.N);
      if (isCenterCell(pt.x, pt.y, pt.z, this.N)) {
        return { legal: false, reason: "first_move_center" };
      }
    }

    // 3. 临时落子
    const temp = new Uint8Array(board);
    temp[idx] = player;

    // 4. 三截面找吃 + 去重提子
    const opponent = player === 1 ? 2 : 1;
    const captures = this.findCaptures(temp, idx, player);
    const deduped = this.dedupeCaptures(captures);

    for (const ci of deduped) {
      temp[ci] = 0;
    }

    // 5. 自杀判定：任一截面落子方的连通块完全无气 → 自杀
    const { suicide, fatalAxes } = this.checkSuicide(temp, idx, player);
    if (suicide) {
      return { legal: false, reason: "suicide", fatalAxes };
    }

    // 6. 超级劫检查（如果有历史哈希）
    if (historyHashes) {
      const hash = boardHash(temp);
      if (historyHashes.has(hash)) {
        return { legal: false, reason: "superko" };
      }
    }

    return { legal: true };
  }

  // ── 挪子合法性检查 ─────────────────────────────────

  /**
   * 检查挪子操作是否合法。
   * @param board 当前棋盘
   * @param sourceIdx 要挪走的棋子（必须是内芯）
   * @param targetIdx 目标位置（必须为空）
   * @param player 当前玩家
   * @param ownVacancies 己方内芯空位集合（不可回填）
   * @param historyHashes 历史哈希集合
   */
  checkMoveStone(
    board: Uint8Array,
    sourceIdx: number,
    targetIdx: number,
    player: number,
    ownVacancies: Set<number>,
    historyHashes?: Set<number>,
  ): MoveStoneCheckResult {
    // 1. 源棋子属于当前玩家
    if (board[sourceIdx] !== player) {
      return { legal: false, reason: "not_inner_core" };
    }

    // 2. 源是内芯
    if (!isInnerCore(board, sourceIdx, this.N)) {
      return { legal: false, reason: "not_inner_core" };
    }

    // 3. 目标为空位或对手内芯空位
    const targetCell = board[targetIdx];
    if (targetCell !== 0) {
      const oppVacancy = (player === 1 && targetCell === 4) || (player === 2 && targetCell === 3);
      if (!oppVacancy) {
        return { legal: false, reason: "target_occupied" };
      }
      // 对手空位：合法（核心入侵，由 caller 处理）
    }

    // 4. 目标不是己方内芯空位
    if (ownVacancies.has(targetIdx)) {
      return { legal: false, reason: "own_vacancy" };
    }

    // 5. 目标与 source 的 3D 连通块相邻（6-邻接）
    if (!this.isAdjacentToBlock(board, sourceIdx, targetIdx)) {
      return { legal: false, reason: "not_adjacent" };
    }

    // 6. 模拟：移除 source → 落子 target → 吃子 → 自杀判定
    const temp = new Uint8Array(board);
    temp[sourceIdx] = 0;  // 移除源棋子
    temp[targetIdx] = player;  // 落子目标

    const opponent = player === 1 ? 2 : 1;

    // 找吃（源位置已空，不会影响）
    const captures = this.findCapturesAfterMove(temp, targetIdx, player, sourceIdx);
    const deduped = this.dedupeCaptures(captures);
    for (const ci of deduped) {
      temp[ci] = 0;
    }

    // 自杀判定：检查 targetIdx 处的连通块
    const { suicide, fatalAxes } = this.checkSuicide(temp, targetIdx, player);
    if (suicide) {
      return { legal: false, reason: "suicide", fatalAxes };
    }

    // 7. 超级劫检查
    if (historyHashes) {
      const hash = boardHash(temp);
      if (historyHashes.has(hash)) {
        return { legal: false, reason: "superko" };
      }
    }

    return { legal: true };
  }

  // ── 辅助：目标与源连通块是否相邻 ──────────────────

  private isAdjacentToBlock(board: Uint8Array, sourceIdx: number, targetIdx: number): boolean {
    const color = board[sourceIdx];
    if (color === 0) return false;

    // BFS 找 source 的 6-邻接同色连通块
    const visited = new Uint8Array(this.N * this.N * this.N);
    const queue: number[] = [sourceIdx];
    visited[sourceIdx] = 1;
    let head = 0;

    while (head < queue.length) {
      const cur = queue[head++];
      // 检查 cur 的 6-邻接是否等于 targetIdx
      for (const ni of getNeighbors6(cur, this.N)) {
        if (ni === targetIdx) return true;
        if (visited[ni] === 0 && board[ni] === color) {
          visited[ni] = 1;
          queue.push(ni);
        }
      }
    }
    return false;
  }

  // ── 截面吃子检测 ────────────────────────────────────

  private findCaptures(board: Uint8Array, playIdx: number, player: number): number[] {
    const opponent = player === 1 ? 2 : 1;
    const captures: number[] = [];

    for (let axis = 0; axis < 3; axis++) {
      this.genCounter++;

      const base = playIdx * 4;
      const n = this.tbl.neigh[axis];
      for (let i = 0; i < 4; i++) {
        const ni = n[base + i];
        if (ni < 0) continue;
        if (board[ni] !== opponent) continue;

        const { head, size } = this.floodFill(board, ni, axis, opponent);
        if (!this.hasLibertyInSection(board, head, size, axis)) {
          for (let j = 0; j < size; j++) {
            captures.push(this.queue[head + j]);
          }
        }
      }
    }

    return captures;
  }

  /** 挪子版的找吃：排除 sourceIdx（已被移除）的影响 */
  private findCapturesAfterMove(
    board: Uint8Array, targetIdx: number, player: number, removedIdx: number
  ): number[] {
    const opponent = player === 1 ? 2 : 1;
    const captures: number[] = [];

    for (let axis = 0; axis < 3; axis++) {
      this.genCounter++;

      const base = targetIdx * 4;
      const n = this.tbl.neigh[axis];
      for (let i = 0; i < 4; i++) {
        const ni = n[base + i];
        if (ni < 0 || ni === removedIdx) continue;
        if (board[ni] !== opponent) continue;

        const { head, size } = this.floodFill(board, ni, axis, opponent);
        if (!this.hasLibertyInSection(board, head, size, axis)) {
          for (let j = 0; j < size; j++) {
            captures.push(this.queue[head + j]);
          }
        }
      }
    }

    return captures;
  }

  private floodFill(
    board: Uint8Array,
    start: number,
    axis: Axis,
    color: number
  ): { head: number; size: number } {
    let head = 0;
    let tail = 0;
    this.queue[tail++] = start;
    this.visited[start] = this.genCounter;

    const n = this.tbl.neigh[axis];

    while (head < tail) {
      const cur = this.queue[head++];
      const base = cur * 4;

      for (let i = 0; i < 4; i++) {
        const ni = n[base + i];
        if (ni < 0) continue;
        if (this.visited[ni] === this.genCounter) continue;
        if (board[ni] !== color) continue;

        this.visited[ni] = this.genCounter;
        this.queue[tail++] = ni;
      }
    }

    return { head: 0, size: tail };
  }

  private hasLibertyInSection(
    board: Uint8Array,
    head: number,
    size: number,
    axis: Axis
  ): boolean {
    const n = this.tbl.neigh[axis];
    for (let i = 0; i < size; i++) {
      const cur = this.queue[head + i];
      const base = cur * 4;

      for (let j = 0; j < 4; j++) {
        const ni = n[base + j];
        if (ni < 0) continue;
        if (board[ni] === 0 || board[ni] === 3 || board[ni] === 4) return true;
      }
    }
    return false;
  }

  // ── 自杀判定 ────────────────────────────────────────

  private checkSuicide(
    board: Uint8Array,
    idx: number,
    player: number
  ): { suicide: boolean; fatalAxes: Axis[] } {
    const fatalAxes: Axis[] = [];

    for (let axis = 0; axis < 3; axis++) {
      this.genCounter++;
      const { head, size } = this.floodFill(board, idx, axis, player);
      const hasLiberty = this.hasLibertyInSection(board, head, size, axis);
      if (!hasLiberty) {
        fatalAxes.push(axis as Axis);
      }
    }

    // 芯棋规则：任一截面无气 → 自杀
    const isSuicide = fatalAxes.length > 0;
    return { suicide: isSuicide, fatalAxes };
  }

  // ── 工具 ────────────────────────────────────────────

  private dedupeCaptures(captures: number[]): number[] {
    if (captures.length === 0) return [];
    const seen = new Set<number>();
    const result: number[] = [];
    for (const ci of captures) {
      if (!seen.has(ci)) {
        seen.add(ci);
        result.push(ci);
      }
    }
    return result;
  }
}

// ── 外部工具函数 ────────────────────────────────────────

/** 简易棋盘哈希（用于超级劫检查） */
export function boardHash(board: Uint8Array): number {
  let hash = 5381;
  for (let i = 0; i < board.length; i++) {
    hash = ((hash << 5) + hash + board[i]) | 0;
  }
  return hash;
}

/** 判断坐标是否在棋盘中心（禁天元用） */
export function isCenterCell(x: number, y: number, z: number, N: number): boolean {
  if (N % 2 === 1) {
    const c = (N - 1) / 2;
    return x === c && y === c && z === c;
  } else {
    const lo = N / 2 - 1;
    const hi = N / 2;
    return x >= lo && x <= hi && y >= lo && y <= hi && z >= lo && z <= hi;
  }
}

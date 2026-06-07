/** 棋子颜色 / 格点状态（与 XinQiCore.h 的 CELL_* 常量对齐） */
export enum CellState {
  Empty = 0,
  Black = 1,
  White = 2,
  BlackVacancy = 3,
  WhiteVacancy = 4,
}

export interface Point {
  x: number;
  y: number;
  z: number;
}

/** 服务端 /api/state 响应格式 */
export interface GameState {
  board_size: number;
  /** 平坦数组 board[i] */
  board: number[];
  current_player: "Black" | "White";
  move_count: number;
  inner_core_count: { Black: number; White: number };
  /** 内芯空位索引列表 */
  vacancy_indices: number[];
  /** 内芯空位所有者：{ "idx": "Black"|"White", ... } */
  vacancy_owners: Record<string, "Black" | "White">;
  terminal: boolean;
  winner?: "Black" | "White" | "Draw";
}

/** 服务端 /api/play 响应 */
export interface PlayResponse {
  legal: boolean;
  captured: Point[];
  captured_count: number;
  next_player: "Black" | "White";
  terminal: boolean;
  winner?: "Black" | "White" | "Draw";
  error?: string;
}

/** 服务端 /api/move_stone 响应 */
export interface MoveStoneResponse {
  legal: boolean;
  captured: Point[];
  captured_count: number;
  new_vacancy: Point;
  next_player: "Black" | "White";
  terminal: boolean;
  winner?: "Black" | "White" | "Draw";
  error?: string;
}

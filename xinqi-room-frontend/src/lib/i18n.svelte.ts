/**
 * i18n 国际化模块。
 *
 * 支持中文 (zh) 和英文 (en) 切换。
 * 使用 Svelte 5 $state 以实现响应式重渲染。
 * 房间版：移除棋谱/AI 相关，添加房间相关条目。
 */

export type Lang = "zh" | "en";

type Translations = Record<string, Record<Lang, string>>;

const dict: Translations = {
  // 标题
  "app.title": { zh: "芯棋", en: "XinQi" },
  "app.subtitle": { zh: "网络对弈", en: "Online Play" },

  // 语言切换
  "lang.switch": { zh: "EN", en: "中" },

  // 加载/错误
  "loading.connecting": { zh: "连接引擎中...", en: "Connecting to engine..." },
  "error.cannot_connect": { zh: "无法连接引擎", en: "Cannot connect to engine" },
  "error.retry": { zh: "重试", en: "Retry" },
  "error.sync_failed": { zh: "同步状态失败", en: "Sync state failed" },
  "error.create_failed": { zh: "创建失败", en: "Create failed" },
  "error.operation_failed": { zh: "操作失败", en: "Operation failed" },

  // 侧栏
  "sidebar.turn": { zh: "回合", en: "Turn" },
  "sidebar.current": { zh: "当前", en: "Current" },
  "sidebar.inner_core": { zh: "内芯", en: "Inner Core" },
  "sidebar.new_game": { zh: "退出游戏", en: "Exit Game" },
  "sidebar.start_game": { zh: "开始游戏", en: "Start Game" },
  "sidebar.board_size": { zh: "棋盘大小", en: "Board Size" },
  "sidebar.section": { zh: "剖面", en: "Section" },
  "sidebar.section_all": { zh: "全", en: "All" },
  "sidebar.section_hint": { zh: "~全 · 1/2/3 X/Y/Z · +/- 移层", en: "~All · 1/2/3 X/Y/Z · +/- Layer" },
  "sidebar.move_mode_hint": { zh: "挪子模式 — 双击目标执行，右键退出", en: "Shift Mode — Double-click target, right-click to exit" },

  // 房间模式
  "room.pvp_local": { zh: "本地对战", en: "Local PvP" },
  "room.create_room": { zh: "创建房间", en: "Create Room" },
  "room.join_room": { zh: "加入房间", en: "Join Room" },
  "room.create": { zh: "创建房间", en: "Create Room" },
  "room.join": { zh: "加入房间", en: "Join Room" },
  "room.room_code": { zh: "房间号", en: "Room Code" },
  "room.waiting_opponent": { zh: "等待对手加入...", en: "Waiting for opponent..." },
  "room.start_game": { zh: "开始游戏", en: "Start Game" },
  "room.your_turn": { zh: "轮到你落子", en: "Your turn" },
  "room.opponent_turn": { zh: "等待对方落子...", en: "Waiting for opponent..." },
  "room.leave": { zh: "离开房间", en: "Leave Room" },
  "room.enter_code": { zh: "输入房间号", en: "Enter room code" },
  "room.host": { zh: "房主", en: "Host" },
  "room.guest": { zh: "客人", en: "Guest" },
  "room.joined": { zh: "已加入", en: "joined" },
  "room.waiting_host": { zh: "等待房主开始...", en: "Waiting for host to start..." },
  "room.ready": { zh: "准备", en: "Ready" },
  "room.ready_done": { zh: "已准备", en: "Ready ✓" },
  "room.guest_ready": { zh: "客人已准备", en: "Guest ready" },
  "room.guest_not_ready": { zh: "客人未准备", en: "Guest not ready" },
  "room.return_room": { zh: "返回房间", en: "Return to Room" },
  "room.room_closed": { zh: "房间已关闭", en: "Room closed" },
  "room.guest_left": { zh: "客人已离开", en: "Guest left" },

  // 玩家
  "player.black": { zh: "黑方", en: "Black" },
  "player.white": { zh: "白方", en: "White" },
  "player.black_short": { zh: "黑", en: "B" },
  "player.white_short": { zh: "白", en: "W" },

  // 终局
  "terminal.black_win": { zh: "黑胜", en: "Black Wins" },
  "terminal.white_win": { zh: "白胜", en: "White Wins" },
  "terminal.draw": { zh: "平局", en: "Draw" },

  // 通知
  "notif.game_started": { zh: "新游戏已开始", en: "New game started" },
  "notif.game_over": { zh: "终局", en: "Game Over" },
  "notif.place_failed": { zh: "落子失败", en: "Place failed" },
  "notif.shift_failed": { zh: "挪子失败", en: "Shift failed" },
  "notif.not_inner_core": { zh: "此棋子不是内芯，无法挪动", en: "This stone is not an inner core, cannot shift" },
  "notif.enter_shift_mode": { zh: "挪子模式", en: "Shift mode" },
  "notif.exit_shift_mode": { zh: "退出挪子模式", en: "Exit shift mode" },
  "notif.valid_targets": { zh: "个合法位置", en: "valid target(s)" },
  "notif.full_view": { zh: "全视图", en: "Full view" },
  "notif.section_view": { zh: "剖面", en: "Section" },

  // 悬停提示
  "hover.legal": { zh: "合法 ✓", en: "Legal ✓" },
  "hover.suicide": { zh: "自杀 ✗", en: "Suicide ✗" },
  "hover.superko": { zh: "超级劫 ✗", en: "Superko ✗" },
  "hover.illegal": { zh: "不合法", en: "Illegal" },
  "hover.occupied": { zh: "已有棋子", en: "Occupied" },
  "hover.first_move_center": { zh: "先手禁天元", en: "First move center forbidden" },
  "hover.inner_core_shift": { zh: "内芯 — 双击挪动", en: "Inner core — double-click to shift" },
  "hover.inner_core": { zh: "内芯", en: "Inner core" },
  "hover.core_vacancy_own": { zh: "己方内芯空位 — 不可回填", en: "Own core vacancy — cannot refill" },
  "hover.inside_block": { zh: "源连通块内", en: "Inside source block" },
  "hover.shift_target_legal": { zh: "挪子目标 ✓", en: "Shift target ✓" },
  "hover.shift_target_illegal": { zh: "挪子目标 ✗", en: "Shift target ✗" },
  "hover.no_liberties": { zh: "均无气", en: "no liberties on" },

  // 规则面板
  "rules.title": { zh: "游戏规则", en: "Game Rules" },
  "rules.board_title": { zh: "棋盘", en: "Board" },
  "rules.board_desc": { zh: "N×N×N 的立体格点棋盘。每个交点称为一个「格点」，是落子的位置。棋盘的最外层六面视为「友方」——相当于己方的棋子，参与包围判定。", en: "An N×N×N 3D grid board. Each intersection is a 'point' where stones are placed. The six outermost faces of the board are considered 'friendly' — acting as your own stones for surround judgment." },
  "rules.liberty_title": { zh: "气", en: "Liberty" },
  "rules.liberty_desc": { zh: "一个棋子通过四个方向（在 X/Y/Z 某个截面内，前后左右）连通的同色棋子组成一个「连通块」。连通块周围的空格点称为「气」。如果一个连通块在某个截面上完全没气，那么该连通块在这个截面上被提走。", en: "Stones of the same color connected via four directions (front/back/left/right within an X/Y/Z section) form a 'group'. Empty points adjacent to a group are 'liberties'. If a group has no liberties on any section, it is captured on that section." },
  "rules.inner_core_title": { zh: "内芯", en: "Inner Core" },
  "rules.inner_core_desc": { zh: "一个棋子的六个方向（±X,±Y,±Z）全被己方棋子或棋盘边界堵住，即为「内芯」。角部靠三面棋壁只需再围三子（共 4 子）即可成内芯。内芯是挪子的前提。", en: "A stone whose six directions (±X,±Y,±Z) are all blocked by own stones or board edges is an 'inner core'. At a corner, only 3 additional stones (4 total) are needed. Inner cores are the prerequisite for shifting." },
  "rules.place_title": { zh: "操作一：落子", en: "Action 1: Place" },
  "rules.place_desc": { zh: "双击一个空格即可落子。系统会自动完成：① 检查合法性 → ② 放置棋子 → ③ 提走对方无气棋子 → ④ 换手。", en: "Double-click an empty point to place a stone. The system automatically: ① checks legality → ② places the stone → ③ captures opponent's dead groups → ④ switches turn." },
  "rules.shift_title": { zh: "操作二：挪子", en: "Action 2: Shift" },
  "rules.shift_desc": { zh: "双击己方的内芯进入挪子模式，此时该内芯所在整个连通块会高亮，合法目标位置会显示绿色标记。双击一个合法目标即可将内芯移过去。原位置变为「内芯空位」——己方永远不可回填，对方可占据此空位获胜。挪子也可以触发提子，挪子后换手。", en: "Double-click your inner core to enter shift mode. The entire connected block is highlighted, and valid targets show green markers. Double-click a valid target to move the inner core there. The original position becomes a 'core vacancy' — you can never refill it, but the opponent can occupy it to win. Shifting can also trigger captures." },
  "rules.capture_title": { zh: "提子（吃子）", en: "Capture" },
  "rules.capture_desc": { zh: "落子或挪子完成后，系统在 X/Y/Z 三个截面中分别检查。在任一截面中，若对方连通块完全无气，则该连通块被整体提走。三个截面的提子独立计算。", en: "After a place or shift, the system checks all three X/Y/Z sections. If an opponent's group has no liberties on any section, the entire group is captured. Captures are computed independently per section." },
  "rules.suicide_title": { zh: "禁手：自杀", en: "Forbidden: Suicide" },
  "rules.suicide_desc": { zh: "落子或挪子并完成提子后，如果在任一截面中己方连通块完全无气，那么这手棋不合法。", en: "After placing or shifting and performing captures, if your own group has no liberties on any section, the move is illegal." },
  "rules.first_move_title": { zh: "禁手：先手第一步禁天元", en: "Forbidden: First move center" },
  "rules.first_move_desc": { zh: "黑方第一手禁止下在棋盘中心格点（奇数 N 的正中心，偶数 N 的最内层 2×2×2）。", en: "Black's first move cannot be at the board center (exact center for odd N, innermost 2×2×2 for even N)." },
  "rules.superko_title": { zh: "禁手：超级劫", en: "Forbidden: Superko" },
  "rules.superko_desc": { zh: "任何落子或挪子不得让全局局面完全恢复到上一手之前的状态。系统自动检测并阻止。", en: "No place or shift may return the board to the exact state before the previous move. The system automatically detects and prevents this." },
  "rules.win_title": { zh: "获胜条件（满足任一即赢）", en: "Win Conditions (any one)" },
  "rules.win_clear": { zh: "① 清台终局 — 你触发吃子后，对方棋盘上没有任何内芯了 → 你赢。", en: "① Board Clear — After your capture, opponent has no inner cores left → You win." },
  "rules.win_invade": { zh: "② 内芯侵入 — 你合法占据对方的内芯空位（对方挪子留下的空位）→ 你赢。", en: "② Core Invasion — You legally occupy an opponent's core vacancy → You win." },
  "rules.win_stalemate": { zh: "③ 无棋可下 — 轮到你时，你既不能合法落子也不能合法挪子 → 你赢。", en: "③ Stalemate — On your turn, you can neither place nor shift legally → You win." },
  "rules.hotkeys_title": { zh: "快捷键", en: "Hotkeys" },
  "rules.hotkeys_desc": { zh: "1/2/3 剖面视图 · ~ 全图 · +/- 翻层 · 右键 退出挪子模式", en: "1/2/3 Section views · ~ Full view · +/- Change layer · Right-click Exit shift mode" },

  // 剖面轴名称
  "section.x": { zh: "X 截面", en: "X Section" },
  "section.y": { zh: "Y 截面", en: "Y Section" },
  "section.z": { zh: "Z 截面", en: "Z Section" },
};

// 使用 Svelte 5 $state 实现响应式 — Svelte 会追踪此变量并在模板中自动重渲染
let currentLang: Lang = $state("zh");

export function setLang(lang: Lang) {
  currentLang = lang;
}

export function getLang(): Lang {
  return currentLang;
}

export function t(key: string): string {
  return dict[key]?.[currentLang] ?? key;
}

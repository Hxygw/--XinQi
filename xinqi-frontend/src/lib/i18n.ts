/**
 * i18n 国际化模块。
 *
 * 当前仅实现中文 (zh)，预留英文 (en) 接口。
 */

export type Lang = "zh" | "en";

type Translations = Record<string, Record<Lang, string>>;

const dict: Translations = {
  // 标题
  "app.title": { zh: "芯棋", en: "XinQi" },
  "app.subtitle": { zh: "探索三维的逻辑之美", en: "Explore the Beauty of 3D Logic" },

  // 侧栏
  "sidebar.turn": { zh: "回合", en: "Turn" },
  "sidebar.current": { zh: "当前", en: "Current" },
  "sidebar.inner_core": { zh: "内芯", en: "Inner Core" },
  "sidebar.new_game": { zh: "新游戏", en: "New Game" },
  "sidebar.save_record": { zh: "保存棋谱", en: "Save Record" },
  "sidebar.browse_records": { zh: "棋谱浏览", en: "Browse Records" },
  "sidebar.exit_browse": { zh: "退出浏览", en: "Exit Browse" },
  "sidebar.board_size": { zh: "棋盘大小", en: "Board Size" },
  "sidebar.section": { zh: "剖面", en: "Section" },
  "sidebar.section_hint": { zh: "~全 · 1/2/3 X/Y/Z · +/- 移层", en: "~All · 1/2/3 X/Y/Z · +/- Layer" },
  "sidebar.move_mode_hint": { zh: "挪子模式 — 双击目标执行，右键退出", en: "Shift Mode — Double-click target, right-click to exit" },

  // 玩家
  "player.black": { zh: "黑方", en: "Black" },
  "player.white": { zh: "白方", en: "White" },

  // 终局
  "terminal.black_win": { zh: "黑胜", en: "Black Wins" },
  "terminal.white_win": { zh: "白胜", en: "White Wins" },
  "terminal.draw": { zh: "平局", en: "Draw" },

  // 通知
  "notif.game_started": { zh: "新游戏已开始", en: "New game started" },
  "notif.game_over": { zh: "终局", en: "Game Over" },
  "notif.place_failed": { zh: "落子失败", en: "Place failed" },
  "notif.shift_failed": { zh: "挪子失败", en: "Shift failed" },

  // 弹窗
  "modal.record_list": { zh: "棋谱列表", en: "Record List" },
  "modal.load_record": { zh: "加载棋谱", en: "Load Record" },
  "modal.confirm": { zh: "确认", en: "Confirm" },
  "modal.cancel": { zh: "取消", en: "Cancel" },
  "modal.empty": { zh: "暂无棋谱", en: "No records" },
  "modal.warn": { zh: "当前对局将被覆盖。", en: "Current game will be overwritten." },

  // 浏览栏
  "browse.mode": { zh: "浏览模式", en: "Browse" },
};

let currentLang: Lang = "zh";

export function setLang(lang: Lang) {
  currentLang = lang;
}

export function getLang(): Lang {
  return currentLang;
}

export function t(key: string): string {
  return dict[key]?.[currentLang] ?? key;
}

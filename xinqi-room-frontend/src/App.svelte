<script lang="ts">
  /**
   * 芯棋房间前端 - 状态中心。
   */

  import { onMount, onDestroy } from "svelte";
  import Board3D from "./components/Board3D.svelte";
  import GameInfo from "./components/GameInfo.svelte";
  import { roomClient } from "./lib/api";
  import type { RoomInfoResponse } from "./lib/api";
  import { LegalityChecker, boardHash } from "./lib/legality";
  import {
    findAllInnerCores, buildInnerCoreSet, getNeighbors6, to3D, to1D,
  } from "./lib/boardUtils";
  import { t, setLang, getLang } from "./lib/i18n.svelte";
  import type { Lang } from "./lib/i18n.svelte";
  import { playPlace, playShift, playCapture, playVictoryIntrusion, playVictoryAnnihilation } from "./lib/sound";

  // ─ 游戏状态 ──
  let status = $state("ready");
  let errorMsg = $state("");
  let notification = $state("");

  // ── 亮暗模式 ──
  let isDark = $state(false);

  function toggleDark() {
    isDark = !isDark;
    document.body.setAttribute("data-theme", isDark ? "dark" : "light");
  }

  // Board3D 场景颜色
  let sceneBg = $derived(isDark ? 0x0B0F19 : 0xFAFAF9);
  let gridColor = $derived(isDark ? 0x1E293B : 0xA8A29E);
  let dotColor = $derived(isDark ? 0x1E293B : 0xA8A29E);
  let innerCoreGlowColor = $derived(isDark ? 0xFFFFFF : 0x000000);
  let gridOpacity = $derived(isDark ? 0.25 : 0.5);

  // 棋子颜色（随主题变化）
  let stonePrimary = $derived(isDark ? 0x38BDF8 : 0x1E293B);
  let stonePrimaryHex = $derived(isDark ? "#38BDF8" : "#1E293B");
  let stoneSecondary = $derived(isDark ? 0xF59E0B : 0xD97706);
  let stoneSecondaryHex = $derived(isDark ? "#F59E0B" : "#D97706");
  let vacancyColor = $derived(isDark ? 0xDC2626 : 0xF43F5E);

  let N = $state(5);
  let board = $state(new Uint8Array(125));
  let currentPlayer = $state<"Black" | "White">("Black");
  let moveCount = $state(0);
  let terminal = $state(false);
  let winner = $state<"Black" | "White" | "Draw" | undefined>();

  // ── 房间模式 ──
  type RoomMode = "none" | "host" | "guest" | "playing";
  let roomMode = $state<RoomMode>("none");
  let localPvP = $derived(!!localBlackId); // 有 localBlackId 即为本地 PvP
  let roomCode = $state("");
  let playerId = $state("");
  let playerRole = $state<"Black" | "White">("Black");
  let roomHostId = $state("");
  let joinCodeInput = $state("");
  let waitingOpponent = $state(false);
  let gameStarted = $state(false);

  // 本地 PvP 双玩家 ID（本地对战用）
  let localBlackId = $state("");
  let localWhiteId = $state("");

  // 轮询定时器
  let pollTimer: ReturnType<typeof setTimeout> | null = null;

  // 内芯
  let innerCores = $state<{ black: number[]; white: number[] }>({ black: [], white: [] });
  let innerCoreSet = $state(new Set<number>());

  // 内芯空位
  let vacancyOwners = $state(new Map<number, "Black" | "White">());
  let vacancySet = $derived(new Set(vacancyOwners.keys()));
  let vacancyBlack = $derived(new Set(
    [...vacancyOwners].filter(([_, o]) => o === "Black").map(([k]) => k)
  ));
  let vacancyWhite = $derived(new Set(
    [...vacancyOwners].filter(([_, o]) => o === "White").map(([k]) => k)
  ));

  // 超级劫
  let historyHashes = $state(new Set<number>());

  // 悬停
  let hoverIdx = $state(-1);
  let hoverLegal = $state(false);
  let hoverFatalAxes = $state<number[] | undefined>();
  let hoverIsInnerCore = $state(false);
  let hoverInfo = $state("");

  // 挪子模式
  let moveMode = $state(false);
  let moveSourceIdx = $state(-1);
  let moveBlockIndices = $state(new Set<number>());
  let validTargets = $state<number[]>([]);
  let validTargetHover = $state<{ idx: number; legal: boolean } | null>(null);

  // 规则面板
  let showRules = $state(false);

  // 剖面
  let sectionAxis = $state<string | null>(null);
  let sectionPos = $state(0);

  let checker: LegalityChecker;

  let boardHoverResult = $derived(
    hoverIdx >= 0
      ? { idx: hoverIdx, legal: hoverLegal, fatalAxes: hoverFatalAxes, isInnerCore: hoverIsInnerCore }
      : null
  );

  let innerCoreCountBlack = $derived(innerCores.black.length);
  let innerCoreCountWhite = $derived(innerCores.white.length);

  // 当前玩家对应的色调 RGB（用于侧栏渐变提示）
  let turnColorRgb = $derived(
    gameStarted
      ? (currentPlayer === "Black"
        ? (isDark ? "14,165,233" : "51,65,85")
        : (isDark ? "249,115,22" : "180,83,9"))
      : ""
  );

  let notifTimer: ReturnType<typeof setTimeout>;

  // ── 语言切换 ──
  function handleToggleLang() {
    setLang(getLang() === "zh" ? "en" : "zh");
  }

  function showNotification(msg: string, duration = 3000) {
    notification = msg;
    clearTimeout(notifTimer);
    notifTimer = setTimeout(() => { notification = ""; }, duration);
  }

  onMount(() => {
    document.body.setAttribute("data-theme", "light");
    checker = new LegalityChecker(N);
  });

  onDestroy(() => {
    if (pollTimer) clearTimeout(pollTimer);
  });

  function refreshInnerCores() {
    innerCores = findAllInnerCores(board, N);
    innerCoreSet = buildInnerCoreSet([...innerCores.black, ...innerCores.white]);
  }

  /** 从 GameState 同步状态 */
  function applyGameState(gs: { board_size: number; board: number[]; current_player: string; move_count: number; terminal: boolean; winner?: string; vacancy_owners: Record<string, string> }) {
    N = gs.board_size;
    board = new Uint8Array(gs.board);
    currentPlayer = gs.current_player as "Black" | "White";
    moveCount = gs.move_count;
    terminal = gs.terminal;
    winner = gs.winner as "Black" | "White" | "Draw" | undefined;
    const newOwners = new Map<number, "Black" | "White">();
    for (const [idxStr, owner] of Object.entries(gs.vacancy_owners)) {
      newOwners.set(parseInt(idxStr), owner as "Black" | "White");
    }
    vacancyOwners = newOwners;
    if (checker.N !== N) checker.reinit(N);
    refreshInnerCores();
  }

  // ── 本地 PvP（通过服务端） ──

  async function initLocalPvP() {
    try {
      const room = await roomClient.createRoom(N);
      const join = await roomClient.joinRoom(room.room_code);
      await roomClient.startGame(room.room_code, room.player_id);
      localBlackId = room.player_id;
      localWhiteId = join.player_id;
      roomCode = room.room_code;
      playerId = room.player_id;
      playerRole = "Black";
      roomHostId = room.player_id;
      roomMode = "playing";
      gameStarted = true;
      // 第一次获取状态
      try {
        const gs = await roomClient.getState(roomCode);
        applyGameState(gs);
      } catch {}
      showNotification(t("room.pvp_local"));
    } catch (e) {
      showNotification(`${t("error.operation_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  // ── 房间操作 ──

  async function handleCreateRoom() {
    try {
      const res = await roomClient.createRoom(N);
      roomCode = res.room_code;
      playerId = res.player_id;
      playerRole = res.role as "Black" | "White";
      roomHostId = res.player_id;
      roomMode = "host";
      waitingOpponent = true;
      startPollInfo();
      showNotification(`${t("room.room_code")}: ${res.room_code}`, 5000);
    } catch (e) {
      showNotification(`${t("error.create_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  async function handleJoinRoom() {
    if (!joinCodeInput.trim()) return;
    try {
      const res = await roomClient.joinRoom(joinCodeInput.trim());
      roomCode = joinCodeInput.trim();
      playerId = res.player_id;
      playerRole = res.role as "Black" | "White";
      roomMode = "guest";
      waitingOpponent = true;
      startPollInfo();
    } catch (e) {
      showNotification(`${t("error.operation_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  async function handleHostStartGame() {
    try {
      await roomClient.startGame(roomCode, playerId);
      waitingOpponent = false;
      // 等待轮询到 started=true 后切换
    } catch (e) {
      showNotification(`${t("error.operation_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  async function handleLeaveRoom() {
    if (pollTimer) clearTimeout(pollTimer);
    pollTimer = null;
    roomMode = "none";
    gameStarted = false;
    localBlackId = "";
    localWhiteId = "";
    roomCode = "";
    playerId = "";
    roomHostId = "";
    waitingOpponent = false;
    joinCodeInput = "";
    const total = N * N * N;
    board = new Uint8Array(total);
    currentPlayer = "Black";
    moveCount = 0;
    terminal = false;
    winner = undefined;
    vacancyOwners = new Map();
    historyHashes = new Set();
    moveMode = false;
    refreshInnerCores();
  }

  // ── 轮询 ──

  function startPollInfo() {
    pollInfo();
  }

  async function pollInfo() {
    if (!roomCode || roomMode === "none" || gameStarted) return;

    try {
      const info = await roomClient.getInfo(roomCode);
      if (!info.started && info.guest_id && roomMode === "host") {
        // 客人已加入，房主可开始游戏
        waitingOpponent = false;
      }
      if (info.started && !gameStarted) {
        // 游戏已开始，切换为 playing
        startRoomGame();
        return;
      }
    } catch {
      // 忽略轮询错误
    }

    if (!gameStarted) {
      pollTimer = setTimeout(pollInfo, 2000);
    }
  }

  async function startRoomGame() {
    roomMode = "playing";
    gameStarted = true;
    waitingOpponent = false;
    if (pollTimer) clearTimeout(pollTimer);
    pollTimer = null;
    status = "ready";

    try {
      const gs = await roomClient.getState(roomCode);
      applyGameState(gs);
      showNotification(t("notif.game_started"));
      // 开始对手落子轮询
      startRoomPoll();
    } catch (e) {
      showNotification(`${t("error.sync_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  function startRoomPoll() {
    scheduleRoomPoll();
  }

  function scheduleRoomPoll() {
    if (terminal || !gameStarted || roomMode !== "playing") return;
    pollTimer = setTimeout(doRoomPoll, 1500);
  }

  async function doRoomPoll() {
    if (terminal || !gameStarted || roomMode !== "playing") {
      pollTimer = null;
      return;
    }

    try {
      const gs = await roomClient.getState(roomCode);
      applyGameState(gs);

      if (terminal) {
        playMoveSound(true, undefined, false, false);
        showNotification(t("notif.game_over"), 3000);
        return;
      }
    } catch {
      // 忽略
    }

    // 继续轮询如果不是自己的回合
    if (!isMyTurn()) {
      scheduleRoomPoll();
    }
  }

  function isMyTurn(): boolean {
    // 本地 PvP：当前玩家轮到谁就是谁
    if (localBlackId && localWhiteId) return true;
    // 房间模式：只有自己的回合
    return playerRole === currentPlayer;
  }

  // ── 游戏操作 ──

  function isCenterCell(x: number, y: number, z: number): boolean {
    if (N % 2 === 1) {
      const c = (N - 1) / 2;
      return x === c && y === c && z === c;
    } else {
      const lo = N / 2 - 1;
      const hi = N / 2;
      return x >= lo && x <= hi && y >= lo && y <= hi && z >= lo && z <= hi;
    }
  }

  async function handlePlace(pt: { x: number; y: number; z: number }) {
    if (terminal) return;
    if (!localBlackId && !isMyTurn()) {
      showNotification(t("room.opponent_turn"), 1500);
      return;
    }

    // 本地 PvP：自动切换玩家 ID
    const activeId = localBlackId ? (currentPlayer === "Black" ? localBlackId : localWhiteId) : playerId;

    try {
      const result = await roomClient.play(roomCode, activeId, pt.x, pt.y, pt.z);
      if (result.legal) {
        playMoveSound(result.terminal, result.result_code, (result.captured_count ?? 0) > 0, false);
        await syncRoomState();
        if (result.terminal) {
          showNotification(t("notif.game_over"), 3000);
        } else if (localBlackId) {
          // 本地 PvP：继续（当前玩家已通过 syncRoomState 切换）
        } else {
          scheduleRoomPoll();
        }
      } else {
        const msg = result.error === "suicide" ? t("hover.suicide") :
          result.error === "first_move_center" ? t("hover.first_move_center") :
          result.error === "core_vacancy" ? t("hover.core_vacancy_own") : t("hover.illegal");
        showNotification(msg, 2000);
      }
    } catch (e) {
      showNotification(`${t("notif.place_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  async function handleMoveStone(pt: { x: number; y: number; z: number }) {
    if (terminal) return;
    if (!localBlackId && !isMyTurn()) {
      showNotification(t("room.opponent_turn"), 1500);
      return;
    }

    const src = to3D(moveSourceIdx, N);
    const activeId = localBlackId ? (currentPlayer === "Black" ? localBlackId : localWhiteId) : playerId;

    try {
      const result = await roomClient.moveStone(roomCode, activeId, src.x, src.y, src.z, pt.x, pt.y, pt.z);
      if (result.legal) {
        playMoveSound(result.terminal, result.result_code, (result.captured_count ?? 0) > 0, true);
        await syncRoomState();
        exitMoveMode();
        if (result.terminal) {
          showNotification(t("notif.game_over"), 3000);
        } else if (!localBlackId) {
          scheduleRoomPoll();
        }
      } else {
        showNotification(`${t("notif.shift_failed")}: ${result.error || ""}`, 2000);
      }
    } catch (e) {
      showNotification(`${t("notif.shift_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  async function syncRoomState() {
    try {
      const gs = await roomClient.getState(roomCode);
      applyGameState(gs);
    } catch (e) {
      showNotification(`${t("error.sync_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  function handleCellClick(pt: { x: number; y: number; z: number }) {
    if (terminal) return;
    if (!gameStarted) return;
    const idx = to1D(pt.x, pt.y, pt.z, N);
    if (moveMode) { handleMoveStone(pt); return; }
    if (innerCoreSet.has(idx) && board[idx] !== 0) {
      const player = currentPlayer === "Black" ? 1 : 2;
      if (board[idx] === player) { enterMoveMode(idx); return; }
    }
    // 对手内芯空位 → 落入空腔（核心入侵）
    if (vacancySet.has(idx) && vacancyOwners.get(idx) !== currentPlayer) { handlePlace(pt); return; }
    if (board[idx] === 0) { handlePlace(pt); return; }
    const cellPlayer = currentPlayer === "Black" ? 1 : 2;
    if (board[idx] === cellPlayer) showNotification(t("notif.not_inner_core"), 1500);
  }

  function enterMoveMode(sourceIdx: number) {
    moveMode = true;
    moveSourceIdx = sourceIdx;
    const player = board[sourceIdx];
    const visited = new Uint8Array(N * N * N);
    const queue: number[] = [sourceIdx];
    visited[sourceIdx] = 1;
    let head = 0;
    const blockSet = new Set<number>();
    const targets: number[] = [];
    while (head < queue.length) {
      const cur = queue[head++];
      blockSet.add(cur);
      for (const ni of getNeighbors6(cur, N)) {
        if (visited[ni] === 0 && board[ni] === player) {
          visited[ni] = 1;
          queue.push(ni);
        }
      }
    }
    moveBlockIndices = blockSet;
    const seenTarget = new Set<number>();
    const ownVacancies = new Set<number>();
    const playerNum = currentPlayer === "Black" ? 1 : 2;
    for (const bi of blockSet) {
      for (const ni of getNeighbors6(bi, N)) {
        if (seenTarget.has(ni)) continue;
        seenTarget.add(ni);
        if (board[ni] !== 0) continue;
        if (ownVacancies.has(ni)) continue;
        const result = checker.checkMoveStone(board, sourceIdx, ni, playerNum, ownVacancies, historyHashes);
        if (result.legal) targets.push(ni);
      }
    }
    validTargets = targets;
    showNotification(`${t("notif.enter_shift_mode")}：${validTargets.length} ${t("notif.valid_targets")}`, 2000);
  }

  function exitMoveMode() {
    moveMode = false;
    moveSourceIdx = -1;
    moveBlockIndices = new Set();
    validTargets = [];
    validTargetHover = null;
    showNotification(t("notif.exit_shift_mode"), 1000);
  }

  function handleRightClick() {
    if (!gameStarted) return;
    if (moveMode) exitMoveMode();
  }

  const axisNames = [t("section.x"), t("section.y"), t("section.z")];

  function handleHover(pt: { x: number; y: number; z: number }) {
    if (!gameStarted) {
      hoverIdx = -1; hoverInfo = ""; hoverLegal = false;
      hoverFatalAxes = undefined; hoverIsInnerCore = false;
      validTargetHover = null;
      return;
    }
    const idx = to1D(pt.x, pt.y, pt.z, N);
    hoverIdx = idx;
    const cellVal = board[idx];
    const owner = vacancyOwners.get(idx);

    // 对方内芯空位 → 直接跳合法性检测
    if (vacancySet.has(idx) && owner !== currentPlayer) {
      const player = currentPlayer === "Black" ? 1 : 2;
      const tempBoard = new Uint8Array(board);
      tempBoard[idx] = 0;
      const result = checker.checkMove(tempBoard, idx, player, historyHashes, moveCount === 0);
      if (result.legal) { hoverInfo = t("hover.legal"); hoverLegal = true; hoverFatalAxes = undefined; }
      else if (result.reason === "suicide" && result.fatalAxes) {
        const names = result.fatalAxes.map((a: number) => axisNames[a]).join("、");
        hoverInfo = `${t("hover.suicide")} (${names}${t("hover.no_liberties")})`; hoverLegal = false; hoverFatalAxes = result.fatalAxes;
      } else if (result.reason === "superko") { hoverInfo = t("hover.superko"); hoverLegal = false; hoverFatalAxes = undefined; }
      else { hoverInfo = t("hover.illegal"); hoverLegal = false; hoverFatalAxes = undefined; }
      return;
    }

    if (moveMode) {
      if (validTargets.includes(idx)) {
        const player = currentPlayer === "Black" ? 1 : 2;
        const result = checker.checkMoveStone(board, moveSourceIdx, idx, player, new Set(), historyHashes);
        hoverLegal = result.legal;
        hoverFatalAxes = result.fatalAxes;
        hoverIsInnerCore = false;
        validTargetHover = { idx, legal: result.legal };
        hoverInfo = result.legal ? t("hover.shift_target_legal") : t("hover.shift_target_illegal");
        return;
      }
      const player = currentPlayer === "Black" ? 1 : 2;
      if (cellVal === player && moveBlockIndices.has(idx)) {
        hoverInfo = t("hover.inside_block");
      }
      hoverLegal = false; hoverFatalAxes = undefined;
      hoverIsInnerCore = false; validTargetHover = null;
      return;
    }
    hoverIsInnerCore = innerCoreSet.has(idx);
    if (moveCount === 0 && cellVal === 0 && isCenterCell(pt.x, pt.y, pt.z)) {
      hoverInfo = t("hover.first_move_center"); hoverLegal = false; hoverFatalAxes = undefined; return;
    }
    if (hoverIsInnerCore) {
      if ((currentPlayer === "Black" && cellVal === 1) || (currentPlayer === "White" && cellVal === 2)) {
        hoverInfo = t("hover.inner_core_shift");
        hoverLegal = true; hoverFatalAxes = undefined; return;
      }
      hoverInfo = t("hover.inner_core"); hoverLegal = true; hoverFatalAxes = undefined; return;
    }
    if (vacancySet.has(idx)) {
      if (owner === currentPlayer) {
        hoverInfo = t("hover.core_vacancy_own"); hoverLegal = false; hoverFatalAxes = undefined; return;
      }
    }
    if (cellVal !== 0) {
      hoverInfo = t("hover.occupied"); hoverLegal = false; hoverFatalAxes = undefined; return;
    }
    const player = currentPlayer === "Black" ? 1 : 2;
    const result = checker.checkMove(board, idx, player, historyHashes, moveCount === 0);
    if (result.legal) {
      hoverInfo = t("hover.legal"); hoverLegal = true; hoverFatalAxes = undefined;
    } else if (result.reason === "suicide" && result.fatalAxes) {
      const names = result.fatalAxes.map((a: number) => axisNames[a]).join("、");
      hoverInfo = `${t("hover.suicide")} (${names}${t("hover.no_liberties")})`; hoverLegal = false; hoverFatalAxes = result.fatalAxes;
    } else if (result.reason === "occupied") {
      hoverInfo = t("hover.occupied"); hoverLegal = false; hoverFatalAxes = undefined;
    } else if (result.reason === "superko") {
      hoverInfo = t("hover.superko"); hoverLegal = false; hoverFatalAxes = undefined;
    } else {
      hoverInfo = t("hover.illegal"); hoverLegal = false; hoverFatalAxes = undefined;
    }
  }

  function handleLeave() {
    hoverIdx = -1; hoverInfo = ""; hoverLegal = false;
    hoverFatalAxes = undefined; hoverIsInnerCore = false;
    validTargetHover = null;
  }

  /** 音效优先级：终局 > 提子 > 落子/挪子 */
  function playMoveSound(terminal: boolean, resultCode: number | undefined, captured: boolean, isShift: boolean) {
    if (terminal) {
      if (resultCode === 2) playVictoryIntrusion(); else playVictoryAnnihilation();
    } else if (captured) playCapture();
    else if (isShift) playShift(); else playPlace();
  }

  function handleSectionChange(axis: string | null, pos: number) {
    sectionAxis = axis; sectionPos = pos;
    showNotification(axis ? `${t("notif.section_view")} ${axis.toUpperCase()} = ${pos}` : t("notif.full_view"), 1500);
  }

  async function changeBoardSize(newN: number) {
    if (gameStarted) return; // 游戏中不可修改
    N = newN; sectionAxis = null;
    checker.reinit(N);
    board = new Uint8Array(N * N * N);
    refreshInnerCores();
  }

  // ── 射线检测开关 ──
  let raycastEnabled = $derived(
    !gameStarted || (!terminal && (localPvP || isMyTurn()))
  );

  // 退出房间
  function handleExitGame() {
    if (localPvP) {
      roomMode = "none";
      gameStarted = false;
      localBlackId = "";
      localWhiteId = "";
      const total = N * N * N;
      moveCount = 0;
      terminal = false;
      winner = undefined;
      vacancyOwners = new Map();
      historyHashes = new Set();
      moveMode = false;
      refreshInnerCores();
    } else {
      handleLeaveRoom();
    }
  }
</script>

<main>
  <!-- 顶部导航栏 -->
  <header class="topbar">
    <div class="topbar-left">
      <svg class="logo-icon" viewBox="0 0 32 32" fill="none">
        <path d="M16 4L28 10V22L16 28L4 22V10L16 4Z" stroke="url(#logo-grad)" stroke-width="1.5" fill="none"/>
        <path d="M16 4V16M16 16L28 22M16 16L4 22" stroke="url(#logo-grad)" stroke-width="1" opacity="0.5"/>
        <defs><linearGradient id="logo-grad" x1="4" y1="4" x2="28" y2="28"><stop stop-color="#7c6df0"/><stop offset="1" stop-color="#a78bfa"/></linearGradient></defs>
      </svg>
      <span class="topbar-title">{t("app.title")}</span>
      <span class="topbar-subtitle">{t("app.subtitle")}</span>
    </div>
    <div class="topbar-right">
      <button class="btn-lang" onclick={toggleDark}>
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          {#if isDark}
            <circle cx="12" cy="12" r="5"/><path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/>
          {:else}
            <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>
          {/if}
        </svg>
      </button>
      <button class="btn-lang" onclick={handleToggleLang}>{t("lang.switch")}</button>
    </div>
  </header>

  <!-- 主内容区 -->
  {#if status === "loading"}
    <div class="loading">
      <div class="spinner"></div>
      <p>{t("loading.connecting")}</p>
    </div>

  {:else if status === "error"}
    <div class="error">
      <h2>{t("error.cannot_connect")}</h2>
      <p class="err-detail">{errorMsg}</p>
      <button class="btn-retry" onclick={() => { status = "ready"; }}>{t("error.retry")}</button>
    </div>

  {:else}
    <div class="game-layout">
      <div class="board-area">
        <Board3D
          bind:board bind:N
          {sectionAxis} {sectionPos}
          {innerCoreSet} {vacancyBlack} {vacancyWhite}
          {moveMode} moveSourceIdx={moveSourceIdx}
          moveBlockIndices={moveBlockIndices} {validTargets}
          hoverResult={boardHoverResult} {validTargetHover}
          browseTick={0}
          onhover={handleHover} onleave={handleLeave}
          oncellclick={handleCellClick} oncellrightclick={handleRightClick}
          onSectionChange={handleSectionChange}
          raycastEnabled={raycastEnabled}
          autoRotate={!gameStarted}
          {sceneBg} {gridColor} {dotColor} {innerCoreGlowColor}
          {stonePrimary} {stoneSecondary} {vacancyColor} {gridOpacity}
        />
      </div>

      <!-- 侧栏卡片 -->
      <aside class="sidebar" style={turnColorRgb ? `--turn-rgb:${turnColorRgb}` : ''}>
        <GameInfo
          {moveCount}
          innerCoreBlack={innerCoreCountBlack}
          innerCoreWhite={innerCoreCountWhite}
          {terminal} {winner}
          stonePrimaryHex={stonePrimaryHex} stoneSecondaryHex={stoneSecondaryHex}
        />

        {#if !gameStarted}
          <!-- 开始屏幕：三种模式选择 -->
          {#if roomMode === "none"}
            <button class="btn-action primary" onclick={initLocalPvP}>
              <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>
              {t("room.pvp_local")}
            </button>

            <button class="btn-action primary" onclick={handleCreateRoom}>
              <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>
              {t("room.create_room")}
            </button>

            <!-- 加入房间输入 -->
            <div class="section-group">
              <div class="section-label">{t("room.enter_code")}</div>
              <input class="room-code-input" type="text" bind:value={joinCodeInput} placeholder={t("room.room_code")}
                onkeydown={(e) => { if (e.key === 'Enter') handleJoinRoom(); }} />
              <button class="btn-action primary" onclick={handleJoinRoom} disabled={!joinCodeInput.trim()}>
                {t("room.join")}
              </button>
            </div>

            <!-- 棋盘大小 -->
            <div class="section-group">
              <div class="section-label">{t("sidebar.board_size")}</div>
              <div class="size-buttons">
                {#each [3, 4, 5, 6, 7] as s}
                  <button class="size-btn" class:active={N === s} onclick={() => changeBoardSize(s)}>
                    {s}³{#if N === s}<svg class="check" width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3"><polyline points="20 6 9 17 4 12"/></svg>{/if}
                  </button>
                {/each}
              </div>
            </div>
          {/if}

          <!-- 等待中 -->
          {#if roomMode === "host" && waitingOpponent}
            <div class="room-status">
              <div class="spinner"></div>
              <p>{t("room.waiting_opponent")}</p>
              <div class="room-code-display">{roomCode}</div>
            </div>
            <button class="btn-action primary" onclick={handleHostStartGame} disabled>
              {t("room.start_game")}
            </button>
            <button class="btn-action" onclick={handleLeaveRoom}>
              {t("room.leave")}
            </button>
          {/if}

          {#if roomMode === "host" && !waitingOpponent && !gameStarted}
            <div class="room-status">
              <p>{t("room.guest")} 已加入！</p>
            </div>
            <button class="btn-action primary" onclick={handleHostStartGame}>
              {t("room.start_game")}
            </button>
            <button class="btn-action" onclick={handleLeaveRoom}>
              {t("room.leave")}
            </button>
          {/if}

          {#if roomMode === "guest" && waitingOpponent}
            <div class="room-status">
              <div class="spinner"></div>
              <p>{t("room.waiting_opponent")}</p>
            </div>
            <button class="btn-action" onclick={handleLeaveRoom}>
              {t("room.leave")}
            </button>
          {/if}

        {:else}
          <!-- 游戏中 -->
          <button class="btn-action primary" onclick={handleExitGame}>
            <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/></svg>
            {t("sidebar.new_game")}
          </button>

          {#if !localPvP}
            <div class="room-status">
              {#if isMyTurn()}
                <p>{t("room.your_turn")}</p>
              {:else}
                <p>{t("room.opponent_turn")}</p>
              {/if}
              <div class="room-code-display" style="font-size:0.75rem">{t("room.room_code")}: {roomCode}</div>
            </div>
          {/if}

          <!-- 游戏中：剖面 -->
          <div class="section-group">
            <div class="section-label">{t("sidebar.section")}</div>
            <div class="size-buttons">
              <button class="size-btn" class:active={sectionAxis === null} onclick={() => handleSectionChange(null, 0)}>{t("sidebar.section_all")}</button>
              <button class="size-btn" class:active={sectionAxis === 'x'} onclick={() => handleSectionChange('x', Math.floor((N-1)/2))}>X</button>
              <button class="size-btn" class:active={sectionAxis === 'y'} onclick={() => handleSectionChange('y', Math.floor((N-1)/2))}>Y</button>
              <button class="size-btn" class:active={sectionAxis === 'z'} onclick={() => handleSectionChange('z', Math.floor((N-1)/2))}>Z</button>
            </div>
            {#if sectionAxis}
              <div class="section-slider">
                <span class="slider-label">{sectionAxis.toUpperCase()}:</span>
                <input type="range" min={0} max={N-1} bind:value={sectionPos}
                  oninput={() => handleSectionChange(sectionAxis, sectionPos)} class="slider" />
                <span class="slider-val">{sectionPos}</span>
              </div>
            {/if}
            <div class="section-hint">{t("sidebar.section_hint")}</div>
          </div>
        {/if}

        <button class="btn-action" onclick={() => showRules = !showRules}>
          <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
          {t("rules.title")}
        </button>
      </aside>
    </div>
  {/if}

  <!-- 规则面板 -->
  {#if showRules}
    <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions -->
    <div class="rules-overlay" role="dialog" tabindex="-1"
      onclick={() => showRules = false} onkeydown={(e) => { if (e.key === 'Escape') showRules = false; }}>
      <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions a11y_no_noninteractive_element_interactions -->
      <div class="rules-panel" role="document" tabindex="-1"
        onclick={(e) => e.stopPropagation()} onkeydown={() => {}}>
        <div class="rules-header">
          <span>{t("rules.title")}</span>
          <button class="rules-close" onclick={() => showRules = false}>✕</button>
        </div>
        <div class="rules-body">
          <h3>{t("rules.board_title")}</h3>
          <p>{t("rules.board_desc")}</p>
          <h3>{t("rules.liberty_title")}</h3>
          <p>{t("rules.liberty_desc")}</p>
          <h3>{t("rules.inner_core_title")}</h3>
          <p>{t("rules.inner_core_desc")}</p>
          <h3>{t("rules.place_title")}</h3>
          <p>{t("rules.place_desc")}</p>
          <h3>{t("rules.shift_title")}</h3>
          <p>{t("rules.shift_desc")}</p>
          <h3>{t("rules.capture_title")}</h3>
          <p>{t("rules.capture_desc")}</p>
          <h3>{t("rules.suicide_title")}</h3>
          <p>{t("rules.suicide_desc")}</p>
          <h3>{t("rules.first_move_title")}</h3>
          <p>{t("rules.first_move_desc")}</p>
          <h3>{t("rules.superko_title")}</h3>
          <p>{t("rules.superko_desc")}</p>
          <h3>{t("rules.win_title")}</h3>
          <p>{t("rules.win_clear")}</p>
          <p>{t("rules.win_invade")}</p>
          <p>{t("rules.win_stalemate")}</p>
          <h3>{t("rules.hotkeys_title")}</h3>
          <p>{t("rules.hotkeys_desc")}</p>
        </div>
      </div>
    </div>
  {/if}

  {#if notification}
    <div class="toast">{notification}</div>
  {/if}
</main>

<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');

  /* ── 主题变量 ── */
  :global(:root), :global([data-theme="light"]) {
    user-select: none;
    -webkit-user-select: none;
    --bg-body: #FAFAF9;
    --text-primary: #44403C;
    --text-secondary: #57534E;
    --text-muted: #78716C;
    --accent: #44403C;
    --accent-text: #44403C;
    --accent-bg-light: rgba(68,64,60,0.08);
    --accent-bg-hover: rgba(68,64,60,0.12);
    --accent-bg-active: rgba(68,64,60,0.15);
    --accent-border: rgba(68,64,60,0.12);
    --hover-bg: rgba(68,64,60,0.05);
    --hover-color: #44403C;
    --sidebar-bg: rgba(255,255,255,0.7);
    --sidebar-shadow: 0 8px 32px rgba(0,0,0,0.06);
    --sidebar-scroll: rgba(0,0,0,0.08);
    --topbar-bg: rgba(255,255,255,0.6);
    --topbar-border: rgba(0,0,0,0.05);
    --btn-default: #57534E;
    --btn-hover-color: #44403C;
    --divider: rgba(0,0,0,0.06);
    --border-light: rgba(0,0,0,0.06);
    --btn-lang-bg: rgba(68,64,60,0.06);
    --btn-lang-border: rgba(68,64,60,0.1);
    --btn-lang-hover: rgba(68,64,60,0.12);
    --btn-lang-color: #44403C;
    --section-label: #78716C;
    --size-btn-color: #57534E;
    --size-btn-hover-bg: rgba(68,64,60,0.05);
    --size-btn-active-bg: rgba(68,64,60,0.1);
    --slider-label: #78716C;
    --slider-accent: #44403C;
    --slider-val: #44403C;
    --section-hint: #A8A29E;
    --move-hint-bg: transparent;
    --move-hint-text: #44403C;
    --move-hint-border: rgba(0,0,0,0.06);
    --turn-alpha: 0.3;
    --turn-border-alpha: 0.3;
    --turn-rgb-default: 250,250,249;
    --error-h2: #DC2626;
    --err-detail: #78716C;
    --retry-border: rgba(68,64,60,0.12);
    --retry-hover-bg: rgba(68,64,60,0.1);
    --toast-bg: #ffffff;
    --toast-border: #44403C;
    --toast-color: #44403C;
    --toast-shadow: 0 8px 32px rgba(0,0,0,0.10);
    --selection-bg: rgba(68,64,60,0.12);
    --overlay-bg: rgba(0,0,0,0.3);
    --spinner-border: rgba(68,64,60,0.15);
    --spinner-top: #44403C;
    --logo-grad1: #44403C;
    --logo-grad2: #78716C;
    --body-grad1: rgba(68,64,60,0.04);
    --body-grad2: rgba(120,113,108,0.02);
    --gameinfo-title: #44403C;
    --gameinfo-title-border: rgba(68,64,60,0.12);
    --gameinfo-label: #78716C;
    --gameinfo-value: #44403C;
    --gameinfo-core-label-text: #57534E;
    --gameinfo-sep: #D6D3D1;
    --gameinfo-hover-text: #44403C;
    --gameinfo-terminal: #44403C;
    --gameinfo-divider: rgba(0,0,0,0.06);
    --modal-bg: #ffffff;
    --modal-border: rgba(0,0,0,0.06);
    --modal-header-text: #44403C;
    --modal-close: #A8A29E;
    --modal-close-hover: #44403C;
    --modal-body-text: #57534E;
    --modal-btn-bg: rgba(68,64,60,0.04);
    --modal-btn-color: #57534E;
    --modal-btn-border: rgba(68,64,60,0.08);
    --modal-btn-hover-bg: rgba(68,64,60,0.08);
    --modal-btn-hover-color: #44403C;
    --modal-empty: #A8A29E;
    --modal-warn: #DC2626;
    --btn-new-bg: rgba(68,64,60,0.04);
    --btn-new-color: #57534E;
    --btn-new-border: rgba(68,64,60,0.08);
    --btn-new-hover-bg: rgba(68,64,60,0.08);
    --btn-new-hover-color: #44403C;
    --rules-bg: #ffffff;
    --rules-border: rgba(0,0,0,0.06);
    --rules-header-text: #44403C;
    --rules-close: #A8A29E;
    --rules-close-hover: #44403C;
    --rules-body-text: #57534E;
    --rules-h3: #44403C;
    --rules-code-bg: rgba(68,64,60,0.06);
    --rules-code-text: #44403C;
    --room-code-input-bg: rgba(68,64,60,0.04);
    --room-code-input-border: rgba(68,64,60,0.1);
    --room-code-input-color: #44403C;
    --room-status-text: #57534E;
  }
  :global([data-theme="dark"]) {
    --bg-body: #0B0F19;
    --text-primary: #94A3B8;
    --text-secondary: #94a3b8;
    --text-muted: #64748b;
    --accent: #38BDF8;
    --accent-text: #38BDF8;
    --accent-bg-light: rgba(56,189,248,0.1);
    --accent-bg-hover: rgba(56,189,248,0.18);
    --accent-bg-active: rgba(56,189,248,0.12);
    --accent-border: rgba(56,189,248,0.15);
    --hover-bg: rgba(255,255,255,0.06);
    --hover-color: #e2e8f0;
    --sidebar-bg: rgba(11,15,25,0.85);
    --sidebar-shadow: 0 8px 32px rgba(0,0,0,0.3);
    --sidebar-scroll: rgba(0,0,0,0.08);
    --topbar-bg: rgba(255,255,255,0.04);
    --topbar-border: rgba(255,255,255,0.06);
    --btn-default: #94a3b8;
    --btn-hover-color: #e2e8f0;
    --divider: rgba(255,255,255,0.06);
    --border-light: rgba(255,255,255,0.06);
    --btn-lang-bg: rgba(255,255,255,0.06);
    --btn-lang-border: rgba(255,255,255,0.1);
    --btn-lang-hover: rgba(255,255,255,0.12);
    --btn-lang-color: #94a3b8;
    --section-label: #64748b;
    --size-btn-color: #94a3b8;
    --size-btn-hover-bg: rgba(255,255,255,0.06);
    --size-btn-active-bg: rgba(56,189,248,0.12);
    --slider-label: #94a3b8;
    --slider-accent: #38BDF8;
    --slider-val: #38BDF8;
    --section-hint: #475569;
    --move-hint-bg: transparent;
    --move-hint-text: #38BDF8;
    --move-hint-border: rgba(255,255,255,0.06);
    --turn-alpha: 0.3;
    --turn-border-alpha: 0.3;
    --turn-rgb-default: 11,15,25;
    --error-h2: #f87171;
    --err-detail: #64748b;
    --retry-border: rgba(56,189,248,0.15);
    --retry-hover-bg: rgba(56,189,248,0.14);
    --toast-bg: #1e293b;
    --toast-border: #38BDF8;
    --toast-color: #94A3B8;
    --toast-shadow: 0 8px 32px rgba(0,0,0,0.3);
    --selection-bg: rgba(56,189,248,0.15);
    --overlay-bg: rgba(0,0,0,0.5);
    --spinner-border: rgba(56,189,248,0.15);
    --spinner-top: #38BDF8;
    --logo-grad1: #38BDF8;
    --logo-grad2: #a78bfa;
    --body-grad1: rgba(30,41,59,0.4);
    --body-grad2: transparent;
    --gameinfo-title: #f1f5f9;
    --gameinfo-title-border: rgba(255,255,255,0.08);
    --gameinfo-label: #64748b;
    --gameinfo-value: #94A3B8;
    --gameinfo-core-label-text: #94A3B8;
    --gameinfo-sep: #475569;
    --gameinfo-hover-text: #38BDF8;
    --gameinfo-terminal: #38BDF8;
    --gameinfo-divider: rgba(255,255,255,0.06);
    --modal-bg: #1e293b;
    --modal-border: rgba(255,255,255,0.06);
    --modal-header-text: #94A3B8;
    --modal-close: #64748b;
    --modal-close-hover: #e2e8f0;
    --modal-body-text: #94A3B8;
    --modal-btn-bg: rgba(255,255,255,0.04);
    --modal-btn-color: #94a3b8;
    --modal-btn-border: rgba(255,255,255,0.06);
    --modal-btn-hover-bg: rgba(255,255,255,0.08);
    --modal-btn-hover-color: #e2e8f0;
    --modal-empty: #64748b;
    --modal-warn: #f87171;
    --btn-new-bg: rgba(255,255,255,0.04);
    --btn-new-color: #94a3b8;
    --btn-new-border: rgba(255,255,255,0.06);
    --btn-new-hover-bg: rgba(255,255,255,0.08);
    --btn-new-hover-color: #e2e8f0;
    --rules-bg: #1e293b;
    --rules-border: rgba(255,255,255,0.06);
    --rules-header-text: #94A3B8;
    --rules-close: #64748b;
    --rules-close-hover: #e2e8f0;
    --rules-body-text: #94A3B8;
    --rules-h3: #38BDF8;
    --rules-code-bg: rgba(56,189,248,0.1);
    --rules-code-text: #38BDF8;
    --room-code-input-bg: rgba(255,255,255,0.06);
    --room-code-input-border: rgba(255,255,255,0.1);
    --room-code-input-color: #94a3b8;
    --room-status-text: #94A3B8;
  }

  :global(*) { margin: 0; padding: 0; box-sizing: border-box; }
  :global(body) {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: var(--bg-body);
    color: var(--text-primary);
    overflow: hidden;
    -webkit-font-smoothing: antialiased;
  }
  :global(body::before) {
    content: '';
    position: fixed; inset: 0;
    background:
      radial-gradient(ellipse 60% 50% at 20% 10%, var(--body-grad1) 0%, transparent 100%),
      radial-gradient(ellipse 50% 40% at 80% 90%, var(--body-grad2) 0%, transparent 100%);
    pointer-events: none; z-index: -1;
  }

  main { width: 100vw; height: 100vh; position: relative; overflow: hidden; }

  /* ── 顶部栏 ── */
  .topbar {
    display: flex; align-items: center; justify-content: space-between;
    height: 56px; padding: 0 24px;
    background: var(--topbar-bg);
    border-bottom: 1px solid var(--topbar-border);
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    z-index: 100;
  }
  .topbar-left { display: flex; align-items: center; gap: 12px; }
  .logo-icon { width: 28px; height: 28px; flex-shrink: 0; }
  .topbar-title {
    font-size: 1.05rem; font-weight: 700;
    background: linear-gradient(135deg, var(--logo-grad1), var(--logo-grad2));
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .topbar-subtitle {
    font-size: 0.78rem; color: var(--text-secondary);
    letter-spacing: 0.02em;
  }
  .topbar-right { display: flex; align-items: center; gap: 8px; }
  .btn-lang {
    all: unset;
    padding: 4px 10px;
    border-radius: 6px;
    font-size: 0.78rem;
    font-weight: 600;
    cursor: pointer;
    color: var(--btn-lang-color);
    background: var(--btn-lang-bg);
    border: 1px solid var(--btn-lang-border);
    transition: all 0.15s;
  }
  .btn-lang:hover { background: var(--btn-lang-hover); }

  /* ── 布局 ── */
  .game-layout {
    display: flex; width: 100%;
    height: calc(100vh - 56px);
  }
  .board-area { flex: 1; height: 100%; min-width: 0; }

  .sidebar {
    width: 260px; margin: 12px 12px 12px 0;
    padding: 18px 16px 14px;
    display: flex; flex-direction: column;
    gap: 4px;
    background:
      linear-gradient(180deg, rgba(var(--turn-rgb, var(--turn-rgb-default)), var(--turn-alpha, 0.03)), transparent 40%),
      var(--sidebar-bg);
    border: 1px solid var(--border-light);
    border-radius: 16px;
    overflow-y: auto; overflow-x: hidden;
    height: calc(100% - 24px);
    align-self: flex-start;
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    box-shadow: var(--sidebar-shadow);
  }
  .sidebar::-webkit-scrollbar { width: 3px; }
  .sidebar::-webkit-scrollbar-thumb { background: var(--sidebar-scroll); border-radius: 3px; }

  .btn-action {
    all: unset; display: flex; align-items: center; justify-content: center;
    gap: 6px; width: 100%; padding: 10px 0;
    border-radius: 8px; font-size: 0.88rem;
    cursor: pointer; color: var(--btn-default);
    transition: all 0.15s; text-align: center;
  }
  .btn-action:hover { color: var(--btn-hover-color); background: var(--hover-bg); }
  .btn-action + .btn-action { margin-top: 8px; }
  .btn-action + .section-group { margin-top: 6px; }
  .section-group + .section-group { margin-top: 6px; }
  .btn-action:disabled { opacity: 0.2; cursor: default; background: none; }
  .btn-action.primary {
    color: var(--accent-text); font-weight: 600;
    background: var(--accent-bg-light);
    border: 1px solid var(--accent-border);
  }
  .btn-action.primary:hover { background: var(--accent-bg-hover); }
  .btn-icon { opacity: 0.7; flex-shrink: 0; }

  /* ── 分组 ── */
  .section-group {
    display: flex; flex-direction: column;
    gap: 10px; padding-top: 14px;
    border-top: 1px solid var(--border-light);
  }
  .section-label {
    font-size: 0.72rem; color: var(--section-label);
    text-transform: uppercase; letter-spacing: 0.08em; font-weight: 600;
  }
  .size-buttons { display: flex; gap: 5px; }
  .size-btn {
    all: unset; flex: 1; padding: 8px 0;
    border-radius: 6px; font-size: 0.85rem;
    text-align: center; cursor: pointer;
    color: var(--size-btn-color); transition: all 0.12s;
    display: flex; align-items: center; justify-content: center; gap: 2px;
  }
  .size-btn:hover { color: var(--btn-hover-color); background: var(--size-btn-hover-bg); }
  .size-btn.active { color: var(--accent-text); background: var(--size-btn-active-bg); font-weight: 700; box-shadow: inset 0 0 0 1px var(--accent-border); }
  .size-btn.active .check { opacity: 1; }
  .check { opacity: 0; width: 10px; height: 10px; }

  .section-slider { display: flex; align-items: center; gap: 6px; }
  .slider-label { font-size: 0.8rem; color: var(--slider-label); min-width: 1.2rem; }
  .slider { flex: 1; accent-color: var(--slider-accent); height: 3px; }
  .slider-val { font-size: 0.82rem; color: var(--slider-val); min-width: 1.2rem; text-align: right; font-variant-numeric: tabular-nums; font-weight: 600; }
  .section-hint { font-size: 0.65rem; color: var(--section-hint); text-align: center; }

  /* ── 加载/错误 ── */
  .loading, .error {
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    height: 100vh; gap: 1rem; padding: 2rem; text-align: center;
  }
  .spinner {
    width: 28px; height: 28px;
    border: 2px solid var(--spinner-border);
    border-top-color: var(--spinner-top);
    border-radius: 50%; animation: spin 0.7s linear infinite;
  }
  @keyframes spin { to { transform: rotate(360deg); } }
  .error h2 { font-weight: 600; font-size: 1.1rem; color: var(--error-h2); }
  .err-detail { color: var(--err-detail); font-size: 0.82rem; max-width: 380px; line-height: 1.6; white-space: pre-line; }
  .btn-retry {
    background: var(--accent-bg-light); color: var(--accent-text);
    border: 1px solid var(--accent-border);
    border-radius: 8px; padding: 0.6rem 2.2rem;
    font-size: 0.9rem; cursor: pointer; transition: background 0.15s;
  }
  .btn-retry:hover { background: var(--accent-bg-hover); }

  /* ── 通知 ── */
  .toast {
    position: fixed; bottom: 32px; left: 50%; transform: translateX(-50%);
    background: var(--toast-bg);
    border: 1px solid var(--toast-border);
    color: var(--toast-color);
    padding: 10px 24px;
    border-radius: 10px;
    font-size: 0.85rem;
    pointer-events: none;
    z-index: 600;
    box-shadow: var(--toast-shadow);
    backdrop-filter: blur(8px);
    -webkit-backdrop-filter: blur(8px);
  }

  /* ── 规则面板 ── */
  .rules-overlay {
    position: fixed; inset: 0;
    background: var(--overlay-bg);
    display: flex; align-items: center; justify-content: center;
    z-index: 500;
  }
  .rules-panel {
    background: var(--rules-bg);
    border: 1px solid var(--rules-border);
    border-radius: 12px;
    max-width: 600px; max-height: 80vh;
    display: flex; flex-direction: column;
    overflow: hidden;
    box-shadow: 0 16px 48px rgba(0,0,0,0.3);
  }
  .rules-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 1rem 1.2rem;
    border-bottom: 1px solid var(--divider);
    color: var(--rules-header-text); font-weight: 600;
  }
  .rules-close {
    background: none; border: none;
    color: var(--rules-close); font-size: 1.2rem; cursor: pointer;
  }
  .rules-close:hover { color: var(--rules-close-hover); }
  .rules-body {
    padding: 1rem 1.2rem; overflow-y: auto;
    display: flex; flex-direction: column; gap: 0.6rem;
  }
  .rules-body p { color: var(--rules-body-text); font-size: 0.85rem; line-height: 1.6; }
  .rules-body h3 { color: var(--rules-h3); font-size: 0.9rem; margin-top: 0.4rem; }

  /* ── 房间 ── */
  .room-status {
    display: flex; flex-direction: column;
    align-items: center; gap: 12px;
    padding: 16px 0;
    text-align: center;
    color: var(--room-status-text);
  }
  .room-status p { font-size: 0.85rem; }
  .room-code-display {
    font-size: 1.4rem; font-weight: 700;
    letter-spacing: 0.15em;
    padding: 8px 16px;
    border-radius: 8px;
    background: var(--accent-bg-light);
    border: 1px solid var(--accent-border);
    color: var(--accent-text);
    font-family: 'Courier New', monospace;
    margin-top: 4px;
  }
  .room-code-input {
    all: unset;
    box-sizing: border-box;
    width: 100%; padding: 10px 12px;
    border-radius: 8px;
    font-size: 0.9rem;
    text-align: center;
    letter-spacing: 0.1em;
    background: var(--room-code-input-bg);
    border: 1px solid var(--room-code-input-border);
    color: var(--room-code-input-color);
  }
  .room-code-input::placeholder {
    color: var(--text-muted);
    letter-spacing: 0;
  }
</style>

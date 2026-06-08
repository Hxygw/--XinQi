<script lang="ts">
  /**
   * 芯棋前端 - 状态中心。
   */

  import { onMount } from "svelte";
  import Board3D from "./components/Board3D.svelte";
  import GameInfo from "./components/GameInfo.svelte";
  import Modal from "./components/Modal.svelte";
  import { apiClient, aiClient } from "./lib/api";
  import { LegalityChecker, boardHash } from "./lib/legality";
  import {
    findAllInnerCores, buildInnerCoreSet, getNeighbors6, to3D, to1D,
  } from "./lib/boardUtils";
  import { t, setLang, getLang } from "./lib/i18n.svelte";
  import type { Lang } from "./lib/i18n.svelte";
  import { playPlace, playShift, playCapture, playVictoryIntrusion, playVictoryAnnihilation } from "./lib/sound";

  // ─ 游戏状态 ──
  let status = $state("loading");
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

  // ── AI 模式 ──
  type GameMode = "pvp" | "ai_black" | "ai_white" | "ai_both";
  let gameMode = $state<GameMode>("pvp");
  let gameStarted = $state(false);
  let aiBusy = $state(false);
  let aiStepMode = $state(false); // AI vs AI 时：true=单步, false=自动

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

  // 保存状态追踪
  let savedAfterLastMove = $state(false);

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

  // 棋谱
  let browseMode = $state(false);
  let recordList = $state<string[]>([]);
  let currentRecord = $state<any>(null);
  let browseStep = $state(0);
  let browseBusy = $state(false);
  let replaySteps = $state<any[] | null>(null);
  let browseTick = $state(0);

  // 弹窗
  let showModal = $state<'none' | 'records' | 'confirm' | 'new_game'>('none');
  let confirmTarget = $state('');

  // 规则面板
  let showRules = $state(false);

  // 剖面
  let sectionAxis = $state<string | null>(null);
  let sectionPos = $state(0);

  // 记录计数
  let recordCount = $state(0);

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
    initGame();
  });

  async function initGame() {
    status = "loading";
    try {
      await apiClient.newGame(N);
      await syncState();
      status = "ready";
    } catch (e) {
      status = "error";
      errorMsg = `${(e as Error).message}\n\n${t("error.cannot_connect")}`;
    }
  }

  function refreshInnerCores() {
    innerCores = findAllInnerCores(board, N);
    innerCoreSet = buildInnerCoreSet([...innerCores.black, ...innerCores.white]);
  }

  async function syncState() {
    try {
      const gs = await apiClient.getState();
      N = gs.board_size;
      board = new Uint8Array(gs.board);
      savedAfterLastMove = false;
      currentPlayer = gs.current_player as "Black" | "White";
      moveCount = gs.move_count;
      terminal = gs.terminal;
      winner = gs.winner;
      const newOwners = new Map<number, "Black" | "White">();
      for (const [idxStr, owner] of Object.entries(gs.vacancy_owners)) {
        newOwners.set(parseInt(idxStr), owner as "Black" | "White");
      }
      vacancyOwners = newOwners;
      if (checker.N !== N) checker.reinit(N);
      refreshInnerCores();
    } catch (e) {
      status = "error";
      errorMsg = `${t("error.sync_failed")}: ${(e as Error).message}`;
    }
  }

  async function handleNewGame() {
    historyHashes = new Set();
    moveMode = false;
    moveSourceIdx = -1;
    moveBlockIndices = new Set();
    validTargets = [];
    validTargetHover = null;
    hoverIdx = -1;
    hoverInfo = "";
    try {
      await apiClient.newGame(N);
      await syncState();
      showNotification(t("notif.game_started"));
    } catch (e) {
      showNotification(`${t("error.operation_failed")}: ${(e as Error).message}`, 5000);
    }
  }

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
    try {
      const result = await apiClient.play(pt.x, pt.y, pt.z);
      if (result.legal) {
        playMoveSound(result.terminal, result.result_code, result.captured_count > 0, false);
        await syncState();
        if (result.terminal) showNotification(t("notif.game_over"), 3000);
        else if (!terminal && isAITurn()) { await new Promise(r => setTimeout(r, 300)); doAIMove(); }
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
    const src = to3D(moveSourceIdx, N);
    try {
      const result = await apiClient.moveStone(src.x, src.y, src.z, pt.x, pt.y, pt.z);
      if (result.legal) {
        playMoveSound(result.terminal, result.result_code, result.captured_count > 0, true);
        await syncState();
        exitMoveMode();
        if (result.terminal) showNotification(t("notif.game_over"), 3000);
        else if (!terminal && isAITurn()) { await new Promise(r => setTimeout(r, 300)); doAIMove(); }
      } else {
        showNotification(`${t("notif.shift_failed")}: ${result.error || ""}`, 2000);
      }
    } catch (e) {
      showNotification(`${t("notif.shift_failed")}: ${(e as Error).message}`, 5000);
    }
  }

  function handleCellClick(pt: { x: number; y: number; z: number }) {
    if (!gameStarted) { startGame(); return; }
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
    if (!gameStarted) { startGame(); return; }
    if (moveMode) exitMoveMode();
  }

  const axisNames = [t("section.x"), t("section.y"), t("section.z")];

  function handleHover(pt: { x: number; y: number; z: number }) {
    if (!gameStarted || browseMode) {
      hoverIdx = -1; hoverInfo = ""; hoverLegal = false;
      hoverFatalAxes = undefined; hoverIsInnerCore = false;
      validTargetHover = null;
      return;
    }
    const idx = to1D(pt.x, pt.y, pt.z, N);
    hoverIdx = idx;
    const cellVal = board[idx];
    const owner = vacancyOwners.get(idx);
    const isOwnVacancy = vacancySet.has(idx) && owner === currentPlayer;

    // 对方内芯空位 → 直接跳合法性检测
    if (vacancySet.has(idx) && owner !== currentPlayer) {
      const player = currentPlayer === "Black" ? 1 : 2;
      // 把对方空位当作空格传给检测器，否则 checkMove 会返回 occupied
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

  async function handleSaveRecord() {
    try {
      await apiClient.saveRecord();
      const files = await apiClient.listRecords();
      recordCount = files.length;
      savedAfterLastMove = true;
      showNotification(`${t("notif.saved")} (${recordCount})`, 2000);
    } catch (e) {
      showNotification(`${t("error.save_failed")}: ${(e as Error).message}`, 3000);
    }
  }

  async function showRecordPicker() {
    try {
      recordList = await apiClient.listRecords();
      recordList.sort().reverse();
      showModal = 'records';
    } catch { showModal = 'none'; }
  }

  function exitBrowse() {
    browseMode = false; currentRecord = null; replaySteps = null;
    moveMode = false; moveSourceIdx = -1; moveBlockIndices = new Set(); validTargets = []; validTargetHover = null;
    const total = N * N * N;
    board = new Uint8Array(total);
    currentPlayer = "Black"; moveCount = 0; terminal = false; winner = undefined;
    vacancyOwners = new Map(); historyHashes = new Set();
    refreshInnerCores(); hoverInfo = "";
  }

  function openConfirm(filename: string) { confirmTarget = filename; showModal = 'confirm'; }

  async function doLoadRecord() {
    if (!confirmTarget) return;
    showModal = 'none';
    const filename = confirmTarget; confirmTarget = ''; browseBusy = true; replaySteps = null;
    try {
      const rec = await apiClient.getRecord(filename);
      currentRecord = rec;
      // 切换棋盘到棋谱的大小
      if (rec.board_size !== N) {
        N = rec.board_size;
        checker.reinit(N);
        board = new Uint8Array(N * N * N);
      }
      const result = await apiClient.replayRecord(rec);
      if (result.broken) showNotification(t("modal.warn"), 3000);
      browseMode = true; replaySteps = result.steps; browseStep = 0; applyBrowseStep(0);
    } catch (e) {
      showNotification(`${t("error.load_failed")}: ${(e as Error).message}`, 3000); currentRecord = null;
    } finally { browseBusy = false; }
  }

  function applyBrowseStep(step: number) {
    if (!replaySteps || step < 0 || step >= replaySteps.length) return;
    const s = replaySteps[step];
    board = new Uint8Array(s.board);
    currentPlayer = s.current_player;
    moveCount = s.move_count;
    terminal = step >= replaySteps.length - 1;
    if (terminal && currentRecord) {
      const w = currentRecord.winner;
      winner = w === "Black" ? "Black" : w === "White" ? "White" : "Draw";
    } else { winner = undefined; }
    // 从棋盘数据还原内芯空位所有者
    const owners = new Map<number, "Black" | "White">();
    for (let i = 0; i < s.board.length; i++) {
      if (s.board[i] === 3) owners.set(i, "Black");
      else if (s.board[i] === 4) owners.set(i, "White");
    }
    vacancyOwners = owners;
    refreshInnerCores(); browseStep = step; browseTick++;
  }

  function browseGo(step: number) {
    if (!replaySteps) return;
    applyBrowseStep(Math.max(0, Math.min(step, replaySteps.length - 1)));
  }

  /** 音效优先级：终局 > 提子 > 落子/挪子 */
  function playMoveSound(terminal: boolean, resultCode: number | undefined, captured: boolean, isShift: boolean) {
    if (terminal) {
      // WIN_CORE_INVASION=2 → 侵入音效；WIN_CLEAR_BOARD=1 / WIN_NO_LEGAL_MOVE=3 → 碾压音效
      if (resultCode === 2) playVictoryIntrusion(); else playVictoryAnnihilation();
    } else if (captured) playCapture();
    else if (isShift) playShift(); else playPlace();
  }

  function browseNext() {
    if (!replaySteps || browseStep >= replaySteps.length - 1) return;
    const newStep = browseStep + 1;
    const s = replaySteps[newStep];
    const isLast = newStep >= replaySteps.length - 1;
    const hasWinner = currentRecord && currentRecord.winner !== "None";
    // 检测提子：比较两步间对方棋子数量
    let captured = false;
    const prev = replaySteps[browseStep];
    if (prev && s.board && prev.board) {
      const enemyColor = s.current_player === "Black" ? 1 : 2;
      let before = 0, after = 0;
      for (let i = 0; i < s.board.length; i++) {
        if (prev.board[i] === enemyColor) before++;
        if (s.board[i] === enemyColor) after++;
      }
      captured = after < before;
    }
    // 音效优先级：终局 > 提子 > 落子/挪子
    playMoveSound(isLast && !!hasWinner, s.result_code, captured, !!s.last_move_is_move);
    applyBrowseStep(newStep);
  }

  function handleSectionChange(axis: string | null, pos: number) {
    sectionAxis = axis; sectionPos = pos;
    showNotification(axis ? `${t("notif.section_view")} ${axis.toUpperCase()} = ${pos}` : t("notif.full_view"), 1500);
  }

  async function changeBoardSize(newN: number) {
    N = newN; sectionAxis = null; checker.reinit(N);
    board = new Uint8Array(N * N * N);
    try { await apiClient.newGame(N); await syncState(); }
    catch (e) { showNotification(`${t("error.create_failed")}: ${(e as Error).message}`, 3000); }
  }

  // ── 射线检测开关 ──
  let raycastEnabled = $derived(
    !browseMode && (
      !gameStarted || (!terminal && !aiBusy &&
      !(gameMode === "ai_black" && currentPlayer === "Black") &&
      !(gameMode === "ai_white" && currentPlayer === "White"))
    )
  );

  function isAITurn(): boolean {
    if (!gameStarted || terminal || aiBusy) return false;
    if (gameMode === "ai_both") return true;
    if (gameMode === "ai_black" && currentPlayer === "Black") return true;
    if (gameMode === "ai_white" && currentPlayer === "White") return true;
    return false;
  }

  async function doAIMove() {
    if (terminal || aiBusy) return;
    aiBusy = true;
    try {
      const r = await aiClient.move(2000);
      if (r.error) { showNotification(`${t("error.ai_error")}: ${r.error}`, 3000); return; }
      const pr = r.type === 0
        ? await apiClient.play(r.x, r.y, r.z)
        : await apiClient.moveStone(r.x, r.y, r.z, r.target_x!, r.target_y!, r.target_z!);
      if (pr?.legal) {
        playMoveSound(!!pr.terminal, pr.result_code, (pr.captured_count ?? 0) > 0, r.type !== 0);
        await syncState();
        showNotification(`AI (${currentPlayer === "Black" ? t("player.black_short") : t("player.white_short")})`, 1500);
        if (!terminal && gameMode === "ai_both" && !aiStepMode) {
          await new Promise(r => setTimeout(r, 500));
          await doAIMove();
        }
      }
    } catch (e) { showNotification(`${t("error.ai_failed")}: ${(e as Error).message}`, 5000); }
    finally { aiBusy = false; }
  }

  async function stepAI() { if (!aiBusy && isAITurn()) await doAIMove(); }
  function toggleStepMode() { aiStepMode = !aiStepMode; showNotification(aiStepMode ? t("ai.step_mode") : t("ai.auto_mode"), 1200); }

  async function startGame() {
    gameStarted = true; moveMode = false; moveSourceIdx = -1; moveBlockIndices = new Set(); validTargets = []; validTargetHover = null; historyHashes = new Set();
    try {
      await apiClient.newGame(N); await syncState();
      showNotification(t("notif.game_start"));
      if (isAITurn()) { await new Promise(r => setTimeout(r, 300)); await doAIMove(); }
    } catch (e) { showNotification(`${t("error.create_failed")}: ${(e as Error).message}`, 5000); gameStarted = false; }
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
      <button class="btn-retry" onclick={initGame}>{t("error.retry")}</button>
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
          hoverResult={boardHoverResult} {validTargetHover} {browseTick}
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
            innerCoreBlack={innerCoreCountBlack} innerCoreWhite={innerCoreCountWhite}
            {terminal} {winner}
            stonePrimaryHex={stonePrimaryHex} stoneSecondaryHex={stoneSecondaryHex}
          />

          {#if !gameStarted}
            {#if browseMode}
              <button class="btn-action primary" onclick={exitBrowse}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/></svg>
                {t("sidebar.exit_browse")}
              </button>
              <button class="btn-action primary" onclick={showRecordPicker}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>
                {t("sidebar.switch_record")}
              </button>

              <!-- 浏览模式下的剖面选择 -->
              <div class="section-group" style="margin-top:12px">
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
            {:else}
              <button class="btn-action primary" onclick={startGame}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>
                {t("sidebar.start_game")}
              </button>

              <button class="btn-action primary" onclick={showRecordPicker}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>
                {t("sidebar.browse_records")}
              </button>

              <!-- 模式选择 -->
              <div class="section-group">
                <div class="section-label">{t("mode.label")}</div>
                <div class="size-buttons">
                  <button class="size-btn" class:active={gameMode === 'pvp'} onclick={() => gameMode = 'pvp'}>{t("mode.pvp")}</button>
                  <button class="size-btn" class:active={gameMode === 'ai_black'} onclick={() => gameMode = 'ai_black'}>{t("mode.ai_black")}</button>
                  <button class="size-btn" class:active={gameMode === 'ai_white'} onclick={() => gameMode = 'ai_white'}>{t("mode.ai_white")}</button>
                  <button class="size-btn" class:active={gameMode === 'ai_both'} onclick={() => gameMode = 'ai_both'}>{t("mode.ai_both")}</button>
                </div>
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

          {:else}
            <button class="btn-action primary" onclick={() => showModal = 'new_game'}>
              <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/></svg>
              {t("sidebar.new_game")}
            </button>

            <!-- 游戏中 -->
            {#if !browseMode}
              <button class="btn-action primary" onclick={handleSaveRecord} disabled={moveCount === 0 || savedAfterLastMove}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
                {t("sidebar.save_record")}
              </button>
            {/if}

            {#if aiBusy}
              <div class="move-mode-hint">{t("ai.thinking")}</div>
            {:else if gameMode === "ai_both"}
              <div class="section-group" style="border-top: none; padding-top: 0;">
                <div class="size-buttons">
                  <button class="size-btn" class:active={!aiStepMode} onclick={() => { aiStepMode = false; showNotification(t("ai.auto_mode"), 1000); if (isAITurn()) doAIMove(); }}>{t("ai.auto")}</button>
                  <button class="size-btn" class:active={aiStepMode} onclick={toggleStepMode}>{t("ai.step")}</button>
                </div>
                {#if aiStepMode}
                  <button class="btn-action primary" onclick={stepAI} style="margin-top:4px">{t("ai.next_step")}</button>
                {/if}
              </div>
            {/if}

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

  <Modal
    show={showModal !== 'none'} mode={showModal}
    {recordList} {confirmTarget} {browseBusy}
    onClose={() => { showModal = 'none'; }}
    onRefresh={async () => {
      try { recordList = await apiClient.listRecords(); recordList.sort().reverse(); } catch {}
    }}
    onSelect={openConfirm}
    onConfirm={() => {
      if (showModal === 'new_game') {
        gameStarted = false; showModal = 'none';
        const total = N * N * N;
        board = new Uint8Array(total);
        currentPlayer = "Black";
        moveCount = 0; terminal = false; winner = undefined;
        vacancyOwners = new Map();
        historyHashes = new Set();
        refreshInnerCores();
        moveMode = false; moveSourceIdx = -1; moveBlockIndices = new Set(); validTargets = []; validTargetHover = null;
      } else { doLoadRecord(); }
    }}
    onCancel={() => { showModal = 'none'; }}
  />

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

  {#if browseMode && replaySteps}
    <div class="browse-bar">
      <div class="browse-bar-inner">
        <span class="browse-label">{t("browse.mode")} {browseStep}/{replaySteps.length - 1}</span>
        <div class="browse-controls">
          <button class="browse-btn" onclick={() => browseGo(0)} disabled={browseStep===0}>|◁</button>
          <button class="browse-btn" onclick={() => browseGo(browseStep-1)} disabled={browseStep===0}>◁</button>
          <input type="range" min={0} max={replaySteps.length-1}
            value={browseStep} oninput={(e) => browseGo(parseInt((e.target as HTMLInputElement).value))}
            class="browse-slider" />
          <button class="browse-btn" onclick={browseNext} disabled={browseStep>=replaySteps.length-1}>▷</button>
          <button class="browse-btn" onclick={() => browseGo(replaySteps.length-1)} disabled={browseStep>=replaySteps.length-1}>▷|</button>
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
    --browse-bar-bg: rgba(255,255,255,0.85);
    --browse-bar-border: rgba(0,0,0,0.06);
    --browse-label: #44403C;
    --browse-btn-color: #57534E;
    --browse-btn-hover-bg: rgba(68,64,60,0.05);
    --browse-btn-hover-color: #44403C;
    --browse-slider-accent: #44403C;
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
    --browse-bar-bg: rgba(11,15,25,0.9);
    --browse-bar-border: rgba(255,255,255,0.06);
    --browse-label: #38BDF8;
    --browse-btn-color: #94a3b8;
    --browse-btn-hover-bg: rgba(255,255,255,0.06);
    --browse-btn-hover-color: #e2e8f0;
    --browse-slider-accent: #38BDF8;
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
    border-radius: 8px; padding: 8px 20px;
    font-size: 0.85rem; cursor: pointer; transition: all 0.2s;
  }
  .btn-retry:hover { background: var(--retry-hover-bg); }

  /* ── 浏览栏 ── */
  .browse-bar {
    position: fixed; bottom: 0; left: 0; right: 0;
    background: var(--browse-bar-bg);
    border-top: 1px solid var(--browse-bar-border);
    z-index: 200; padding: 8px 16px;
    backdrop-filter: blur(12px);
  }
  .browse-bar-inner { display: flex; align-items: center; justify-content: center; gap: 8px; max-width: 520px; margin: 0 auto; }
  .browse-label { color: var(--browse-label); font-size: 0.8rem; white-space: nowrap; font-weight: 500; }
  .browse-controls { display: flex; align-items: center; gap: 4px; flex: 1; }
  .browse-btn {
    all: unset; padding: 4px 8px; border-radius: 4px;
    font-size: 0.8rem; cursor: pointer; color: var(--browse-btn-color); transition: all 0.12s;
  }
  .browse-btn:hover { color: var(--browse-btn-hover-color); background: var(--browse-btn-hover-bg); }
  .browse-btn:disabled { opacity: 0.15; cursor: default; background: none; }
  .browse-slider { flex: 1; accent-color: var(--browse-slider-accent); height: 3px; max-width: 260px; }

  /* ── Toast ─ */
  .toast {
    position: fixed; top: 72px; left: 50%; transform: translateX(-50%);
    background: var(--toast-bg);
    border-left: 3px solid var(--toast-border);
    border-top: 1px solid var(--accent-border);
    border-right: 1px solid var(--accent-border);
    border-bottom: 1px solid var(--accent-border);
    border-radius: 8px;
    padding: 10px 18px;
    color: var(--toast-color); font-size: 0.88rem; font-weight: 500;
    z-index: 1000;
    pointer-events: none;
    box-shadow: var(--toast-shadow);
    animation: toastIn 0.25s ease;
  }
  @keyframes toastIn {
    from { opacity: 0; transform: translateX(-50%) translateY(-6px) scale(0.96); }
    to { opacity: 1; transform: translateX(-50%) translateY(0) scale(1); }
  }

  :global(::selection) { background: var(--selection-bg); }

  /* ── 规则面板 ─ */
  .rules-overlay {
    position: fixed; inset: 0;
    background: var(--overlay-bg);
    display: flex; align-items: center; justify-content: center;
    z-index: 500;
  }
  .rules-panel {
    background: var(--rules-bg);
    border: 1px solid var(--rules-border);
    border-radius: 14px;
    width: 440px; max-height: 75vh;
    display: flex; flex-direction: column;
    box-shadow: 0 16px 48px rgba(0,0,0,0.3);
  }
  .rules-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 1rem 1.2rem;
    border-bottom: 1px solid var(--divider);
    font-size: 1rem; color: var(--rules-header-text); font-weight: 600;
  }
  .rules-close {
    background: none; border: none;
    color: var(--rules-close); font-size: 1.2rem; cursor: pointer;
  }
  .rules-close:hover { color: var(--rules-close-hover); }
  .rules-body {
    padding: 1rem 1.2rem; overflow-y: auto;
    font-size: 0.85rem; line-height: 1.7;
    color: var(--rules-body-text);
    user-select: text; -webkit-user-select: text;
  }
  .rules-body::-webkit-scrollbar { width: 5px; }
  .rules-body::-webkit-scrollbar-track { background: transparent; }
  .rules-body::-webkit-scrollbar-thumb { background: var(--rules-close); border-radius: 3px; }
  .rules-body h3 {
    font-size: 0.88rem; color: var(--rules-h3);
    margin: 0.8rem 0 0.3rem;
    font-weight: 600;
  }
  .rules-body h3:first-child { margin-top: 0; }
  .rules-body p { margin: 0.2rem 0; }
  .rules-body code {
    background: var(--rules-code-bg); color: var(--rules-code-text);
    padding: 1px 6px; border-radius: 4px;
    font-size: 0.8rem;
  }
</style>

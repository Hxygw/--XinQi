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

  async function openBrowser() {
    if (browseMode) {
      browseMode = false; currentRecord = null; replaySteps = null;
      moveMode = false; moveSourceIdx = -1; moveBlockIndices = new Set(); validTargets = []; validTargetHover = null;
      // 清空棋盘
      const total = N * N * N;
      board = new Uint8Array(total);
      currentPlayer = "Black"; moveCount = 0; terminal = false; winner = undefined;
      vacancyOwners = new Map(); historyHashes = new Set();
      refreshInnerCores(); hoverInfo = "";
      return;
    }
    browseMode = true; browseStep = 0; replaySteps = null;
    try {
      recordList = await apiClient.listRecords();
      recordList.sort().reverse();
      showModal = 'records';
    } catch { showModal = 'none'; }
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
      replaySteps = result.steps; browseStep = 0; applyBrowseStep(0);
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
    refreshInnerCores(); browseStep = step; browseTick++;
  }

  function browseGo(step: number) {
    if (!replaySteps) return;
    applyBrowseStep(Math.max(0, Math.min(step, replaySteps.length - 1)));
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
        />
      </div>

      <!-- 侧栏卡片 -->
      <aside class="sidebar">
          <GameInfo
            {currentPlayer} {moveCount}
            innerCoreBlack={innerCoreCountBlack} innerCoreWhite={innerCoreCountWhite}
            {terminal} {winner} {hoverInfo}
          />

          {#if !gameStarted}
            {#if browseMode}
              <button class="btn-action primary" onclick={openBrowser}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>
                {t("sidebar.exit_browse")}
              </button>
              <button class="btn-action primary" onclick={() => { browseMode = false; setTimeout(() => openBrowser(), 0); }}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>
                {t("sidebar.switch_record")}
              </button>
            {:else}
              <button class="btn-action primary" onclick={startGame}>
                <svg class="btn-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>
                {t("sidebar.start_game")}
              </button>

              <button class="btn-action primary" onclick={openBrowser}>
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
            {#if moveMode}
              <div class="move-mode-hint">{t("sidebar.move_mode_hint")}</div>
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
    onClose={() => { showModal = 'none'; browseMode = false; }}
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
    onCancel={() => { const wasConfirm = showModal === 'confirm'; showModal = 'none'; if (wasConfirm) browseMode = false; }}
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
          <button class="browse-btn" onclick={() => browseGo(browseStep+1)} disabled={browseStep>=replaySteps.length-1}>▷</button>
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

  :global(*) { margin: 0; padding: 0; box-sizing: border-box; }
  :global(body) {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: #f5f3ff;
    color: #1e1b4b;
    overflow: hidden;
    -webkit-font-smoothing: antialiased;
  }
  :global(body::before) {
    content: '';
    position: fixed; inset: 0;
    background:
      radial-gradient(ellipse 60% 50% at 20% 10%, rgba(124, 109, 240, 0.06) 0%, transparent 100%),
      radial-gradient(ellipse 50% 40% at 80% 90%, rgba(167, 139, 250, 0.04) 0%, transparent 100%);
    pointer-events: none; z-index: -1;
  }

  main { width: 100vw; height: 100vh; position: relative; overflow: hidden; }

  /* ── 顶部栏 ── */
  .topbar {
    display: flex; align-items: center; justify-content: space-between;
    height: 56px; padding: 0 24px;
    background: rgba(255,255,255,0.7);
    border-bottom: 1px solid rgba(124,109,240,0.08);
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    z-index: 100;
  }
  .topbar-left { display: flex; align-items: center; gap: 12px; }
  .logo-icon { width: 28px; height: 28px; flex-shrink: 0; }
  .topbar-title {
    font-size: 1.05rem; font-weight: 700;
    background: linear-gradient(135deg, #5c4fd0, #7c6df0);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .topbar-subtitle {
    font-size: 0.78rem; color: #6b7280;
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
    color: #7c6df0;
    background: rgba(124,109,240,0.1);
    border: 1px solid rgba(124,109,240,0.15);
    transition: all 0.15s;
  }
  .btn-lang:hover { background: rgba(124,109,240,0.2); }

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
    background: rgba(255,255,255,0.6);
    border: 1px solid rgba(255,255,255,0.3);
    border-radius: 16px;
    overflow-y: auto; overflow-x: hidden;
    height: calc(100% - 24px);
    align-self: flex-start;
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    box-shadow: 0 8px 32px rgba(0,0,0,0.06);
  }
  .sidebar::-webkit-scrollbar { width: 3px; }
  .sidebar::-webkit-scrollbar-thumb { background: rgba(0,0,0,0.08); border-radius: 3px; }

  .btn-action {
    all: unset; display: flex; align-items: center; justify-content: center;
    gap: 6px; width: 100%; padding: 10px 0;
    border-radius: 8px; font-size: 0.88rem;
    cursor: pointer; color: #4b5563;
    transition: all 0.15s; text-align: center;
  }
  .btn-action:hover { color: #1e1b4b; background: #f0ecff; }
  .btn-action + .btn-action { margin-top: 8px; }
  .btn-action + .section-group { margin-top: 6px; }
  .section-group + .section-group { margin-top: 6px; }
  .btn-action:disabled { opacity: 0.2; cursor: default; background: none; }
  .btn-action.primary {
    color: #7c6df0; font-weight: 600;
    background: rgba(124,109,240,0.1);
    border: 1px solid rgba(124,109,240,0.15);
  }
  .btn-action.primary:hover { background: rgba(124,109,240,0.16); }
  .btn-icon { opacity: 0.7; flex-shrink: 0; }

  /* ── 分组 ── */
  .section-group {
    display: flex; flex-direction: column;
    gap: 10px; padding-top: 14px;
    border-top: 1px solid rgba(0,0,0,0.05);
  }
  .section-label {
    font-size: 0.72rem; color: #6b7280;
    text-transform: uppercase; letter-spacing: 0.08em; font-weight: 600;
  }
  .size-buttons { display: flex; gap: 5px; }
  .size-btn {
    all: unset; flex: 1; padding: 8px 0;
    border-radius: 6px; font-size: 0.85rem;
    text-align: center; cursor: pointer;
    color: #4b5563; transition: all 0.12s;
    display: flex; align-items: center; justify-content: center; gap: 2px;
  }
  .size-btn:hover { color: #1e1b4b; background: #f0ecff; }
  .size-btn.active { color: #7c6df0; background: #ddd5ff; font-weight: 700; box-shadow: inset 0 0 0 1px rgba(124,109,240,0.2); }
  .size-btn.active .check { opacity: 1; }
  .check { opacity: 0; width: 10px; height: 10px; }

  .section-slider { display: flex; align-items: center; gap: 6px; }
  .slider-label { font-size: 0.8rem; color: #6b7280; min-width: 1.2rem; }
  .slider { flex: 1; accent-color: #7c6df0; height: 3px; }
  .slider-val { font-size: 0.82rem; color: #7c6df0; min-width: 1.2rem; text-align: right; font-variant-numeric: tabular-nums; font-weight: 600; }
  .section-hint { font-size: 0.65rem; color: #a5a5b8; text-align: center; }
  .move-mode-hint {
    color: #7c6df0; font-size: 0.82rem; padding: 12px 0;
    text-align: center; border-top: 1px solid rgba(0,0,0,0.05);
    letter-spacing: 0.02em;
  }

  /* ── 加载/错误 ── */
  .loading, .error {
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    height: 100vh; gap: 1rem; padding: 2rem; text-align: center;
  }
  .spinner {
    width: 28px; height: 28px;
    border: 2px solid rgba(124,109,240,0.15);
    border-top-color: #7c6df0;
    border-radius: 50%; animation: spin 0.7s linear infinite;
  }
  @keyframes spin { to { transform: rotate(360deg); } }
  .error h2 { font-weight: 600; font-size: 1.1rem; color: #e74c3c; }
  .err-detail { color: #9ca3af; font-size: 0.82rem; max-width: 380px; line-height: 1.6; white-space: pre-line; }
  .btn-retry {
    background: rgba(124,109,240,0.08); color: #7c6df0;
    border: 1px solid rgba(124,109,240,0.15);
    border-radius: 8px; padding: 8px 20px;
    font-size: 0.85rem; cursor: pointer; transition: all 0.2s;
  }
  .btn-retry:hover { background: rgba(124,109,240,0.14); }

  /* ── 浏览栏 ── */
  .browse-bar {
    position: fixed; bottom: 0; left: 0; right: 0;
    background: rgba(255,255,255,0.9);
    border-top: 1px solid rgba(0,0,0,0.06);
    z-index: 200; padding: 8px 16px;
    backdrop-filter: blur(12px);
  }
  .browse-bar-inner { display: flex; align-items: center; justify-content: center; gap: 8px; max-width: 520px; margin: 0 auto; }
  .browse-label { color: #7c6df0; font-size: 0.8rem; white-space: nowrap; font-weight: 500; }
  .browse-controls { display: flex; align-items: center; gap: 4px; flex: 1; }
  .browse-btn {
    all: unset; padding: 4px 8px; border-radius: 4px;
    font-size: 0.8rem; cursor: pointer; color: #6b7280; transition: all 0.12s;
  }
  .browse-btn:hover { color: #1e1b4b; background: #f5f3ff; }
  .browse-btn:disabled { opacity: 0.15; cursor: default; background: none; }
  .browse-slider { flex: 1; accent-color: #7c6df0; height: 3px; max-width: 260px; }

  /* ── Toast ─ */
  .toast {
    position: fixed; top: 72px; left: 50%; transform: translateX(-50%);
    background: #ffffff;
    border-left: 3px solid #7c6df0;
    border-top: 1px solid rgba(124,109,240,0.12);
    border-right: 1px solid rgba(124,109,240,0.12);
    border-bottom: 1px solid rgba(124,109,240,0.12);
    border-radius: 8px;
    padding: 10px 18px;
    color: #1e1b4b; font-size: 0.88rem; font-weight: 500;
    z-index: 1000;
    pointer-events: none;
    box-shadow: 0 8px 32px rgba(0,0,0,0.10);
    animation: toastIn 0.25s ease;
  }
  @keyframes toastIn {
    from { opacity: 0; transform: translateX(-50%) translateY(-6px) scale(0.96); }
    to { opacity: 1; transform: translateX(-50%) translateY(0) scale(1); }
  }

  :global(::selection) { background: rgba(124,109,240,0.15); }

  /* ── 规则面板 ─ */
  .rules-overlay {
    position: fixed; inset: 0;
    background: rgba(0,0,0,0.3);
    display: flex; align-items: center; justify-content: center;
    z-index: 500;
  }
  .rules-panel {
    background: #ffffff;
    border: 1px solid rgba(0,0,0,0.06);
    border-radius: 14px;
    width: 440px; max-height: 75vh;
    display: flex; flex-direction: column;
    box-shadow: 0 16px 48px rgba(0,0,0,0.08);
  }
  .rules-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 1rem 1.2rem;
    border-bottom: 1px solid rgba(0,0,0,0.06);
    font-size: 1rem; color: #1e1b4b; font-weight: 600;
  }
  .rules-close {
    background: none; border: none;
    color: #9ca3af; font-size: 1.2rem; cursor: pointer;
  }
  .rules-close:hover { color: #1e1b4b; }
  .rules-body {
    padding: 1rem 1.2rem; overflow-y: auto;
    font-size: 0.85rem; line-height: 1.7;
    color: #374151;
  }
  .rules-body h3 {
    font-size: 0.88rem; color: #7c6df0;
    margin: 0.8rem 0 0.3rem;
    font-weight: 600;
  }
  .rules-body h3:first-child { margin-top: 0; }
  .rules-body p { margin: 0.2rem 0; }
  .rules-body code {
    background: #f0ecff; color: #7c6df0;
    padding: 1px 6px; border-radius: 4px;
    font-size: 0.8rem;
  }
</style>

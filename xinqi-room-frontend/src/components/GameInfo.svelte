<script lang="ts">
  import { t } from "../lib/i18n.svelte";

  interface Props {
    moveCount: number;
    innerCoreBlack: number;
    innerCoreWhite: number;
    terminal: boolean;
    winner?: "Black" | "White" | "Draw";
    stonePrimaryHex: string;
    stoneSecondaryHex: string;
  }

  let {
    moveCount,
    innerCoreBlack,
    innerCoreWhite,
    terminal,
    winner,
    stonePrimaryHex = "#1E293B",
    stoneSecondaryHex = "#D97706",
  }: Props = $props();
</script>

<div class="panel" style="--cp: {stonePrimaryHex}; --cs: {stoneSecondaryHex};">
  <h2 class="title">{t("app.title")}</h2>

  <div class="info-row">
    <span class="label">{t("sidebar.turn")}</span>
    <span class="value">{moveCount}</span>
  </div>

  <!-- 内芯圆点指示器 -->
  <div class="core-section">
    <div class="core-label">{t("sidebar.inner_core")}</div>
    <div class="core-dots-row">
      {#each Array(5) as _, i}
        <span class="core-dot" class:filled={i < innerCoreBlack} class:first={true}></span>
      {/each}
    </div>
    <div class="core-dots-row">
      {#each Array(5) as _, i}
        <span class="core-dot" class:filled={i < innerCoreWhite} class:second={true}></span>
      {/each}
    </div>
  </div>

  {#if terminal}
    <div class="info-row terminal">
      <span class="value winner">
        {#if winner === "Black"}<span class="win-dot blue"></span> {t("terminal.black_win")}
        {:else if winner === "White"}<span class="win-dot orange"></span> {t("terminal.white_win")}
        {:else}{t("terminal.draw")}{/if}
      </span>
    </div>
  {/if}

  <div class="divider"></div>
</div>

<style>
  .panel {
    width: 100%;
    display: flex; flex-direction: column;
    gap: 6px; padding-bottom: 4px;
  }
  .title {
    font-size: 3rem; font-weight: 700;
    color: var(--gameinfo-title); margin: 0;
    text-align: center;
    line-height: 1.2;
    padding-bottom: 10px;
    border-bottom: 1px solid var(--gameinfo-title-border);
  }
  .info-row {
    display: flex; align-items: center; gap: 8px;
    font-size: 0.9rem;
  }
  .info-row:first-of-type { margin-top: 20px; }
  .label {
    color: var(--gameinfo-label); min-width: 2.4em;
    font-size: 0.75rem; text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .value {
    color: var(--gameinfo-value); font-weight: 600;
    font-variant-numeric: tabular-nums;
  }

  /* ── 内芯圆点指示器 ── */
  .core-section {
    display: flex; flex-direction: column;
    gap: 14px; margin: 6px 0 2px;
  }
  .core-label {
    font-size: 0.72rem; color: var(--gameinfo-label);
    text-transform: uppercase; letter-spacing: 0.04em;
    margin-bottom: 2px;
  }
  .core-dots-row {
    display: flex; align-items: center;
    justify-content: space-evenly;
    padding: 0;
  }
  .core-dot {
    width: 14px; height: 14px;
    border-radius: 50%;
    background: transparent;
    transition: all 0.2s ease;
  }
  .core-dot.first {
    border: 1.5px solid var(--cp);
  }
  .core-dot.first.filled {
    background: var(--cp);
    border-color: var(--cp);
  }
  .core-dot.second {
    border: 1.5px solid var(--cs);
  }
  .core-dot.second.filled {
    background: var(--cs);
    border-color: var(--cs);
  }

  .divider {
    height: 1px; background: var(--gameinfo-divider); margin: 2px 0;
  }

  .terminal .winner {
    color: var(--gameinfo-terminal); font-weight: 700; font-size: 0.92rem;
    display: flex; align-items: center; gap: 4px;
  }
  .win-dot {
    display: inline-block; width: 8px; height: 8px;
    border-radius: 50%; flex-shrink: 0;
  }
  .win-dot.blue { background: var(--cp); }
  .win-dot.orange { background: var(--cs); }

</style>

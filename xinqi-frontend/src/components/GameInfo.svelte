<script lang="ts">
  import { COLORS } from "../lib/colors";

  interface Props {
    currentPlayer: "Black" | "White";
    moveCount: number;
    innerCoreBlack: number;
    innerCoreWhite: number;
    terminal: boolean;
    winner?: "Black" | "White" | "Draw";
    hoverInfo: string;
  }

  let {
    currentPlayer,
    moveCount,
    innerCoreBlack,
    innerCoreWhite,
    terminal,
    winner,
    hoverInfo = "",
  }: Props = $props();
</script>

<div class="panel" style="--cp: {COLORS.stonePrimaryHex}; --cs: {COLORS.stoneSecondaryHex};">
  <h2 class="title">芯棋</h2>

  <div class="info-row">
    <span class="label">回合</span>
    <span class="value">{moveCount}</span>
  </div>

  <div class="info-row">
    <span class="label">当前</span>
    <span class="player-dot" class:black={currentPlayer === "Black"} class:white={currentPlayer === "White"}></span>
    <span class="value">{currentPlayer === "Black" ? "黑方" : "白方"}</span>
  </div>

  <div class="info-row">
    <span class="label">内芯</span>
    <span class="core-group">
      <span class="core-dot dark"><span class="dot"></span><span class="label-text">黑</span><span class="num">{innerCoreBlack}</span></span>
      <span class="core-sep">/</span>
      <span class="core-dot light"><span class="dot"></span><span class="label-text">白</span><span class="num">{innerCoreWhite}</span></span>
    </span>
  </div>

  <div class="hover-row">
    <span class="hover-text">{hoverInfo}</span>
  </div>

  {#if terminal}
    <div class="info-row terminal">
      <span class="value winner">
        {#if winner === "Black"}<span class="win-dot blue"></span> 黑胜{:else if winner === "White"}<span class="win-dot orange"></span> 白胜{:else}平局{/if}
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
    color: #1e1b4b; margin: 0;
    text-align: center;
    line-height: 1.2;
    padding-bottom: 10px;
    border-bottom: 4px solid #7c6df0;
  }
  .info-row {
    display: flex; align-items: center; gap: 8px;
    font-size: 0.9rem;
  }
  .info-row:first-of-type { margin-top: 20px; }
  .label {
    color: #6b7280; min-width: 2.4em;
    font-size: 0.75rem; text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .value {
    color: #1e1b4b; font-weight: 600;
    font-variant-numeric: tabular-nums;
  }
  .player-dot {
    width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0;
    border: 1px solid rgba(0,0,0,0.06);
  }
  .player-dot.black { background: var(--cp); }
  .player-dot.white { background: var(--cs); }

  .core-group {
    display: flex; align-items: center; gap: 4px;
    font-weight: 600; color: #1e1b4b;
  }
  .core-dot {
    display: inline-flex; align-items: center; gap: 4px;
  }
  .core-dot .dot {
    display: inline-block; width: 8px; height: 8px;
    border-radius: 50%; flex-shrink: 0;
  }
  .core-dot .label-text {
    font-size: 0.85rem;
  }
  .core-dot .num {
    font-variant-numeric: tabular-nums;
    font-size: 0.9rem;
  }
  .core-dot.dark .dot { background: var(--cp); }
  .core-dot.dark .label-text { color: #374151; }
  .core-dot.light .dot { background: var(--cs); }
  .core-dot.light .label-text { color: #374151; }
  .core-sep { color: #d4d4e0; font-size: 0.8rem; margin: 0 4px; }

  .hover-row { min-height: 1.2em; }
  .hover-text {
    color: #7c6df0; font-size: 0.72rem;
    font-family: ui-monospace, monospace;
  }
  .hover-text:empty { opacity: 0; }

  .divider {
    height: 1px; background: rgba(0,0,0,0.05); margin: 2px 0;
  }

  .terminal .winner {
    color: #7c6df0; font-weight: 700; font-size: 0.92rem;
    display: flex; align-items: center; gap: 4px;
  }
  .win-dot {
    display: inline-block; width: 8px; height: 8px;
    border-radius: 50%; flex-shrink: 0;
  }
  .win-dot.blue { background: var(--cp); }
  .win-dot.orange { background: var(--cs); }

</style>

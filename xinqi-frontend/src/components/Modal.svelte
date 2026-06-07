<script lang="ts">
  interface Props {
    show: boolean;
    /** 'records' | 'confirm' | 'none' */
    mode: string;
    recordList: string[];
    confirmTarget: string;
    browseBusy: boolean;
    onClose: () => void;
    onRefresh: () => void;
    onSelect: (filename: string) => void;
    onConfirm: () => void;
    onCancel: () => void;
  }

  let {
    show, mode, recordList, confirmTarget, browseBusy,
    onClose, onRefresh, onSelect, onConfirm, onCancel,
  }: Props = $props();
</script>

{#if show}
  <div role="dialog" aria-modal="true" class="modal-overlay" tabindex="-1"
    onclick={onClose} onkeydown={(e) => { if (e.key === 'Escape') onClose(); }}>
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div role="document" class="modal-panel" tabindex="-1"
      onclick={(e) => e.stopPropagation()} onkeydown={() => {}}>
      {#if mode === 'records'}
        <div class="modal-header">
          <span>棋谱列表</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <button class="modal-refresh" onclick={onRefresh}>↻ 刷新</button>
          {#if recordList.length === 0}
            <div class="modal-empty">暂无棋谱</div>
          {:else}
            {#each recordList as file}
              <button class="modal-record" onclick={() => onSelect(file)}>
                {file.slice(5, -5)}
              </button>
            {/each}
          {/if}
        </div>
      {:else if mode === 'confirm'}
        <div class="modal-header">
          <span>加载棋谱</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <p>确认加载棋谱 <strong>{confirmTarget.slice(5, -5)}</strong>？</p>
          <p class="modal-warn">当前对局将被覆盖。</p>
          <div class="modal-actions">
            <button class="btn-new" onclick={onCancel}>取消</button>
            <button class="btn-new" onclick={onConfirm} disabled={browseBusy} style="background:#1a5276;">
              {browseBusy ? '加载中...' : '确认'}
            </button>
          </div>
        </div>
      {:else if mode === 'new_game'}
        <div class="modal-header">
          <span>新游戏</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <p>确定要结束当前对局吗？</p>
          <div class="modal-actions">
            <button class="btn-new" onclick={onCancel}>取消</button>
            <button class="btn-new" onclick={onConfirm} style="background:#7c6df0; color:#fff;">确认</button>
          </div>
        </div>
      {/if}
    </div>
  </div>
{/if}

<style>
  .modal-overlay {
    position: fixed; inset: 0;
    background: rgba(0,0,0,0.3);
    display: flex; align-items: center; justify-content: center;
    z-index: 500;
  }
  .modal-panel {
    background: #ffffff;
    border: 1px solid rgba(0,0,0,0.06);
    border-radius: 12px;
    min-width: 340px; max-width: 500px;
    max-height: 70vh;
    display: flex; flex-direction: column;
    box-shadow: 0 16px 48px rgba(0,0,0,0.08);
  }
  .modal-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 1rem 1.2rem;
    border-bottom: 1px solid rgba(0,0,0,0.06);
    font-size: 0.95rem; color: #1e1b4b; font-weight: 600;
  }
  .modal-close {
    background: none; border: none;
    color: #9ca3af; font-size: 1.2rem; cursor: pointer;
  }
  .modal-close:hover { color: #1e1b4b; }
  .modal-body {
    padding: 0.8rem 1.2rem; overflow-y: auto;
    display: flex; flex-direction: column; gap: 0.4rem;
  }
  .modal-refresh, .modal-record {
    background: #f5f3ff; color: #6b7280;
    border: 1px solid rgba(0,0,0,0.04);
    border-radius: 6px; padding: 0.5rem 0.8rem;
    font-size: 0.85rem; cursor: pointer;
    text-align: left; transition: background 0.15s;
  }
  .modal-refresh:hover, .modal-record:hover { background: #e8e5ff; color: #1e1b4b; }
  .modal-empty { color: #9ca3af; text-align: center; padding: 1rem; }
  .modal-warn { color: #e74c3c; font-size: 0.82rem; }
  .modal-actions {
    display: flex; gap: 0.6rem;
    justify-content: flex-end; margin-top: 0.5rem;
  }
  .btn-new {
    background: #f5f3ff; color: #6b7280;
    border: 1px solid rgba(0,0,0,0.04);
    border-radius: 8px; padding: 0.55rem 1.5rem;
    font-size: 0.85rem; cursor: pointer; transition: all 0.15s;
  }
  .btn-new:hover { background: #e8e5ff; color: #1e1b4b; }
  .btn-new:disabled { opacity: 0.25; cursor: default; background: none; }
</style>

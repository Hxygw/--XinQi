<script lang="ts">
  import { t } from "../lib/i18n.svelte";

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
          <span>{t("modal.record_list")}</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <button class="modal-refresh" onclick={onRefresh}>{t("modal.refresh")}</button>
          {#if recordList.length === 0}
            <div class="modal-empty">{t("modal.empty")}</div>
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
          <span>{t("modal.load_record")}</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <p>{t("modal.load_confirm")} <strong>{confirmTarget.slice(5, -5)}</strong>？</p>
          <p class="modal-warn">{t("modal.warn")}</p>
          <div class="modal-actions">
            <button class="btn-new" onclick={onCancel}>{t("modal.cancel")}</button>
            <button class="btn-new" onclick={onConfirm} disabled={browseBusy} style="background:var(--accent); color:var(--modal-bg);">
              {browseBusy ? t("modal.loading") : t("modal.confirm")}
            </button>
          </div>
        </div>
      {:else if mode === 'new_game'}
        <div class="modal-header">
          <span>{t("modal.new_game_title")}</span>
          <button class="modal-close" onclick={onClose}>✕</button>
        </div>
        <div class="modal-body">
          <p>{t("modal.new_game_confirm")}</p>
          <div class="modal-actions">
            <button class="btn-new" onclick={onCancel}>{t("modal.cancel")}</button>
            <button class="btn-new" onclick={onConfirm} style="background:var(--accent); color:var(--modal-bg);">{t("modal.confirm")}</button>
          </div>
        </div>
      {/if}
    </div>
  </div>
{/if}

<style>
  .modal-overlay {
    position: fixed; inset: 0;
    background: var(--overlay-bg);
    display: flex; align-items: center; justify-content: center;
    z-index: 500;
  }
  .modal-panel {
    background: var(--modal-bg);
    border: 1px solid var(--modal-border);
    border-radius: 12px;
    min-width: 340px; max-width: 500px;
    max-height: 70vh;
    display: flex; flex-direction: column;
    box-shadow: 0 16px 48px rgba(0,0,0,0.3);
  }
  .modal-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 1rem 1.2rem;
    border-bottom: 1px solid var(--divider);
    font-size: 0.95rem; color: var(--modal-header-text); font-weight: 600;
  }
  .modal-close {
    background: none; border: none;
    color: var(--modal-close); font-size: 1.2rem; cursor: pointer;
  }
  .modal-close:hover { color: var(--modal-close-hover); }
  .modal-body {
    padding: 0.8rem 1.2rem; overflow-y: auto;
    display: flex; flex-direction: column; gap: 0.4rem;
  }
  .modal-body p { color: var(--modal-body-text); }
  .modal-refresh, .modal-record {
    background: var(--modal-btn-bg); color: var(--modal-btn-color);
    border: 1px solid var(--modal-btn-border);
    border-radius: 6px; padding: 0.5rem 0.8rem;
    font-size: 0.85rem; cursor: pointer;
    text-align: left; transition: background 0.15s;
  }
  .modal-refresh:hover, .modal-record:hover { background: var(--modal-btn-hover-bg); color: var(--modal-btn-hover-color); }
  .modal-empty { color: var(--modal-empty); text-align: center; padding: 1rem; }
  .modal-warn { color: var(--modal-warn); font-size: 0.82rem; }
  .modal-actions {
    display: flex; gap: 0.6rem;
    justify-content: flex-end; margin-top: 0.5rem;
  }
  .btn-new {
    background: var(--btn-new-bg); color: var(--btn-new-color);
    border: 1px solid var(--btn-new-border);
    border-radius: 8px; padding: 0.55rem 1.5rem;
    font-size: 0.85rem; cursor: pointer; transition: all 0.15s;
  }
  .btn-new:hover { background: var(--btn-new-hover-bg); color: var(--btn-new-hover-color); }
  .btn-new:disabled { opacity: 0.25; cursor: default; background: none; }
</style>

<script>
  let {
    open = $bindable(false),
    title = 'Подтвердите действие',
    description = '',
    changes = [],
    danger = false,
    confirmLabel = 'Подтвердить',
    onconfirm
  } = $props();

  function accept() { open = false; onconfirm?.(); }
  function focusIn(node) { node.focus(); }
</script>

<svelte:window onkeydown={(e) => { if (open && e.key === 'Escape') open = false; }} />

{#if open}

  <div
    class="backdrop"
    role="presentation"
    onclick={(e) => { if (e.target === e.currentTarget) open = false; }}
  >
    <div class="modal glass" role="dialog" aria-modal="true" aria-label={title} tabindex="-1" use:focusIn>
      <h2>{title}</h2>
      {#if description}<p class="muted">{description}</p>{/if}
      {#if changes.length}
        <ul class="changes">
          {#each changes as c}<li>{c}</li>{/each}
        </ul>
      {/if}
      <div class="actions">
        <button class="btn" onclick={() => (open = false)}>Отмена</button>
        <button class="btn {danger ? 'btn--danger' : 'btn--primary'}" onclick={accept}>{confirmLabel}</button>
      </div>
    </div>
  </div>
{/if}

<style>
  .backdrop {
    position: fixed; inset: 0; z-index: 300;
    background: rgba(3, 10, 11, 0.7);
    backdrop-filter: blur(4px);
    -webkit-backdrop-filter: blur(4px);
    display: flex; align-items: flex-end; justify-content: center;
    padding: 14px;
    padding-bottom: calc(14px + var(--safe-b));
  }
  .modal {
    width: 100%;
    max-width: 440px;
    border-radius: var(--r-lg);
    padding: 20px;
    animation: fade-in 0.28s var(--ease) both;
  }
  .modal:focus { outline: none; }
  p { margin: 9px 0 0; font-size: 0.88rem; line-height: 1.5; }

  .changes {
    margin: 13px 0 0;
    padding-left: 18px;
    font-size: 0.85rem;
    color: var(--warn);
    line-height: 1.5;
  }
  .changes li + li { margin-top: 3px; }

  .actions { display: flex; gap: 9px; margin-top: 18px; }
  .actions .btn { flex: 1 1 0; }

  @media (min-width: 620px) {
    .backdrop { align-items: center; padding-bottom: 14px; }
    .actions { justify-content: flex-end; }
    .actions .btn { flex: 0 0 auto; min-width: 116px; }
  }
</style>

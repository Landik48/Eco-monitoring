<script>
  import { toasts } from '../lib/store.svelte.js';
</script>

<div class="wrap" aria-live="polite">
  {#each toasts.items as t (t.id)}
    <div class="toast glass toast--{t.kind}">{t.message}</div>
  {/each}
</div>

<style>
  .wrap {
    position: fixed;
    left: 12px; right: 12px;
    bottom: calc(var(--tabbar) + 14px + var(--safe-b));
    z-index: 200;
    display: flex; flex-direction: column; gap: 8px;
    pointer-events: none;
  }
  .toast {
    border-radius: var(--r-md);
    border-left: 3px solid var(--info);
    padding: 11px 14px;
    font-size: 0.86rem;
    line-height: 1.4;
    animation: fade-in 0.3s var(--ease) both;
  }
  .toast--ok { border-left-color: var(--accent); }
  .toast--warn { border-left-color: var(--warn); }
  .toast--error { border-left-color: var(--danger); }

  @media (min-width: 900px) {
    .wrap { left: auto; right: 20px; bottom: 20px; width: min(400px, calc(100vw - 40px)); }
  }
</style>

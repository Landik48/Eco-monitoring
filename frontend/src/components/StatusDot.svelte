<script>
  let { online = false, label = '' } = $props();
</script>

<span class="badge" class:on={online}>
  <span class="dot" class:pulse={online}></span>
  {label || (online ? 'На связи' : 'Нет связи')}
</span>

<style>
  .badge {
    display: inline-flex; align-items: center; gap: 7px;
    padding: 5px 12px; border-radius: 999px;
    border: 1px solid var(--border);
    background: var(--bg-soft);
    font-size: 0.82rem; color: var(--text-muted);
    white-space: nowrap;
  }
  .badge.on { color: var(--accent-2); border-color: var(--accent-dim); }
  .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--danger); }
  .badge.on .dot { background: var(--ok); }
  .pulse { animation: pulse-ring 2s infinite; }
</style>

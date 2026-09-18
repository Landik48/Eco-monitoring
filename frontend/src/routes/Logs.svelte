<script>
  import { api } from '../lib/api.js';
  import { app, toast } from '../lib/store.svelte.js';
  import { dateTime, LOG_COLORS } from '../lib/format.js';

  let entries = $state([]);
  let summary = $state({});
  let level = $state('');
  let source = $state('');
  let hours = $state(24);
  let loading = $state(true);

  const LEVELS = [
    { key: '', label: 'Все' },
    { key: 'info', label: 'Инфо' },
    { key: 'warn', label: 'Внимание' },
    { key: 'error', label: 'Ошибки' }
  ];

  async function load() {
    if (!app.device) { loading = false; return; }
    loading = true;
    try {
      const base = { device_id: app.device.device_id, hours };
      [entries, summary] = await Promise.all([
        api.logs({ ...base, limit: 300, level: level || undefined, source: source || undefined }),
        api.logsSummary(base)
      ]);
    } catch (e) {
      toast(e.message, 'error');
    } finally {
      loading = false;
    }
  }

  $effect(() => {
    level; source; hours; app.device?.device_id;
    load();
  });
</script>

<div class="toolbar">
  <div class="seg" role="group" aria-label="Уровень">
    {#each LEVELS as l (l.key)}
      <button class="seg__item" aria-pressed={level === l.key} onclick={() => (level = l.key)}>
        {l.label}{#if l.key && summary[l.key]}<span class="count">{summary[l.key]}</span>{/if}
      </button>
    {/each}
  </div>

  <div class="toolbar__tail">
    <select class="input select" bind:value={source} aria-label="Источник">
      <option value="">Все источники</option>
      <option value="device">Станция</option>
      <option value="panel">Оператор</option>
    </select>
    <select class="input select" bind:value={hours} aria-label="Период">
      <option value={6}>6 часов</option>
      <option value={24}>24 часа</option>
      <option value={168}>7 дней</option>
      <option value={720}>30 дней</option>
    </select>
  </div>
</div>

<div class="plate list-wrap">
  {#if loading}
    <div class="pad">
      {#each Array(6) as _, i}
        <div class="skeleton" style="height:16px;margin-bottom:10px;opacity:{1 - i * 0.13}"></div>
      {/each}
    </div>
  {:else if !entries.length}
    <p class="pad faint empty">За выбранный период записей нет</p>
  {:else}
    <ul class="list">
      {#each entries as e (e.id)}
        <li class="entry" style:--c={LOG_COLORS[e.level] ?? 'var(--faint)'}>
          <span class="entry__level">{e.level}</span>
          <span class="entry__msg">{e.msg}</span>
          <span class="entry__meta faint">
            {dateTime(e.ts)}
            {#if e.code}<span class="entry__code">{e.code}</span>{/if}
            <span class="entry__src">{e.source === 'panel' ? (e.actor ?? 'оператор') : 'станция'}</span>
          </span>
        </li>
      {/each}
    </ul>
  {/if}
</div>

<style>
  .toolbar { display: flex; flex-wrap: wrap; gap: 9px; margin-bottom: var(--gap); }
  .toolbar .seg { flex: 1 1 100%; }
  .toolbar__tail { display: flex; gap: 9px; flex: 1 1 100%; }
  .select { flex: 1 1 0; width: auto; min-width: 0; }

  .count {
    display: inline-block;
    margin-left: 5px;
    padding: 0 5px;
    border-radius: 999px;
    background: rgba(255, 255, 255, 0.1);
    font-size: 0.68rem;
  }

  .list-wrap { overflow: hidden; }
  .pad { padding: 16px; }
  .empty { text-align: center; padding: 30px 16px; font-size: 0.88rem; }

  .list { list-style: none; margin: 0; padding: 0; }

  .entry {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 11px 15px;
    border-bottom: 1px solid var(--hair-soft);
    border-left: 2px solid var(--c);
    font-size: 0.85rem;
  }
  .entry:last-child { border-bottom: none; }

  .entry__level {
    color: var(--c);
    font-size: 0.68rem;
    font-weight: 600;
    letter-spacing: 0.02em;
  }
  .entry__msg { line-height: 1.45; overflow-wrap: anywhere; }
  .entry__meta { display: flex; flex-wrap: wrap; gap: 4px 10px; font-size: 0.72rem; }
  .entry__code { opacity: 0.8; }
  .entry__src { opacity: 0.8; }

  @media (min-width: 620px) {
    .toolbar .seg { flex: 0 0 auto; }
    .toolbar__tail { flex: 1 1 auto; justify-content: flex-end; }
    .select { flex: 0 0 auto; min-width: 150px; }

    .entry {
      display: grid;
      grid-template-columns: 76px minmax(0, 1fr) auto;
      align-items: baseline;
      gap: 12px;
      padding: 10px 16px;
    }
    .entry__meta { justify-content: flex-end; flex-wrap: nowrap; white-space: nowrap; }
  }

  @media (min-width: 900px) {
    .list { max-height: 68vh; overflow-y: auto; }
  }
</style>

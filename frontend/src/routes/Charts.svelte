<script>
  import { api } from '../lib/api.js';
  import { app, toast, METRICS, SECTIONS, sectionMetrics, RANGES } from '../lib/store.svelte.js';
  import LineChart from '../components/LineChart.svelte';
  import Icon from '../components/Icon.svelte';

  let rangeKey = $state('24h');
  let mode = $state('sections');
  let selected = $state(['temp', 'tds']);
  let data = $state(null);
  let loading = $state(true);

  const range = $derived(RANGES.find((r) => r.key === rangeKey) ?? RANGES[1]);
  const allKeys = Object.keys(METRICS);

  async function load() {
    const device = app.device;
    if (!device) { loading = false; return; }
    loading = true;
    try {
      data = await api.series({
        device_id: device.device_id,
        metrics: allKeys.join(','),
        hours: range.hours,
        location_id: app.locationId
      });
    } catch (e) {
      toast(e.message, 'error');
      data = null;
    } finally {
      loading = false;
    }
  }

  function toggle(key) {
    const i = selected.indexOf(key);
    if (i >= 0) { if (selected.length > 1) selected.splice(i, 1); }
    else selected.push(key);
  }

  const mixedUnits = $derived(new Set(selected.map((k) => METRICS[k].unit)).size > 1);

  const bucketLabel = $derived.by(() => {
    const s = data?.bucket_seconds;
    if (!s) return '';
    if (s < 60) return `${s} с`;
    if (s < 3600) return `${Math.round(s / 60)} мин`;
    return `${Math.round(s / 3600)} ч`;
  });

  async function downloadCsv() {
    try {
      const res = await api.exportCsv({
        device_id: app.device.device_id,
        hours: range.hours,
        location_id: app.locationId
      });
      const blob = await res.blob();
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = `${app.device.device_id}-${range.key}.csv`;
      a.click();
      URL.revokeObjectURL(a.href);
    } catch (e) {
      toast(e.message, 'error');
    }
  }

  $effect(() => {
    rangeKey;
    app.locationId;
    app.device?.device_id;
    load();
  });
</script>

<div class="toolbar">
  <div class="seg" role="group" aria-label="Период">
    {#each RANGES as r (r.key)}
      <button class="seg__item" aria-pressed={rangeKey === r.key} onclick={() => (rangeKey = r.key)}>
        {r.label}
      </button>
    {/each}
  </div>

  <div class="seg" role="group" aria-label="Вид">
    <button class="seg__item" aria-pressed={mode === 'sections'} onclick={() => (mode = 'sections')}>
      Разделы
    </button>
    <button class="seg__item" aria-pressed={mode === 'combined'} onclick={() => (mode = 'combined')}>
      Вместе
    </button>
  </div>

  <div class="toolbar__tail">
    {#if app.locations.length > 1}
      <select class="input select" bind:value={app.locationId} aria-label="Локация">
        <option value={null}>Все локации</option>
        {#each app.locations as loc (loc.id)}
          <option value={loc.id}>{loc.name}</option>
        {/each}
      </select>
    {/if}
    <button class="btn btn--sm" onclick={downloadCsv} disabled={!app.device}>
      <Icon name="download" size={16} color="var(--dim)" /> CSV
    </button>
  </div>
</div>

{#if data}
  <p class="faint meta">Шаг агрегации {bucketLabel}, точек {data.points.length}</p>
{/if}

{#if mode === 'combined'}
  <section class="plate card">
    <div class="picker">
      {#each allKeys as key (key)}
        <button
          class="chip"
          class:chip--on={selected.includes(key)}
          onclick={() => toggle(key)}
          aria-pressed={selected.includes(key)}
        >
          <span class="dot" style:background={METRICS[key].color}></span>
          {METRICS[key].short}
        </button>
      {/each}
    </div>
    <LineChart
      points={data?.points ?? []}
      metrics={selected}
      bucketSeconds={data?.bucket_seconds ?? 60}
      height={300}
      normalized={mixedUnits}
      showLimit={selected.length === 1}
      {loading}
    />
  </section>
{:else}
  <div class="grid sections">
    {#each SECTIONS as section (section)}
      {@const keys = sectionMetrics(section)}
      <section class="plate card">
        <header class="card__head">
          <h2>{section}</h2>
          <span class="faint">{keys.map((k) => METRICS[k].short).join(', ')}</span>
        </header>
        <LineChart
          points={data?.points ?? []}
          metrics={keys}
          bucketSeconds={data?.bucket_seconds ?? 60}
          height={210}
          normalized={new Set(keys.map((k) => METRICS[k].unit)).size > 1}
          showLimit={keys.length === 1}
          {loading}
        />
      </section>
    {/each}
  </div>
{/if}

<style>
  .toolbar { display: flex; flex-wrap: wrap; gap: 9px; margin-bottom: 10px; }
  .toolbar .seg { flex: 1 1 100%; }
  .toolbar__tail { display: flex; gap: 9px; flex: 1 1 100%; }
  .select { flex: 1 1 auto; width: auto; min-width: 0; }

  .meta { font-size: 0.76rem; margin: 0 0 var(--gap); }

  .card { padding: 16px 17px; display: flex; flex-direction: column; gap: 12px; }
  .card__head { display: flex; align-items: baseline; gap: 10px; }
  .card__head .faint {
    margin-left: auto; font-size: 0.72rem;
    min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  }

  .sections { grid-template-columns: minmax(0, 1fr); }

  .picker { display: flex; flex-wrap: wrap; gap: 7px; }
  .chip {
    display: inline-flex; align-items: center; gap: 6px;
    min-height: 36px;
    padding: 0 12px;
    border-radius: 999px;
    border: 1px solid var(--hair);
    background: transparent;
    color: var(--dim);
    font-size: 0.8rem;
    cursor: pointer;
    transition: background 0.2s, color 0.2s, border-color 0.2s;
  }
  .chip:hover { color: var(--text); }
  .chip--on { background: var(--glass-strong); color: var(--text); border-color: var(--hair); }
  .dot { width: 8px; height: 8px; border-radius: 50%; flex: none; }

  @media (min-width: 620px) {
    .toolbar .seg { flex: 0 0 auto; }
    .toolbar__tail { flex: 1 1 auto; justify-content: flex-end; }
    .select { flex: 0 0 auto; min-width: 180px; }
  }
  @media (min-width: 1100px) {
    .sections { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  }
</style>

<script>
  import { METRICS } from '../lib/store.svelte.js';
  import { axisTime, num } from '../lib/format.js';

  let { points = [], metric = 'tds', height = 170, loading = false } = $props();

  const PAD = { t: 8, b: 20 };
  const TIP_W = 104;

  let boxWidth = $state(0);
  let hover = $state(null);

  const innerH = $derived(Math.max(height - PAD.t - PAD.b, 10));
  const barCount = $derived(Math.max(4, Math.min(14, Math.floor(boxWidth / 26))));

  const groups = $derived.by(() => {
    const values = [];
    for (const p of points) {
      const v = p[metric]?.avg;
      if (v !== null && v !== undefined) values.push({ t: p.t, v });
    }

    if (!values.length || !barCount) return [];
    if (values.length <= barCount) return values;

    const size = Math.ceil(values.length / barCount);
    const out = [];
    for (let i = 0; i < values.length; i += size) {
      const chunk = values.slice(i, i + size);
      let sum = 0;
      for (const c of chunk) sum += c.v;
      out.push({ t: chunk[0].t, v: sum / chunk.length });
    }
    return out;
  });

  const max = $derived(groups.length ? Math.max(...groups.map((g) => g.v)) : 1);
  const step = $derived(groups.length ? boxWidth / groups.length : boxWidth);
  const barW = $derived(Math.min(Math.max(step * 0.46, 5), 20));
  const ready = $derived(boxWidth > 0 && groups.length > 0);

  const color = $derived(METRICS[metric]?.color ?? 'var(--accent)');

  const limitY = $derived.by(() => {
    const limit = METRICS[metric]?.limit;
    if (limit === undefined || !ready) return null;
    if (limit > max * 1.6) return null;
    return innerH - (limit / max) * innerH;
  });

  const tipLeft = $derived.by(() => {
    if (hover === null) return 0;
    const wanted = step * hover + step / 2 - TIP_W / 2;
    return Math.max(2, Math.min(boxWidth - TIP_W - 2, wanted));
  });
</script>

<div class="wrap" bind:clientWidth={boxWidth} style:height="{height}px">
  {#if loading && !ready}
    <div class="state"><span class="spinner"></span></div>
  {:else if !ready && boxWidth > 0}
    <div class="state faint">Нет данных</div>
  {/if}

  {#if ready}
    <svg width={boxWidth} {height} viewBox="0 0 {boxWidth} {height}" role="img"
         aria-label="{METRICS[metric]?.label ?? metric} по периодам">
      <g transform="translate(0,{PAD.t})">
        <line x1="0" x2={boxWidth} y1={innerH} y2={innerH} class="base" />

        {#if limitY !== null}
          <line x1="0" x2={boxWidth} y1={limitY} y2={limitY} class="limit" stroke={color} />
        {/if}

        {#each groups as g, i (g.t)}
          {@const h = Math.max((g.v / max) * innerH, 3)}
          {@const cx = step * i + step / 2}
          <rect
            x={cx - barW / 2}
            y={innerH - h}
            width={barW}
            height={h}
            rx={Math.min(barW / 2, 4)}
            fill={color}
            opacity={hover === null || hover === i ? 0.95 : 0.3}
            role="presentation"
            onmouseenter={() => (hover = i)}
            onmouseleave={() => (hover = null)}
          />
        {/each}

        {#if groups.length > 1 && boxWidth > 140}
          <text class="tick" x="0" y={innerH + 15} text-anchor="start">
            {axisTime(groups[0].t, 3600)}
          </text>
          <text class="tick" x={boxWidth} y={innerH + 15} text-anchor="end">
            {axisTime(groups[groups.length - 1].t, 3600)}
          </text>
        {/if}
      </g>
    </svg>
  {/if}

  {#if hover !== null && groups[hover]}
    <div class="tip" style:left="{tipLeft}px" style:width="{TIP_W}px">
      {num(groups[hover].v, METRICS[metric]?.digits ?? 1)}
      <span class="faint">{METRICS[metric]?.unit ?? ''}</span>
    </div>
  {/if}
</div>

<style>
  .wrap { position: relative; width: 100%; max-width: 100%; user-select: none; }
  svg { display: block; max-width: 100%; }

  .base { stroke: rgba(190, 225, 225, 0.1); stroke-width: 1; }
  .limit { stroke-width: 1; stroke-dasharray: 4 4; opacity: 0.45; }
  .tick { fill: var(--faint); font-size: 10.5px; }

  rect { transition: opacity 0.2s; }

  .state {
    position: absolute; inset: 0;
    display: flex; align-items: center; justify-content: center;
    font-size: 0.85rem; pointer-events: none;
  }

  .tip {
    position: absolute; top: 0;
    padding: 4px 10px;
    border-radius: 999px;
    background: rgba(6, 22, 24, 0.94);
    border: 1px solid var(--hair);
    font-size: 0.78rem;
    text-align: center;
    white-space: nowrap;
    pointer-events: none;
    box-shadow: 0 8px 22px rgba(0, 0, 0, 0.36);
  }
</style>

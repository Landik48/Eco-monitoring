<script>
  import { METRICS } from '../lib/store.svelte.js';
  import { axisTime, num } from '../lib/format.js';

  let {
    points = [],
    metrics = [],
    bucketSeconds = 60,
    height = 180,
    showLimit = true,
    normalized = false,
    compact = false,
    loading = false
  } = $props();

  const TIP_W = 168;

  let boxWidth = $state(0);
  let hover = $state(null);

  const active = $derived(
    metrics.filter((key) => points.some((p) => p[key] && p[key].avg !== null))
  );

  const units = $derived(new Set(active.map((k) => METRICS[k]?.unit)));
  const multiUnit = $derived(units.size > 1);

  const pad = $derived.by(() => {
    if (compact) return { l: 0, r: 0, t: 4, b: 4 };
    const left = normalized || multiUnit ? 6 : 42;
    return { l: left, r: 8, t: 10, b: 22 };
  });

  const innerW = $derived(Math.max(boxWidth - pad.l - pad.r, 0));
  const innerH = $derived(Math.max(height - pad.t - pad.b, 10));
  const ready = $derived(boxWidth > 0 && active.length > 0);

  function valuesOf(key) {
    const out = [];
    for (const p of points) {
      if (p[key] && p[key].avg !== null) out.push(p[key].avg);
    }
    return out;
  }

  const scales = $derived.by(() => {
    const out = {};

    for (const key of active) {
      const values = valuesOf(key);
      if (!values.length) continue;

      let lo = Math.min(...values);
      let hi = Math.max(...values);

      const limit = showLimit ? METRICS[key]?.limit : undefined;
      if (limit !== undefined) {
        lo = Math.min(lo, limit);
        hi = Math.max(hi, limit);
      }

      if (hi - lo < 0.000001) {
        lo -= 1;
        hi += 1;
      }

      const margin = (hi - lo) * 0.12;
      out[key] = { lo: lo - margin, hi: hi + margin };
    }

    const keys = Object.keys(out);
    if (!normalized && !multiUnit && keys.length > 1) {
      const lo = Math.min(...keys.map((k) => out[k].lo));
      const hi = Math.max(...keys.map((k) => out[k].hi));
      for (const k of keys) out[k] = { lo, hi };
    }

    return out;
  });

  function x(i) {
    if (points.length < 2) return innerW / 2;
    return (i / (points.length - 1)) * innerW;
  }

  function y(key, value) {
    const s = scales[key];
    if (!s) return innerH;
    return innerH - ((value - s.lo) / (s.hi - s.lo)) * innerH;
  }

  function linePath(key) {
    let d = '';
    let open = false;
    points.forEach((p, i) => {
      const v = p[key];
      if (!v || v.avg === null) {
        open = false;
        return;
      }
      d += `${open ? 'L' : 'M'}${x(i).toFixed(1)} ${y(key, v.avg).toFixed(1)}`;
      open = true;
    });
    return d;
  }

  function areaPath(key) {
    const line = linePath(key);
    if (!line) return '';
    let lastX = 0;
    points.forEach((p, i) => {
      if (p[key] && p[key].avg !== null) lastX = x(i);
    });
    return `${line}L${lastX.toFixed(1)} ${innerH}L0 ${innerH}Z`;
  }

  const areaKey = $derived(active.length === 1 ? active[0] : null);

  const lastPoint = $derived.by(() => {
    if (!areaKey) return null;
    for (let i = points.length - 1; i >= 0; i--) {
      const v = points[i][areaKey];
      if (v && v.avg !== null) return { x: x(i), y: y(areaKey, v.avg) };
    }
    return null;
  });

  const limitLine = $derived.by(() => {
    if (!showLimit || compact || active.length !== 1) return null;
    const key = active[0];
    const limit = METRICS[key]?.limit;
    if (limit === undefined || !scales[key]) return null;
    return { y: y(key, limit), label: METRICS[key].limitLabel ?? `норма ${limit}` };
  });

  const yTicks = $derived.by(() => {
    if (compact || normalized || multiUnit) return [];
    const key = active[0];
    if (!key || !scales[key]) return [];
    const { lo, hi } = scales[key];
    return [
      { v: hi, y: 0 },
      { v: (hi + lo) / 2, y: innerH / 2 },
      { v: lo, y: innerH }
    ];
  });

  const xTicks = $derived.by(() => {
    if (compact || points.length < 2 || innerW < 140) return [];
    const count = innerW < 420 ? 2 : 4;
    const step = (points.length - 1) / (count - 1);
    const out = [];
    for (let i = 0; i < count; i++) {
      const idx = Math.round(i * step);
      out.push({ idx, label: axisTime(points[idx].t, bucketSeconds) });
    }
    return out;
  });

  function locate(clientX, el) {
    if (!points.length) return null;
    if (points.length < 2) return 0;
    const rect = el.getBoundingClientRect();
    const px = clientX - rect.left - pad.l;
    const idx = Math.round((px / Math.max(innerW, 1)) * (points.length - 1));
    if (idx < 0) return 0;
    if (idx > points.length - 1) return points.length - 1;
    return idx;
  }

  function setHover(clientX, el) {
    const idx = locate(clientX, el);
    hover = idx === null ? null : { index: idx, x: x(idx) };
  }

  function onMove(event) {
    if (compact) return;
    setHover(event.clientX, event.currentTarget);
  }

  function onTouch(event) {
    if (compact || !event.touches.length) return;
    setHover(event.touches[0].clientX, event.currentTarget);
  }

  const tipLeft = $derived.by(() => {
    if (hover === null) return 0;
    const wanted = hover.x + pad.l - TIP_W / 2;
    const maxLeft = boxWidth - TIP_W - 4;
    return Math.max(4, Math.min(maxLeft, wanted));
  });
</script>

<div class="chart" class:chart--compact={compact} bind:clientWidth={boxWidth} style:height="{height}px">
  {#if loading && !ready}
    <div class="state"><span class="spinner"></span></div>
  {:else if !ready && boxWidth > 0}
    <div class="state faint">Нет данных за период</div>
  {/if}

  {#if ready}
    <svg
      width={boxWidth}
      {height}
      viewBox="0 0 {boxWidth} {height}"
      role="img"
      aria-label={active.map((k) => METRICS[k]?.label ?? k).join(', ')}
      onmousemove={onMove}
      onmouseleave={() => (hover = null)}
      ontouchstart={onTouch}
      ontouchmove={onTouch}
      ontouchend={() => (hover = null)}
    >
      <defs>
        {#if areaKey}
          <linearGradient id="area-{areaKey}-{height}" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stop-color={METRICS[areaKey]?.color} stop-opacity="0.26" />
            <stop offset="100%" stop-color={METRICS[areaKey]?.color} stop-opacity="0" />
          </linearGradient>
        {/if}
      </defs>

      <g transform="translate({pad.l},{pad.t})">
        {#if !compact}
          <line class="grid" x1="0" x2={innerW} y1={innerH} y2={innerH} />
          <line class="grid grid--soft" x1="0" x2={innerW} y1={innerH / 2} y2={innerH / 2} />
        {/if}

        {#each yTicks as tick}
          <text class="tick" x="-8" y={tick.y + 4} text-anchor="end">{num(tick.v, 0)}</text>
        {/each}

        {#if areaKey}
          <path d={areaPath(areaKey)} fill="url(#area-{areaKey}-{height})" />
        {/if}

        {#if limitLine}
          <line
            class="limit"
            x1="0" x2={innerW}
            y1={limitLine.y} y2={limitLine.y}
            stroke={METRICS[active[0]]?.color}
          />
          {#if innerW > 200}
            <text class="limit-label" x={innerW - 2} y={limitLine.y - 6} text-anchor="end"
                  fill={METRICS[active[0]]?.color}>{limitLine.label}</text>
          {/if}
        {/if}

        {#each active as key (key)}
          <path class="line" d={linePath(key)} stroke={METRICS[key]?.color} />
        {/each}

        {#if lastPoint && !compact}
          <circle class="last" cx={lastPoint.x} cy={lastPoint.y} r="4.5"
                  fill={METRICS[areaKey]?.color} />
        {/if}

        {#each xTicks as tick, i}
          <text class="tick" x={x(tick.idx)} y={innerH + 16}
                text-anchor={i === 0 ? 'start' : i === xTicks.length - 1 ? 'end' : 'middle'}>
            {tick.label}
          </text>
        {/each}

        {#if hover}
          <line class="cursor" x1={hover.x} x2={hover.x} y1="0" y2={innerH} />
          {#each active as key (key)}
            {#if points[hover.index]?.[key]}
              <circle cx={hover.x} cy={y(key, points[hover.index][key].avg)} r="9"
                      fill={METRICS[key]?.color} opacity="0.2" />
              <circle cx={hover.x} cy={y(key, points[hover.index][key].avg)} r="4"
                      fill={METRICS[key]?.color} stroke="var(--base-2)" stroke-width="2.5" />
            {/if}
          {/each}
        {/if}
      </g>
    </svg>
  {/if}

  {#if hover && points[hover.index]}
    <div class="tip" style:left="{tipLeft}px" style:width="{TIP_W}px">
      <div class="tip__time">{axisTime(points[hover.index].t, Math.min(bucketSeconds, 3600))}</div>
      {#each active as key (key)}
        {#if points[hover.index][key]}
          <div class="tip__row">
            <span class="dot" style:background={METRICS[key]?.color}></span>
            <span class="faint">{METRICS[key]?.short ?? key}</span>
            <span class="tip__val">
              {num(points[hover.index][key].avg, METRICS[key]?.digits ?? 1)}
              <span class="faint">{METRICS[key]?.unit ?? ''}</span>
            </span>
          </div>
        {/if}
      {/each}
    </div>
  {/if}
</div>

{#if active.length > 1 && !compact}
  <div class="legend">
    {#each active as key (key)}
      <span class="legend__item">
        <span class="dot" style:background={METRICS[key]?.color}></span>
        {METRICS[key]?.label ?? key}
        {#if METRICS[key]?.unit}<span class="faint">{METRICS[key].unit}</span>{/if}
      </span>
    {/each}
    {#if normalized || multiUnit}
      <span class="faint legend__note">шкалы нормированы, сравнивайте форму</span>
    {/if}
  </div>
{/if}

<style>
  .chart { position: relative; width: 100%; max-width: 100%; user-select: none; touch-action: pan-y; }
  svg { display: block; max-width: 100%; }

  .grid { stroke: rgba(190, 225, 225, 0.1); stroke-width: 1; }
  .grid--soft { stroke: rgba(190, 225, 225, 0.05); }
  .tick { fill: var(--faint); font-size: 10.5px; }

  .limit { stroke-width: 1; stroke-dasharray: 4 4; opacity: 0.5; }
  .limit-label { font-size: 10px; opacity: 0.85; }

  .line {
    fill: none;
    stroke-width: 2.2;
    stroke-linecap: round;
    stroke-linejoin: round;
  }

  .last { stroke: var(--base-2); stroke-width: 3; }
  .cursor { stroke: var(--hair); stroke-width: 1; stroke-dasharray: 3 3; }

  .state {
    position: absolute; inset: 0;
    display: flex; align-items: center; justify-content: center;
    font-size: 0.85rem; pointer-events: none;
  }

  .tip {
    position: absolute;
    top: 4px;
    padding: 8px 11px;
    border-radius: 12px;
    background: rgba(6, 22, 24, 0.94);
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    border: 1px solid var(--hair);
    box-shadow: 0 10px 28px rgba(0, 0, 0, 0.4);
    font-size: 0.8rem;
    pointer-events: none;
    z-index: 3;
  }
  .tip__time { color: var(--faint); font-size: 0.74rem; margin-bottom: 4px; }
  .tip__row { display: flex; align-items: center; gap: 7px; margin-top: 3px; }
  .tip__val { margin-left: auto; white-space: nowrap; }

  .dot { width: 8px; height: 8px; border-radius: 50%; flex: none; display: inline-block; }

  .legend { display: flex; flex-wrap: wrap; gap: 6px 14px; margin-top: 10px; font-size: 0.8rem; }
  .legend__item { display: inline-flex; align-items: center; gap: 6px; }
  .legend__note { flex-basis: 100%; font-size: 0.75rem; }
</style>

<script>

  let { name, size = 20, stroke = 1.7, color = 'currentColor' } = $props();

  const PATHS = {
    gauge: 'M3 13a9 9 0 0 1 18 0M12 13l4.5-4M12 13h.01',
    chart: 'M4 19V5M4 19h16M8 15l4-5 3 3 4-6',
    pin: 'M12 21s7-5.2 7-11a7 7 0 1 0-14 0c0 5.8 7 11 7 11z',
    slider: 'M4 8h10M18 8h2M4 16h4M12 16h8',
    list: 'M4 7h16M4 12h16M4 17h10',
    drop: 'M12 3c3.2 3.6 5 6.6 5 9.2A5 5 0 0 1 7 12.2C7 9.6 8.8 6.6 12 3z',
    signal: 'M4 18v-3M9 18v-7M14 18v-11M19 18V6',
    clock: 'M12 7v5l3 2',
    refresh: 'M20 11a8 8 0 1 0-2.3 5.7M20 5v6h-6',
    power: 'M12 4v7M7.5 7a7 7 0 1 0 9 0',
    download: 'M12 4v10M8 11l4 4 4-4M5 19h14',
    logout: 'M14 6H7a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h7M17 9l3 3-3 3M20 12h-8',
    check: 'M5 12.5l4.5 4.5L19 7',
    close: 'M6 6l12 12M18 6L6 18',
    edit: 'M4 20h4l10-10-4-4L4 16v4z',
    alert: 'M12 8v5M12 16.5v.01M12 3.5 2.5 20h19L12 3.5z'
  };

  const CIRCLES = {
    gauge: [[12, 13, 1.4]],
    pin: [[12, 10, 2.5]],
    slider: [[16, 8, 2], [10, 16, 2]],
    clock: [[12, 12, 8.5]]
  };
</script>

<svg
  width={size}
  height={size}
  viewBox="0 0 24 24"
  fill="none"
  {stroke}
  stroke-width={stroke}
  stroke-linecap="round"
  stroke-linejoin="round"
  style:stroke={color}
  aria-hidden="true"
  focusable="false"
>
  <path d={PATHS[name] ?? PATHS.list} />
  {#each CIRCLES[name] ?? [] as [cx, cy, r]}
    <circle {cx} {cy} {r} />
  {/each}
</svg>

<style>
  svg { display: block; flex: none; }
</style>

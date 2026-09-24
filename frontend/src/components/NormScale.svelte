<script>
  let { axis, norm, value, over = false } = $props();

  const span = $derived(Math.max(axis[1] - axis[0], 1e-6));
  const bandLeft = $derived(((norm[0] - axis[0]) / span) * 100);
  const bandWidth = $derived(((norm[1] - norm[0]) / span) * 100);
  const pos = $derived(
    value === null || value === undefined
      ? null
      : Math.max(0, Math.min(100, ((value - axis[0]) / span) * 100))
  );
</script>

<div class="track" aria-hidden="true">
  <div class="band" class:band--over={over} style:left="{bandLeft}%" style:width="{bandWidth}%"></div>
  {#if pos !== null}
    <div class="mark" class:mark--over={over} style:left="{pos}%"></div>
  {/if}
</div>

<style>
  .track {
    position: relative;
    height: 4px;
    border-radius: 2px;
    background: rgba(190, 225, 225, 0.09);
    margin-top: 2px;
  }
  .band {
    position: absolute;
    top: 0;
    bottom: 0;
    border-radius: 2px;
    background: rgba(95, 208, 174, 0.2);
  }
  .band--over { background: rgba(240, 178, 103, 0.18); }
  .mark {
    position: absolute;
    top: -3px;
    width: 2px;
    height: 10px;
    border-radius: 1px;
    background: var(--text);
    transform: translateX(-1px);
    transition: left 0.5s var(--ease);
  }
  .mark--over { background: var(--warn); }
</style>

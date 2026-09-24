<script>
  import Icon from './Icon.svelte';
  import { STABILITY_COLORS } from '../lib/format.js';

  let { stability = null } = $props();

  const state = $derived(stability?.state ?? 'ok');
  const color = $derived(STABILITY_COLORS[state] ?? 'var(--accent)');
  const show = $derived(Boolean(stability) && state !== 'ok');
</script>

{#if show}
  <section
    class="alert"
    class:alert--hard={state === 'unstable'}
    role="status"
  >
    <span class="alert__icon" style:background="color-mix(in srgb, {color} 16%, transparent)">
      <Icon name="alert" size={18} {color} stroke={2} />
    </span>

    <div class="alert__body">
      <div class="alert__head">
        <h3 style:color={color}>{stability.title}</h3>
        {#if stability.score !== null && stability.score !== undefined}
          <span class="alert__score" style:color>{stability.score}<span class="faint">/100</span></span>
        {/if}
      </div>

      <p class="alert__text">{stability.summary}</p>

      {#if stability.reasons?.length}
        <ul class="alert__tags">
          {#each stability.reasons as reason}
            <li style:border-color="color-mix(in srgb, {color} 40%, transparent)" style:color>
              {reason}
            </li>
          {/each}
        </ul>
      {/if}

      {#if stability.vbat_min}
        <p class="alert__meta faint">
          Минимальное напряжение за период: {stability.vbat_min} В{#if stability.sag_events}, просадок: {stability.sag_events}{/if}
        </p>
      {/if}
    </div>
  </section>
{/if}

<style>
  .alert {
    display: flex;
    gap: 13px;
    padding: 15px 16px;
    margin-bottom: var(--gap);
    border-radius: var(--r-lg);
    background: rgba(240, 178, 103, 0.07);
    border: 1px solid rgba(240, 178, 103, 0.3);
  }
  .alert--hard {
    background: rgba(240, 138, 128, 0.08);
    border-color: rgba(240, 138, 128, 0.34);
  }

  .alert__icon {
    width: 34px;
    height: 34px;
    flex: none;
    border-radius: var(--r-sm);
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .alert__body { min-width: 0; flex: 1 1 auto; }

  .alert__head { display: flex; align-items: baseline; gap: 10px; }
  .alert__head h3 { font-size: 0.95rem; font-weight: 600; letter-spacing: -0.01em; }
  .alert__score {
    margin-left: auto;
    font-size: 0.9rem;
    font-weight: 700;
    font-feature-settings: 'tnum' 1;
    white-space: nowrap;
  }
  .alert__score .faint { font-size: 0.7rem; font-weight: 400; }

  .alert__text {
    margin: 6px 0 0;
    font-size: 0.85rem;
    line-height: 1.5;
    color: var(--dim);
  }

  .alert__tags {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    margin: 10px 0 0;
    padding: 0;
    list-style: none;
  }
  .alert__tags li {
    padding: 3px 9px;
    border-radius: 999px;
    border: 1px solid;
    font-size: 0.74rem;
    white-space: nowrap;
  }

  .alert__meta { margin: 9px 0 0; font-size: 0.75rem; }

  @media (min-width: 900px) {
    .alert { padding: 18px 20px; }
  }
</style>

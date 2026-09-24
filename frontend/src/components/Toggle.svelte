<script>
  let { checked = $bindable(false), disabled = false, label = '' } = $props();
</script>

<button
  type="button"
  class="toggle"
  class:toggle--on={checked}
  {disabled}
  role="switch"
  aria-checked={checked}
  aria-label={label}
  onclick={() => (checked = !checked)}
>
  <span class="knob"></span>
</button>

<style>
  .toggle {
    width: 50px; height: 30px; flex: none;
    border-radius: 999px;
    border: 1px solid var(--hair);
    background: rgba(0, 0, 0, 0.26);
    padding: 2px;
    cursor: pointer;
    transition: background 0.26s var(--ease), border-color 0.26s;
  }
  .toggle--on { background: var(--accent); border-color: transparent; }
  .toggle:disabled { opacity: 0.4; cursor: not-allowed; }
  .toggle:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }

  .knob {
    display: block;
    width: 24px; height: 24px;
    border-radius: 50%;
    background: var(--dim);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.4);
    transition: transform 0.26s var(--ease), background 0.26s;
  }
  .toggle--on .knob { transform: translateX(20px); background: var(--accent-ink); }
</style>

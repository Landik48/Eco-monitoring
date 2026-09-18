<script>
  import { api } from '../lib/api.js';
  import { app, toast } from '../lib/store.svelte.js';
  import Toggle from '../components/Toggle.svelte';
  import ConfirmDialog from '../components/ConfirmDialog.svelte';
  import Icon from '../components/Icon.svelte';

  let schema = $state({});
  let saved = $state({});
  let form = $state({});
  let rev = $state(0);
  let pending = $state(false);
  let loading = $state(true);
  let saving = $state(false);

  let confirmSave = $state({ open: false, changes: [] });
  let confirmCmd = $state({ open: false, cmd: '', title: '', description: '' });

  const GROUPS = [
    { title: 'Общее', hint: 'Название и частота работы',
      keys: ['name', 'publish_interval_ms', 'status_interval_ms', 'filter_size'] },
    { title: 'Подсистемы', hint: 'Отключение влияет на сбор данных', danger: true,
      keys: ['enable_bme', 'enable_tds', 'enable_gps', 'enable_battery', 'enable_sd', 'sd_buffering', 'ota_enabled'] },
    { title: 'Калибровка', hint: 'Поправки к показаниям датчиков',
      keys: ['gps_min_sats', 'temp_offset', 'tds_offset', 'tds_scale'] }
  ];

  const changed = $derived(
    Object.keys(form).filter((k) => JSON.stringify(form[k]) !== JSON.stringify(saved[k]))
  );

  async function load() {
    if (!app.device) { loading = false; return; }
    loading = true;
    try {
      const data = await api.settings(app.device.device_id);
      schema = data.schema;
      saved = data.settings;
      form = { ...data.settings };
      rev = data.rev;
      pending = data.pending;
    } catch (e) {
      toast(e.message, 'error');
    } finally {
      loading = false;
    }
  }

  async function save(confirm = false) {
    if (!changed.length) return;
    saving = true;
    try {
      const patch = Object.fromEntries(changed.map((k) => [k, form[k]]));
      const res = await api.saveSettings(app.device.device_id, patch, confirm);
      saved = { ...saved, ...patch };
      rev = res.rev;
      pending = res.pending;
      toast(
        res.published
          ? 'Настройки отправлены станции через брокер'
          : 'Настройки сохранены, уйдут при подключении станции',
        res.published ? 'ok' : 'warn'
      );
    } catch (e) {
      if (e.status === 409 && e.detail?.error === 'confirmation_required') {
        confirmSave = { open: true, changes: e.detail.changes ?? [] };
      } else {
        toast(e.message, 'error');
      }
    } finally {
      saving = false;
    }
  }

  async function runCommand(cmd) {
    try {
      const res = await api.sendCommand(app.device.device_id, cmd, true);
      toast(res.device_online ? 'Команда отправлена' : 'Станция офлайн, команда в очереди',
            res.device_online ? 'ok' : 'warn');
      if (cmd === 'factory_reset') setTimeout(load, 1600);
    } catch (e) {
      toast(e.message, 'error');
    }
  }

  function label(k) {
    const raw = schema[k]?.label ?? k;
    return isInterval(k) ? raw.replace(/,\s*мс$/i, '') : raw;
  }
  const isBool = (k) => schema[k]?.type === 'bool';
  const isText = (k) => schema[k]?.type === 'str';
  const isInterval = (k) => k.endsWith('_interval_ms');
  const seconds = (ms) => Math.round((ms ?? 0) / 1000);

  function hint(k) {
    const s = schema[k];
    if (!s || s.type === 'bool' || s.type === 'str') return '';
    if (isInterval(k)) return `${seconds(s.min)}–${seconds(s.max)} с`;
    return `${s.min}–${s.max}`;
  }

  $effect(() => { app.device?.device_id; load(); });
</script>

{#if loading}
  <div class="plate" style="padding:18px"><div class="skeleton" style="height:180px"></div></div>
{:else if !app.device}
  <div class="plate" style="padding:24px"><h2>Станция не зарегистрирована</h2></div>
{:else}
  <div class="statusbar plate">
    <div>
      <div class="faint tiny">Версия конфигурации</div>
      <strong>rev {rev}</strong>
    </div>
    <span class="state" class:state--wait={pending}>
      {pending ? 'Ожидает применения' : 'Применена станцией'}
    </span>
    <span class="spacer"></span>
    <button class="btn btn--sm btn--ghost" onclick={load}>
      <Icon name="refresh" size={16} color="var(--dim)" /> Обновить
    </button>
  </div>

  {#each GROUPS as group (group.title)}
    <section class="plate group" class:group--danger={group.danger}>
      <header class="group__head">
        <h2>{group.title}</h2>
        <p class="faint tiny">{group.hint}</p>
      </header>

      <div class="fields">
        {#each group.keys.filter((k) => schema[k]) as key (key)}
          <div class="field">
            <div class="field__text">
              <span class="field__label">{label(key)}</span>
              {#if hint(key)}<span class="faint tiny">{hint(key)}</span>{/if}
            </div>

            <div class="field__control">
              {#if isBool(key)}
                <Toggle bind:checked={form[key]} label={label(key)} />
              {:else if isText(key)}
                <input class="input" type="text" bind:value={form[key]} maxlength={schema[key].max_len} />
              {:else if isInterval(key)}
                <div class="num-wrap">
                  <input
                    class="input num"
                    type="number"
                    inputmode="numeric"
                    value={seconds(form[key])}
                    min={seconds(schema[key].min)}
                    max={seconds(schema[key].max)}
                    oninput={(e) => (form[key] = Number(e.currentTarget.value) * 1000)}
                  />
                  <span class="faint unit">с</span>
                </div>
              {:else}
                <input
                  class="input num"
                  type="number"
                  inputmode="decimal"
                  bind:value={form[key]}
                  min={schema[key].min}
                  max={schema[key].max}
                  step={schema[key].type === 'float' ? 0.1 : 1}
                />
              {/if}
            </div>
          </div>
        {/each}
      </div>
    </section>
  {/each}

  <section class="plate group group--stop">
    <header class="group__head">
      <h2>Необратимые действия</h2>
      <p class="faint tiny">Каждое требует подтверждения</p>
    </header>
    <div class="stop-actions">
      <button
        class="btn btn--sm btn--danger"
        onclick={() => (confirmCmd = {
          open: true, cmd: 'clear_spool',
          title: 'Очистить буфер SD?',
          description: 'Измерения, накопленные во время обрыва связи и ещё не выгруженные, будут удалены безвозвратно.'
        })}
      >Очистить буфер SD</button>

      <button
        class="btn btn--sm btn--danger"
        onclick={() => (confirmCmd = {
          open: true, cmd: 'factory_reset',
          title: 'Сбросить настройки станции?',
          description: 'Все параметры вернутся к заводским, станция перезагрузится. Данные на карте не пострадают.'
        })}
      >Сброс к заводским</button>
    </div>
  </section>

  <div class="savebar glass" class:savebar--on={changed.length > 0}>
    <span class="muted tiny">Изменено: <b>{changed.length}</b></span>
    <span class="spacer"></span>
    <button class="btn btn--sm btn--ghost" onclick={() => (form = { ...saved })}>Отменить</button>
    <button class="btn btn--sm btn--primary" disabled={saving} onclick={() => save(false)}>
      {#if saving}<span class="spinner"></span>{/if}
      Сохранить
    </button>
  </div>
{/if}

<ConfirmDialog
  bind:open={confirmSave.open}
  title="Изменения затрагивают работу станции"
  description="Эти правки могут оставить станцию без данных или без связи. Подтвердите, что понимаете последствия."
  changes={confirmSave.changes}
  danger
  confirmLabel="Применить"
  onconfirm={() => save(true)}
/>

<ConfirmDialog
  bind:open={confirmCmd.open}
  title={confirmCmd.title}
  description={confirmCmd.description}
  danger
  confirmLabel="Выполнить"
  onconfirm={() => runCommand(confirmCmd.cmd)}
/>

<style>
  .tiny { font-size: 0.74rem; }

  .statusbar {
    display: flex; align-items: center; flex-wrap: wrap; gap: 12px;
    padding: 13px 16px;
    margin-bottom: var(--gap);
  }
  .state {
    padding: 5px 11px;
    border-radius: 999px;
    border: 1px solid rgba(95, 208, 174, 0.3);
    color: var(--accent);
    font-size: 0.76rem;
    white-space: nowrap;
  }
  .state--wait { border-color: rgba(240, 178, 103, 0.34); color: var(--warn); }

  .group { padding: 16px 17px; margin-bottom: var(--gap); }
  .group--danger { border-color: rgba(240, 178, 103, 0.22); }
  .group--stop { border-color: rgba(240, 138, 128, 0.24); }
  .group__head { margin-bottom: 4px; }
  .group__head p { margin: 2px 0 0; }

  .fields { display: flex; flex-direction: column; }

  .field {
    display: flex; align-items: center; gap: 14px;
    padding: 12px 0;
    border-bottom: 1px solid var(--hair-soft);
    min-width: 0;
  }
  .field:last-child { border-bottom: none; }

  .field__text { display: flex; flex-direction: column; gap: 2px; min-width: 0; flex: 1 1 auto; }
  .field__label { font-size: 0.9rem; }

  .field__control { flex: 0 0 auto; max-width: 56%; display: flex; }
  .field__control :global(input[type="text"]) { min-width: 0; }
  .num-wrap { display: flex; align-items: center; gap: 7px; min-width: 0; }
  .num { width: 90px; min-width: 0; }
  .unit { font-size: 0.8rem; }

  .stop-actions { display: flex; flex-wrap: wrap; gap: 9px; margin-top: 12px; }
  .stop-actions .btn { flex: 1 1 auto; }

  .savebar {
    position: fixed;
    left: 12px; right: 12px;
    bottom: calc(var(--tabbar) + 12px + var(--safe-b));
    z-index: 50;
    display: flex; align-items: center; gap: 9px;
    padding: 10px 14px;
    border-radius: var(--r-md);
    opacity: 0;
    transform: translateY(14px);
    pointer-events: none;
    transition: opacity 0.26s var(--ease), transform 0.26s var(--ease);
  }
  .savebar--on { opacity: 1; transform: none; pointer-events: auto; }

  @media (min-width: 620px) {
    .stop-actions .btn { flex: 0 0 auto; }
    .num { width: 108px; }
  }

  @media (min-width: 900px) {
    .group { padding: 19px 21px; }
    .savebar { left: auto; right: 24px; bottom: 24px; width: min(420px, calc(100vw - 48px)); }
    .field__control { max-width: 46%; }
  }
</style>

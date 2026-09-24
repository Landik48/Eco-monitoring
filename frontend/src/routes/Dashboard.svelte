<script>
  import { api } from '../lib/api.js';
  import { app, toast, METRICS, DASHBOARD_GROUPS } from '../lib/store.svelte.js';
  import { num, bytes, duration, timeAgo, LEVEL_COLORS } from '../lib/format.js';
  import LineChart from '../components/LineChart.svelte';
  import BarChart from '../components/BarChart.svelte';
  import NormScale from '../components/NormScale.svelte';
  import ConfirmDialog from '../components/ConfirmDialog.svelte';
  import StabilityAlert from '../components/StabilityAlert.svelte';
  import Icon from '../components/Icon.svelte';

  let report = $state(null);
  let latest = $state(null);
  let series = $state(null);
  let loading = $state(true);
  let hours = $state(24);
  let busy = $state('');
  let dialog = $state({ open: false, cmd: '', title: '', description: '' });

  const PERIODS = [
    { h: 6, label: '6 ч' },
    { h: 24, label: '24 ч' },
    { h: 168, label: '7 дней' }
  ];

  const SENSORS = [
    { key: 'bme', label: 'Метеоблок' },
    { key: 'tds', label: 'TDS и EC' },
    { key: 'gps', label: 'GPS' },
    { key: 'rtc', label: 'Часы' },
    { key: 'battery', label: 'Питание' },
    { key: 'sd', label: 'SD-карта' }
  ];

  async function load() {
    const device = app.device;
    if (!device) { loading = false; return; }
    loading = true;
    try {
      [report, latest, series] = await Promise.all([
        api.analysis({ device_id: device.device_id, hours, location_id: app.locationId }),
        api.latest(device.device_id),
        api.series({
          device_id: device.device_id,
          metrics: 'temp,hum,pres,tds,vbat,ibat',
          hours,
          location_id: app.locationId
        })
      ]);
    } catch (e) {
      toast(e.message, 'error');
    } finally {
      loading = false;
    }
  }

  const metricOf = (key) => report?.metrics?.find((m) => m.key === key);

  const isOver = (key) => {
    if (METRICS[key]?.informational) return false;
    return ['high', 'hazard'].includes(metricOf(key)?.level);
  };

  const level = $derived(report?.overall?.level ?? 'unknown');

  const headline = $derived.by(() => {
    if (!report) return '';
    const driver = report.overall.driver;
    const label = METRICS[driver]?.label;
    if (!label) return report.overall.title;
    if (['high', 'hazard'].includes(level)) return `${label} выше норматива`;
    if (level === 'elevated') return `${label} близко к пределу`;
    return 'Показатели в пределах нормы';
  });
  const alarm = $derived(['elevated', 'high', 'hazard'].includes(level));
  const indexPct = $derived(
    report?.overall?.index == null ? 0 : Math.min(report.overall.index / 125, 1)
  );

  const stability = $derived(report?.stability ?? null);

  const timeValid = $derived(app.device?.status?.time_valid !== false);
  const timeSource = $derived(app.device?.status?.time_source ?? '—');

  const sats = $derived(latest?.sats ?? 0);
  const spoolFiles = $derived(app.device?.status?.spool_files ?? 0);

  const TIME_SOURCE_LABELS = {
    gps: 'спутники',
    server: 'сервер',
    rtc: 'встроенные часы',
    none: 'нет источника'
  };

  async function run(cmd, confirm = false) {
    busy = cmd;
    try {
      const res = await api.sendCommand(app.device.device_id, cmd, confirm);
      toast(
        res.device_online
          ? 'Команда отправлена через брокер'
          : 'Станция офлайн, команда в очереди',
        res.device_online ? 'ok' : 'warn'
      );
    } catch (e) {
      toast(e.message, 'error');
    } finally {
      busy = '';
    }
  }

  $effect(() => {
    hours;
    app.locationId;
    app.device?.device_id;
    load();
  });
</script>

{#if !app.device}
  <div class="plate empty">
    <h2>Станция не зарегистрирована</h2>
    <p class="muted">Проверьте переменную DEVICE_ID в файле .env сервера.</p>
  </div>
{:else}
  <div class="toolbar">
    <div class="seg" role="group" aria-label="Период">
      {#each PERIODS as p (p.h)}
        <button class="seg__item" aria-pressed={hours === p.h} onclick={() => (hours = p.h)}>
          {p.label}
        </button>
      {/each}
    </div>
    {#if app.locations.length > 1}
      <select class="input select" bind:value={app.locationId} aria-label="Локация">
        <option value={null}>Все локации</option>
        {#each app.locations as loc (loc.id)}
          <option value={loc.id}>{loc.name}</option>
        {/each}
      </select>
    {/if}
  </div>

  <StabilityAlert {stability} />

  {#if !timeValid}
    <section class="timewarn" role="status">
      <Icon name="clock" size={17} color="var(--warn)" stroke={2} />
      <div>
        <strong>Часы станции не синхронизированы</strong>
        <p class="muted">
          Замеры сохраняются по времени приёма на сервере. Проверьте связь
          или нажмите «Синхронизировать часы».
        </p>
      </div>
      <button class="btn btn--sm" disabled={busy === 'sync_time'} onclick={() => run('sync_time')}>
        Синхронизировать
      </button>
    </section>
  {/if}

  <section class="verdict glass" class:verdict--alarm={alarm}>
    {#if loading && !report}
      <div class="skeleton" style="height:96px"></div>
    {:else if report}
      <div class="verdict__body">
        <div class="verdict__text">
          <div class="verdict__tag" style:color={alarm ? 'var(--warn)' : 'var(--accent)'}>
            <span class="tagdot" style:background={LEVEL_COLORS[level]}></span>
            {report.overall.title}
          </div>
          <h2 class="verdict__head">{headline}</h2>
          <p class="muted verdict__summary">{report.overall.summary}</p>
          <p class="faint verdict__meta">
            Окно {report.window_hours} ч, проб {num(report.samples, 0)}{report.location ? `, ${report.location}` : ''}
            {#if stability?.suspect_samples}
              · исключено недостоверных: {stability.suspect_samples}
            {/if}
          </p>
        </div>

        {#if report.overall.index !== null}
          <div class="ring" aria-hidden="true">
            <svg viewBox="0 0 128 128" aria-hidden="true" focusable="false">
              <circle cx="64" cy="64" r="54" class="ring__bg" />
              <circle
                cx="64" cy="64" r="54"
                class="ring__fg"
                stroke={LEVEL_COLORS[level]}
                stroke-dasharray="339"
                stroke-dashoffset={339 - indexPct * 339}
                transform="rotate(-90 64 64)"
              />
            </svg>
            <span class="ring__val">
              <strong>{Math.round(report.overall.index)}</strong>
              <span class="faint">из 125</span>
            </span>
          </div>
        {/if}
      </div>

      {#if report.recommendations?.length}
        <ul class="recs">
          {#each report.recommendations.slice(0, 2) as r}<li>{r}</li>{/each}
        </ul>
      {/if}
    {/if}
  </section>

  {#each DASHBOARD_GROUPS as group (group.key)}
    <section class="group">
      <header class="group__head">
        <Icon name={group.icon} size={15} color="var(--dim)" />
        <h2>{group.title}</h2>
      </header>

      <div class="grid readings" style:--cols={group.metrics.length}>
        {#each group.metrics as key (key)}
          {@const m = METRICS[key]}
          {@const over = isOver(key)}
          {@const value = latest?.[key]}
          <div class="plate reading" class:reading--over={over}>
            <span class="reading__label">{m.label}</span>
            <span class="reading__value" style:color={over ? 'var(--warn)' : 'var(--text)'}>
              {num(value, m.digits)}<span class="reading__unit">{m.unit}</span>
            </span>
            <NormScale axis={m.axis} norm={m.norm} {value} {over} />
            <span class="reading__hint" style:color={over ? 'var(--warn)' : 'var(--faint)'}>
              {#if m.hint}
                {m.hint}
              {:else if key === 'tds'}
                ПДК 1000
              {:else}
                норма {m.norm[0]}–{m.norm[1]}
              {/if}
            </span>
          </div>
        {/each}
      </div>
    </section>
  {/each}

  <div class="grid charts">
    <section class="plate card">
      <header class="card__head">
        <h2>Минерализация</h2>
        <span class="faint">{series ? `шаг ${Math.round(series.bucket_seconds / 60)} мин` : ''}</span>
      </header>
      <LineChart
        points={series?.points ?? []}
        metrics={['tds']}
        bucketSeconds={series?.bucket_seconds ?? 60}
        height={192}
        {loading}
      />
    </section>

    <section class="plate card">
      <header class="card__head">
        <h2>Температура</h2>
        <span class="faint">по периодам</span>
      </header>
      <BarChart points={series?.points ?? []} metric="temp" height={192} {loading} />
    </section>

    <section class="plate card">
      <header class="card__head">
        <h2>Давление</h2>
        <span class="faint">гПа</span>
      </header>
      <LineChart
        points={series?.points ?? []}
        metrics={['pres']}
        bucketSeconds={series?.bucket_seconds ?? 60}
        height={168}
        {loading}
      />
    </section>

    <section class="plate card">
      <header class="card__head">
        <h2>Питание</h2>
        <span class="faint">напряжение и ток</span>
      </header>
      <LineChart
        points={series?.points ?? []}
        metrics={['vbat', 'ibat']}
        bucketSeconds={series?.bucket_seconds ?? 60}
        height={168}
        {loading}
      />
    </section>
  </div>

  <div class="grid station">
    <section class="plate card">
      <header class="card__head"><h2>Датчики</h2></header>
      <div class="sensors">
        {#each SENSORS as s (s.key)}
          {@const ok = app.device.status?.sensors?.[s.key]}
          <div class="sensors__row">
            <span class="led" class:led--on={ok} class:led--off={ok === false}></span>
            <span class="sensors__name">{s.label}</span>
            <span class="sensors__state faint">
              {#if s.key === 'gps' && ok}
                {sats} спутн.
              {:else if s.key === 'rtc' && ok}
                {timeValid ? TIME_SOURCE_LABELS[timeSource] ?? timeSource : 'не синхронизированы'}
              {:else if ok}
                норма
              {:else if ok === false}
                нет ответа
              {:else}
                выключен
              {/if}
            </span>
          </div>
        {/each}
      </div>

      {#if stability}
        <div class="quality">
          <div class="quality__head">
            <span class="led"
                  class:led--on={stability.state === 'ok'}
                  class:led--warn={stability.state !== 'ok'}></span>
            <span>Достоверность данных</span>
          </div>
          <div class="kv">
            <div class="kv__row">
              <span class="muted">Замеров за период</span>
              <b>{num(stability.total_samples, 0)}</b>
            </div>
            <div class="kv__row">
              <span class="muted">Исключено недостоверных</span>
              <b style:color={stability.suspect_samples ? 'var(--warn)' : 'var(--text)'}>
                {num(stability.suspect_samples, 0)}
              </b>
            </div>
            <div class="kv__row">
              <span class="muted">Резких изменений подтверждено</span>
              <b>{num(stability.confirmed_samples, 0)}</b>
            </div>
            {#if stability.vbat_min}
              <div class="kv__row">
                <span class="muted">Минимум напряжения</span>
                <b style:color={stability.vbat_min < 3.4 ? 'var(--warn)' : 'var(--text)'}>
                  {num(stability.vbat_min, 2)} В
                </b>
              </div>
            {/if}
          </div>
        </div>
      {/if}
    </section>

    <section class="plate card">
      <header class="card__head"><h2>Состояние</h2></header>
      <div class="kv">
        <div class="kv__row"><span class="muted">Прошивка</span><b>{app.device.fw ?? '—'}</b></div>
        <div class="kv__row"><span class="muted">Сигнал WiFi</span><b>{app.device.status?.rssi ?? '—'} dBm</b></div>
        <div class="kv__row"><span class="muted">Время работы</span><b>{duration(app.device.status?.uptime_s)}</b></div>
        <div class="kv__row"><span class="muted">Последняя связь</span><b>{timeAgo(app.device.last_seen)}</b></div>
        <div class="kv__row">
          <span class="muted">Источник времени</span>
          <b style:color={timeValid ? 'var(--text)' : 'var(--warn)'}>
            {timeValid ? TIME_SOURCE_LABELS[timeSource] ?? timeSource : 'нет'}
          </b>
        </div>
        <div class="kv__row">
          <span class="muted">Стабильность</span>
          <b style:color={stability?.state === 'ok' ? 'var(--text)' : 'var(--warn)'}>
            {stability ? `${stability.score}/100` : '—'}
          </b>
        </div>
        <div class="kv__row">
          <span class="muted">Буфер на SD</span>
          <b style:color={spoolFiles ? 'var(--warn)' : 'var(--text)'}>
            {spoolFiles ? `${spoolFiles} файл., ${bytes(app.device.status?.spool_bytes)}` : 'пуст'}
          </b>
        </div>
        <div class="kv__row"><span class="muted">Замеров принято</span><b>{num(app.device.status?.sent ?? 0, 0)}</b></div>
      </div>

      {#if app.device.settings_pending}
        <div class="pending">
          <Icon name="alert" size={16} color="var(--warn)" />
          <span>Настройки отправлены, станция ещё не применила</span>
        </div>
      {/if}

      <div class="actions">
        <button class="btn btn--sm" disabled={busy === 'ping'} onclick={() => run('ping')}>
          <Icon name="signal" size={16} color="var(--dim)" /> Проверить связь
        </button>
        <button class="btn btn--sm" disabled={busy === 'publish_now'} onclick={() => run('publish_now')}>
          <Icon name="refresh" size={16} color="var(--dim)" /> Замер сейчас
        </button>
        <button class="btn btn--sm" disabled={busy === 'sync_time'} onclick={() => run('sync_time')}>
          <Icon name="clock" size={16} color="var(--dim)" /> Синхронизировать часы
        </button>
        <button
          class="btn btn--sm btn--danger"
          onclick={() => (dialog = {
            open: true, cmd: 'reboot',
            title: 'Перезагрузить станцию?',
            description: 'Станция уйдёт в офлайн примерно на двадцать секунд. Данные в буфере на SD не потеряются.'
          })}
        >
          <Icon name="power" size={16} color="var(--danger)" /> Перезагрузить
        </button>
      </div>
    </section>
  </div>
{/if}

<ConfirmDialog
  bind:open={dialog.open}
  title={dialog.title}
  description={dialog.description}
  danger
  confirmLabel="Выполнить"
  onconfirm={() => run(dialog.cmd, true)}
/>

<style>
  .empty { padding: 28px; text-align: center; }
  .empty p { margin: 10px 0 0; font-size: 0.9rem; }

  .toolbar {
    display: flex; align-items: center; gap: 10px; flex-wrap: wrap;
    margin-bottom: var(--gap);
  }
  .toolbar .seg { flex: 1 1 240px; }
  .select { flex: 1 1 180px; width: auto; min-width: 0; }

  .timewarn {
    display: flex; align-items: flex-start; gap: 11px;
    padding: 13px 15px;
    margin-bottom: var(--gap);
    border-radius: var(--r-md);
    background: rgba(240, 178, 103, 0.08);
    border: 1px solid rgba(240, 178, 103, 0.28);
    flex-wrap: wrap;
  }
  .timewarn > div { flex: 1 1 260px; min-width: 0; }
  .timewarn strong { font-size: 0.88rem; color: var(--warn); }
  .timewarn p { margin: 4px 0 0; font-size: 0.8rem; line-height: 1.45; }

  .verdict {
    border-radius: var(--r-lg);
    padding: 18px;
    margin-bottom: var(--gap);
  }
  .verdict--alarm { border-color: rgba(240, 178, 103, 0.34); }

  .verdict__body { display: flex; align-items: flex-start; gap: 16px; }
  .verdict__text { flex: 1 1 auto; min-width: 0; }

  .verdict__tag {
    display: flex; align-items: center; gap: 8px;
    font-size: 0.78rem; font-weight: 500;
    margin-bottom: 7px;
  }
  .tagdot { width: 6px; height: 6px; border-radius: 50%; flex: none; }

  .verdict__head {
    font-size: 1.12rem;
    font-weight: 700;
    letter-spacing: -0.022em;
    line-height: 1.28;
  }
  .verdict__summary { margin: 8px 0 0; font-size: 0.86rem; line-height: 1.5; }
  .verdict__meta { margin: 7px 0 0; font-size: 0.76rem; }

  .ring { position: relative; width: 78px; height: 78px; flex: none; }
  .ring svg { width: 100%; height: 100%; display: block; }
  .ring__bg { fill: none; stroke: rgba(190, 225, 225, 0.12); stroke-width: 11; }
  .ring__fg {
    fill: none; stroke-width: 11; stroke-linecap: round;
    transition: stroke-dashoffset 0.9s var(--ease);
  }
  .ring__val {
    position: absolute; inset: 0;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    line-height: 1.05;
  }
  .ring__val strong { font-size: 1.4rem; font-weight: 700; letter-spacing: -0.03em; }
  .ring__val span { font-size: 0.56rem; }

  .recs {
    margin: 14px 0 0;
    padding-left: 18px;
    font-size: 0.85rem;
    color: var(--dim);
    line-height: 1.5;
  }
  .recs li + li { margin-top: 4px; }

  .group { margin-bottom: var(--gap); }
  .group__head {
    display: flex; align-items: center; gap: 7px;
    margin: 0 0 8px 2px;
  }
  .group__head h2 {
    font-size: 0.76rem;
    font-weight: 600;
    color: var(--dim);
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }

  .readings { grid-template-columns: repeat(2, minmax(0, 1fr)); }

  .reading { padding: 14px 15px; display: flex; flex-direction: column; gap: 5px; }
  .reading--over { border-color: rgba(240, 178, 103, 0.3); }
  .reading__label { font-size: 0.78rem; color: var(--dim); }
  .reading__value {
    font-size: 1.55rem; font-weight: 600; letter-spacing: -0.022em;
    display: flex; align-items: baseline; gap: 4px;
  }
  .reading__unit { font-size: 0.74rem; font-weight: 400; color: var(--dim); }
  .reading__hint { font-size: 0.7rem; }

  .card { padding: 16px 17px; display: flex; flex-direction: column; gap: 12px; }
  .card__head { display: flex; align-items: baseline; gap: 10px; }
  .card__head .faint { margin-left: auto; font-size: 0.74rem; white-space: nowrap; }

  .charts { grid-template-columns: minmax(0, 1fr); margin-bottom: var(--gap); }
  .station { grid-template-columns: minmax(0, 1fr); }

  .sensors { display: flex; flex-direction: column; }
  .sensors__row {
    display: flex; align-items: center; gap: 10px;
    padding: 9px 0;
    border-bottom: 1px solid var(--hair-soft);
    font-size: 0.87rem;
  }
  .sensors__row:last-child { border-bottom: none; }
  .sensors__name { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .sensors__state { margin-left: auto; font-size: 0.8rem; white-space: nowrap; }

  .led { width: 6px; height: 6px; flex: none; border-radius: 50%; background: var(--faint); }
  .led--on { background: var(--accent); }
  .led--off { background: var(--danger); }
  .led--warn { background: var(--warn); }

  .quality {
    margin-top: auto;
    padding-top: 13px;
    border-top: 1px solid var(--hair-soft);
  }
  .quality__head {
    display: flex; align-items: center; gap: 9px;
    font-size: 0.78rem;
    font-weight: 600;
    color: var(--dim);
    margin-bottom: 4px;
  }

  .kv { display: flex; flex-direction: column; }
  .kv__row {
    display: flex; align-items: baseline; gap: 10px;
    padding: 8px 0;
    border-bottom: 1px solid var(--hair-soft);
    font-size: 0.85rem;
  }
  .kv__row:last-child { border-bottom: none; }
  .kv__row b { margin-left: auto; font-weight: 500; text-align: right; }

  .pending {
    display: flex; align-items: flex-start; gap: 8px;
    padding: 9px 11px;
    border-radius: var(--r-sm);
    background: rgba(240, 178, 103, 0.1);
    border: 1px solid rgba(240, 178, 103, 0.28);
    color: var(--warn);
    font-size: 0.8rem;
    line-height: 1.4;
  }

  .actions { display: flex; flex-wrap: wrap; gap: 8px; }
  .actions .btn { flex: 1 1 auto; }

  @media (min-width: 620px) {
    .readings { grid-template-columns: repeat(var(--cols, 3), minmax(0, 1fr)); }
    .actions .btn { flex: 0 0 auto; }
  }

  @media (min-width: 900px) {
    .verdict { padding: 24px 26px; }
    .verdict__head { font-size: 1.5rem; letter-spacing: -0.03em; }
    .verdict__body { gap: 28px; align-items: center; }
    .ring { width: 116px; height: 116px; }
    .ring__val strong { font-size: 2rem; }
    .ring__val span { font-size: 0.64rem; }
    .reading__value { font-size: 1.85rem; }
    .card { padding: 19px 21px; }
    .charts { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .station { grid-template-columns: minmax(0, 1fr) minmax(0, 1.25fr); }
    .toolbar .seg { flex: 0 0 auto; }
    .select { flex: 0 0 auto; min-width: 190px; }
  }
</style>

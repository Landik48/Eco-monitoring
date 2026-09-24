<script>
  import { api } from '../lib/api.js';
  import { app, toast } from '../lib/store.svelte.js';
  import { dateTime, timeAgo, num } from '../lib/format.js';
  import Icon from '../components/Icon.svelte';

  let items = $state([]);
  let loading = $state(true);
  let editing = $state(null);
  let draft = $state('');

  async function load() {
    if (!app.device) { loading = false; return; }
    loading = true;
    try {
      items = await api.locations(app.device.device_id);
      app.locations = items;
    } catch (e) {
      toast(e.message, 'error');
    } finally {
      loading = false;
    }
  }

  function startEdit(loc) { editing = loc.id; draft = loc.name; }

  async function rename(loc) {
    const name = draft.trim();
    editing = null;
    if (!name || name === loc.name) return;
    try {
      await api.renameLocation(loc.id, name);
      toast('Локация переименована', 'ok');
      await load();
    } catch (e) {
      toast(e.message, 'error');
    }
  }

  function openCharts(loc) {
    app.locationId = loc.id;
    location.hash = '#/charts';
  }

  const grouped = $derived.by(() => {
    const map = new Map();
    for (const loc of items) {
      const key = loc.region || loc.country || 'Без региона';
      if (!map.has(key)) map.set(key, []);
      map.get(key).push(loc);
    }
    return [...map.entries()];
  });

  function focusAndSelect(node) { node.focus(); node.select(); }

  $effect(() => { app.device?.device_id; load(); });
</script>

{#if loading}
  <div class="plate" style="padding:18px"><div class="skeleton" style="height:140px"></div></div>
{:else if !items.length}
  <div class="plate empty">
    <h2>Локаций пока нет</h2>
    <p class="muted">
      Раздел заполняется сам, когда станция впервые пришлёт координаты с достоверным
      фиксом GPS. В помещении фикса обычно не бывает: нужен открытый небосвод и до
      пятнадцати минут на холодный старт.
    </p>
  </div>
{:else}
  {#each grouped as [region, locs] (region)}
    <h3 class="region">{region}</h3>
    <div class="grid list">
      {#each locs as loc (loc.id)}
        <article class="plate loc" class:loc--active={app.locationId === loc.id}>
          <header class="loc__head">
            {#if editing === loc.id}
              <input
                class="input"
                bind:value={draft}
                onblur={() => rename(loc)}
                onkeydown={(e) => {
                  if (e.key === 'Enter') rename(loc);
                  if (e.key === 'Escape') editing = null;
                }}
                use:focusAndSelect
              />
            {:else}
              <h2>{loc.name}</h2>
              <button class="btn btn--sm btn--ghost edit" onclick={() => startEdit(loc)} aria-label="Переименовать">
                <Icon name="edit" size={16} color="var(--dim)" />
              </button>
            {/if}
          </header>

          {#if loc.city || loc.region}
            <p class="faint place">{[loc.city, loc.region, loc.country].filter(Boolean).join(', ')}</p>
          {/if}
          <p class="faint coords">{loc.lat.toFixed(5)}, {loc.lon.toFixed(5)}</p>

          <div class="stats">
            <span><b>{num(loc.measurements, 0)}</b> замеров</span>
            <span><b>{num(loc.points, 0)}</b> точек GPS</span>
          </div>

          <p class="faint times">
            Впервые {dateTime(loc.first_seen)}<br />
            Последний раз {timeAgo(loc.last_seen)}
          </p>

          <div class="loc__foot">
            <button class="btn btn--sm" onclick={() => openCharts(loc)}>Графики локации</button>
            <a
              class="btn btn--sm btn--ghost"
              href={`https://www.openstreetmap.org/?mlat=${loc.lat}&mlon=${loc.lon}#map=14/${loc.lat}/${loc.lon}`}
              target="_blank"
              rel="noopener noreferrer"
            >На карте</a>
          </div>
        </article>
      {/each}
    </div>
  {/each}

  <p class="faint note">
    Точки группируются по радиусу, а не по названию населённого пункта: станция может
    стоять в поле между двумя посёлками, и дробить историю по капризам геокодера незачем.
    Если название определилось неверно, поправьте его вручную — на данные это не влияет.
  </p>
{/if}

<style>
  .empty { padding: 26px 20px; text-align: center; }
  .empty p { margin: 10px auto 0; max-width: 46ch; font-size: 0.88rem; line-height: 1.55; }

  .region { margin: 0 0 10px; color: var(--dim); font-size: 0.82rem; }
  .region:not(:first-child) { margin-top: 22px; }

  .list { grid-template-columns: minmax(0, 1fr); }

  .loc { padding: 16px 17px; display: flex; flex-direction: column; gap: 4px; }
  .loc--active { border-color: rgba(95, 208, 174, 0.4); }

  .loc__head { display: flex; align-items: center; gap: 8px; min-width: 0; }
  .loc__head h2 { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .edit { flex: none; min-height: 32px; padding: 0 8px; margin-left: auto; }

  .place, .coords, .times { margin: 0; }
  .place { font-size: 0.82rem; }
  .coords { font-size: 0.76rem; }

  .stats { display: flex; flex-wrap: wrap; gap: 6px 18px; margin: 10px 0 6px; font-size: 0.78rem; color: var(--dim); }
  .stats b { font-size: 1.08rem; color: var(--accent); font-weight: 600; margin-right: 3px; }

  .times { font-size: 0.74rem; line-height: 1.55; }

  .loc__foot { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
  .loc__foot .btn { flex: 1 1 auto; }

  .note { margin: 20px 0 0; font-size: 0.78rem; max-width: 62ch; line-height: 1.6; }

  @media (min-width: 620px) {
    .loc__foot .btn { flex: 0 0 auto; }
  }
  @media (min-width: 900px) {
    .list { grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); }
  }
</style>

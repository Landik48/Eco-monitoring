<script>
  import { api, setToken, token } from './lib/api.js';
  import { session, app, toast } from './lib/store.svelte.js';
  import Login from './routes/Login.svelte';
  import Dashboard from './routes/Dashboard.svelte';
  import Charts from './routes/Charts.svelte';
  import Settings from './routes/Settings.svelte';
  import Logs from './routes/Logs.svelte';
  import Locations from './routes/Locations.svelte';
  import Toasts from './components/Toasts.svelte';
  import Icon from './components/Icon.svelte';
  import { timeAgo } from './lib/format.js';

  const ROUTES = [
    { path: 'dashboard', label: 'Сводка',    icon: 'gauge',  component: Dashboard },
    { path: 'charts',    label: 'Графики',   icon: 'chart',  component: Charts },
    { path: 'locations', label: 'Локации',   icon: 'pin',    component: Locations },
    { path: 'settings',  label: 'Настройки', icon: 'slider', component: Settings },
    { path: 'logs',      label: 'Журнал',    icon: 'list',   component: Logs }
  ];

  let route = $state(location.hash.slice(2) || 'dashboard');
  const current = $derived(ROUTES.find((r) => r.path === route) ?? ROUTES[0]);

  function go(path) { location.hash = `#/${path}`; }

  function logout() {
    setToken(null);
    session.token = null;
    session.user = null;
  }

  async function loadShared() {
    try {
      const devices = await api.devices();
      app.device = devices[0] ?? null;
      if (app.device) app.locations = await api.locations(app.device.device_id);
      app.lastRefresh = new Date();
    } catch (e) {
      if (e.status !== 401) toast(e.message, 'error');
    }
  }

  $effect(() => {
    const onHash = () => { route = location.hash.slice(2) || 'dashboard'; };
    window.addEventListener('hashchange', onHash);
    window.addEventListener('envmon:logout', logout);
    return () => {
      window.removeEventListener('hashchange', onHash);
      window.removeEventListener('envmon:logout', logout);
    };
  });

  $effect(() => {
    if (!token) { session.ready = true; return; }
    session.token = token;
    api.me()
      .then((user) => { session.user = user; })
      .catch(() => {})
      .finally(() => { session.ready = true; });
  });

  $effect(() => {
    if (!session.user) return;
    loadShared();
  });

  $effect(() => {
    if (!session.user || !app.autoRefresh) return;
    const id = setInterval(loadShared, app.refreshMs);
    return () => clearInterval(id);
  });

  const online = $derived(app.device?.online ?? false);
</script>

{#if !session.user}
  {#if session.ready}<Login />{/if}
{:else}
  <div class="shell">

    <aside class="sidebar">
      <div class="brand">
        <span class="brand__mark"><Icon name="drop" size={17} color="var(--accent-ink)" stroke={2} /></span>
        <span class="brand__text">
          <strong>{app.device?.name ?? 'Станция'}</strong>
          <span class="faint">{app.device?.device_id ?? ''}</span>
        </span>
      </div>

      <nav aria-label="Разделы">
        {#each ROUTES as r (r.path)}
          <button
            class="navitem"
            class:navitem--active={route === r.path}
            class:glass={route === r.path}
            aria-current={route === r.path ? 'page' : undefined}
            onclick={() => go(r.path)}
          >
            <Icon name={r.icon} size={20} color={route === r.path ? 'var(--accent)' : 'var(--dim)'} />
            <span>{r.label}</span>
          </button>
        {/each}
      </nav>

      <div class="status plate">
        <div class="status__row">
          <span class="led" class:led--on={online}></span>
          <span>{online ? 'На связи' : 'Нет связи'}</span>
        </div>
        <div class="faint status__meta">
          {#if app.device?.status?.time_valid}
            Часы по {app.device.status.time_source === 'gps' ? 'GPS' : app.device.status.time_source === 'server' ? 'серверу' : 'RTC'}
          {:else}
            Часы не выставлены
          {/if}
          <br />
          {app.device?.last_seen ? `Связь ${timeAgo(app.device.last_seen)}` : 'Связи ещё не было'}
        </div>
      </div>

      <button class="btn btn--ghost btn--sm logout" onclick={logout}>
        <Icon name="logout" size={17} color="var(--dim)" />
        Выйти
      </button>
    </aside>

    <main>
      <header class="topbar glass">
        <div class="topbar__title">
          <h1>{current.label}</h1>
          <p class="faint">{app.device?.name ?? 'Станция не найдена'}</p>
        </div>
        <span class="spacer"></span>
        <span class="pill" class:pill--on={online}>
          <span class="led" class:led--on={online}></span>
          <span class="pill__text">{online ? 'На связи' : 'Нет связи'}</span>
        </span>
        <span class="avatar" title={session.user.email}>
          {session.user.email[0].toUpperCase()}
        </span>
      </header>

      <div class="page">
        {#key route}
          {@const Page = current.component}
          <div class="page-enter"><Page /></div>
        {/key}
      </div>
    </main>

    <nav class="tabbar glass" aria-label="Разделы">
      {#each ROUTES as r (r.path)}
        <button
          class="tab"
          class:tab--active={route === r.path}
          aria-current={route === r.path ? 'page' : undefined}
          onclick={() => go(r.path)}
        >
          <Icon name={r.icon} size={21} color={route === r.path ? 'var(--accent)' : 'var(--faint)'} />
          <span>{r.label}</span>
        </button>
      {/each}
    </nav>
  </div>
{/if}

<Toasts />

<style>
  .shell { position: relative; z-index: 1; min-height: 100dvh; display: flex; }

  .sidebar { display: none; }

  .brand { display: flex; align-items: center; gap: 11px; padding: 0 8px; }
  .brand__mark {
    width: 30px; height: 30px; flex: none;
    border-radius: var(--r-sm);
    background: var(--accent);
    display: flex; align-items: center; justify-content: center;
  }
  .brand__text { display: flex; flex-direction: column; min-width: 0; line-height: 1.25; }
  .brand__text strong { font-size: 0.94rem; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .brand__text span { font-size: 0.74rem; }

  .sidebar nav { display: flex; flex-direction: column; gap: 4px; }

  .navitem {
    display: flex; align-items: center; gap: 12px;
    min-height: var(--tap);
    padding: 0 14px;
    border: 1px solid transparent;
    border-radius: 12px;
    background: transparent;
    color: var(--dim);
    font-size: 0.94rem;
    text-align: left;
    cursor: pointer;
    transition: background 0.2s, color 0.2s;
  }
  .navitem:hover { background: var(--glass); color: var(--text); }
  .navitem--active { color: var(--text); font-weight: 500; }
  .navitem:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }

  .status { margin-top: auto; padding: 13px 15px; display: flex; flex-direction: column; gap: 7px; }
  .status__row { display: flex; align-items: center; gap: 9px; font-size: 0.85rem; font-weight: 500; }
  .status__meta { font-size: 0.75rem; line-height: 1.5; }

  .led {
    width: 7px; height: 7px; flex: none;
    border-radius: 50%;
    background: var(--faint);
  }
  .led--on { background: var(--accent); box-shadow: 0 0 0 4px rgba(95, 208, 174, 0.16); }

  .logout { justify-content: flex-start; padding-left: 12px; }

  main { flex: 1 1 auto; min-width: 0; display: flex; flex-direction: column; }

  .topbar {
    display: flex; align-items: center; gap: 12px;
    padding: 12px var(--pad);
    position: sticky; top: 0; z-index: 30;
    border-width: 0 0 1px 0;
    box-shadow: none;
  }
  .topbar__title { min-width: 0; }
  .topbar__title p {
    margin: 1px 0 0;
    font-size: 0.78rem;
    overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  }

  .pill {
    display: inline-flex; align-items: center; gap: 7px;
    padding: 6px 12px;
    border-radius: 999px;
    border: 1px solid var(--hair);
    background: var(--glass);
    font-size: 0.78rem;
    white-space: nowrap;
  }
  .pill--on { color: var(--accent); border-color: rgba(95, 208, 174, 0.3); }
  .pill__text { display: none; }

  .avatar {
    width: 34px; height: 34px; flex: none;
    border-radius: 50%;
    border: 1px solid var(--hair);
    background: var(--glass);
    display: flex; align-items: center; justify-content: center;
    font-size: 0.84rem; font-weight: 600;
    color: var(--accent);
  }

  .page {
    padding: var(--pad);

    padding-bottom: calc(var(--tabbar) + 28px + var(--safe-b));
    width: 100%;
    max-width: 1460px;
  }

  .tabbar {
    position: fixed; left: 0; right: 0; bottom: 0; z-index: 40;
    display: flex;
    flex-direction: row;
    align-items: stretch;
    padding: 6px 4px calc(6px + var(--safe-b));
    border-width: 1px 0 0 0;
    border-radius: 0;
  }
  .tab {
    flex: 1 1 0;
    min-width: 0;
    min-height: 48px;
    display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 3px;
    border: 0; background: transparent;
    color: var(--faint);
    cursor: pointer;
    border-radius: var(--r-sm);
    transition: color 0.2s;
  }
  .tab span {
    font-size: 0.62rem;
    max-width: 100%;
    overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  }
  .tab--active { color: var(--accent); }
  .tab--active span { font-weight: 600; }
  .tab:focus-visible { outline: 2px solid var(--accent); outline-offset: -2px; }

  @media (min-width: 900px) {
    .sidebar {
      display: flex;
      flex-direction: column;
      gap: 24px;
      width: var(--sidebar);
      flex: none;
      padding: 20px 14px;
      border-right: 1px solid var(--hair);
      position: sticky;
      top: 0;
      height: 100dvh;
    }

    .tabbar { display: none; }
    .page { padding-bottom: 32px; }
    .pill__text { display: inline; }
  }
</style>

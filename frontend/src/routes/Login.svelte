<script>
  import { api, setToken } from '../lib/api.js';
  import { session } from '../lib/store.svelte.js';
  import Icon from '../components/Icon.svelte';

  let email = $state('');
  let password = $state('');
  let busy = $state(false);
  let error = $state('');

  async function submit(event) {
    event.preventDefault();
    if (busy) return;
    busy = true;
    error = '';
    try {
      const { access_token } = await api.login(email.trim(), password);
      setToken(access_token);
      session.token = access_token;
      session.user = await api.me();
    } catch (e) {
      error = e.message;
      password = '';
    } finally {
      busy = false;
    }
  }
</script>

<div class="screen">
  <form class="panel glass" onsubmit={submit}>
    <div class="head">
      <span class="mark"><Icon name="drop" size={20} color="var(--accent-ink)" stroke={2} /></span>
      <div>
        <h1>Мониторинг среды</h1>
        <p class="faint">Панель управления станцией</p>
      </div>
    </div>

    <label class="label" for="email">Email</label>
    <input id="email" class="input" type="text" bind:value={email} autocomplete="username" required />

    <label class="label" for="pwd">Пароль</label>
    <input id="pwd" class="input" type="password" bind:value={password} autocomplete="current-password" required />

    {#if error}<div class="error" role="alert">{error}</div>{/if}

    <button class="btn btn--primary submit" type="submit" disabled={busy}>
      {#if busy}<span class="spinner"></span>{/if}
      Войти
    </button>
  </form>
</div>

<style>
  .screen {
    position: relative; z-index: 1;
    min-height: 100dvh;
    display: flex; align-items: center; justify-content: center;
    padding: 18px;
  }
  .panel {
    width: 100%;
    max-width: 400px;
    border-radius: var(--r-lg);
    padding: 24px;
  }
  .head { display: flex; align-items: center; gap: 13px; margin-bottom: 22px; }
  .head h1 { font-size: 1.24rem; }
  .head p { margin: 2px 0 0; font-size: 0.82rem; }

  .mark {
    width: 38px; height: 38px; flex: none;
    border-radius: 12px;
    background: var(--accent);
    display: flex; align-items: center; justify-content: center;
  }

  .label { margin-top: 14px; }
  .label:first-of-type { margin-top: 0; }

  .error {
    margin-top: 14px;
    padding: 9px 12px;
    border-radius: var(--r-sm);
    background: rgba(240, 138, 128, 0.12);
    border: 1px solid rgba(240, 138, 128, 0.32);
    color: var(--danger);
    font-size: 0.85rem;
  }

  .submit { width: 100%; margin-top: 20px; }
</style>

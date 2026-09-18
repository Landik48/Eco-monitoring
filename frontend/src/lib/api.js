const TOKEN_KEY = 'envmon.token';

export let token = localStorage.getItem(TOKEN_KEY) || null;

export function setToken(value) {
  token = value;
  if (value) localStorage.setItem(TOKEN_KEY, value);
  else localStorage.removeItem(TOKEN_KEY);
}

export class ApiError extends Error {
  constructor(status, detail) {
    super(typeof detail === 'string' ? detail : detail?.error || 'Ошибка запроса');
    this.status = status;
    this.detail = detail;
  }
}

async function request(path, { method = 'GET', body, params, raw = false } = {}) {
  const url = new URL(path, location.origin);
  if (params) {
    for (const [k, v] of Object.entries(params)) {
      if (v !== undefined && v !== null && v !== '') url.searchParams.set(k, v);
    }
  }

  const headers = {};
  if (body) headers['Content-Type'] = 'application/json';
  if (token) headers.Authorization = `Bearer ${token}`;

  const res = await fetch(url, { method, headers, body: body ? JSON.stringify(body) : undefined });

  if (res.status === 401) {
    setToken(null);
    window.dispatchEvent(new CustomEvent('envmon:logout'));
    throw new ApiError(401, 'Сессия истекла, войдите заново');
  }
  if (!res.ok) {
    let detail = res.statusText;
    try { detail = (await res.json()).detail ?? detail; } catch {}
    throw new ApiError(res.status, detail);
  }
  if (raw) return res;
  if (res.status === 204) return null;
  return res.json();
}

export const api = {
  login: (email, password) => request('/api/auth/login', { method: 'POST', body: { email, password } }),
  me: () => request('/api/auth/me'),
  users: () => request('/api/auth/users'),
  createUser: (email, password) => request('/api/auth/users', { method: 'POST', body: { email, password } }),

  devices: () => request('/api/devices'),
  device: (id) => request(`/api/devices/${id}`),

  settings: (id) => request(`/api/devices/${id}/settings`),
  saveSettings: (id, settings, confirm = false) =>
    request(`/api/devices/${id}/settings`, { method: 'PUT', body: { settings, confirm } }),

  commands: (id) => request(`/api/devices/${id}/commands`),
  sendCommand: (id, cmd, confirm = false, args = {}) =>
    request(`/api/devices/${id}/commands`, { method: 'POST', body: { cmd, args, confirm } }),

  series: (params) => request('/api/measurements/series', { params }),
  latest: (device_id) => request('/api/measurements/latest', { params: { device_id } }),
  exportUrl: (params) => {
    const url = new URL('/api/measurements/export.csv', location.origin);
    for (const [k, v] of Object.entries(params)) if (v) url.searchParams.set(k, v);
    return url.toString();
  },
  exportCsv: (params) => request('/api/measurements/export.csv', { params, raw: true }),

  analysis: (params) => request('/api/analysis', { params }),
  locations: (device_id) => request('/api/locations', { params: { device_id } }),
  renameLocation: (id, name) => request(`/api/locations/${id}`, { method: 'PATCH', params: { name } }),

  logs: (params) => request('/api/logs', { params }),
  logsSummary: (params) => request('/api/logs/summary', { params })
};

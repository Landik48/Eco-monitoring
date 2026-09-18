export function num(value, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(value)) return '—';
  return Number(value).toLocaleString('ru-RU', {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  });
}

export function bytes(value) {
  if (!value) return '0 Б';
  const units = ['Б', 'КБ', 'МБ', 'ГБ'];
  let i = 0;
  let v = value;
  while (v >= 1024 && i < units.length - 1) { v /= 1024; i++; }
  return `${v.toFixed(i ? 1 : 0)} ${units[i]}`;
}

export function duration(seconds) {
  if (!seconds && seconds !== 0) return '—';
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d) return `${d} д ${h} ч`;
  if (h) return `${h} ч ${m} мин`;
  return `${m} мин`;
}

export function dateTime(value) {
  if (!value) return '—';
  return new Date(value).toLocaleString('ru-RU', {
    day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
}

export function timeAgo(value) {
  if (!value) return 'никогда';
  const diff = (Date.now() - new Date(value).getTime()) / 1000;
  if (diff < 5) return 'только что';
  if (diff < 60) return `${Math.floor(diff)} с назад`;
  if (diff < 3600) return `${Math.floor(diff / 60)} мин назад`;
  if (diff < 86400) return `${Math.floor(diff / 3600)} ч назад`;
  return `${Math.floor(diff / 86400)} д назад`;
}

export function axisTime(iso, bucketSeconds) {
  const d = new Date(iso);
  if (bucketSeconds >= 86400) return d.toLocaleDateString('ru-RU', { day: '2-digit', month: 'short' });
  if (bucketSeconds >= 3600) return d.toLocaleString('ru-RU', { day: '2-digit', hour: '2-digit', minute: '2-digit' }).replace(',', '');
  return d.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
}

export const LEVEL_COLORS = {
  good: 'var(--ok)',
  moderate: '#9fd3b0',
  elevated: 'var(--warn)',
  high: '#ef9f6c',
  hazard: 'var(--danger)',
  unknown: 'var(--text-faint)'
};

export const LOG_COLORS = {
  debug: 'var(--text-faint)',
  info: 'var(--info)',
  warn: 'var(--warn)',
  error: 'var(--danger)'
};

export const session = $state({
  token: null,
  user: null,
  ready: false
});

export const app = $state({
  device: null,
  locations: [],
  locationId: null,
  autoRefresh: true,
  refreshMs: 15000,
  lastRefresh: null
});

export const toasts = $state({ items: [] });

let toastSeq = 0;

export function toast(message, kind = 'info', ttl = 4200) {
  const id = ++toastSeq;
  toasts.items.push({ id, message, kind });
  setTimeout(() => {
    const i = toasts.items.findIndex((t) => t.id === id);
    if (i >= 0) toasts.items.splice(i, 1);
  }, ttl);
}

export const METRICS = {
  temp: {
    label: 'Температура',
    short: 'Темп.',
    unit: '°C',
    color: 'var(--accent)',
    section: 'Микроклимат',
    axis: [-20, 45],
    norm: [12, 28],
    digits: 1
  },
  hum: {
    label: 'Влажность',
    short: 'Влажн.',
    unit: '%',
    color: 'var(--info)',
    section: 'Микроклимат',
    axis: [0, 100],
    norm: [30, 60],
    digits: 0
  },
  pres: {
    label: 'Давление',
    short: 'Давл.',
    unit: 'гПа',
    color: '#a8a0e0',
    section: 'Микроклимат',
    axis: [960, 1060],
    norm: [1000, 1025],
    digits: 0
  },
  tds: {
    label: 'Минерализация',
    short: 'TDS',
    unit: 'мг/л',
    color: 'var(--warn)',
    section: 'Вода',
    axis: [0, 1500],
    norm: [0, 1000],
    digits: 0,
    limit: 1000,
    limitLabel: 'ПДК 1000'
  },
  ec: {
    label: 'Электропроводность',
    short: 'EC',
    unit: 'мкСм/см',
    color: '#8fd9c4',
    section: 'Вода',
    axis: [0, 2500],
    norm: [0, 1800],
    digits: 0
  },
  vbat: {
    label: 'Напряжение',
    short: 'Батарея',
    unit: 'В',
    color: '#e0a458',
    section: 'Питание',
    axis: [3.2, 4.3],
    norm: [3.4, 4.3],
    digits: 2
  },
  ibat: {
    label: 'Ток',
    short: 'Ток',
    unit: 'А',
    color: '#d98b8b',
    section: 'Питание',
    axis: [-2, 2],
    norm: [-1, 1],
    digits: 3
  },
  speed: {
    label: 'Скорость',
    short: 'Скор.',
    unit: 'км/ч',
    color: '#8fb4ff',
    section: 'Движение',
    axis: [0, 120],
    norm: [0, 90],
    digits: 1
  },
  alt: {
    label: 'Высота',
    short: 'Высота',
    unit: 'м',
    color: '#b6d98f',
    section: 'Движение',
    axis: [-50, 1000],
    norm: [0, 800],
    digits: 0
  },
  sats: {
    label: 'Спутники',
    short: 'Спутн.',
    unit: '',
    color: '#9fd3b0',
    section: 'Движение',
    axis: [0, 16],
    norm: [4, 16],
    digits: 0
  }
};

export const SUMMARY_METRICS = ['temp', 'hum', 'tds', 'vbat'];

export const SECTIONS = ['Микроклимат', 'Вода', 'Питание', 'Движение'];

export function sectionMetrics(section) {
  const out = [];
  for (const [key, m] of Object.entries(METRICS)) {
    if (m.section === section) out.push(key);
  }
  return out;
}

export const RANGES = [
  { key: '6h', label: '6 ч', hours: 6 },
  { key: '24h', label: '24 ч', hours: 24 },
  { key: '7d', label: '7 дней', hours: 24 * 7 },
  { key: '30d', label: '30 дней', hours: 24 * 30 }
];

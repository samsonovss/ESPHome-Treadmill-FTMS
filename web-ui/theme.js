'use strict';
// Theme — the ONE place for every colour of the web UI (dark theme).
// Code never writes colours: CSS uses var(--token) or color-mix(in srgb,var(--token) N%,transparent).
// Canvas, Leaflet and SVG attributes can't read CSS variables — they pass the same strings
// through Theme.resolve(), which returns the real colour.
const Theme = (() => {
  const tokens = {
    // Surfaces — page, cards, lines
    bg: '#0b0d10',
    panel: '#14171c',
    panel2: '#1a1e24',
    surface: '#14171c',
    line: '#262b33',
    border: '#323944',
    glow: '#16202c',
    // Text and ink
    text: '#e6e9ee',
    muted: '#8a929e',
    sub: '#c3c8d0',
    ink: '#ffffff',
    shade: '#000000',  // shadows, masks
    veil: '#000000',   // dimming plates behind text
    onaccent: '#0b0d10',
    // Brand
    accent: '#5ec4ff',
    // Data — HR zones, speed, incline, charts
    z1: '#3b82c4',
    z2: '#3fae5a',
    z3: '#e0a23a',
    z4: '#e5603c',
    z5: '#c0304a',
    speed: '#73bf69',
    inc: '#f2cc0c',
    'chart-speed': '#54a9ff',
    'chart-hr': '#e18b48',
    'chart-incline': '#4bd18b',
    // Neutrals
    'gray-3': '#05070a',
    'gray-7': '#111111',
    'gray-9': '#161616',
    'gray-13': '#1c1f24',
    'gray-14': '#1f2328',
    'gray-27': '#3a4452',
    'gray-40': '#5d6470',
    'gray-41': '#5d6672',
    'gray-46': '#6b7280',
    'gray-53': '#888888',
    'gray-57': '#8e8e93',
    'gray-59': '#8a96a3',
    'gray-60': '#88aa99',
    // Reds — danger, delete, overheat
    'red-15': '#3a1519',
    'red-27': '#6b2029',
    'red-29': '#633035',
    'red-33': '#891d26',
    'red-39': '#785051',
    'red-40': '#a8263e',
    'red-50': '#d24f2c',
    'red-57': '#e5483c',
    'red-58': '#c47765',
    'red-65': '#c68389',
    'red-66': '#df7373',
    'red-68': '#ff5a6e',
    'red-70': '#ff8a65',
    'red-71': '#ff6b6b',
    'red-72': '#ff7a6e',
    'red-74': '#ff8a7a',
    'red-74a': '#ff9a7a',
    'red-75': '#ff9d7d',
    'red-75a': '#ff8a80',
    'red-76': '#ee9994',
    'red-77': '#ff8a8a',
    'red-80': '#ff9aa6',
    'red-95': '#ffe3e3',
    // Oranges — warnings, temperature
    'orange-8': '#2a1b00',
    'orange-9': '#20160f',
    'orange-14': '#3a1d0c',
    'orange-20': '#684300',
    'orange-45': '#c98a1c',
    'orange-54': '#fc6719',
    'orange-55': '#ef8a2c',
    'orange-58': '#ff8a2a',
    'orange-64': '#fc8a4c',
    'orange-65': '#ffa24f',
    'orange-69': '#f4c46c',
    'orange-71': '#ffb86b',
    'orange-74': '#ffd27a',
    // Ambers / yellows
    'amber-39': '#c79a00',
    'amber-50': '#b89b46',
    'amber-53': '#facc15',
    'amber-55': '#e0b23a',
    'amber-55a': '#e0c23a',
    'amber-57': '#d6a84b',
    'amber-61': '#f5c542',
    'amber-65': '#f0d35a',
    'amber-90': '#fff1cb',
    'amber-93': '#f2efe9',
    // Greens — ok, in zone
    'green-20': '#15522e',
    'green-37': '#2f8f3a',
    'green-39': '#2f9a4a',
    'green-49': '#56a574',
    'green-52': '#4fbb83',
    'green-60': '#5fd17a',
    'green-65': '#7ad48d',
    'green-66': '#7fd18f',
    'green-68': '#7fdc8f',
    'green-72': '#8fe0a0',
    'green-92': '#def7e8',
    // Teals
    'teal-80': '#9be7ff',
    // Blues — info, belt, routes
    'blue-6a': '#06121a',
    'blue-7': '#0f1216',
    'blue-8': '#11151a',
    'blue-8a': '#11141a',
    'blue-10': '#0f1824',
    'blue-12': '#0b2230',
    'blue-13': '#16222e',
    'blue-13a': '#0c2238',
    'blue-14': '#1a222c',
    'blue-21': '#2a3442',
    'blue-23': '#2f3a46',
    'blue-36': '#1f6f9a',
    'blue-44': '#2f6fb0',
    'blue-49': '#4699b5',
    'blue-50': '#2aa9d6',
    'blue-62': '#3d9bff',
    'blue-63': '#7aa2c8',
    'blue-65': '#4cc3ff',
    'blue-65a': '#6fa8dc',
    'blue-72': '#6e9fff',
    // Violets
    'violet-76': '#c58af9',
  };
  // 'var(--x)', 'var(--x)AA' or 'color-mix(in srgb,var(--x) N%,transparent)' → '#rrggbb[aa]'; anything else unchanged.
  function resolve(v) {
    if (typeof v !== 'string' || v[0] === '#') return v;
    let a = 1, n, m = /^var\(--([\w-]+)\)([0-9a-f]{2})?$/i.exec(v);
    if (m) { n = m[1]; if (m[2]) a = parseInt(m[2], 16) / 255; }
    else if ((m = /^color-mix\(in srgb,var\(--([\w-]+)\) ([\d.]+)%,transparent\)$/.exec(v))) { n = m[1]; a = +m[2] / 100; }
    else return v;
    const c = tokens[n]; if (!c) return v;
    a *= c.length > 7 ? parseInt(c.slice(7, 9), 16) / 255 : 1;
    return a >= 1 ? c.slice(0, 7) : c.slice(0, 7) + Math.round(a * 255).toString(16).padStart(2, '0');
  }
  // Every token becomes a CSS variable on <html>; the shadow-root page inherits them.
  function apply(el) {
    for (const [n, c] of Object.entries(tokens)) el.style.setProperty('--' + n, c);
    el.style.colorScheme = 'dark';
  }
  return {tokens, resolve, apply};
})();
if (typeof module !== 'undefined') module.exports = Theme;

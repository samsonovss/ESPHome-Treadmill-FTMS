// Install-only service worker: makes the page installable over HTTPS (reverse proxy).
// No fetch handler and no cache on purpose: live data (/events) and the UI always come from the board,
// so a firmware update is never hidden behind a stale cached page.
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

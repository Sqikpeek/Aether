import { translations } from './i18n.js';

// Application State
const state = {
  config: null,
  containers: [],
  activeContainer: null,
  mcVersions: [],
  javaRuntimes: [],
  installedMods: [],
  currentTab: 'containers',
  lang: 'ru',
  gameState: 'idle',
  eventSource: null,
  updateManifest: null,
  isWindowFocused: true
};

// Initialize App
document.addEventListener('DOMContentLoaded', async () => {
  setupEventListeners();
  setupLowCpuListeners();
  await loadInitialData();
  setupEventSource();
});

// Setup Low CPU Window Listeners
function setupLowCpuListeners() {
  // Desktop Heartbeat
  setInterval(() => {
    fetch('/api/heartbeat').catch(() => {});
  }, 3000);

  // Quit App Button
  const quitBtn = document.getElementById('quitAppBtn');
  if (quitBtn) {
    quitBtn.addEventListener('click', async () => {
      const msg = state.lang === 'ru' ? 'Вы уверены, что хотите выйти из Aether Launcher?' : 'Are you sure you want to quit Aether Launcher?';
      if (confirm(msg)) {
        await fetch('/api/app/quit', { method: 'POST' }).catch(() => {});
        window.close();
      }
    });
  }

  window.addEventListener('focus', () => {
    state.isWindowFocused = true;
    document.body.classList.remove('low-cpu-active');
  });

  window.addEventListener('blur', () => {
    state.isWindowFocused = false;
    if (state.config?.lowCpuMode) {
      document.body.classList.add('low-cpu-active');
    }
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden && state.config?.lowCpuMode) {
      document.body.classList.add('low-cpu-active');
    } else if (!document.hidden) {
      document.body.classList.remove('low-cpu-active');
    }
  });
}

// Server-Sent Events (Zero Polling CPU usage!)
function setupEventSource() {
  if (state.eventSource) {
    state.eventSource.close();
  }

  state.eventSource = new EventSource('/api/events');

  state.eventSource.addEventListener('state', e => {
    const data = JSON.parse(e.data);
    updateGameState(data.state);
  });

  state.eventSource.addEventListener('log', e => {
    const entry = JSON.parse(e.data);
    appendLogLine(entry);
  });

  state.eventSource.addEventListener('crashed', e => {
    const data = JSON.parse(e.data);
    updateGameState('crashed');
    showCrashDiagnostic(data.diagnostic);
  });

  state.eventSource.addEventListener('stopped', () => {
    updateGameState('idle');
  });

  state.eventSource.addEventListener('update_progress', e => {
    const progress = JSON.parse(e.data);
    renderUpdateProgress(progress);
  });

  state.eventSource.onerror = () => {
    setTimeout(setupEventSource, 5000);
  };
}

async function loadInitialData() {
  try {
    // 1. Load Config
    const cfgRes = await fetch('/api/config');
    state.config = await cfgRes.json();
    state.lang = state.config.language || 'ru';
    applyLanguage(state.lang);
    applyTheme(state.config.theme || 'void-purple');

    // 2. Load Minecraft Versions
    const mcRes = await fetch('/api/minecraft/versions');
    state.mcVersions = await mcRes.json();
    populateVersionSelects();

    // 3. Load Containers
    await loadContainers();

    // 4. Load Java Runtimes
    const javaRes = await fetch('/api/java');
    state.javaRuntimes = await javaRes.json();
    populateJavaSelect();

    // 5. Populate Settings
    populateSettings();

    // 6. Check updates if enabled
    if (state.config.checkUpdatesOnStartup) {
      checkForUpdates(false);
    }
  } catch (err) {
    console.error('Failed to initialize launcher:', err);
  }
}

async function loadContainers() {
  const res = await fetch('/api/containers');
  state.containers = await res.json();

  if (state.containers.length > 0) {
    const activeId = state.config.activeContainerId;
    const found = state.containers.find(c => c.id === activeId);
    state.activeContainer = found || state.containers[0];
  } else {
    state.activeContainer = null;
  }

  renderContainersGrid();
  renderContainerSelect();
  updateDockInfo();

  if (state.activeContainer) {
    loadInstalledMods(state.activeContainer.id);
  }
}

function updateDockInfo() {
  const nameEl = document.getElementById('dockContainerName');
  const verEl = document.getElementById('dockContainerVersion');

  if (state.activeContainer) {
    nameEl.textContent = state.activeContainer.name;
    const savesText = state.activeContainer.shareSaves ? ' • Shared Saves' : '';
    verEl.textContent = `${state.activeContainer.loaderType.toUpperCase()} ${state.activeContainer.minecraftVersion}${savesText}`;
  } else {
    nameEl.textContent = 'No Container Selected';
    verEl.textContent = '-';
  }
}

function renderContainersGrid() {
  const grid = document.getElementById('containersGrid');
  grid.innerHTML = '';

  for (const c of state.containers) {
    const card = document.createElement('div');
    const isSelected = state.activeContainer && state.activeContainer.id === c.id;
    card.className = `container-card ${isSelected ? 'selected' : ''}`;

    const t = translations[state.lang];
    const savesBadge = c.shareSaves ? `<span class="card-badge" style="color: #34d399;">Shared Saves</span>` : '';

    card.innerHTML = `
      <div class="card-top">
        <h3 class="card-title">${escapeHtml(c.name)}</h3>
        <span class="card-badge">${c.loaderType.toUpperCase()}</span>
      </div>
      <div class="card-meta">
        <div><strong>Version:</strong> ${c.minecraftVersion}</div>
        <div><strong>Last Played:</strong> ${c.lastPlayed ? new Date(c.lastPlayed).toLocaleDateString() : 'Never'}</div>
        <div>${savesBadge}</div>
      </div>
      <div class="card-actions">
        <button class="btn btn-secondary card-edit-btn" data-id="${c.id}">${t.btn_edit}</button>
        <button class="btn btn-secondary card-clone-btn" data-id="${c.id}">${t.btn_clone}</button>
        <button class="btn btn-secondary card-delete-btn" data-id="${c.id}" style="color: var(--danger);">${t.btn_delete}</button>
      </div>
    `;

    card.addEventListener('click', e => {
      if ((e.target).closest('.card-actions')) return;
      selectContainer(c.id);
    });

    const editBtn = card.querySelector('.card-edit-btn');
    editBtn.addEventListener('click', () => openEditContainerModal(c));

    const cloneBtn = card.querySelector('.card-clone-btn');
    cloneBtn.addEventListener('click', () => cloneContainer(c.id));

    const deleteBtn = card.querySelector('.card-delete-btn');
    deleteBtn.addEventListener('click', () => deleteContainer(c.id));

    grid.appendChild(card);
  }
}

function renderContainerSelect() {
  const select = document.getElementById('activeContainerSelect');
  select.innerHTML = '';

  for (const c of state.containers) {
    const opt = document.createElement('option');
    opt.value = c.id;
    opt.textContent = `${c.name} (${c.minecraftVersion})`;
    if (state.activeContainer && state.activeContainer.id === c.id) {
      opt.selected = true;
    }
    select.appendChild(opt);
  }
}

async function selectContainer(id) {
  const found = state.containers.find(c => c.id === id);
  if (!found) return;

  state.activeContainer = found;
  state.config.activeContainerId = id;

  renderContainersGrid();
  renderContainerSelect();
  updateDockInfo();
  loadInstalledMods(id);

  await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ activeContainerId: id })
  });
}

async function cloneContainer(id) {
  await fetch(`/api/containers/${id}/clone`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({})
  });
  await loadContainers();
}

async function deleteContainer(id) {
  const c = state.containers.find(item => item.id === id);
  if (!c) return;

  const msg = state.lang === 'ru'
    ? `Вы уверены, что хотите удалить контейнер "${c.name}"?`
    : `Are you sure you want to delete container "${c.name}"?`;

  if (confirm(msg)) {
    await fetch(`/api/containers/${id}`, { method: 'DELETE' });
    await loadContainers();
  }
}

// Mods & Modrinth
async function loadInstalledMods(containerId) {
  const res = await fetch(`/api/containers/${containerId}/mods`);
  state.installedMods = await res.json();
  renderInstalledMods();
}

function renderInstalledMods() {
  const container = document.getElementById('installedModsList');
  container.innerHTML = '';

  const t = translations[state.lang];

  if (state.installedMods.length === 0) {
    container.innerHTML = `<div style="text-align: center; color: var(--text-muted); padding: 40px;">${t.mods_no_installed}</div>`;
    return;
  }

  for (const mod of state.installedMods) {
    const item = document.createElement('div');
    item.className = 'mod-item';

    const sizeMb = (mod.fileSizeBytes / (1024 * 1024)).toFixed(1);

    item.innerHTML = `
      <div class="mod-info">
        <div style="font-size: 24px;">🧩</div>
        <div class="mod-details">
          <h4>${escapeHtml(mod.name)} <span style="font-size: 11px; color: var(--text-dim);">${mod.version || ''}</span></h4>
          <p>${escapeHtml(mod.fileName)} • ${sizeMb} MB</p>
        </div>
      </div>
      <div style="display: flex; align-items: center; gap: 16px;">
        <label class="mod-toggle-switch" title="${mod.enabled ? t.mods_toggle_on : t.mods_toggle_off}">
          <input type="checkbox" class="mod-toggle-input" data-filename="${escapeHtml(mod.fileName)}" ${mod.enabled ? 'checked' : ''}>
          <span class="slider"></span>
        </label>
        <button class="btn btn-secondary mod-delete-btn" data-filename="${escapeHtml(mod.fileName)}" style="color: var(--danger); padding: 4px 8px;">✕</button>
      </div>
    `;

    const toggle = item.querySelector('.mod-toggle-input');
    toggle.addEventListener('change', async () => {
      await fetch(`/api/containers/${state.activeContainer.id}/mods/toggle`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ fileName: mod.fileName })
      });
      loadInstalledMods(state.activeContainer.id);
    });

    const delBtn = item.querySelector('.mod-delete-btn');
    delBtn.addEventListener('click', async () => {
      await fetch(`/api/containers/${state.activeContainer.id}/mods/delete`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ fileName: mod.fileName })
      });
      loadInstalledMods(state.activeContainer.id);
    });

    container.appendChild(item);
  }
}

async function searchModrinth(query) {
  const list = document.getElementById('modrinthResultsList');
  list.innerHTML = `<div style="text-align: center; color: var(--text-muted); padding: 20px;">Searching...</div>`;

  const loader = state.activeContainer?.loaderType || 'fabric';
  const version = state.activeContainer?.minecraftVersion || '1.21.1';

  const params = new URLSearchParams({
    query,
    loader,
    version,
    limit: '15'
  });

  try {
    const res = await fetch(`/api/modrinth/search?${params.toString()}`);
    const data = await res.json();
    renderModrinthResults(data.hits || []);
  } catch (err) {
    list.innerHTML = `<div style="text-align: center; color: var(--danger); padding: 20px;">Error searching Modrinth: ${err.message}</div>`;
  }
}

function renderModrinthResults(hits) {
  const container = document.getElementById('modrinthResultsList');
  container.innerHTML = '';

  const t = translations[state.lang];

  if (hits.length === 0) {
    container.innerHTML = `<div style="text-align: center; color: var(--text-muted); padding: 40px;">No results found.</div>`;
    return;
  }

  for (const hit of hits) {
    const item = document.createElement('div');
    item.className = 'mod-item';

    const iconSrc = hit.icon_url || 'data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" width="44" height="44"><rect width="44" height="44" fill="%231a1e2e"/><text x="14" y="28" font-size="18" fill="%238c93a8">M</text></svg>';

    item.innerHTML = `
      <div class="mod-info">
        <img class="mod-icon" src="${escapeHtml(iconSrc)}" alt="${escapeHtml(hit.title)}">
        <div class="mod-details">
          <h4>${escapeHtml(hit.title)} <span style="font-size: 11px; color: var(--text-dim);">by ${escapeHtml(hit.author)}</span></h4>
          <p>${escapeHtml(hit.description)}</p>
        </div>
      </div>
      <div>
        <button class="btn btn-primary mod-install-btn" data-id="${hit.project_id}">${t.btn_install}</button>
      </div>
    `;

    const btn = item.querySelector('.mod-install-btn');
    btn.addEventListener('click', async () => {
      btn.textContent = 'Installing...';
      btn.disabled = true;

      try {
        const vRes = await fetch(`/api/modrinth/versions?projectId=${hit.project_id}&loader=${state.activeContainer.loaderType}&version=${state.activeContainer.minecraftVersion}`);
        const versions = await vRes.json();

        if (versions.length === 0) {
          alert('No compatible version found for this loader/version');
          btn.textContent = t.btn_install;
          btn.disabled = false;
          return;
        }

        await fetch('/api/modrinth/install', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            containerId: state.activeContainer.id,
            version: versions[0]
          })
        });

        btn.textContent = t.btn_installed;
        btn.className = 'btn btn-secondary';
        loadInstalledMods(state.activeContainer.id);
      } catch (err) {
        alert(`Failed to install: ${err.message}`);
        btn.textContent = t.btn_install;
        btn.disabled = false;
      }
    });

    container.appendChild(item);
  }
}

// Server Ping
async function pingServer(host) {
  const card = document.getElementById('serverPingCard');
  card.style.display = 'block';

  document.getElementById('serverHostName').textContent = host;
  document.getElementById('serverVersionText').textContent = 'Pinging...';
  document.getElementById('serverOnlineBadge').textContent = '...';
  document.getElementById('serverPingMs').textContent = '';
  document.getElementById('serverMotd').textContent = 'Connecting to server...';
  document.getElementById('serverPlayersCount').textContent = '-';

  try {
    const res = await fetch('/api/servers/ping', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ host })
    });
    const data = await res.json();

    const t = translations[state.lang];

    if (data.online) {
      document.getElementById('serverOnlineBadge').textContent = t.server_online;
      document.getElementById('serverOnlineBadge').style.color = 'var(--success)';
      document.getElementById('serverVersionText').textContent = data.versionName || '';
      document.getElementById('serverPingMs').textContent = `${data.latencyMs} ${t.server_ping_ms}`;
      document.getElementById('serverMotd').textContent = data.motdText || 'Minecraft Server';
      document.getElementById('serverPlayersCount').textContent = `${data.playersOnline} / ${data.playersMax}`;

      if (data.favicon) {
        const fav = document.getElementById('serverFavicon');
        fav.src = data.favicon;
        fav.style.display = 'block';
      }
    } else {
      document.getElementById('serverOnlineBadge').textContent = t.server_offline;
      document.getElementById('serverOnlineBadge').style.color = 'var(--danger)';
      document.getElementById('serverMotd').textContent = data.error || 'Server unreachable';
      document.getElementById('serverPlayersCount').textContent = '0 / 0';
    }
  } catch (err) {
    document.getElementById('serverMotd').textContent = err.message;
  }
}

// Logs and Crash Diagnostics
function appendLogLine(entry) {
  const output = document.getElementById('logsOutput');
  const line = document.createElement('div');
  line.className = `log-line ${entry.level}`;
  line.textContent = `[${entry.timestamp}] [${entry.level}] ${entry.message}`;

  output.appendChild(line);

  // Auto-scroll to bottom
  output.scrollTop = output.scrollHeight;
}

function showCrashDiagnostic(diag) {
  const card = document.getElementById('crashDiagnosticCard');
  if (!diag || !diag.hasIssue) {
    card.classList.remove('active');
    return;
  }

  const isRu = state.lang === 'ru';
  document.getElementById('diagnosticTitle').textContent = isRu ? diag.title_ru : diag.title;
  document.getElementById('diagnosticCause').textContent = isRu ? diag.cause_ru : diag.cause;
  document.getElementById('diagnosticSolution').textContent = `${isRu ? 'Решение: ' : 'Solution: '}${isRu ? diag.solution_ru : diag.solution}`;

  card.classList.add('active');

  // Switch to logs tab automatically on crash
  switchTab('logs');
}

// Game Launching and Stopping
async function toggleGameLaunch() {
  if (state.gameState === 'running' || state.gameState === 'preparing') {
    // Stop game
    updateGameState('stopping');
    await fetch('/api/launcher/stop', { method: 'POST' });
  } else {
    // Launch game
    if (!state.activeContainer) {
      alert('Please select or create a container first!');
      return;
    }

    updateGameState('preparing');
    try {
      await fetch('/api/launcher/launch', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ containerId: state.activeContainer.id })
      });
    } catch (err) {
      alert(`Launch error: ${err.message}`);
      updateGameState('idle');
    }
  }
}

function updateGameState(newState) {
  state.gameState = newState;
  const playBtn = document.getElementById('playBtn');
  const btnText = document.getElementById('playBtnText');
  const statusText = document.getElementById('launchStatusText');
  const t = translations[state.lang];

  playBtn.className = 'play-btn';
  document.body.classList.remove('game-active');

  switch (newState) {
    case 'running':
      playBtn.classList.add('running');
      btnText.textContent = t.btn_stop;
      statusText.textContent = t.status_running;
      statusText.style.color = 'var(--success)';
      document.body.classList.add('game-active');
      break;

    case 'preparing':
    case 'downloading':
      btnText.textContent = t.btn_preparing;
      statusText.textContent = t.status_preparing;
      statusText.style.color = 'var(--warning)';
      break;

    case 'stopping':
      btnText.textContent = t.btn_stopping;
      statusText.textContent = t.btn_stopping;
      statusText.style.color = 'var(--warning)';
      break;

    case 'crashed':
      btnText.textContent = t.btn_play;
      statusText.textContent = t.status_crashed;
      statusText.style.color = 'var(--danger)';
      break;

    case 'idle':
    default:
      btnText.textContent = t.btn_play;
      statusText.textContent = t.status_idle;
      statusText.style.color = 'var(--text-muted)';
      break;
  }
}

// Auto-Updater
async function checkForUpdates(userTriggered = false) {
  try {
    const res = await fetch('/api/updater/check');
    const result = await res.json();

    if (result.hasUpdate && result.manifest) {
      state.updateManifest = result.manifest;
      showUpdateModal(result.manifest);
    } else if (userTriggered) {
      alert(translations[state.lang].update_up_to_date);
    }
  } catch (err) {
    if (userTriggered) {
      alert(`Failed to check updates: ${err.message}`);
    }
  }
}

function showUpdateModal(manifest) {
  const modal = document.getElementById('updateModal');
  const isRu = state.lang === 'ru';

  document.getElementById('updateVersionBadge').textContent = `v${manifest.version}`;
  document.getElementById('updateNotesText').textContent = isRu ? (manifest.notes_ru || manifest.notes) : (manifest.notes_en || manifest.notes);

  const box = document.getElementById('updateChangelogBox');
  box.innerHTML = '';
  const changes = isRu ? (manifest.changes_ru || manifest.changes) : (manifest.changes_en || manifest.changes);

  if (Array.isArray(changes)) {
    const ul = document.createElement('ul');
    ul.style.paddingLeft = '18px';
    for (const ch of changes) {
      const li = document.createElement('li');
      li.textContent = ch;
      ul.appendChild(li);
    }
    box.appendChild(ul);
  }

  modal.classList.add('active');
}

async function startUpdateDownload() {
  if (!state.updateManifest) return;

  const btn = document.getElementById('startUpdateBtn');
  btn.disabled = true;
  document.getElementById('updateDownloadSection').style.display = 'block';

  try {
    await fetch('/api/updater/download', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        url: state.updateManifest.url,
        sha256: state.updateManifest.sha256,
        size: state.updateManifest.size
      })
    });
  } catch (err) {
    alert(`Update download failed: ${err.message}`);
    btn.disabled = false;
  }
}

function renderUpdateProgress(progress) {
  const bar = document.getElementById('updateProgressBar');
  const percentText = document.getElementById('updatePercentText');
  const statusText = document.getElementById('updateStatusText');

  const pct = `${progress.percent}%`;
  bar.style.width = pct;
  percentText.textContent = pct;

  const mb = (progress.bytesDownloaded / (1024 * 1024)).toFixed(1);
  const totalMb = (progress.totalBytes / (1024 * 1024)).toFixed(1);
  const speedMb = (progress.speedBytesPerSec / (1024 * 1024)).toFixed(1);

  statusText.textContent = `${mb} / ${totalMb} MB (${speedMb} MB/s)`;

  if (progress.percent === 100) {
    statusText.textContent = translations[state.lang].update_ready;
  }
}

// UI Controls & Event Listeners
function setupEventListeners() {
  // Navigation Tabs
  const navItems = document.querySelectorAll('.nav-item');
  navItems.forEach(item => {
    item.addEventListener('click', () => {
      const tab = item.dataset.tab;
      switchTab(tab);
    });
  });

  // Top Bar Container Select
  document.getElementById('activeContainerSelect').addEventListener('change', e => {
    selectContainer((e.target).value);
  });

  // Language buttons
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const lang = btn.dataset.lang;
      applyLanguage(lang);
      saveSetting('language', lang);
    });
  });

  // Play button
  document.getElementById('playBtn').addEventListener('click', toggleGameLaunch);

  // Mod subnav
  document.getElementById('subtabCatalogBtn').addEventListener('click', () => {
    document.getElementById('modsCatalogView').style.display = 'block';
    document.getElementById('modsInstalledView').style.display = 'none';
    document.getElementById('subtabCatalogBtn').classList.add('active');
    document.getElementById('subtabInstalledBtn').classList.remove('active');
  });

  document.getElementById('subtabInstalledBtn').addEventListener('click', () => {
    document.getElementById('modsCatalogView').style.display = 'none';
    document.getElementById('modsInstalledView').style.display = 'block';
    document.getElementById('subtabInstalledBtn').classList.add('active');
    document.getElementById('subtabCatalogBtn').classList.remove('active');
  });

  // Mod Search
  document.getElementById('modSearchBtn').addEventListener('click', () => {
    const query = document.getElementById('modSearchInput').value.trim();
    searchModrinth(query);
  });

  document.getElementById('modSearchInput').addEventListener('keydown', e => {
    if (e.key === 'Enter') {
      searchModrinth(e.target.value.trim());
    }
  });

  // Server Ping
  document.getElementById('pingServerBtn').addEventListener('click', () => {
    const host = document.getElementById('serverAddressInput').value.trim();
    if (host) pingServer(host);
  });

  // Logs actions
  document.getElementById('clearLogsBtn').addEventListener('click', () => {
    document.getElementById('logsOutput').innerHTML = '';
  });

  document.getElementById('copyLogsBtn').addEventListener('click', () => {
    const text = document.getElementById('logsOutput').innerText;
    navigator.clipboard.writeText(text);
    alert(state.lang === 'ru' ? 'Логи скопированы в буфер обмена!' : 'Logs copied to clipboard!');
  });

  // Create Container Modal
  document.getElementById('openCreateContainerModalBtn').addEventListener('click', openCreateContainerModal);
  document.getElementById('closeContainerModalBtn').addEventListener('click', closeContainerModal);
  document.getElementById('cancelContainerModalBtn').addEventListener('click', closeContainerModal);
  document.getElementById('saveContainerModalBtn').addEventListener('click', saveContainerModal);

  // Update Modal
  document.getElementById('closeUpdateModalBtn').addEventListener('click', () => {
    document.getElementById('updateModal').classList.remove('active');
  });
  document.getElementById('dismissUpdateModalBtn').addEventListener('click', () => {
    document.getElementById('updateModal').classList.remove('active');
  });
  document.getElementById('startUpdateBtn').addEventListener('click', startUpdateDownload);
  document.getElementById('checkUpdatesManualBtn').addEventListener('click', () => checkForUpdates(true));

  // Settings inputs
  document.getElementById('ramSlider').addEventListener('input', e => {
    const val = parseInt((e.target).value, 10);
    document.getElementById('ramValText').textContent = `${(val / 1024).toFixed(1)} GB`;
  });

  document.getElementById('ramSlider').addEventListener('change', e => {
    saveSetting('allocatedMemory', parseInt((e.target).value, 10));
  });

  document.getElementById('jvmPresetSelect').addEventListener('change', e => {
    saveSetting('jvmPreset', (e.target).value);
  });

  document.getElementById('customJvmArgsInput').addEventListener('change', e => {
    saveSetting('customJvmArgs', (e.target).value);
  });

  document.getElementById('lowCpuModeCheckbox').addEventListener('change', e => {
    saveSetting('lowCpuMode', (e.target).checked);
  });

  document.getElementById('themeSelect').addEventListener('change', e => {
    const theme = (e.target).value;
    applyTheme(theme);
    saveSetting('theme', theme);
  });

  document.getElementById('usernameInput').addEventListener('change', e => {
    saveSetting('offlineUsername', (e.target).value.trim());
  });

  document.getElementById('javaSelect').addEventListener('change', e => {
    saveSetting('javaPath', (e.target).value);
  });
}

function switchTab(tabId) {
  state.currentTab = tabId;

  document.querySelectorAll('.nav-item').forEach(item => {
    item.classList.toggle('active', item.dataset.tab === tabId);
  });

  document.querySelectorAll('.tab-content').forEach(content => {
    content.classList.toggle('active', content.id === `tab-${tabId}`);
  });
}

function applyTheme(theme) {
  document.body.dataset.theme = theme;
  document.getElementById('themeSelect').value = theme;
}

function applyLanguage(lang) {
  state.lang = lang;
  const dict = translations[lang] || translations.ru;

  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.dataset.i18n;
    if (dict[key]) {
      el.textContent = dict[key];
    }
  });

  document.querySelectorAll('[data-i18n-attr]').forEach(el => {
    const [attr, key] = el.dataset.i18nAttr.split(':');
    if (dict[key]) {
      el.setAttribute(attr, dict[key]);
    }
  });

  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.lang === lang);
  });

  updateGameState(state.gameState);
}

function populateVersionSelects() {
  const modalSelect = document.getElementById('modalMcVersionSelect');
  modalSelect.innerHTML = '';

  for (const v of state.mcVersions) {
    const opt = document.createElement('option');
    opt.value = v;
    opt.textContent = v;
    modalSelect.appendChild(opt);
  }
}

function populateJavaSelect() {
  const sel = document.getElementById('javaSelect');
  sel.innerHTML = '<option value="">Auto-Detect (Recommended)</option>';

  for (const j of state.javaRuntimes) {
    const opt = document.createElement('option');
    opt.value = j.path;
    opt.textContent = `Java ${j.majorVersion} (${j.vendor}) [${j.is64Bit ? '64-bit' : '32-bit'}]`;
    if (state.config?.javaPath === j.path) {
      opt.selected = true;
    }
    sel.appendChild(opt);
  }
}

function populateSettings() {
  const c = state.config;
  if (!c) return;

  document.getElementById('ramSlider').value = c.allocatedMemory;
  document.getElementById('ramValText').textContent = `${(c.allocatedMemory / 1024).toFixed(1)} GB`;
  document.getElementById('jvmPresetSelect').value = c.jvmPreset || 'aikar';
  document.getElementById('customJvmArgsInput').value = c.customJvmArgs || '';
  document.getElementById('lowCpuModeCheckbox').checked = c.lowCpuMode !== false;
  document.getElementById('usernameInput').value = c.offlineUsername || 'Player';
  document.getElementById('storagePathInput').value = c.storagePath || '';
}

async function saveSetting(key, value) {
  state.config[key] = value;
  await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ [key]: value })
  });
}

// Modal creation/editing
function openCreateContainerModal() {
  document.getElementById('editContainerId').value = '';
  document.getElementById('containerModalTitle').textContent = translations[state.lang].btn_create_container;
  document.getElementById('modalContainerName').value = 'Survival World';
  document.getElementById('modalShareSavesCheckbox').checked = true;
  document.getElementById('containerModal').classList.add('active');
}

function openEditContainerModal(c) {
  document.getElementById('editContainerId').value = c.id;
  document.getElementById('containerModalTitle').textContent = translations[state.lang].btn_edit;
  document.getElementById('modalContainerName').value = c.name;
  document.getElementById('modalMcVersionSelect').value = c.minecraftVersion;
  document.getElementById('modalLoaderSelect').value = c.loaderType;
  document.getElementById('modalShareSavesCheckbox').checked = !!c.shareSaves;
  document.getElementById('containerModal').classList.add('active');
}

function closeContainerModal() {
  document.getElementById('containerModal').classList.remove('active');
}

async function saveContainerModal() {
  const id = document.getElementById('editContainerId').value;
  const name = document.getElementById('modalContainerName').value.trim() || 'Minecraft';
  const version = document.getElementById('modalMcVersionSelect').value;
  const loader = document.getElementById('modalLoaderSelect').value;
  const shareSaves = document.getElementById('modalShareSavesCheckbox').checked;

  if (id) {
    // Edit
    await fetch(`/api/containers/${id}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, minecraftVersion: version, loaderType: loader, shareSaves })
    });
  } else {
    // Create
    await fetch('/api/containers', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, minecraftVersion: version, loaderType: loader, shareSaves })
    });
  }

  closeContainerModal();
  await loadContainers();
}

function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

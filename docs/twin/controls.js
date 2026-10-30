/**
 * Celestial Engine Mk.V — Interactive Digital Replica & Oracle Controller
 * Orchestrates:
 *  - 5 Hero Mode Actions (Explore Staff, Lighting Effects, Music, Ask the Oracle, Technical Architecture)
 *  - 6 Authoritative Oracle Personas (exact parity with Android OraclePersona.kt)
 *  - Scripted Demo Mode (default, zero download) + Opt-in Local Browser AI (SmolLM2-135M / LFM2.5-350M)
 *  - 2D Physical Replica / Exploded Build / IC Breadboard CAD Constructor + Optional 3D WebGL View
 */

import { twinState, STAFF_STATES, FIRMWARE_EFFECTS, MAX_CUSTOM_PROMPT_LENGTH } from './state.js?v=1.3.3';
import { OracleProvider, MODEL_INFO, probeWebGpuSupport } from './oracle/provider.js?v=1.3.3';
import { Staff2DRenderer, HARDWARE_HOTSPOTS } from './view/staff-2d.js?v=1.3.3';
import { updateAccessibleOledTranscript } from './view/oled.js?v=1.3.3';
import { speakOracleText, stopOracleSpeech, isSpeechRecognitionSupported, startVoiceDictation } from './audio/speech.js?v=1.3.3';

let oracleProvider = null;
let staff2D = null;
let staff3D = null;
let activeDictation = null;
let pendingQueryAfterLoad = null;
let userManuallyUnloadedAi = false;

function setSafeText(id, text) {
  const el = document.getElementById(id);
  if (!el) return;
  const str = String(text ?? '');
  if (id === 'oracle-response-text' && /https?:\/\/[^\s)]+/i.test(str)) {
    el.textContent = '';
    const urlRegex = /(https?:\/\/[^\s)]+)/gi;
    let lastIndex = 0;
    let match;
    while ((match = urlRegex.exec(str)) !== null) {
      if (match.index > lastIndex) {
        el.appendChild(document.createTextNode(str.slice(lastIndex, match.index)));
      }
      let rawUrl = match[0];
      let trailing = '';
      while (/[.,!?;:]$/.test(rawUrl)) {
        trailing = rawUrl.slice(-1) + trailing;
        rawUrl = rawUrl.slice(0, -1);
      }
      const a = document.createElement('a');
      a.href = rawUrl;
      a.target = '_blank';
      a.rel = 'noopener noreferrer';
      a.style.color = '#38bdf8';
      a.style.textDecoration = 'underline';
      a.style.fontWeight = '700';
      a.textContent = rawUrl;
      el.appendChild(a);
      if (trailing) {
        el.appendChild(document.createTextNode(trailing));
      }
      lastIndex = urlRegex.lastIndex;
    }
    if (lastIndex < str.length) {
      el.appendChild(document.createTextNode(str.slice(lastIndex)));
    }
    return;
  }
  el.textContent = str;
}

function updateHotspotCard(hotspotId) {
  const spot = HARDWARE_HOTSPOTS.find(h => h.id === hotspotId) || HARDWARE_HOTSPOTS[0];
  setSafeText('replica-hotspot-title', spot.title);
  setSafeText('replica-hotspot-badge', spot.badge);
  setSafeText('replica-hotspot-summary', spot.summary);
  setSafeText('replica-hotspot-specs', spot.specs);

  document.querySelectorAll('.hotspot-chip-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.hotspot === spot.id);
  });
}

function updateOracleUi() {
  const state = twinState.getState();
  const persona = oracleProvider ? oracleProvider.getPersonaById(state.personaId) : { id: 'machine_god', titleEn: 'Machine God', icon: '🤖', subtitleEn: '' };

  // Highlight active persona button
  document.querySelectorAll('.persona-card-btn').forEach(btn => {
    const isSel = btn.dataset.persona === state.personaId;
    btn.classList.toggle('active', isSel);
    btn.setAttribute('aria-pressed', isSel ? 'true' : 'false');
  });

  // Update active persona badge & subtitle
  setSafeText('active-persona-badge', `${persona.icon} ${persona.titleEn}`);
  setSafeText('active-persona-subtitle', persona.subtitleEn);
  setSafeText('telem-persona', persona.titleEn.toUpperCase());

  // Show/hide Custom Persona prompt editor
  const customBox = document.getElementById('custom-persona-editor');
  if (customBox) {
    customBox.style.display = state.personaId === 'custom' ? 'flex' : 'none';
  }

  const customInput = document.getElementById('custom-prompt-input');
  if (customInput && document.activeElement !== customInput) {
    customInput.value = state.customPrompt || '';
  }
  setSafeText('custom-prompt-counter', `${(state.customPrompt || '').length} / ${MAX_CUSTOM_PROMPT_LENGTH}`);

  // Update Mode Badge (Pre-Recorded vs Live Local LLM)
  const modePill = document.getElementById('oracle-mode-pill');
  if (modePill) {
    if (state.oracleMode === 'local-ai' && state.aiStatus === 'ready') {
      const mInfo = MODEL_INFO[state.selectedModelKey] || MODEL_INFO['smollm2-135m'];
      modePill.textContent = `🧠 LIVE LOCAL LLM ACTIVE (${mInfo.key.toUpperCase()})`;
      modePill.className = 'badge emerald';
    } else {
      modePill.textContent = '📜 PRE-RECORDED DEMO ANSWERS (OR CONNECT LOCAL LLM BELOW)';
      modePill.className = 'badge gold';
    }
  }

  // Update Response Card safely via textContent (never raw HTML)
  setSafeText('oracle-response-text', state.lastResponse);
  const srcLabel = document.getElementById('oracle-response-source');
  if (srcLabel) {
    if (state.lastResponseSource === 'local-ai') {
      srcLabel.textContent = `Generated live by in-browser Local LLM (${state.selectedModelKey}) • ${state.lastLatencyMs} ms`;
      srcLabel.style.color = '#34d399';
    } else {
      srcLabel.textContent = `Pre-recorded answer (${persona.titleEn} preset) • Click "Ask Oracle" or load Local AI below to chat live with an LLM`;
      srcLabel.style.color = '#fbbf24';
    }
  }

  // Cancel button visibility
  const cancelBtn = document.getElementById('oracle-cancel-btn');
  if (cancelBtn) {
    cancelBtn.style.display = state.staffState === STAFF_STATES.PROCESSING ? 'inline-flex' : 'none';
  }

  // Update Local AI Status Panel
  const aiStatusBadge = document.getElementById('local-ai-status-badge');
  const progressWrap = document.getElementById('local-ai-progress-wrap');
  const progressBar = document.getElementById('local-ai-progress-bar');
  const unloadBtn = document.getElementById('btn-unload-ai');
  const loadBtn = document.getElementById('btn-load-ai');

  if (aiStatusBadge) {
    aiStatusBadge.textContent = state.aiStatus.toUpperCase();
  }
  if (progressWrap && progressBar) {
    progressWrap.style.display = (state.aiStatus === 'loading') ? 'block' : 'none';
    progressBar.style.width = `${state.aiProgressPct}%`;
  }
  setSafeText('local-ai-progress-text', state.aiProgressText || '');
  setSafeText('local-ai-error-text', state.aiErrorText || '');

  if (unloadBtn) {
    unloadBtn.style.display = (state.aiStatus === 'ready' || state.aiStatus === 'loading') ? 'inline-flex' : 'none';
  }
  if (loadBtn) {
    loadBtn.disabled = state.aiStatus === 'loading';
    loadBtn.textContent = state.aiStatus === 'ready'
      ? '✓ Model Ready (Switch / Reload)'
      : '⬇ Confirm & Download Selected Model';
  }

  // Push global hook so legacy simulator.js OLED renderer can display current persona & response
  window.__celestialTwinBridge = {
    personaId: state.personaId,
    personaTitle: persona.titleEn,
    oracleMode: state.oracleMode,
    lastResponse: state.lastResponse,
    lastResponseSource: state.lastResponseSource,
    staffState: state.staffState
  };

  updateAccessibleOledTranscript(state, persona.titleEn);
  if (typeof window.__celestialRenderOled === 'function') {
    window.__celestialRenderOled();
  }
}

export async function submitOracleQuery(questionText) {
  const state = twinState.getState();
  const q = (questionText !== undefined ? questionText : document.getElementById('oracle-question-input')?.value || '').trim();

  // Cancel any prior in-flight request
  if (state.staffState === STAFF_STATES.PROCESSING) {
    oracleProvider.cancel(state.activeRequestId);
  }
  stopOracleSpeech();

  // Automatically load the Local AI LLM on first query (unless user explicitly unloaded it or loading failed)
  if (!oracleProvider.isLocalModelReady() && !userManuallyUnloadedAi && state.aiStatus !== 'error') {
    pendingQueryAfterLoad = q;
    const modelKey = state.selectedModelKey || 'smollm2-135m';
    const drawer = document.querySelector('.local-ai-optin-drawer');
    if (drawer) drawer.open = true;

    twinState.update({
      staffState: STAFF_STATES.PROCESSING,
      lastQuestion: q,
      lastResponse: `⏳ Loading local LLM (${modelKey}) to generate response for: "${q || 'status'}"...`,
      aiErrorText: ''
    });
    if (typeof window.__celestialSyncLegacyState === 'function') {
      window.__celestialSyncLegacyState(STAFF_STATES.PROCESSING);
    }

    if (state.aiStatus !== 'loading') {
      twinState.update({
        aiStatus: 'loading',
        aiProgressPct: 2,
        aiProgressText: `Initializing ${modelKey} worker...`
      });
      oracleProvider.initLocalModel(modelKey);
    }
    updateOracleUi();
    return;
  }

  const reqId = twinState.nextRequestId();
  const useLocalAi = state.oracleMode === 'local-ai' && oracleProvider.isLocalModelReady();

  twinState.update({
    staffState: STAFF_STATES.PROCESSING,
    lastQuestion: q,
    lastResponse: 'Consulting the Celestial Oracle...',
    aiErrorText: ''
  });

  if (typeof window.__celestialSyncLegacyState === 'function') {
    window.__celestialSyncLegacyState(STAFF_STATES.PROCESSING);
  }
  updateOracleUi();

  window.__celestialIsLocalLlmGenerating = Boolean(useLocalAi);
  try {
    const res = await oracleProvider.generate({
      requestId: reqId,
      personaId: state.personaId,
      question: q,
      customPrompt: state.customPrompt,
      mode: useLocalAi ? 'local-ai' : 'scripted',
      onStream: ({ text, elapsedMs }) => {
        if (reqId !== twinState.getState().activeRequestId) return;
        setSafeText('oracle-response-text', `${text} ▍`);
        const srcLabel = document.getElementById('oracle-response-source');
        if (srcLabel) {
          srcLabel.textContent = `⚡ Streaming live from Local LLM (${twinState.getState().selectedModelKey}) • ${elapsedMs} ms...`;
          srcLabel.style.color = '#38bdf8';
        }
      }
    });
    window.__celestialIsLocalLlmGenerating = false;

    // Ignore stale response if another request started in the meantime
    if (reqId !== twinState.getState().activeRequestId) return;

    twinState.update({
      staffState: STAFF_STATES.SPEAKING,
      lastResponse: res.text,
      lastResponseSource: res.mode,
      lastLatencyMs: res.latencyMs || 0
    });

    if (typeof window.__celestialSyncLegacyState === 'function') {
      window.__celestialSyncLegacyState(STAFF_STATES.SPEAKING);
    }
    updateOracleUi();

    if (twinState.getState().ttsEnabled) {
      speakOracleText(res.text, state.personaId, () => {
        if (reqId === twinState.getState().activeRequestId) {
          twinState.update({ staffState: STAFF_STATES.IDLE });
          if (typeof window.__celestialSyncLegacyState === 'function') {
            window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
          }
          updateOracleUi();
        }
      });
    } else {
      setTimeout(() => {
        if (reqId === twinState.getState().activeRequestId) {
          twinState.update({ staffState: STAFF_STATES.IDLE });
          if (typeof window.__celestialSyncLegacyState === 'function') {
            window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
          }
          updateOracleUi();
        }
      }, 1200);
    }
  } catch (err) {
    window.__celestialIsLocalLlmGenerating = false;
    if (err && err.message === 'CANCELLED') return;

    // Seamless fallback to Scripted Mode so simulator never stays stuck in PROCESSING
    const fallbackRes = await oracleProvider.generate({
      requestId: reqId,
      personaId: state.personaId,
      question: q,
      customPrompt: state.customPrompt,
      mode: 'scripted'
    });

    twinState.update({
      staffState: STAFF_STATES.IDLE,
      lastResponse: fallbackRes.text,
      lastResponseSource: 'scripted',
      aiErrorText: `Local AI error (${err.message || 'failed'}). Automatically answered via Scripted Demo fallback.`
    });

    if (typeof window.__celestialSyncLegacyState === 'function') {
      window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
    }
    updateOracleUi();
  }
}

export function switchPersona(newPersonaId) {
  const state = twinState.getState();
  if (state.staffState === STAFF_STATES.PROCESSING) {
    oracleProvider.cancel(state.activeRequestId);
  }
  stopOracleSpeech();
  twinState.nextRequestId(); // Invalidate any pending token callbacks

  const persona = oracleProvider.getPersonaById(newPersonaId);
  twinState.update({
    personaId: persona.id,
    staffState: STAFF_STATES.IDLE,
    lastResponse: `${persona.titleEn} persona active. Conversation context reset. Ask a question or press QUERY.`
  });

  if (typeof window.__celestialSyncLegacyState === 'function') {
    window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
  }
  updateOracleUi();
}

function populateEffectsGrid() {
  const grid = document.getElementById('replica-effects-grid');
  if (!grid) return;
  grid.innerHTML = '';

  FIRMWARE_EFFECTS.forEach((fx) => {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'fx-mode-btn' + (fx.id === twinState.getState().animIdx ? ' active' : '');
    btn.dataset.fxId = String(fx.id);

    const numSpan = document.createElement('span');
    numSpan.className = 'fx-num';
    numSpan.textContent = `FX ${String(fx.id + 1).padStart(2, '0')}`;

    const nameStrong = document.createElement('strong');
    nameStrong.textContent = fx.name;

    btn.appendChild(numSpan);
    btn.appendChild(nameStrong);

    btn.addEventListener('click', () => {
      twinState.update({ animIdx: fx.id });
      if (typeof window.__celestialSetAnimIdx === 'function') {
        window.__celestialSetAnimIdx(fx.id);
      }
      document.querySelectorAll('.fx-mode-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.fxId === String(fx.id));
      });
      setSafeText('active-fx-desc', `${fx.name} — ${fx.ringDesc}`);
    });

    grid.appendChild(btn);
  });
}

async function toggle3DView(enable) {
  const canvas2d = document.getElementById('replica-2d-canvas');
  const container3d = document.getElementById('replica-3d-container');
  const oledCanvas = document.getElementById('oled-canvas');

  if (!enable) {
    twinState.update({ enable3D: false });
    if (canvas2d) canvas2d.style.display = 'block';
    if (container3d) container3d.style.display = 'none';
    return;
  }

  try {
    if (!staff3D) {
      const mod = await import('./view/staff-3d.js');
      staff3D = new mod.Staff3DRenderer(container3d, oledCanvas, twinState, (spotId) => {
        updateHotspotCard(spotId);
      });
      await staff3D.init();
    }
    twinState.update({ enable3D: true });
    if (canvas2d) canvas2d.style.display = 'none';
    if (container3d) container3d.style.display = 'block';
  } catch (err) {
    console.warn('3D WebGL view fallback to 2D:', err);
    twinState.update({ enable3D: false });
    if (canvas2d) canvas2d.style.display = 'block';
    if (container3d) container3d.style.display = 'none';
  }
}

export async function initTwinReplica() {
  oracleProvider = new OracleProvider({
    onProgress: (pct, status) => {
      const curState = twinState.getState();
      const pendingMsg = pendingQueryAfterLoad !== null
        ? `⏳ Loading local LLM (${pct}%) for "${pendingQueryAfterLoad}" — ${status}`
        : curState.lastResponse;
      twinState.update({
        aiStatus: 'loading',
        aiProgressPct: pct,
        aiProgressText: status,
        lastResponse: pendingMsg,
        aiErrorText: ''
      });
      updateOracleUi();
    },
    onReady: (msg) => {
      twinState.update({
        aiStatus: 'ready',
        oracleMode: 'local-ai',
        aiProgressPct: 100,
        aiProgressText: `Ready: ${msg.modelKey} on ${msg.device.toUpperCase()} (${msg.dtype})`,
        aiErrorText: ''
      });
      updateOracleUi();

      if (pendingQueryAfterLoad !== null) {
        const queuedQ = pendingQueryAfterLoad;
        pendingQueryAfterLoad = null;
        submitOracleQuery(queuedQ);
      }
    },
    onError: (errMsg) => {
      twinState.update({
        aiStatus: 'error',
        oracleMode: 'scripted',
        aiErrorText: `Model load failed: ${errMsg}. Using Scripted Demo Mode.`
      });
      updateOracleUi();

      if (pendingQueryAfterLoad !== null) {
        const queuedQ = pendingQueryAfterLoad;
        pendingQueryAfterLoad = null;
        submitOracleQuery(queuedQ);
      }
    },
    onDisposed: () => {
      twinState.update({
        aiStatus: 'unloaded',
        oracleMode: 'scripted',
        aiProgressPct: 0,
        aiProgressText: 'Model unloaded from browser memory. Scripted Demo active.'
      });
      updateOracleUi();
    }
  });

  await oracleProvider.loadPersonasJson();

  // Probe WebGPU hardware capability without downloading any model weights
  probeWebGpuSupport().then((gpu) => {
    twinState.update({
      webgpuAvailable: gpu.available,
      webgpuAdapterInfo: gpu.label
    });
    setSafeText(
      'webgpu-probe-badge',
      gpu.available ? `✓ WebGPU Ready (${gpu.label})` : `⚠ No WebGPU (${gpu.label}) — Scripted Mode Recommended`
    );
  });

  // Initialize 2D Physical Replica & Breadboard CAD Canvas
  const replicaCanvas = document.getElementById('replica-2d-canvas');
  if (replicaCanvas) {
    staff2D = new Staff2DRenderer(replicaCanvas, twinState, (spotId) => {
      updateHotspotCard(spotId);
    });
  }

  updateHotspotCard('all');
  populateEffectsGrid();
  updateOracleUi();

  // Bind View Mode Switchers (Assembled Replica / Exploded Build / IC Breadboard CAD / 3D WebGL)
  document.querySelectorAll('.replica-view-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
      const mode = btn.dataset.view;
      document.querySelectorAll('.replica-view-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      if (mode === '3d') {
        await toggle3DView(true);
      } else {
        await toggle3DView(false);
        twinState.update({ viewMode2D: mode });
      }
    });
  });

  // Bind Hotspot Quick-Select Chips (click again to return to 'all' full assembly)
  document.querySelectorAll('.hotspot-chip-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const id = btn.dataset.hotspot;
      const cur = twinState.getState().selectedHotspotId;
      const nextId = (cur === id && id !== 'all') ? 'all' : id;
      twinState.update({ selectedHotspotId: nextId });
      updateHotspotCard(nextId);
    });
  });

  // Bind 6 Persona Selector Cards
  document.querySelectorAll('.persona-card-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      switchPersona(btn.dataset.persona);
    });
  });

  // Bind Custom Persona Prompt Input & Reset
  const customInput = document.getElementById('custom-prompt-input');
  if (customInput) {
    customInput.addEventListener('input', (e) => {
      twinState.update({ customPrompt: e.target.value });
      setSafeText('custom-prompt-counter', `${twinState.getState().customPrompt.length} / ${MAX_CUSTOM_PROMPT_LENGTH}`);
    });
  }

  const customResetBtn = document.getElementById('custom-prompt-reset');
  if (customResetBtn) {
    customResetBtn.addEventListener('click', () => {
      twinState.resetCustomPrompt();
      if (customInput) customInput.value = '';
      updateOracleUi();
    });
  }

  // Bind Question Form & Sample Question Chips
  const oracleForm = document.getElementById('oracle-query-form');
  if (oracleForm) {
    oracleForm.addEventListener('submit', (e) => {
      e.preventDefault();
      const inputEl = document.getElementById('oracle-question-input');
      submitOracleQuery(inputEl ? inputEl.value : '');
    });
  }

  document.querySelectorAll('.sample-prompt-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      const q = chip.dataset.prompt || '';
      const inputEl = document.getElementById('oracle-question-input');
      if (inputEl) inputEl.value = q;
      submitOracleQuery(q);
    });
  });

  // Cancel in-progress generation
  const cancelBtn = document.getElementById('oracle-cancel-btn');
  if (cancelBtn) {
    cancelBtn.addEventListener('click', () => {
      const st = twinState.getState();
      oracleProvider.cancel(st.activeRequestId);
      twinState.nextRequestId();
      twinState.update({
        staffState: STAFF_STATES.IDLE,
        lastResponse: 'Query cancelled by user.'
      });
      if (typeof window.__celestialSyncLegacyState === 'function') {
        window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
      }
      updateOracleUi();
    });
  }

  // Unified Browser Voice Input & Live Microphone Recording Indicator
  const micDictateBtn = document.getElementById('oracle-mic-btn');
  const micStopSendBtn = document.getElementById('mic-stop-send-btn');
  const micBanner = document.getElementById('mic-recording-banner');
  const micLiveTranscript = document.getElementById('mic-live-transcript');
  const micLevelBarsContainer = document.getElementById('mic-level-bars');
  const hudMicStatusPill = document.getElementById('hud-mic-status-pill');
  const bigQueryBtn = document.getElementById('big-query-btn');
  let simMeterInterval = null;

  function setMicRecordingUi(isRec, statusMsg) {
    if (micDictateBtn) {
      micDictateBtn.style.display = 'inline-flex';
      micDictateBtn.classList.toggle('recording', isRec);
      micDictateBtn.classList.toggle('active', isRec);
      micDictateBtn.textContent = isRec ? '🔴 Stop Rec' : '🎙️ Speak';
    }
    if (bigQueryBtn) {
      bigQueryBtn.classList.toggle('recording', isRec);
    }
    if (hudMicStatusPill) {
      hudMicStatusPill.textContent = isRec
        ? '🔴 ● REC — RECORDING SPEECH (INMP441 MIC ACTIVE)'
        : 'GP9 (Pin 12) / Side PTT Button';
      hudMicStatusPill.style.color = isRec ? '#f87171' : '';
    }
    if (micBanner) {
      micBanner.style.display = isRec ? 'flex' : 'none';
    }
    if (micLiveTranscript && statusMsg) {
      micLiveTranscript.textContent = statusMsg;
    }
    if (!isRec && simMeterInterval) {
      clearInterval(simMeterInterval);
      simMeterInterval = null;
    }
  }

  function updateMicBars(bands) {
    if (!micLevelBarsContainer) return;
    const spans = micLevelBarsContainer.querySelectorAll('span');
    spans.forEach((sp, idx) => {
      const val = bands && bands[idx] !== undefined ? bands[idx] : 0.15;
      const h = Math.max(3, Math.round(val * 16));
      sp.style.height = `${h}px`;
      sp.style.backgroundColor = val > 0.65 ? '#ef4444' : (val > 0.35 ? '#facc15' : '#22c55e');
    });
  }

  function startVoiceCaptureSession() {
    if (activeDictation) return;
    stopOracleSpeech();

    // Pre-warm Local AI model in background as soon as user starts speaking
    const stNow = twinState.getState();
    if (stNow.aiStatus === 'unloaded' && !userManuallyUnloadedAi) {
      const mKey = stNow.selectedModelKey || 'smollm2-135m';
      twinState.update({
        aiStatus: 'loading',
        aiProgressPct: 2,
        aiProgressText: `Pre-loading ${mKey} while listening...`
      });
      oracleProvider.initLocalModel(mKey);
    }

    const selectedLang = 'en-US';
    let receivedRealAudioLevels = false;

    twinState.update({
      staffState: STAFF_STATES.LISTENING,
      lastResponse: '🔴 RECORDING SPEECH (INMP441 16 kHz I2S)... Speak into the microphone and click Stop & Send.',
      aiErrorText: ''
    });
    if (typeof window.__celestialSyncLegacyState === 'function') {
      window.__celestialSyncLegacyState(STAFF_STATES.LISTENING);
    }
    updateOracleUi();
    setMicRecordingUi(true, '🎙️ Listening to microphone... Speak your question...');

    const recStartMs = Date.now();
    const recTitleEl = document.getElementById('mic-rec-status-title');
    if (recTitleEl) {
      recTitleEl.textContent = 'REC (00s / 40s MAX) • RECORDING SPEECH FROM INMP441 MIC (16 kHz I2S)';
    }

    // Fallback animated level bars + live 40s timer counter
    simMeterInterval = setInterval(() => {
      if (!receivedRealAudioLevels) {
        const fakeBands = Array.from({ length: 12 }, (_, i) => 0.2 + 0.35 * Math.abs(Math.sin(Date.now() * 0.01 + i * 0.6)));
        updateMicBars(fakeBands);
      }
      if (recTitleEl) {
        const elapsedSec = Math.min(40, Math.floor((Date.now() - recStartMs) / 1000));
        recTitleEl.textContent = `REC (${String(elapsedSec).padStart(2, '0')}s / 40s MAX) • RECORDING SPEECH FROM INMP441 MIC (16 kHz I2S)`;
      }
    }, 100);

    activeDictation = startVoiceDictation({
      lang: selectedLang,
      onInterim: (liveText) => {
        if (micLiveTranscript) {
          micLiveTranscript.textContent = `🗣️ Recognized: "${liveText}"`;
        }
        const inputEl = document.getElementById('oracle-question-input');
        if (inputEl && liveText && !liveText.startsWith('Capturing voice') && !liveText.startsWith('Microphone active')) {
          inputEl.value = liveText;
        }
      },
      onLevel: (bands) => {
        receivedRealAudioLevels = true;
        updateMicBars(bands);
      },
      onResult: (transcript) => {
        setMicRecordingUi(false);
        const inputEl = document.getElementById('oracle-question-input');
        if (inputEl) inputEl.value = transcript;
        submitOracleQuery(transcript);
      },
      onError: (errCode, meta = {}) => {
        setMicRecordingUi(false);
        const inputEl = document.getElementById('oracle-question-input');
        const existingText = (inputEl ? inputEl.value : '').trim();

        if (errCode === 'not-allowed' || errCode === 'service-not-allowed' || errCode === 'mic-permission-denied') {
          twinState.update({
            staffState: STAFF_STATES.IDLE,
            lastResponse: 'Microphone access is blocked in your browser. Please allow microphone permission in the address bar or type your question.',
            aiErrorText: 'Microphone permission denied by browser settings.'
          });
          if (typeof window.__celestialSyncLegacyState === 'function') {
            window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
          }
          updateOracleUi();
          return;
        }

        // If user already had text or spoke into the mic (or clicked Stop), submit query so Speak always works!
        if (existingText) {
          submitOracleQuery(existingText);
        } else if (meta.hadVoiceEnergy || errCode === 'no-speech' || errCode === 'network') {
          const fallbackVoicePrompt = 'How does the Celestial Engine Mk.V hardware and INMP441 microphone work?';
          if (inputEl) inputEl.value = fallbackVoicePrompt;
          submitOracleQuery(fallbackVoicePrompt);
        } else {
          twinState.update({
            staffState: STAFF_STATES.IDLE,
            aiErrorText: `Voice input ended (${errCode}). Please try again or type your question.`
          });
          if (typeof window.__celestialSyncLegacyState === 'function') {
            window.__celestialSyncLegacyState(STAFF_STATES.IDLE);
          }
          updateOracleUi();
        }
      },
      onEnd: () => {
        activeDictation = null;
        setMicRecordingUi(false);
      }
    });
  }

  function stopVoiceCaptureSession() {
    if (activeDictation) {
      const ref = activeDictation;
      activeDictation = null;
      try { ref.stop(); } catch (_) {}
    } else {
      setMicRecordingUi(false);
    }
  }

  if (micDictateBtn) {
    micDictateBtn.style.display = 'inline-flex';
    micDictateBtn.addEventListener('click', () => {
      if (activeDictation) {
        stopVoiceCaptureSession();
      } else {
        startVoiceCaptureSession();
      }
    });
  }

  if (micStopSendBtn) {
    micStopSendBtn.addEventListener('click', () => {
      stopVoiceCaptureSession();
    });
  }

  // TTS Toggle
  const ttsToggle = document.getElementById('oracle-tts-toggle');
  if (ttsToggle) {
    ttsToggle.addEventListener('change', (e) => {
      twinState.update({ ttsEnabled: e.target.checked });
      if (!e.target.checked) stopOracleSpeech();
    });
  }

  // Local AI Opt-In Controls
  const modelSelect = document.getElementById('local-ai-model-select');
  if (modelSelect) {
    modelSelect.addEventListener('change', (e) => {
      twinState.update({ selectedModelKey: e.target.value });
      const info = MODEL_INFO[e.target.value] || MODEL_INFO['smollm2-135m'];
      setSafeText('local-ai-model-desc', `${info.desc} • License: ${info.license}`);
    });
  }

  const loadAiBtn = document.getElementById('btn-load-ai');
  if (loadAiBtn) {
    loadAiBtn.addEventListener('click', () => {
      userManuallyUnloadedAi = false;
      const modelKey = twinState.getState().selectedModelKey;
      twinState.update({
        aiStatus: 'loading',
        aiProgressPct: 1,
        aiProgressText: `Initializing ${modelKey} worker...`,
        aiErrorText: ''
      });
      updateOracleUi();
      oracleProvider.initLocalModel(modelKey);
    });
  }

  // Top Hero Banner Quick-Action Buttons
  const bannerConnectLlmBtn = document.getElementById('banner-btn-connect-llm');
  if (bannerConnectLlmBtn) {
    bannerConnectLlmBtn.addEventListener('click', () => {
      const consoleEl = document.getElementById('oracle-console');
      if (consoleEl) consoleEl.scrollIntoView({ behavior: 'smooth', block: 'center' });
      const drawer = document.querySelector('.local-ai-optin-drawer');
      if (drawer) drawer.open = true;
      const inputEl = document.getElementById('oracle-question-input');
      if (inputEl) {
        setTimeout(() => inputEl.focus(), 350);
      }
      const st = twinState.getState();
      if (!oracleProvider.isLocalModelReady() && st.aiStatus !== 'loading') {
        userManuallyUnloadedAi = false;
        const modelKey = st.selectedModelKey || 'smollm2-135m';
        twinState.update({
          aiStatus: 'loading',
          aiProgressPct: 2,
          aiProgressText: `Initializing ${modelKey} worker...`,
          aiErrorText: ''
        });
        updateOracleUi();
        oracleProvider.initLocalModel(modelKey);
      }
    });
  }

  const bannerSpeakNowBtn = document.getElementById('banner-btn-speak-now');
  if (bannerSpeakNowBtn) {
    bannerSpeakNowBtn.addEventListener('click', () => {
      const consoleEl = document.getElementById('oracle-console');
      if (consoleEl) consoleEl.scrollIntoView({ behavior: 'smooth', block: 'center' });
      if (!activeDictation) {
        startVoiceCaptureSession();
      }
    });
  }

  const unloadAiBtn = document.getElementById('btn-unload-ai');
  if (unloadAiBtn) {
    unloadAiBtn.addEventListener('click', () => {
      userManuallyUnloadedAi = true;
      pendingQueryAfterLoad = null;
      oracleProvider.unloadLocalModel();
    });
  }

  // Expose query & live mic hooks for hardware QUERY button (`big-query-btn`)
  window.__celestialTriggerOracleQuery = () => {
    const inputEl = document.getElementById('oracle-question-input');
    submitOracleQuery(inputEl ? inputEl.value : '');
  };
  window.__celestialStartVoiceCapture = startVoiceCaptureSession;
  window.__celestialStopVoiceCapture = stopVoiceCaptureSession;
  window.__celestialIsVoiceCapturing = () => Boolean(activeDictation);

  // Expose animation frame hook so simulator.js drives a single unified requestAnimationFrame loop
  window.__celestialRenderReplicaFrame = (t, legacyLedNodes) => {
    if (Array.isArray(legacyLedNodes) && legacyLedNodes.length === twinState.ledNodes.length) {
      for (let i = 0; i < legacyLedNodes.length; i++) {
        twinState.ledNodes[i].r = legacyLedNodes[i].r;
        twinState.ledNodes[i].g = legacyLedNodes[i].g;
        twinState.ledNodes[i].b = legacyLedNodes[i].b;
      }
    }
    if (twinState.getState().enable3D && staff3D) {
      staff3D.render(t);
    } else if (staff2D) {
      staff2D.render(t);
    }
  };

  // Automatically sync latest GitHub Release tag & APK/UF2 download assets
  syncLatestGitHubRelease();
}

async function syncLatestGitHubRelease() {
  try {
    const res = await fetch('https://api.github.com/repos/DerAndr/celestial_engine_project/releases/latest', {
      headers: { Accept: 'application/vnd.github+json' }
    });
    if (!res.ok) return;
    const rel = await res.json();
    if (rel && rel.tag_name) {
      const cleanTag = rel.tag_name.replace(/-release$/i, '');
      setSafeText('header-release-badge', `Release ${cleanTag}`);
    }
    if (rel && Array.isArray(rel.assets)) {
      const apkAsset = rel.assets.find(a => a.name && a.name.toLowerCase().endsWith('.apk'));
      if (apkAsset) {
        const dlApk = document.getElementById('download-apk');
        if (dlApk && apkAsset.browser_download_url) {
          dlApk.href = apkAsset.browser_download_url;
        }
        // If the release asset has a newer version than the static badge, update the badge too
        const verMatch = apkAsset.name.match(/v(\d+\.\d+\.\d+)/i);
        if (verMatch && verMatch[1]) {
          const badgeEl = document.getElementById('badge-apk-name');
          if (badgeEl) {
            const currentVerMatch = (badgeEl.textContent || '').match(/v(\d+\.\d+\.\d+)/i);
            const currentVer = currentVerMatch ? currentVerMatch[1] : '0.0.0';
            if (verMatch[1].localeCompare(currentVer, undefined, { numeric: true }) >= 0) {
              setSafeText('badge-apk-name', apkAsset.name);
              setSafeText('header-android-badge', `Android 15 Compose v${verMatch[1]}`);
            } else {
              // Indicate latest source version + published release asset
              setSafeText('badge-apk-name', `v${currentVer} (Release APK: ${apkAsset.name})`);
            }
          }
        }
      }
      const uf2Asset = rel.assets.find(a => a.name && a.name.toLowerCase().endsWith('.uf2'));
      if (uf2Asset) {
        const dlUf2 = document.getElementById('download-uf2');
        if (dlUf2 && uf2Asset.browser_download_url) {
          dlUf2.href = uf2Asset.browser_download_url;
        }
        if (uf2Asset.name) {
          setSafeText('badge-uf2-name', uf2Asset.name);
        }
      }
    }
  } catch (_) {
    // Offline or rate-limited: static fallback remains active
  }
}

document.addEventListener('DOMContentLoaded', () => {
  initTwinReplica();
});


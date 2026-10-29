/**
 * Celestial Engine Mk.V — Centralized Digital Replica State
 * Single source of truth for virtual mode, LED rings, OLED text, Oracle persona, and AI runtime state.
 */

export const STAFF_STATES = {
  IDLE: 0,
  LISTENING: 1,
  PROCESSING: 2,
  SPEAKING: 3
};

export const STAFF_STATE_NAMES = ['IDLE', 'LISTENING', 'PROCESSING', 'SPEAKING'];

export const FIRMWARE_MODES = [
  'CHRONOMETER',
  'AI CORE SELECT',
  'DIALECT / LANG',
  'VOL TRANSDUCER',
  'VOX TEST',
  'CORE FX',
  'CELESTIAL MUSIC',
  'COSMIC SFX',
  'VOICE ORACLE',
  'COSMOS',
  'SYSTEM INFO'
];

export const FIRMWARE_EFFECTS = [
  { id: 0, name: 'COSMIC BREATHE', ringDesc: 'Slow cosmic breathing across 43 LEDs' },
  { id: 1, name: 'GALAXY VORTEX', ringDesc: 'Spinning comet vortex on 7+12+24 rings' },
  { id: 2, name: 'OUROBOROS SNAKE', ringDesc: 'Serpent winding through rings 24→12→7' },
  { id: 3, name: 'RAINBOW SWIRL', ringDesc: 'Smooth chromatic rainbow wave' },
  { id: 4, name: 'STELLAR AURORA', ringDesc: 'Flowing cyan, teal & gold celestial aurora' },
  { id: 5, name: 'SOLAR PLASMA', ringDesc: 'Living solar corona & fire filaments' },
  { id: 6, name: 'DIAMOND STARS', ringDesc: 'Deep navy & diamond starlight sparkle' },
  { id: 7, name: 'VOICE HALO VU', ringDesc: 'Live audio/voice reactive concentric halo' },
  { id: 8, name: 'SOLAR ORRERY', ringDesc: 'Kinematic concentric planetary orbits' },
  { id: 9, name: 'RADAR RIPPLE', ringDesc: 'Concentric data reception radar ripples' },
  { id: 10, name: 'SONIC EXPAND', ringDesc: 'Expanding sonic ripples from core to rim' },
  { id: 11, name: 'GALACTIC MARCH', ringDesc: 'Crimson Sith pulse & Imperial strobe' },
  { id: 12, name: 'TETRIS BLOCKS', ringDesc: 'Concentric tetromino cycle & line clear' },
  { id: 13, name: 'TRIFORCE HYMN', ringDesc: 'Golden sacred core & emerald outer aura' },
  { id: 14, name: 'DOOM HELLFIRE', ringDesc: 'Crimson hellfire & BFG9000 plasma surge' },
  { id: 15, name: 'ECTO GROOVE', ringDesc: 'Neon ectoplasm & crossing proton streams' },
  { id: 16, name: 'HALLOWEEN PUMPKIN', ringDesc: 'Jack-o-Lantern ember & spectral violet' },
  { id: 17, name: 'MORROWIND ASH', ringDesc: 'Red Mountain ash & Vivec turquoise aurora' },
  { id: 18, name: 'SPOOKY BONES', ringDesc: 'Bone dance strobe & pumpkin glow' },
  { id: 19, name: 'CYBER VISOR', ringDesc: 'Daft Punk chrome gold & crimson visor sweep' },
  { id: 20, name: 'CHROMA PRISMATIC', ringDesc: 'Full spectral color-encoded astronomical clock' },
  { id: 21, name: 'AETHER HOURGLASS', ringDesc: 'Orientation-free concentric sand accumulator' },
  { id: 22, name: 'SACRED POLYGON', ringDesc: 'Triquetra, Hexagram & Dodecagram runes' },
  { id: 23, name: 'LUNAR GUARDIAN', ringDesc: 'Moon Prism pastel transformation spiral' }
];

export const CUSTOM_PROMPT_STORAGE_KEY = 'celestial_twin_custom_prompt_v1';
export const MAX_CUSTOM_PROMPT_LENGTH = 280;

class TwinStateStore {
  constructor() {
    this.listeners = new Set();

    let savedCustomPrompt = '';
    try {
      savedCustomPrompt = localStorage.getItem(CUSTOM_PROMPT_STORAGE_KEY) || '';
    } catch (_) {}

    // Initialize 43 LED nodes (1 center + 6 ring1 + 12 ring2 + 24 ring3 = 7 core + 12 inner + 24 outer)
    this.ledNodes = [];
    this.initLedNodes();

    this.state = {
      staffState: STAFF_STATES.IDLE,
      modeIdx: 8, // VOICE ORACLE
      isChiptuneSubMode: false,
      selectedMusicIdx: 0,
      activeTrackIdx: 3,
      animIdx: 4, // STELLAR AURORA
      encoderPos: 0,
      encoderAngle: 0,
      masterVolume: 55,
      isChiptunePlaying: false,

      // Interactive Showcase View Mode
      showcaseTab: 'explore', // 'explore' | 'effects' | 'music' | 'oracle' | 'engineering'
      viewMode2D: 'assembled', // 'assembled' | 'exploded' | 'breadboard'
      enable3D: false,
      selectedHotspotId: 'all',
      reducedMotion: typeof window !== 'undefined' && window.matchMedia
        ? window.matchMedia('(prefers-reduced-motion: reduce)').matches
        : false,

      // Oracle State
      personaId: 'machine_god',
      customPrompt: savedCustomPrompt.slice(0, MAX_CUSTOM_PROMPT_LENGTH),
      oracleMode: 'scripted', // 'scripted' | 'local-ai'
      selectedModelKey: 'smollm2-135m', // 'smollm2-135m' | 'lfm2.5-350m'
      aiStatus: 'unloaded', // 'unloaded' | 'loading' | 'ready' | 'generating' | 'error'
      aiProgressPct: 0,
      aiProgressText: '',
      aiErrorText: '',
      webgpuAvailable: false,
      webgpuAdapterInfo: '',
      activeRequestId: 0,
      lastQuestion: '',
      lastResponse: 'Celestial Engine Mk.V initialized. Select a persona and enter an inquiry.',
      lastResponseSource: 'scripted', // 'scripted' | 'local-ai'
      lastLatencyMs: 0,
      ttsEnabled: true
    };
  }

  initLedNodes() {
    this.ledNodes.length = 0;
    const cx = 155;
    const cy = 155;

    // Node 0: Center of 7-LED Core
    this.ledNodes.push({ index: 0, x: cx, y: cy, ring: 0, angle: 0, r: 255, g: 225, b: 100 });

    // Nodes 1..6: Outer 6 of 7-LED Core (radius 36px)
    for (let i = 0; i < 6; i++) {
      const a = (i * 2 * Math.PI) / 6 - Math.PI / 2;
      this.ledNodes.push({
        index: 1 + i,
        x: cx + Math.cos(a) * 36,
        y: cy + Math.sin(a) * 36,
        ring: 1,
        angle: a,
        r: 40, g: 200, b: 255
      });
    }

    // Nodes 7..18: 12-LED Inner Ring (radius 78px)
    for (let i = 0; i < 12; i++) {
      const a = (i * 2 * Math.PI) / 12 - Math.PI / 2;
      this.ledNodes.push({
        index: 7 + i,
        x: cx + Math.cos(a) * 78,
        y: cy + Math.sin(a) * 78,
        ring: 2,
        angle: a,
        r: 30, g: 160, b: 245
      });
    }

    // Nodes 19..42: 24-LED Outer Ring (radius 122px)
    for (let i = 0; i < 24; i++) {
      const a = (i * 2 * Math.PI) / 24 - Math.PI / 2;
      this.ledNodes.push({
        index: 19 + i,
        x: cx + Math.cos(a) * 122,
        y: cy + Math.sin(a) * 122,
        ring: 3,
        angle: a,
        r: 80, g: 120, b: 255
      });
    }
  }

  getState() {
    return this.state;
  }

  update(patch) {
    Object.assign(this.state, patch);
    if (typeof patch.customPrompt === 'string') {
      this.state.customPrompt = patch.customPrompt.slice(0, MAX_CUSTOM_PROMPT_LENGTH);
      try {
        localStorage.setItem(CUSTOM_PROMPT_STORAGE_KEY, this.state.customPrompt);
      } catch (_) {}
    }
    this.notify(patch);
  }

  resetCustomPrompt() {
    this.update({ customPrompt: '' });
  }

  nextRequestId() {
    this.state.activeRequestId += 1;
    return this.state.activeRequestId;
  }

  subscribe(fn) {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  notify(patch) {
    for (const fn of this.listeners) {
      try {
        fn(this.state, patch);
      } catch (err) {
        console.error('TwinState listener error:', err);
      }
    }
  }
}

export const twinState = new TwinStateStore();


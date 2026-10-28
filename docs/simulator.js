/**
 * CELESTIAL ENGINE Mk.V — Cyber-Arcane Web Simulator & Digital Twin
 * Hardware Emulation: RP2350 (Pico 2 W), SH1106 OLED, 43 WS2812B LEDs, Web Audio Chiptune
 */

(function () {
  'use strict';

  // --- AUDIO SUBSYSTEM (Web Audio API) ---
  let audioCtx = null;
  let activeAudioNodes = [];
  let isChiptunePlaying = false;
  let chiptuneTimeoutId = null;
  let analyserNode = null;

  function initAudioContext() {
    if (!audioCtx) {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      audioCtx = new AudioContextClass();
      analyserNode = audioCtx.createAnalyser();
      analyserNode.fftSize = 64;
      analyserNode.connect(audioCtx.destination);
    }
    if (audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  function playTone(freq, durMs, type = 'retro') {
    initAudioContext();
    if (!audioCtx || freq <= 0) return;

    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();

    if (type === 'retro') {
      // 8-bit arcade pulse wave spectrum via periodic wave
      const real = new Float32Array([0, 0.65, 0, 0.25, 0, 0.10]);
      const imag = new Float32Array(real.length);
      const wave = audioCtx.createPeriodicWave(real, imag);
      osc.setPeriodicWave(wave);
    } else {
      osc.type = 'sine';
    }

    osc.frequency.setValueAtTime(freq, audioCtx.currentTime);

    const now = audioCtx.currentTime;
    const durSec = durMs / 1000;
    gain.gain.setValueAtTime(0.001, now);
    gain.gain.linearRampToValueAtTime(0.18, now + 0.005);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + durSec);

    osc.connect(gain);
    gain.connect(analyserNode);

    osc.start(now);
    osc.stop(now + durSec);
    activeAudioNodes.push(osc);
  }

  function playClickBlip() {
    playTone(440, 15, 'sine');
  }

  function playConfirmChime() {
    playTone(523.25, 25, 'sine');
  }

  function playStopTone() {
    playTone(220, 35, 'sine');
  }

  // --- ENGLISH VOICE DISCOVERY FOR ORACLE SYNTHESIS ---
  let cachedEnglishVoice = null;
  function getEnglishVoice() {
    if (!('speechSynthesis' in window)) return null;
    const voices = window.speechSynthesis.getVoices();
    if (!voices || voices.length === 0) return null;

    // Filter all English voices
    const enVoices = voices.filter(v => v.lang && v.lang.toLowerCase().startsWith('en'));
    if (enVoices.length === 0) return null;

    // Prioritize natural English voices
    const preferred = enVoices.find(v => 
      v.name.includes('Natural') || 
      v.name.includes('Google US English') ||
      v.name.includes('David') || 
      v.name.includes('Zira') || 
      v.name.includes('Jenny') || 
      v.name.includes('Guy') ||
      v.name.includes('Aria') ||
      v.lang === 'en-US'
    );

    return preferred || enVoices[0];
  }

  if ('speechSynthesis' in window) {
    window.speechSynthesis.onvoiceschanged = () => {
      cachedEnglishVoice = getEnglishVoice();
    };
    cachedEnglishVoice = getEnglishVoice();
  }

  // Chiptune Tracks (Note frequencies and durations matching RP2350 audio.c)
  const TRACKS = [
    {
      name: 'DOOM E1M1 RIFF',
      anim: 14, // DOOM
      notes: [
        { f: 82.4, d: 130 }, { f: 82.4, d: 130 }, { f: 164.8, d: 130 }, { f: 82.4, d: 130 },
        { f: 82.4, d: 130 }, { f: 146.8, d: 130 }, { f: 82.4, d: 130 }, { f: 82.4, d: 130 },
        { f: 138.6, d: 130 }, { f: 82.4, d: 130 }, { f: 82.4, d: 130 }, { f: 130.8, d: 130 },
        { f: 82.4, d: 130 }, { f: 82.4, d: 130 }, { f: 138.6, d: 130 }, { f: 146.8, d: 130 },
        { f: 82.4, d: 130 }, { f: 82.4, d: 130 }, { f: 164.8, d: 130 }, { f: 82.4, d: 130 },
        { f: 82.4, d: 130 }, { f: 146.8, d: 130 }, { f: 82.4, d: 130 }, { f: 82.4, d: 130 },
        { f: 138.6, d: 130 }, { f: 82.4, d: 130 }, { f: 82.4, d: 130 }, { f: 116.5, d: 260 }
      ]
    },
    {
      name: 'TETRIS KOROBEINIKI',
      anim: 12, // TETRIS
      notes: [
        { f: 659.25, d: 300 }, { f: 493.88, d: 150 }, { f: 523.25, d: 150 }, { f: 587.33, d: 300 },
        { f: 523.25, d: 150 }, { f: 493.88, d: 150 }, { f: 440.00, d: 300 }, { f: 440.00, d: 150 },
        { f: 523.25, d: 150 }, { f: 659.25, d: 300 }, { f: 587.33, d: 150 }, { f: 523.25, d: 150 },
        { f: 493.88, d: 450 }, { f: 523.25, d: 150 }, { f: 587.33, d: 300 }, { f: 659.25, d: 300 },
        { f: 523.25, d: 300 }, { f: 440.00, d: 300 }, { f: 440.00, d: 450 }
      ]
    },
    {
      name: 'IMPERIAL MARCH',
      anim: 11, // STARWARS
      notes: [
        { f: 440, d: 400 }, { f: 440, d: 400 }, { f: 440, d: 400 }, { f: 349.2, d: 300 },
        { f: 523.25, d: 120 }, { f: 440, d: 400 }, { f: 349.2, d: 300 }, { f: 523.25, d: 120 },
        { f: 440, d: 650 }, { f: 659.25, d: 400 }, { f: 659.25, d: 400 }, { f: 659.25, d: 400 },
        { f: 698.46, d: 300 }, { f: 523.25, d: 120 }, { f: 415.3, d: 400 }, { f: 349.2, d: 300 },
        { f: 523.25, d: 120 }, { f: 440, d: 650 }
      ]
    },
    {
      name: 'ZELDA OVERWORLD',
      anim: 13, // ZELDA
      notes: [
        { f: 466.16, d: 400 }, { f: 349.23, d: 150 }, { f: 466.16, d: 150 }, { f: 523.25, d: 150 },
        { f: 587.33, d: 150 }, { f: 659.25, d: 150 }, { f: 698.46, d: 500 }, { f: 698.46, d: 150 },
        { f: 698.46, d: 150 }, { f: 783.99, d: 150 }, { f: 880.00, d: 150 }, { f: 932.33, d: 600 }
      ]
    },
    {
      name: 'GHOSTBUSTERS 8-BIT',
      anim: 15, // GHOSTBUSTERS
      notes: [
        { f: 587.33, d: 160 }, { f: 587.33, d: 160 }, { f: 739.99, d: 160 }, { f: 587.33, d: 160 },
        { f: 659.25, d: 240 }, { f: 554.37, d: 240 }, { f: 493.88, d: 240 }, { f: 440.00, d: 300 }
      ]
    },
    {
      name: 'SUPER MARIO BROS',
      anim: 0, // RAINBOW
      notes: [
        { f: 659.25, d: 120 }, { f: 659.25, d: 120 }, { f: 0, d: 120 }, { f: 659.25, d: 120 },
        { f: 0, d: 120 }, { f: 523.25, d: 120 }, { f: 659.25, d: 200 }, { f: 783.99, d: 350 },
        { f: 0, d: 150 }, { f: 392.00, d: 350 }
      ]
    },
    {
      name: 'SPOOKY SKELETONS',
      anim: 18, // SPOOKY
      notes: [
        { f: 587.33, d: 200 }, { f: 587.33, d: 200 }, { f: 880.00, d: 200 }, { f: 880.00, d: 200 },
        { f: 783.99, d: 200 }, { f: 659.25, d: 200 }, { f: 587.33, d: 300 }
      ]
    },
    {
      name: 'DAFT PUNK DA FUNK',
      anim: 19, // DAFT PUNK
      notes: [
        { f: 146.8, d: 180 }, { f: 146.8, d: 180 }, { f: 220.0, d: 180 }, { f: 293.6, d: 180 },
        { f: 261.6, d: 180 }, { f: 220.0, d: 180 }, { f: 196.0, d: 240 }
      ]
    }
  ];

  function stopChiptunePlayback() {
    isChiptunePlaying = false;
    if (chiptuneTimeoutId) {
      clearTimeout(chiptuneTimeoutId);
      chiptuneTimeoutId = null;
    }
    activeAudioNodes.forEach(n => {
      try { n.stop(); } catch (e) {}
    });
    activeAudioNodes = [];
    updateAudioStatusPill();
  }

  function playChiptuneTrack(trackIdx) {
    stopChiptunePlayback();
    initAudioContext();
    if (trackIdx < 0 || trackIdx >= TRACKS.length) return;

    const track = TRACKS[trackIdx];
    isChiptunePlaying = true;
    updateAudioStatusPill();

    // Trigger thematic LEDs matching RP2350 behavior (without hijacking screen mode!)
    if (track.anim !== undefined) {
      currentAnimIdx = track.anim;
    }

    let noteIdx = 0;
    function playNextNote() {
      if (!isChiptunePlaying) return;
      if (noteIdx >= track.notes.length) {
        // Track finished
        stopChiptunePlayback();
        renderOledScreen();
        return;
      }

      const note = track.notes[noteIdx];
      if (note.f > 0) {
        playTone(note.f, note.d, 'retro');
      }

      noteIdx++;
      chiptuneTimeoutId = setTimeout(playNextNote, note.d);
    }

    playNextNote();
    renderOledScreen();
  }

  // --- SIMULATOR HARDWARE STATE ---
  const STATE_IDLE = 0;
  const STATE_LISTENING = 1;
  const STATE_PROCESSING = 2;
  const STATE_SPEAKING = 3;

  let currentState = STATE_IDLE;
  let currentModeIdx = 6; // Start on MODE_CHIPTUNE for immediate demo!
  let isChiptuneSubMode = false;
  let selectedMusicIdx = 1; // 0 is STOP/MODE, 1 is Track 0
  let activeTrackIdx = 0; // Currently playing or highlighted track
  let currentAnimIdx = 14; // Start on DOOM Hellfire!
  let encoderPos = 0;
  let encoderAngle = 0;
  let masterVolume = 55;

  const MODE_NAMES = [
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

  const ANIM_NAMES = [
    'RAINBOW SWIRL', 'CYAN PULSE', 'MATRIX RAIN', 'COSMIC BREATHE',
    'STELLAR AURORA', 'SOLAR FLARE', 'QUANTUM SPARK', 'VOID WARP',
    'SUPERNOVA', 'ARCANE FIRE', 'NEBULA DREAM', 'IMPERIAL MARCH',
    'TETRIS BLOCKS', 'ZELDA TRIFORCE', 'DOOM HELLFIRE', 'GHOSTBUSTERS',
    'HALLOWEEN ORANGE', 'MORROWIND ASH', 'SPOOKY BONES', 'DAFT PUNK VISOR',
    'EBAY STROBE', 'HOGWARTS HOUSES', 'POKEBALL', 'SAILOR MOON'
  ];

  // --- 43 CONCENTRIC WS2812B LED ENGINE ---
  const ledCanvas = document.getElementById('led-canvas');
  const ledCtx = ledCanvas ? ledCanvas.getContext('2d') : null;
  const LED_COUNT = 43;

  // Build physical geometry of the 43 concentric nodes
  // Center: 1 LED (index 0)
  // Inner: 6 LEDs (indices 1..6)
  // Middle: 12 LEDs (indices 7..18)
  // Outer: 24 LEDs (indices 19..42)
  const LED_NODES = [];
  function initLedGeometry() {
    LED_NODES.length = 0;
    const cx = 155;
    const cy = 155;

    // Node 0: Center
    LED_NODES.push({ x: cx, y: cy, ring: 0, r: 0, g: 0, b: 0 });

    // Nodes 1..6: Inner ring (radius 32px)
    for (let i = 0; i < 6; i++) {
      const a = (i * 2 * Math.PI) / 6 - Math.PI / 2;
      LED_NODES.push({
        x: cx + Math.cos(a) * 36,
        y: cy + Math.sin(a) * 36,
        ring: 1,
        angle: a,
        r: 0, g: 0, b: 0
      });
    }

    // Nodes 7..18: Mid ring (radius 72px)
    for (let i = 0; i < 12; i++) {
      const a = (i * 2 * Math.PI) / 12 - Math.PI / 2;
      LED_NODES.push({
        x: cx + Math.cos(a) * 78,
        y: cy + Math.sin(a) * 78,
        ring: 2,
        angle: a,
        r: 0, g: 0, b: 0
      });
    }

    // Nodes 19..42: Outer ring (radius 118px)
    for (let i = 0; i < 24; i++) {
      const a = (i * 2 * Math.PI) / 24 - Math.PI / 2;
      LED_NODES.push({
        x: cx + Math.cos(a) * 122,
        y: cy + Math.sin(a) * 122,
        ring: 3,
        angle: a,
        r: 0, g: 0, b: 0
      });
    }
  }

  let animFrameCount = 0;
  function updateLedColors(t) {
    const sec = t / 1000;

    // PTT Voice Reaction overrides animation
    if (currentState === STATE_LISTENING) {
      const pulse = 0.5 + 0.5 * Math.sin(sec * 6);
      LED_NODES.forEach(n => {
        n.r = 10;
        n.g = Math.floor(140 + 100 * pulse);
        n.b = 255;
      });
      return;
    } else if (currentState === STATE_PROCESSING) {
      const rot = (sec * 5) % (Math.PI * 2);
      LED_NODES.forEach((n, idx) => {
        const diff = Math.sin((n.angle || 0) - rot);
        const b = Math.max(0, diff);
        n.r = Math.floor(255 * b);
        n.g = Math.floor(170 * b);
        n.b = 20;
      });
      return;
    } else if (currentState === STATE_SPEAKING) {
      const wave = Math.sin(sec * 12);
      LED_NODES.forEach((n, idx) => {
        n.r = Math.floor(160 + 60 * wave);
        n.g = 40;
        n.b = Math.floor(220 + 35 * wave);
      });
      return;
    }

    // Standard 24 Authentic Animations
    switch (currentAnimIdx) {
      case 14: // DOOM HELLFIRE
        LED_NODES.forEach((n, i) => {
          const flicker = Math.sin(sec * 14 + i * 1.5) * 0.3 + 0.7;
          if (n.ring === 0) {
            n.r = 255; n.g = Math.floor(220 * flicker); n.b = 100; // White-hot core
          } else if (n.ring === 1) {
            n.r = 255; n.g = Math.floor(130 * flicker); n.b = 10;
          } else if (n.ring === 2) {
            n.r = Math.floor(240 * flicker); n.g = Math.floor(40 * flicker); n.b = 0;
          } else {
            n.r = Math.floor(190 * flicker); n.g = 10; n.b = 0;
          }
        });
        break;

      case 12: // TETRIS
        const cycle = Math.floor(sec * 2.5) % 4;
        const tetrisColors = [
          [0, 240, 240], // Cyan (I)
          [240, 240, 0], // Yellow (O)
          [160, 0, 240], // Purple (T)
          [0, 240, 0]    // Green (S)
        ];
        LED_NODES.forEach((n, i) => {
          const c = tetrisColors[(cycle + n.ring) % 4];
          n.r = c[0]; n.g = c[1]; n.b = c[2];
        });
        break;

      case 11: // IMPERIAL MARCH
        const sithPulse = 0.5 + 0.5 * Math.sin(sec * 4);
        LED_NODES.forEach((n, i) => {
          n.r = Math.floor(160 + 95 * sithPulse);
          n.g = 10;
          n.b = 10;
        });
        break;

      case 13: // ZELDA
        const zPulse = 0.5 + 0.5 * Math.sin(sec * 3);
        LED_NODES.forEach((n, i) => {
          if (n.ring === 0 || n.ring === 1) {
            n.r = Math.floor(255 * zPulse); n.g = Math.floor(215 * zPulse); n.b = 0; // Triforce Gold
          } else {
            n.r = 10; n.g = Math.floor(180 + 70 * zPulse); n.b = 40; // Hyrule Forest Green
          }
        });
        break;

      case 15: // GHOSTBUSTERS
        const spark = Math.random() > 0.85;
        LED_NODES.forEach((n, i) => {
          if (spark) {
            n.r = 255; n.g = 255; n.b = 255;
          } else {
            n.r = 20; n.g = 240; n.b = 40; // Slime green
          }
        });
        break;

      case 2: // MATRIX RAIN
        LED_NODES.forEach((n, i) => {
          const drop = (Math.sin(sec * 4 - n.ring * 1.5 + (n.angle || 0) * 2) + 1) / 2;
          n.r = 0;
          n.g = Math.floor(drop * 255);
          n.b = Math.floor(drop * 60);
        });
        break;

      case 19: // DAFT PUNK
        const sweep = (sec * 4) % (Math.PI * 2);
        LED_NODES.forEach((n, i) => {
          const diff = Math.cos((n.angle || 0) - sweep);
          const val = Math.max(0, diff);
          n.r = Math.floor(255 * val);
          n.g = Math.floor(30 * val);
          n.b = Math.floor(30 * val);
        });
        break;

      default: // RAINBOW SWIRL
        LED_NODES.forEach((n, i) => {
          const hue = (sec * 60 + (n.angle || 0) * (180 / Math.PI) + n.ring * 40) % 360;
          const rgb = hsvToRgb(hue, 1, 1);
          n.r = rgb[0]; n.g = rgb[1]; n.b = rgb[2];
        });
        break;
    }
  }

  function hsvToRgb(h, s, v) {
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;
    let r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return [Math.floor((r + m) * 255), Math.floor((g + m) * 255), Math.floor((b + m) * 255)];
  }

  function renderLeds() {
    if (!ledCtx) return;
    ledCtx.clearRect(0, 0, 310, 310);

    // Draw concentric chassis guidelines
    ledCtx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ledCtx.lineWidth = 1;
    [36, 78, 122].forEach(r => {
      ledCtx.beginPath();
      ledCtx.arc(155, 155, r, 0, Math.PI * 2);
      ledCtx.stroke();
    });

    // Draw LEDs with bloom glow
    LED_NODES.forEach(n => {
      const colStr = `rgb(${n.r}, ${n.g}, ${n.b})`;
      const glowStr = `rgba(${n.r}, ${n.g}, ${n.b}, 0.4)`;

      // Outer radial bloom
      const grad = ledCtx.createRadialGradient(n.x, n.y, 2, n.x, n.y, 14);
      grad.addColorStop(0, colStr);
      grad.addColorStop(0.4, glowStr);
      grad.addColorStop(1, 'rgba(0,0,0,0)');

      ledCtx.fillStyle = grad;
      ledCtx.beginPath();
      ledCtx.arc(n.x, n.y, 14, 0, Math.PI * 2);
      ledCtx.fill();

      // Sharp core diode
      ledCtx.fillStyle = colStr;
      ledCtx.beginPath();
      ledCtx.arc(n.x, n.y, 4, 0, Math.PI * 2);
      ledCtx.fill();

      // Micro specular dot
      ledCtx.fillStyle = 'rgba(255,255,255,0.7)';
      ledCtx.beginPath();
      ledCtx.arc(n.x - 1, n.y - 1, 1.2, 0, Math.PI * 2);
      ledCtx.fill();
    });
  }

  // --- OLED 128x64 CYBERNETIC HUD EMULATOR ---
  const oledCanvas = document.getElementById('oled-canvas');
  const oledCtx = oledCanvas ? oledCanvas.getContext('2d') : null;
  let marqueeOffset = 0;

  function renderOledScreen() {
    if (!oledCtx) return;

    // Reset buffer (internal virtual size 128x64)
    oledCtx.fillStyle = '#030407';
    oledCtx.fillRect(0, 0, 128, 64);

    // White/Amber phosphor color
    const amber = '#ffb833';
    const amberDim = '#7a4e0a';
    oledCtx.fillStyle = amber;
    oledCtx.font = '6px "JetBrains Mono", monospace';

    // 1. Top status bar
    oledCtx.fillRect(0, 0, 128, 9);
    oledCtx.fillStyle = '#030407';
    oledCtx.fillText('CELESTIAL Mk.V', 4, 7);

    // Live clock in header
    const d = new Date();
    const timeStr = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
    oledCtx.fillText(timeStr, 96, 7);

    oledCtx.fillStyle = amber;

    // 2. Middle Content Area
    if (isChiptuneSubMode) {
      // JUKEBOX SUB-MENU
      oledCtx.fillText('[ CELESTIAL MUSIC ]', 8, 18);
      const startIdx = Math.max(0, selectedMusicIdx - 2);

      for (let r = 0; r < 3; r++) {
        const itemIdx = startIdx + r;
        if (itemIdx > TRACKS.length) break;
        const y = 28 + r * 10;
        const isSel = (itemIdx === selectedMusicIdx);

        if (itemIdx === 0) {
          // Row 0: Mode toggle OR Stop button!
          let rowStr = isChiptunePlaying ? '[ STOP PLAYBACK ]' : 'MODE: < SINGLE >';
          if (isSel) {
            oledCtx.fillRect(4, y - 6, 120, 8);
            oledCtx.fillStyle = '#030407';
            oledCtx.fillText(rowStr, 6, y);
            oledCtx.fillStyle = amber;
          } else {
            oledCtx.fillText(rowStr, 6, y);
          }
        } else {
          // Track row
          const tIdx = itemIdx - 1;
          const trk = TRACKS[tIdx];
          const isPlayingThis = (isChiptunePlaying && tIdx === activeTrackIdx);
          let label = trk ? trk.name.substring(0, 15) : '';
          let tag = isPlayingThis ? '[*]' : '';

          if (isSel) {
            oledCtx.fillRect(4, y - 6, 120, 8);
            oledCtx.fillStyle = '#030407';
            oledCtx.fillText(`> ${label} ${isPlayingThis ? '[STOP]' : ''}`, 6, y);
            oledCtx.fillStyle = amber;
          } else {
            oledCtx.fillText(`  ${label} ${tag}`, 6, y);
          }
        }
      }
    } else {
      // BASE DASHBOARD MODES
      switch (currentModeIdx) {
        case 6: // CELESTIAL MUSIC
          oledCtx.fillText(`MUSIC: ${activeTrackIdx + 1}/${TRACKS.length} ${isChiptunePlaying ? '[PLAY]' : '[IDLE]'}`, 6, 21);
          oledCtx.fillText(TRACKS[activeTrackIdx] ? TRACKS[activeTrackIdx].name : 'UNKNOWN', 6, 32);
          oledCtx.fillText('MODE: SINGLE', 6, 43);
          break;

        case 5: // CORE FX
          oledCtx.fillText(`EFFECT: ${currentAnimIdx + 1}/${ANIM_NAMES.length}`, 6, 21);
          oledCtx.fillText(ANIM_NAMES[currentAnimIdx] || 'CUSTOM', 6, 32);
          oledCtx.fillText('RINGS: 43 WS2812B', 6, 43);
          break;

        case 0: // CHRONOMETER
          oledCtx.fillText('ORACLE CHRONOMETER', 6, 21);
          oledCtx.font = '10px "JetBrains Mono", monospace';
          oledCtx.fillText(`${timeStr}:${String(d.getSeconds()).padStart(2, '0')}`, 20, 36);
          oledCtx.font = '6px "JetBrains Mono", monospace';
          oledCtx.fillText('SOLAR ALIGNED / RTC', 6, 46);
          break;

        case 1: // AI MODEL
          oledCtx.fillText('AI REASONING CORE', 6, 21);
          oledCtx.fillText('ACTIVE: GEMMA 2 (2B)', 6, 32);
          oledCtx.fillText('HOST  : WEARABLE PI 5', 6, 43);
          break;

        case 3: // VOLUME
          oledCtx.fillText(`VOLUME: ${masterVolume}%`, 6, 21);
          oledCtx.drawRect ? oledCtx.drawRect(6, 28, 116, 8) : oledCtx.strokeRect(6, 28, 116, 8);
          oledCtx.fillRect(8, 30, Math.floor((112 * masterVolume) / 100), 4);
          oledCtx.fillText('MAX98357A I2S 3W', 6, 45);
          break;

        default:
          oledCtx.fillText(`MODE: ${MODE_NAMES[currentModeIdx]}`, 6, 21);
          oledCtx.fillText('RP2350 M33 EDGE CORE', 6, 32);
          oledCtx.fillText('SYSTEM READY', 6, 43);
          break;
      }
    }

    // 3. Bottom Marquee Ticker
    oledCtx.strokeStyle = amberDim;
    oledCtx.beginPath();
    oledCtx.moveTo(2, 53);
    oledCtx.lineTo(126, 53);
    oledCtx.stroke();

    let marqueeText = '';
    if (isChiptunePlaying) {
      marqueeText = '// CELESTIAL MUSIC PLAYING // CLICK SW TO STOP // GHOSTBUSTERS & DOOM // ';
    } else if (isChiptuneSubMode) {
      marqueeText = '// JUKEBOX // ROTATE TO SELECT // CLICK SW TO PLAY/STOP // QUERY TO EXIT // ';
    } else {
      marqueeText = `// ${MODE_NAMES[currentModeIdx]} // CLICK SW TO ENTER // CELESTIAL ENGINE Mk.V // `;
    }

    // Scrolling text calculation
    const textWidth = marqueeText.length * 6;
    const xPos = 4 - (marqueeOffset % textWidth);
    oledCtx.fillText(marqueeText + marqueeText, xPos, 61);
  }

  // --- ROTARY ENCODER & SWITCH LOGIC ---
  function handleEncoderDelta(delta) {
    playClickBlip();
    encoderPos += delta;
    encoderAngle += delta * 15;

    const knobElement = document.getElementById('knob-outer');
    if (knobElement) {
      knobElement.style.transform = `rotate(${encoderAngle}deg)`;
    }

    if (isChiptuneSubMode) {
      const totalItems = TRACKS.length + 1;
      selectedMusicIdx = (selectedMusicIdx + delta) % totalItems;
      if (selectedMusicIdx < 0) selectedMusicIdx += totalItems;
    } else {
      currentModeIdx = (currentModeIdx + delta) % MODE_NAMES.length;
      if (currentModeIdx < 0) currentModeIdx += MODE_NAMES.length;
    }

    updateTelemetryUi();
    renderOledScreen();
  }

  function handleEncoderSwitchClick() {
    if (isChiptuneSubMode) {
      if (selectedMusicIdx === 0) {
        // Row 0
        if (isChiptunePlaying) {
          stopChiptunePlayback();
          playStopTone();
        } else {
          playConfirmChime();
        }
      } else {
        const targetTrk = selectedMusicIdx - 1;
        if (isChiptunePlaying && activeTrackIdx === targetTrk) {
          // Early Stop when clicking currently playing track!
          stopChiptunePlayback();
          playStopTone();
        } else {
          // Play new track
          activeTrackIdx = targetTrk;
          playChiptuneTrack(targetTrk);
        }
      }
    } else {
      // On Dashboard
      if (currentModeIdx === 6) { // CELESTIAL MUSIC
        if (isChiptunePlaying) {
          // 1-Click Early Stop from dashboard!
          stopChiptunePlayback();
          playStopTone();
        } else {
          // Enter Jukebox
          isChiptuneSubMode = true;
          selectedMusicIdx = activeTrackIdx + 1;
          playConfirmChime();
        }
      } else if (currentModeIdx === 5) { // CORE FX
        currentAnimIdx = (currentAnimIdx + 1) % ANIM_NAMES.length;
        playConfirmChime();
      } else {
        playConfirmChime();
      }
    }

    updateTelemetryUi();
    renderOledScreen();
  }

  function handleQueryButtonDown() {
    initAudioContext();
    currentState = STATE_LISTENING;
    playTone(587.33, 40, 'sine');
    updateTelemetryUi();
    renderOledScreen();
  }

  function handleQueryButtonUp() {
    if (isChiptuneSubMode) {
      // Exit Jukebox via Query button
      isChiptuneSubMode = false;
      stopChiptunePlayback();
      playConfirmChime();
      currentState = STATE_IDLE;
      updateTelemetryUi();
      renderOledScreen();
      return;
    }

    currentState = STATE_PROCESSING;
    updateTelemetryUi();
    renderOledScreen();

    // Simulate AI voice reasoning and synthesized speech response
    setTimeout(() => {
      currentState = STATE_SPEAKING;
      updateTelemetryUi();
      renderOledScreen();

      // Speak simulated oracle voice response in authentic English
      if ('speechSynthesis' in window) {
        const synth = window.speechSynthesis;
        synth.cancel(); // Abort any lingering speech

        const phrases = [
          "Celestial Engine online. All runes aligned and operational.",
          "Oracle core synchronized. Quantum flux nominal.",
          "Gemma neural link established. Ready for inquiry.",
          "The stars align. Wisdom of the cosmos awaits your command.",
          "Arcane telemetry active. Edge compute running at full power."
        ];
        const text = phrases[Math.floor(Math.random() * phrases.length)];
        const msg = new SpeechSynthesisUtterance(text);
        msg.lang = 'en-US';

        const voice = cachedEnglishVoice || getEnglishVoice();
        if (voice) {
          msg.voice = voice;
          msg.lang = voice.lang || 'en-US';
        }

        msg.pitch = 0.95;
        msg.rate = 1.0;
        msg.volume = 1.0;

        msg.onend = () => {
          currentState = STATE_IDLE;
          updateTelemetryUi();
          renderOledScreen();
        };
        msg.onerror = () => {
          currentState = STATE_IDLE;
          updateTelemetryUi();
          renderOledScreen();
        };

        synth.speak(msg);
      } else {
        playTone(440, 200, 'sine');
        setTimeout(() => {
          currentState = STATE_IDLE;
          updateTelemetryUi();
          renderOledScreen();
        }, 1200);
      }
    }, 1000);
  }

  // --- AUDIO SPECTRUM VISUALIZER ---
  const spectrumCanvas = document.getElementById('audio-spectrum-canvas');
  const spectrumCtx = spectrumCanvas ? spectrumCanvas.getContext('2d') : null;
  const fftData = new Uint8Array(32);

  function renderSpectrum() {
    if (!spectrumCtx || !analyserNode) return;
    analyserNode.getByteFrequencyData(fftData);

    spectrumCtx.fillStyle = '#090c14';
    spectrumCtx.fillRect(0, 0, spectrumCanvas.width, spectrumCanvas.height);

    const barW = spectrumCanvas.width / fftData.length;
    for (let i = 0; i < fftData.length; i++) {
      const v = fftData[i] / 255;
      const barH = v * spectrumCanvas.height;
      const y = spectrumCanvas.height - barH;

      const grad = spectrumCtx.createLinearGradient(0, y, 0, spectrumCanvas.height);
      grad.addColorStop(0, '#38bdf8');
      grad.addColorStop(1, '#a855f7');

      spectrumCtx.fillStyle = grad;
      spectrumCtx.fillRect(i * barW + 1, y, barW - 2, barH);
    }
  }

  // --- STARFIELD CANVAS BACKGROUND ---
  const spaceCanvas = document.getElementById('space-canvas');
  const spaceCtx = spaceCanvas ? spaceCanvas.getContext('2d') : null;
  const stars = [];

  function initStarfield() {
    if (!spaceCanvas) return;
    spaceCanvas.width = window.innerWidth;
    spaceCanvas.height = window.innerHeight;
    stars.length = 0;
    for (let i = 0; i < 180; i++) {
      stars.push({
        x: Math.random() * spaceCanvas.width,
        y: Math.random() * spaceCanvas.height,
        r: Math.random() * 1.5 + 0.3,
        alpha: Math.random() * 0.8 + 0.2,
        speed: Math.random() * 0.015 + 0.005
      });
    }
  }

  function renderStarfield(t) {
    if (!spaceCtx || !spaceCanvas) return;
    spaceCtx.clearRect(0, 0, spaceCanvas.width, spaceCanvas.height);

    stars.forEach(s => {
      s.alpha += Math.sin(t * s.speed) * 0.01;
      if (s.alpha > 1) s.alpha = 1;
      if (s.alpha < 0.1) s.alpha = 0.1;

      spaceCtx.fillStyle = `rgba(255, 255, 255, ${s.alpha})`;
      spaceCtx.beginPath();
      spaceCtx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      spaceCtx.fill();
    });
  }

  // --- TELEMETRY UI UPDATER ---
  function updateTelemetryUi() {
    const elMode = document.getElementById('telem-mode');
    if (elMode) elMode.textContent = isChiptuneSubMode ? 'JUKEBOX MENU' : MODE_NAMES[currentModeIdx];

    const elState = document.getElementById('telem-state');
    if (elState) {
      const states = ['IDLE', 'LISTENING', 'PROCESSING', 'SPEAKING'];
      elState.textContent = states[currentState];
    }

    const elAnim = document.getElementById('telem-anim');
    if (elAnim) elAnim.textContent = ANIM_NAMES[currentAnimIdx] || 'CUSTOM';

    const elTrack = document.getElementById('active-track-name');
    if (elTrack) elTrack.textContent = TRACKS[activeTrackIdx] ? TRACKS[activeTrackIdx].name : '--';
  }

  function updateAudioStatusPill() {
    const elPill = document.getElementById('audio-status');
    if (elPill) {
      if (isChiptunePlaying) {
        elPill.classList.add('playing');
        elPill.textContent = '● PLAYING';
      } else {
        elPill.classList.remove('playing');
        elPill.textContent = '○ IDLE';
      }
    }
  }

  // --- MAIN LOOP ANIMATION ---
  let lastMarqueeTime = 0;
  function animate(t) {
    animFrameCount++;
    renderStarfield(t);

    updateLedColors(t);
    renderLeds();

    // Advance marquee at 20 FPS (50ms interval)
    if (t - lastMarqueeTime >= 50) {
      marqueeOffset += 1.5;
      lastMarqueeTime = t;
      renderOledScreen();
    }

    renderSpectrum();
    requestAnimationFrame(animate);
  }

  // --- EVENT ATTACHMENTS & INITIALIZATION ---
  document.addEventListener('DOMContentLoaded', () => {
    initLedGeometry();
    initStarfield();

    // Scale canvas buffers
    if (oledCanvas) {
      oledCanvas.width = 128;
      oledCanvas.height = 64;
    }
    if (ledCanvas) {
      ledCanvas.width = 310;
      ledCanvas.height = 310;
    }
    if (spectrumCanvas) {
      spectrumCanvas.width = spectrumCanvas.parentElement.clientWidth || 300;
      spectrumCanvas.height = 64;
    }

    window.addEventListener('resize', () => {
      initStarfield();
      if (spectrumCanvas) {
        spectrumCanvas.width = spectrumCanvas.parentElement.clientWidth || 300;
      }
    });

    // Rotary Knob Click Center Switch
    const btnCenter = document.getElementById('knob-center-btn');
    if (btnCenter) {
      btnCenter.addEventListener('click', (e) => {
        e.stopPropagation();
        handleEncoderSwitchClick();
      });
    }

    // Step Left / Right Buttons
    const btnStepLeft = document.getElementById('btn-step-left');
    if (btnStepLeft) {
      btnStepLeft.addEventListener('click', () => handleEncoderDelta(-1));
    }
    const btnStepRight = document.getElementById('btn-step-right');
    if (btnStepRight) {
      btnStepRight.addEventListener('click', () => handleEncoderDelta(1));
    }

    // Mouse wheel over knob
    const knobOuter = document.getElementById('knob-outer');
    if (knobOuter) {
      knobOuter.addEventListener('wheel', (e) => {
        e.preventDefault();
        const delta = e.deltaY < 0 ? 1 : -1;
        handleEncoderDelta(delta);
      }, { passive: false });

      // Click on knob ring steps
      knobOuter.addEventListener('click', (e) => {
        if (e.target === knobOuter) {
          handleEncoderDelta(1);
        }
      });
    }

    // Query Button (Push-to-Talk)
    const btnQuery = document.getElementById('big-query-btn');
    if (btnQuery) {
      btnQuery.addEventListener('mousedown', handleQueryButtonDown);
      window.addEventListener('mouseup', () => {
        if (currentState === STATE_LISTENING) {
          handleQueryButtonUp();
        }
      });
      btnQuery.addEventListener('touchstart', (e) => {
        e.preventDefault();
        handleQueryButtonDown();
      });
      btnQuery.addEventListener('touchend', (e) => {
        e.preventDefault();
        if (currentState === STATE_LISTENING) {
          handleQueryButtonUp();
        }
      });
    }

    // Preset Demo Buttons
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const trk = parseInt(btn.dataset.track, 10);
        if (!isNaN(trk)) {
          activeTrackIdx = trk;
          currentModeIdx = 6; // CELESTIAL MUSIC
          isChiptuneSubMode = false;
          playChiptuneTrack(trk);
          updateTelemetryUi();
        }
      });
    });

    updateTelemetryUi();
    updateAudioStatusPill();
    renderOledScreen();
    requestAnimationFrame(animate);
  });

})();

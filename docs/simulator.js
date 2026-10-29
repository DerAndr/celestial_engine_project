/**
 * CELESTIAL ENGINE Mk.V — Cyber-Arcane Web Simulator & Digital Twin
 * Hardware Emulation: RP2350 (Pico 2 W), SH1106 OLED (512x256 Sharp), 43 WS2812B LEDs, 3D Schematic Explorer
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

  // --- CROSS-PLATFORM ROBUST ENGLISH SPEECH SYNTHESIS ---
  function speakEnglishOracle(text, onComplete) {
    if (!('speechSynthesis' in window)) {
      if (onComplete) onComplete();
      return;
    }

    try {
      window.speechSynthesis.cancel();
      const msg = new SpeechSynthesisUtterance(text);
      msg.lang = 'en-US';

      const voices = window.speechSynthesis.getVoices();
      if (voices && voices.length > 0) {
        // Works across Windows, macOS, Linux, iOS, Android
        const enVoice = voices.find(v => {
          const l = (v.lang || '').toLowerCase();
          return l === 'en-us' || l === 'en_us' || l.startsWith('en');
        });
        if (enVoice) {
          msg.voice = enVoice;
        }
      }

      msg.rate = 1.0;
      msg.pitch = 0.95;
      msg.volume = 1.0;

      msg.onend = () => { if (onComplete) onComplete(); };
      msg.onerror = () => { if (onComplete) onComplete(); };

      window.speechSynthesis.speak(msg);
    } catch (e) {
      console.warn('Speech synthesis error:', e);
      if (onComplete) onComplete();
    }
  }

  // Chiptune Tracks (Note frequencies and durations matching RP2350 audio.c)
  const TRACKS = [
    {
      name: 'INFERNAL PHOBOS (8-BIT)',
      anim: 14, // INFERNAL
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
      name: 'KOROBEINIKI (FOLK 1861)',
      anim: 12, // BLOCKS
      notes: [
        { f: 659.25, d: 300 }, { f: 493.88, d: 150 }, { f: 523.25, d: 150 }, { f: 587.33, d: 300 },
        { f: 523.25, d: 150 }, { f: 493.88, d: 150 }, { f: 440.00, d: 300 }, { f: 440.00, d: 150 },
        { f: 523.25, d: 150 }, { f: 659.25, d: 300 }, { f: 587.33, d: 150 }, { f: 523.25, d: 150 },
        { f: 493.88, d: 450 }, { f: 523.25, d: 150 }, { f: 587.33, d: 300 }, { f: 659.25, d: 300 },
        { f: 523.25, d: 300 }, { f: 440.00, d: 300 }, { f: 440.00, d: 450 }
      ]
    },
    {
      name: 'GALACTIC MARCH (HOMAGE)',
      anim: 11, // SPACE
      notes: [
        { f: 440, d: 400 }, { f: 440, d: 400 }, { f: 440, d: 400 }, { f: 349.2, d: 300 },
        { f: 523.25, d: 120 }, { f: 440, d: 400 }, { f: 349.2, d: 300 }, { f: 523.25, d: 120 },
        { f: 440, d: 650 }, { f: 659.25, d: 400 }, { f: 659.25, d: 400 }, { f: 659.25, d: 400 },
        { f: 698.46, d: 300 }, { f: 523.25, d: 120 }, { f: 415.3, d: 400 }, { f: 349.2, d: 300 },
        { f: 523.25, d: 120 }, { f: 440, d: 650 }
      ]
    },
    {
      name: 'TRIFORCE HYMN (HOMAGE)',
      anim: 13, // ARCANE
      notes: [
        { f: 466.16, d: 400 }, { f: 349.23, d: 150 }, { f: 466.16, d: 150 }, { f: 523.25, d: 150 },
        { f: 587.33, d: 150 }, { f: 659.25, d: 150 }, { f: 698.46, d: 500 }, { f: 698.46, d: 150 },
        { f: 698.46, d: 150 }, { f: 783.99, d: 150 }, { f: 880.00, d: 150 }, { f: 932.33, d: 600 }
      ]
    },
    {
      name: 'ECTO GROOVE (8-BIT)',
      anim: 15, // PROTON
      notes: [
        { f: 587.33, d: 160 }, { f: 587.33, d: 160 }, { f: 739.99, d: 160 }, { f: 587.33, d: 160 },
        { f: 659.25, d: 240 }, { f: 554.37, d: 240 }, { f: 493.88, d: 240 }, { f: 440.00, d: 300 }
      ]
    },
    {
      name: 'PIXEL KINGDOM (8-BIT)',
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
      name: 'CYBER VISOR FUNK',
      anim: 19, // CYBER
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

    if (track.anim !== undefined) {
      currentAnimIdx = track.anim;
    }

    let noteIdx = 0;
    function playNextNote() {
      if (!isChiptunePlaying) return;
      if (noteIdx >= track.notes.length) {
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
  let currentModeIdx = 8; // Start on VOICE ORACLE (Neutral, futuristic AI HUD)
  let isChiptuneSubMode = false;
  let selectedMusicIdx = 0; // Standby
  let activeTrackIdx = 3;   // ZELDA / Celestial
  let currentAnimIdx = 4;   // Start on STELLAR AURORA (Neutral, beautiful cosmic aurora)
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
    'SUPERNOVA', 'ARCANE FIRE', 'NEBULA DREAM', 'GALACTIC MARCH',
    'GEOMETRIC BLOCKS', 'TRIFORCE HYMN', 'INFERNAL HELLFIRE', 'ECTO GROOVE',
    'HALLOWEEN ORANGE', 'ASH MOUNTAIN', 'SPOOKY BONES', 'CYBER VISOR',
    'STROBE SURGE', 'ARCANE CITADEL', 'COSMIC SPHERE', 'LUNAR GUARDIAN'
  ];

  // --- 43 CONCENTRIC WS2812B LED ENGINE ---
  const ledCanvas = document.getElementById('led-canvas');
  const ledCtx = ledCanvas ? ledCanvas.getContext('2d') : null;
  const LED_NODES = [];

  function initLedGeometry() {
    LED_NODES.length = 0;
    const cx = 155;
    const cy = 155;

    // Node 0: Center
    LED_NODES.push({ x: cx, y: cy, ring: 0, r: 0, g: 0, b: 0 });

    // Nodes 1..6: Inner ring (radius 36px)
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

    // Nodes 7..18: Mid ring (radius 78px)
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

    // Nodes 19..42: Outer ring (radius 122px)
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

  function updateLedColors(t) {
    const sec = t / 1000;

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

    switch (currentAnimIdx) {
      case 4: // STELLAR AURORA (Neutral, magical, celestial flowing aurora)
        LED_NODES.forEach((n, i) => {
          const wave1 = Math.sin(sec * 1.6 + (n.angle || 0) * 2 + n.ring * 0.8);
          const wave2 = Math.cos(sec * 1.2 - n.ring * 1.1);
          const blend = (wave1 + wave2 + 2) / 4;
          if (n.ring === 0) {
            n.r = 255; n.g = 225; n.b = 100; // Warm gold center star
          } else {
            // Elegant flowing teal, cyan, and deep celestial indigo
            n.r = Math.floor(20 + 80 * blend);
            n.g = Math.floor(160 + 90 * Math.sin(sec * 1.5 + n.ring));
            n.b = Math.floor(215 + 40 * Math.cos(sec * 1.3 + (n.angle || 0)));
          }
        });
        break;

      case 1: // CYAN PULSE
        const cPulse = 0.5 + 0.5 * Math.sin(sec * 2.5);
        LED_NODES.forEach((n, i) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 220; n.b = 80;
          } else {
            n.r = Math.floor(10 + 20 * cPulse);
            n.g = Math.floor(150 + 90 * cPulse);
            n.b = Math.floor(220 + 35 * cPulse);
          }
        });
        break;

      case 3: // COSMIC BREATHE
        const bPulse = 0.5 + 0.5 * Math.sin(sec * 1.8);
        LED_NODES.forEach((n, i) => {
          n.r = Math.floor(90 + 90 * bPulse);
          n.g = Math.floor(40 + 70 * bPulse);
          n.b = Math.floor(190 + 65 * bPulse);
        });
        break;

      case 14: // DOOM HELLFIRE
        LED_NODES.forEach((n, i) => {
          const flicker = Math.sin(sec * 14 + i * 1.5) * 0.3 + 0.7;
          if (n.ring === 0) {
            n.r = 255; n.g = Math.floor(220 * flicker); n.b = 100;
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
          [0, 240, 240], [240, 240, 0], [160, 0, 240], [0, 240, 0]
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
            n.r = Math.floor(255 * zPulse); n.g = Math.floor(215 * zPulse); n.b = 0;
          } else {
            n.r = 10; n.g = Math.floor(180 + 70 * zPulse); n.b = 40;
          }
        });
        break;

      case 15: // GHOSTBUSTERS
        const spark = Math.random() > 0.85;
        LED_NODES.forEach((n, i) => {
          if (spark) {
            n.r = 255; n.g = 255; n.b = 255;
          } else {
            n.r = 20; n.g = 240; n.b = 40;
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

    ledCtx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ledCtx.lineWidth = 1;
    [36, 78, 122].forEach(r => {
      ledCtx.beginPath();
      ledCtx.arc(155, 155, r, 0, Math.PI * 2);
      ledCtx.stroke();
    });

    LED_NODES.forEach(n => {
      const colStr = `rgb(${n.r}, ${n.g}, ${n.b})`;
      const glowStr = `rgba(${n.r}, ${n.g}, ${n.b}, 0.4)`;

      const grad = ledCtx.createRadialGradient(n.x, n.y, 2, n.x, n.y, 14);
      grad.addColorStop(0, colStr);
      grad.addColorStop(0.4, glowStr);
      grad.addColorStop(1, 'rgba(0,0,0,0)');

      ledCtx.fillStyle = grad;
      ledCtx.beginPath();
      ledCtx.arc(n.x, n.y, 14, 0, Math.PI * 2);
      ledCtx.fill();

      ledCtx.fillStyle = colStr;
      ledCtx.beginPath();
      ledCtx.arc(n.x, n.y, 4, 0, Math.PI * 2);
      ledCtx.fill();

      ledCtx.fillStyle = 'rgba(255,255,255,0.7)';
      ledCtx.beginPath();
      ledCtx.arc(n.x - 1, n.y - 1, 1.2, 0, Math.PI * 2);
      ledCtx.fill();
    });
  }

  // --- RAZOR-SHARP HIGH-CONTRAST OLED 128x64 HUD EMULATOR (512x256 BUFFER) ---
  const oledCanvas = document.getElementById('oled-canvas');
  const oledCtx = oledCanvas ? oledCanvas.getContext('2d') : null;
  let marqueeOffset = 0;

  function renderOledScreen() {
    if (!oledCtx) return;

    // Razor-sharp 512x256 buffer
    oledCtx.fillStyle = '#000000';
    oledCtx.fillRect(0, 0, 512, 256);

    const amber = '#ffb833';
    const amberDim = '#855307';
    oledCtx.fillStyle = amber;

    // 1. Top status bar
    oledCtx.fillRect(0, 0, 512, 34);
    oledCtx.fillStyle = '#000000';
    oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
    oledCtx.fillText('CELESTIAL Mk.V', 16, 24);

    const d = new Date();
    const timeStr = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
    oledCtx.fillText(timeStr, 420, 24);

    oledCtx.fillStyle = amber;

    // 2. Middle Content Area
    if (isChiptuneSubMode) {
      // JUKEBOX SUB-MENU
      oledCtx.font = 'bold 20px "JetBrains Mono", monospace';
      oledCtx.fillText('[ CELESTIAL MUSIC ]', 20, 68);

      const startIdx = Math.max(0, selectedMusicIdx - 2);

      for (let r = 0; r < 3; r++) {
        const itemIdx = startIdx + r;
        if (itemIdx > TRACKS.length) break;
        const y = 110 + r * 38;
        const isSel = (itemIdx === selectedMusicIdx);

        if (itemIdx === 0) {
          let rowStr = isChiptunePlaying ? '[ STOP PLAYBACK ]' : 'MODE: < SINGLE >';
          if (isSel) {
            oledCtx.fillRect(16, y - 26, 480, 34);
            oledCtx.fillStyle = '#000000';
            oledCtx.fillText(rowStr, 24, y - 2);
            oledCtx.fillStyle = amber;
          } else {
            oledCtx.fillText(rowStr, 24, y - 2);
          }
        } else {
          const tIdx = itemIdx - 1;
          const trk = TRACKS[tIdx];
          const isPlayingThis = (isChiptunePlaying && tIdx === activeTrackIdx);
          let label = trk ? trk.name : '';
          let tag = isPlayingThis ? '[*]' : '';

          if (isSel) {
            oledCtx.fillRect(16, y - 26, 480, 34);
            oledCtx.fillStyle = '#000000';
            oledCtx.fillText(`> ${label} ${isPlayingThis ? '[STOP]' : ''}`, 24, y - 2);
            oledCtx.fillStyle = amber;
          } else {
            oledCtx.fillText(`  ${label} ${tag}`, 24, y - 2);
          }
        }
      }
    } else {
      // BASE DASHBOARD MODES
      oledCtx.font = 'bold 20px "JetBrains Mono", monospace';

      switch (currentModeIdx) {
        case 6: // CELESTIAL MUSIC
          oledCtx.fillText(`MUSIC: ${activeTrackIdx + 1}/${TRACKS.length} ${isChiptunePlaying ? '[PLAY]' : '[IDLE]'}`, 24, 80);
          oledCtx.font = 'bold 24px "JetBrains Mono", monospace';
          oledCtx.fillText(TRACKS[activeTrackIdx] ? TRACKS[activeTrackIdx].name : 'UNKNOWN', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('MODE: SINGLE', 24, 168);
          break;

        case 5: // CORE FX
          oledCtx.fillText(`EFFECT: ${currentAnimIdx + 1}/${ANIM_NAMES.length}`, 24, 80);
          oledCtx.font = 'bold 24px "JetBrains Mono", monospace';
          oledCtx.fillText(ANIM_NAMES[currentAnimIdx] || 'CUSTOM', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('RINGS: 43 CONCENTRIC WS2812B', 24, 168);
          break;

        case 0: // CHRONOMETER
          oledCtx.fillText('ORACLE CHRONOMETER', 24, 76);
          oledCtx.font = 'bold 36px "JetBrains Mono", monospace';
          oledCtx.fillText(`${timeStr}:${String(d.getSeconds()).padStart(2, '0')}`, 90, 134);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('SOLAR ALIGNED / HARDWARE RTC', 24, 178);
          break;

        case 1: // AI MODEL
          oledCtx.fillText('AI REASONING CORE', 24, 80);
          oledCtx.font = 'bold 24px "JetBrains Mono", monospace';
          oledCtx.fillText('ACTIVE: GEMINI 2.5 FLASH', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('HOST  : PHONE BRAIN (UDP)', 24, 168);
          break;

        case 3: // VOLUME
          oledCtx.fillText(`VOLUME: ${masterVolume}%`, 24, 80);
          oledCtx.strokeRect(24, 108, 464, 30);
          oledCtx.fillRect(28, 112, Math.floor((456 * masterVolume) / 100), 22);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('MAX98357A I2S CLASS-D (3W)', 24, 178);
          break;

        default:
          oledCtx.fillText(`MODE: ${MODE_NAMES[currentModeIdx]}`, 24, 80);
          oledCtx.font = 'bold 22px "JetBrains Mono", monospace';
          oledCtx.fillText('RP2350 M33 EDGE CORE', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('SYSTEM OPERATIONAL', 24, 168);
          break;
      }
    }

    // 3. Bottom Marquee Ticker
    oledCtx.fillStyle = amberDim;
    oledCtx.fillRect(0, 208, 512, 2);

    let marqueeText = '';
    if (isChiptunePlaying) {
      marqueeText = '// CELESTIAL MUSIC PLAYING // CLICK SW TO STOP // 8-BIT RETRO SYNTHESIS // ';
    } else if (isChiptuneSubMode) {
      marqueeText = '// JUKEBOX // ROTATE TO SELECT // CLICK SW TO PLAY/STOP // QUERY TO EXIT // ';
    } else {
      marqueeText = `// ${MODE_NAMES[currentModeIdx]} // CLICK SW TO ENTER // CELESTIAL ENGINE Mk.V // `;
    }

    oledCtx.fillStyle = amber;
    oledCtx.font = 'bold 17px "JetBrains Mono", monospace';
    const textWidth = marqueeText.length * 11;
    const xPos = 16 - (marqueeOffset % textWidth);
    oledCtx.fillText(marqueeText + marqueeText, xPos, 240);
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
        if (isChiptunePlaying) {
          stopChiptunePlayback();
          playStopTone();
        } else {
          playConfirmChime();
        }
      } else {
        const targetTrk = selectedMusicIdx - 1;
        if (isChiptunePlaying && activeTrackIdx === targetTrk) {
          stopChiptunePlayback();
          playStopTone();
        } else {
          activeTrackIdx = targetTrk;
          playChiptuneTrack(targetTrk);
        }
      }
    } else {
      if (currentModeIdx === 6) { // CELESTIAL MUSIC
        if (isChiptunePlaying) {
          stopChiptunePlayback();
          playStopTone();
        } else {
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

    setTimeout(() => {
      currentState = STATE_SPEAKING;
      updateTelemetryUi();
      renderOledScreen();

      const phrases = [
        "Celestial Engine online. All runes aligned and operational.",
        "Oracle core synchronized. Quantum flux nominal.",
        "Gemma neural link established. Ready for inquiry.",
        "The stars align. Wisdom of the cosmos awaits your command.",
        "Arcane telemetry active. Edge compute running at full power."
      ];
      const text = phrases[Math.floor(Math.random() * phrases.length)];

      speakEnglishOracle(text, () => {
        currentState = STATE_IDLE;
        updateTelemetryUi();
        renderOledScreen();
      });
    }, 900);
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
    if (elTrack) {
      if (isChiptunePlaying) {
        elTrack.textContent = TRACKS[activeTrackIdx] ? TRACKS[activeTrackIdx].name : '--';
      } else {
        elTrack.textContent = 'STANDBY (SELECT PRESET)';
      }
    }
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

  // ==========================================================================
  // INTERACTIVE 360° 3D HARDWARE SCHEMATIC & COMPONENT EXPLORER
  // ==========================================================================
  const schematicCanvas = document.getElementById('schematic-canvas');
  const schematicCtx = schematicCanvas ? schematicCanvas.getContext('2d') : null;

  let schemRotX = 0.38; // Tilt angle (pitch)
  let schemRotY = -0.45; // Rotation angle (yaw)
  let schemZoom = 1.0;
  let isDraggingSchem = false;
  let lastMouseX = 0;
  let lastMouseY = 0;
  let autoOrbitSchem = true;
  let selectedComponentId = 'pico';

  const SCHEMATIC_COMPONENTS = [
    {
      id: 'pico',
      name: 'Raspberry Pi Pico 2 W',
      chip: 'RP2350 (Dual ARM Cortex-M33 @ 150MHz)',
      category: 'MCU Central Node',
      voltage: '3.3V Logic (5V VBUS In)',
      x: 0, y: 0, z: 0,
      w: 80, h: 48,
      color: '#38bdf8',
      desc: 'Central controller node running baremetal C firmware. Controls dual PIO I2S audio channels, hardware watchdog (8s), CYW43 Wi-Fi binary UDP stack, and 43-LED ring animations.',
      pins: [
        { mod: 'VBUS (Pin 40)', pico: 'Physical 40', func: '5V Power Rail', note: 'Direct USB-C supply' },
        { mod: '3V3 (Pin 36)', pico: 'Physical 36', func: '3.3V Output', note: 'Max 300mA logic rail' },
        { mod: 'GND (Pin 3,8,13,18,23,28,38)', pico: 'Physical Pins', func: 'Star Ground', note: 'Strict single star ground' }
      ]
    },
    {
      id: 'leds',
      name: '43 Concentric WS2812B Rings',
      chip: 'WS2812B-V5 Addressable',
      category: 'Optical Headpiece',
      voltage: '5V Rail (3.3V Logic Safe)',
      x: -210, y: -90, z: 30,
      w: 72, h: 72,
      color: '#10b981',
      desc: '3 Concentric optical rings (1 Center + 6 Ring 1 + 12 Ring 2 + 24 Ring 3). Driven by RP2350 PIO0 SM1 at 800 kHz with 18% brightness cap.',
      pins: [
        { mod: 'DIN', pico: 'GPIO 15 (Pin 20)', func: 'PIO0 SM1 WS2812B Driver', note: 'DMA paced timing' },
        { mod: '5V VCC', pico: 'VBUS (Pin 40)', func: '5V Clean Rail', note: '1000µF capacitor fitted' },
        { mod: 'GND', pico: 'GND (Pin 18/23)', func: 'Common Ground', note: 'Star ground reference' }
      ]
    },
    {
      id: 'oled',
      name: 'SH1106 1.3" I2C OLED',
      chip: 'SH1106 Controller (128x64)',
      category: 'Cybernetic Display',
      voltage: '3.3V (Strict)',
      x: -60, y: -160, z: 40,
      w: 80, h: 44,
      color: '#ffb833',
      desc: 'Cybernetic HUD with 13 system modes, 20 FPS marquee ticker, Jukebox track selection, and hardware telemetry. 400 kHz Fast-Mode I2C1.',
      pins: [
        { mod: 'SDA', pico: 'GPIO 2 (Pin 4)', func: 'I2C1 SDA', note: '400 kHz Fast-Mode' },
        { mod: 'SCL', pico: 'GPIO 3 (Pin 5)', func: 'I2C1 SCL', note: 'Onboard pull-ups to 3V3' },
        { mod: 'VCC', pico: '3V3 (Pin 36)', func: '3.3V Rail', note: 'Do NOT connect to 5V' },
        { mod: 'GND', pico: 'GND (Pin 3)', func: 'Ground', note: 'Common ground' }
      ]
    },
    {
      id: 'encoder',
      name: 'Bourns PEC11R Rotary Encoder',
      chip: 'Mechanical Detent + Push Switch',
      category: 'Tactile Navigation',
      voltage: '3.3V Logic',
      x: 180, y: -100, z: 25,
      w: 64, h: 46,
      color: '#06b6d4',
      desc: 'Tactile quadrature encoder for rotating modes, adjusting volume, choosing chiptunes, and center push-button switch.',
      pins: [
        { mod: 'CLK (A)', pico: 'GPIO 6 (Pin 9)', func: 'Quadrature A', note: 'GPIO IRQ fall/rise (Pull-up)' },
        { mod: 'DT (B)', pico: 'GPIO 7 (Pin 10)', func: 'Quadrature B', note: 'Direction decode (Pull-up)' },
        { mod: 'SW (Switch)', pico: 'GPIO 8 (Pin 11)', func: 'Encoder Button', note: 'Mode confirm / Zoom click' },
        { mod: 'GND', pico: 'GND (Pin 8)', func: 'Ground', note: 'Common ground' }
      ]
    },
    {
      id: 'query',
      name: 'Momentary Query Button',
      chip: 'Push-To-Talk Trigger',
      category: 'AI Voice Trigger',
      voltage: '3.3V Logic',
      x: 210, y: 10, z: 15,
      w: 56, h: 40,
      color: '#3b82f6',
      desc: 'Main inquiry button. Hold to activate 16 kHz I2S microphone streaming to Android Companion / Pi 5. Release to synthesize response.',
      pins: [
        { mod: 'Pin 1', pico: 'GPIO 9 (Pin 12)', func: 'PTT Interrupt', note: 'Pull-up, Active LOW' },
        { mod: 'Pin 2', pico: 'GND (Pin 13)', func: 'Ground', note: 'Common ground' }
      ]
    },
    {
      id: 'amp',
      name: 'MAX98357A I2S DAC/Amp',
      chip: '3.2W Mono Class-D',
      category: 'Audio Output',
      voltage: '5V Power / 3.3V Logic',
      x: -140, y: 130, z: 30,
      w: 70, h: 46,
      color: '#ec4899',
      desc: 'Class-D audio amplifier driving 4Ω 3W speaker. Powered by RP2350 PIO1 SM0 I2S DMA at 16 kHz 16-bit mono with auto sleep.',
      pins: [
        { mod: 'BCLK', pico: 'GPIO 16 (Pin 21)', func: 'PIO1 Bit Clock', note: '512 kHz clock' },
        { mod: 'LRC', pico: 'GPIO 17 (Pin 22)', func: 'PIO1 Word Select', note: '16 kHz frame sync' },
        { mod: 'DIN', pico: 'GPIO 18 (Pin 24)', func: 'PIO1 Audio Data', note: '16-bit PCM stream' },
        { mod: 'VIN', pico: 'VBUS (Pin 40)', func: '5V Power Rail', note: '1000µF capacitor fitted' },
        { mod: 'GND', pico: 'GND (Pin 23)', func: 'Power Ground', note: 'Star ground' }
      ]
    },
    {
      id: 'mic',
      name: 'INMP441 I2S MEMS Mic',
      chip: 'Omnidirectional MEMS',
      category: 'Audio Input',
      voltage: '3.3V (Strict)',
      x: -180, y: 20, z: 20,
      w: 64, h: 44,
      color: '#8b5cf6',
      desc: 'High-sensitivity digital microphone. Sampled via RP2350 PIO0 SM0 DMA into double buffers, streamed over UDP to smartphone / SBC.',
      pins: [
        { mod: 'SCK (BCLK)', pico: 'GPIO 10 (Pin 14)', func: 'PIO0 Bit Clock', note: '512 kHz clock' },
        { mod: 'WS (LRC)', pico: 'GPIO 11 (Pin 15)', func: 'PIO0 Word Select', note: '16 kHz frame' },
        { mod: 'SD (Data)', pico: 'GPIO 12 (Pin 16)', func: 'PIO0 Audio Data', note: '16-bit PCM In (Left Ch tied to GND)' },
        { mod: 'VDD', pico: '3V3 (Pin 36)', func: '3.3V Clean Rail', note: 'Clean regulated supply' },
        { mod: 'GND/L/R', pico: 'GND (Pin 18)', func: 'Ground (Left Ch)', note: 'Common ground' }
      ]
    },
    {
      id: 'phone',
      name: 'Google Pixel 9 Pro XL',
      chip: 'Android 15 / Tensor G4',
      category: 'Mobile AI Companion',
      voltage: 'Wireless Wi-Fi UDP 5005',
      x: 180, y: 150, z: 45,
      w: 86, h: 54,
      color: '#34d399',
      desc: 'Primary AI Brain over local Wi-Fi Hotspot (10.169.248.1 -> 10.169.248.64:5005). Hosts Gemini 2.5 Flash default voice reasoning, Gemma 4 options, Arcane Grimoire, and Spell Forge (<20ms).',
      pins: [
        { mod: 'Hotspot Gateway', pico: '10.169.248.1', func: 'Wi-Fi AP Router', note: 'Phone Hotspot' },
        { mod: 'UDP Socket', pico: 'Port 5005', func: 'Binary UDP v2', note: 'Zero-latency (<20ms)' },
        { mod: 'Audio Stream', pico: 'UDP 0x03', func: 'Gemini 2.5 Flash / Google TTS', note: 'Cloud streaming' }
      ]
    },
    {
      id: 'pi5',
      name: 'Wearable Pi 5 SBC',
      chip: 'BCM2712 Quad A76 + NM790 1TB',
      category: 'Autonomous Edge AI',
      voltage: 'Wireless Wi-Fi UDP 5005',
      x: 30, y: 180, z: 35,
      w: 82, h: 50,
      color: '#f59e0b',
      desc: '100% Offline Edge Brain daemon (celestial-brain.service). Runs Whisper.cpp, Ollama (Llama 3.2 1B & Gemma 2B), and Piper C++ TTS.',
      pins: [
        { mod: 'Service IP', pico: 'CYW43439 Wi-Fi', func: '10.169.248.50', note: 'Pi 5 static IP' },
        { mod: 'Daemon', pico: 'Port 5005', func: 'UDP Server', note: 'Failover redundancy' },
        { mod: 'Offline TTS', pico: 'Piper ONNX', func: '16 kHz WAV Stream', note: '<300ms inference' }
      ]
    }
  ];

  const SCHEMATIC_WIRES = [
    { from: 'pico', to: 'leds', color: '#10b981', label: 'GPIO 15 (DIN)' },
    { from: 'pico', to: 'oled', color: '#ffb833', label: 'I2C1 (GPIO 2, 3)' },
    { from: 'pico', to: 'encoder', color: '#06b6d4', label: 'GPIO 6, 7, 8' },
    { from: 'pico', to: 'query', color: '#3b82f6', label: 'GPIO 9' },
    { from: 'pico', to: 'amp', color: '#ec4899', label: 'I2S Out (GPIO 16, 17, 18)' },
    { from: 'pico', to: 'mic', color: '#8b5cf6', label: 'I2S In (GPIO 10, 11, 12)' },
    { from: 'pico', to: 'phone', color: '#34d399', label: 'Wi-Fi UDP 5005', isWireless: true },
    { from: 'pico', to: 'pi5', color: '#f59e0b', label: 'Wi-Fi UDP 5005', isWireless: true }
  ];

  function project3D(x, y, z, cx, cy) {
    // 3D Rotation matrices
    const cosY = Math.cos(schemRotY);
    const sinY = Math.sin(schemRotY);
    const cosX = Math.cos(schemRotX);
    const sinX = Math.sin(schemRotX);

    // Rotate around Y axis
    const x1 = x * cosY - z * sinY;
    const z1 = z * cosY + x * sinY;

    // Rotate around X axis
    const y2 = y * cosX - z1 * sinX;
    const z2 = z1 * cosX + y * sinX;

    // Perspective projection
    const fov = 450;
    const dist = z2 + 550;
    const scale = (dist > 10) ? (fov / dist) * schemZoom : 0.5;

    return {
      x: cx + x1 * scale,
      y: cy + y2 * scale,
      scale: scale,
      depth: z2
    };
  }

  function renderSchematic(t) {
    if (!schematicCtx || !schematicCanvas) return;
    const w = schematicCanvas.width;
    const h = schematicCanvas.height;
    const cx = w / 2;
    const cy = h / 2;

    schematicCtx.clearRect(0, 0, w, h);

    if (autoOrbitSchem && !isDraggingSchem) {
      schemRotY += 0.003;
    }

    // Grid platform
    schematicCtx.strokeStyle = 'rgba(255, 255, 255, 0.03)';
    schematicCtx.lineWidth = 1;
    for (let gx = -250; gx <= 250; gx += 50) {
      const p1 = project3D(gx, 100, -250, cx, cy);
      const p2 = project3D(gx, 100, 250, cx, cy);
      schematicCtx.beginPath();
      schematicCtx.moveTo(p1.x, p1.y);
      schematicCtx.lineTo(p2.x, p2.y);
      schematicCtx.stroke();
    }

    // Draw wires with animated packet pulses
    SCHEMATIC_WIRES.forEach((wire, wireIdx) => {
      const c1 = SCHEMATIC_COMPONENTS.find(c => c.id === wire.from);
      const c2 = SCHEMATIC_COMPONENTS.find(c => c.id === wire.to);
      if (!c1 || !c2) return;

      const p1 = project3D(c1.x, c1.y, c1.z, cx, cy);
      const p2 = project3D(c2.x, c2.y, c2.z, cx, cy);

      schematicCtx.strokeStyle = wire.isWireless ? 'rgba(56, 189, 248, 0.35)' : wire.color;
      schematicCtx.lineWidth = wire.isWireless ? 1.5 : 2;
      if (wire.isWireless) {
        schematicCtx.setLineDash([4, 4]);
      } else {
        schematicCtx.setLineDash([]);
      }

      schematicCtx.beginPath();
      schematicCtx.moveTo(p1.x, p1.y);
      schematicCtx.lineTo(p2.x, p2.y);
      schematicCtx.stroke();
      schematicCtx.setLineDash([]);

      // Animated traveling pulse
      const pulseT = ((t * 0.001 * 0.8) + wireIdx * 0.35) % 1;
      const px = p1.x + (p2.x - p1.x) * pulseT;
      const py = p1.y + (p2.y - p1.y) * pulseT;

      schematicCtx.fillStyle = wire.color;
      schematicCtx.shadowColor = wire.color;
      schematicCtx.shadowBlur = 8;
      schematicCtx.beginPath();
      schematicCtx.arc(px, py, 3.5, 0, Math.PI * 2);
      schematicCtx.fill();
      schematicCtx.shadowBlur = 0;
    });

    // Sort components by 3D depth for painter's algorithm
    const sorted = [...SCHEMATIC_COMPONENTS].map(comp => {
      const p = project3D(comp.x, comp.y, comp.z, cx, cy);
      return { comp, p };
    }).sort((a, b) => b.p.depth - a.p.depth);

    // Draw component cards
    sorted.forEach(({ comp, p }) => {
      const isSel = (comp.id === selectedComponentId);
      const bw = comp.w * p.scale;
      const bh = comp.h * p.scale;
      const bx = p.x - bw / 2;
      const by = p.y - bh / 2;

      // Card shadow
      schematicCtx.fillStyle = 'rgba(0, 0, 0, 0.6)';
      schematicCtx.fillRect(bx + 4, by + 4, bw, bh);

      // Card Body
      schematicCtx.fillStyle = isSel ? 'rgba(20, 30, 50, 0.95)' : 'rgba(10, 16, 28, 0.9)';
      schematicCtx.strokeStyle = isSel ? '#38bdf8' : comp.color;
      schematicCtx.lineWidth = isSel ? 2.5 : 1.2;

      if (isSel) {
        schematicCtx.shadowColor = '#38bdf8';
        schematicCtx.shadowBlur = 14;
      }

      schematicCtx.beginPath();
      schematicCtx.roundRect ? schematicCtx.roundRect(bx, by, bw, bh, 6) : schematicCtx.rect(bx, by, bw, bh);
      schematicCtx.fill();
      schematicCtx.stroke();
      schematicCtx.shadowBlur = 0;

      // Color top indicator bar
      schematicCtx.fillStyle = comp.color;
      schematicCtx.fillRect(bx, by, bw, 4 * p.scale);

      // Component Name Text
      schematicCtx.fillStyle = '#ffffff';
      schematicCtx.font = `bold ${Math.max(8, Math.floor(10 * p.scale))}px "JetBrains Mono", monospace`;
      schematicCtx.fillText(comp.id.toUpperCase(), bx + 6, by + 16 * p.scale);

      schematicCtx.fillStyle = comp.color;
      schematicCtx.font = `${Math.max(6, Math.floor(7.5 * p.scale))}px "JetBrains Mono", monospace`;
      schematicCtx.fillText(comp.category, bx + 6, by + 28 * p.scale);
    });
  }

  function selectSchematicComponent(id) {
    const comp = SCHEMATIC_COMPONENTS.find(c => c.id === id);
    if (!comp) return;

    selectedComponentId = id;
    const elTitle = document.getElementById('inspector-title');
    const elSub = document.getElementById('inspector-subtitle');
    const elDesc = document.getElementById('inspector-desc');
    const elTbody = document.getElementById('inspector-spec-tbody');

    if (elTitle) elTitle.textContent = comp.name;
    if (elSub) elSub.textContent = `${comp.chip} • ${comp.voltage}`;
    if (elDesc) elDesc.textContent = comp.desc;

    if (elTbody) {
      elTbody.innerHTML = '';
      comp.pins.forEach(pin => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td><strong>${pin.mod}</strong></td>
          <td style="color: #38bdf8;">${pin.pico}</td>
          <td>${pin.func}</td>
          <td style="color: var(--text-muted);">${pin.note}</td>
        `;
        elTbody.appendChild(tr);
      });
    }
  }

  function handleSchematicPointerDown(e) {
    isDraggingSchem = true;
    autoOrbitSchem = false;
    lastMouseX = e.clientX || (e.touches && e.touches[0].clientX) || 0;
    lastMouseY = e.clientY || (e.touches && e.touches[0].clientY) || 0;

    // Check hit test on components
    const rect = schematicCanvas.getBoundingClientRect();
    const clickX = (e.clientX || (e.touches && e.touches[0].clientX)) - rect.left;
    const clickY = (e.clientY || (e.touches && e.touches[0].clientY)) - rect.top;
    const cx = schematicCanvas.width / 2;
    const cy = schematicCanvas.height / 2;

    for (const comp of SCHEMATIC_COMPONENTS) {
      const p = project3D(comp.x, comp.y, comp.z, cx, cy);
      const bw = comp.w * p.scale;
      const bh = comp.h * p.scale;
      if (Math.abs(clickX - p.x) <= bw / 2 && Math.abs(clickY - p.y) <= bh / 2) {
        selectSchematicComponent(comp.id);
        playTone(659.25, 20, 'sine');
        break;
      }
    }
  }

  function handleSchematicPointerMove(e) {
    if (!isDraggingSchem) return;
    const clientX = e.clientX || (e.touches && e.touches[0].clientX) || 0;
    const clientY = e.clientY || (e.touches && e.touches[0].clientY) || 0;
    const dx = clientX - lastMouseX;
    const dy = clientY - lastMouseY;

    schemRotY += dx * 0.008;
    schemRotX -= dy * 0.008;

    if (schemRotX > 0.8) schemRotX = 0.8;
    if (schemRotX < -0.6) schemRotX = -0.6;

    lastMouseX = clientX;
    lastMouseY = clientY;
  }

  function handleSchematicPointerUp() {
    isDraggingSchem = false;
  }

  // --- MAIN LOOP ANIMATION ---
  let lastMarqueeTime = 0;
  function animate(t) {
    renderStarfield(t);
    updateLedColors(t);
    renderLeds();

    if (t - lastMarqueeTime >= 50) {
      marqueeOffset += 2;
      lastMarqueeTime = t;
      renderOledScreen();
    }

    renderSpectrum();
    renderSchematic(t);
    requestAnimationFrame(animate);
  }

  // --- INITIALIZATION ---
  document.addEventListener('DOMContentLoaded', () => {
    initLedGeometry();
    initStarfield();

    // Sharp 512x256 buffer for OLED
    if (oledCanvas) {
      oledCanvas.width = 512;
      oledCanvas.height = 256;
    }
    if (ledCanvas) {
      ledCanvas.width = 310;
      ledCanvas.height = 310;
    }
    if (spectrumCanvas) {
      spectrumCanvas.width = spectrumCanvas.parentElement.clientWidth || 300;
      spectrumCanvas.height = 64;
    }
    if (schematicCanvas) {
      schematicCanvas.width = schematicCanvas.parentElement.clientWidth || 700;
      schematicCanvas.height = 480;
    }

    window.addEventListener('resize', () => {
      initStarfield();
      if (spectrumCanvas) {
        spectrumCanvas.width = spectrumCanvas.parentElement.clientWidth || 300;
      }
      if (schematicCanvas) {
        schematicCanvas.width = schematicCanvas.parentElement.clientWidth || 700;
      }
    });

    // 3D Schematic Interactive Listeners
    if (schematicCanvas) {
      schematicCanvas.addEventListener('mousedown', handleSchematicPointerDown);
      window.addEventListener('mousemove', handleSchematicPointerMove);
      window.addEventListener('mouseup', handleSchematicPointerUp);

      schematicCanvas.addEventListener('touchstart', handleSchematicPointerDown, { passive: true });
      window.addEventListener('touchmove', handleSchematicPointerMove, { passive: true });
      window.addEventListener('touchend', handleSchematicPointerUp, { passive: true });

      schematicCanvas.addEventListener('wheel', (e) => {
        e.preventDefault();
        schemZoom += e.deltaY < 0 ? 0.08 : -0.08;
        if (schemZoom < 0.6) schemZoom = 0.6;
        if (schemZoom > 1.8) schemZoom = 1.8;
      }, { passive: false });
    }

    // 3D Schematic Toolbar Buttons
    const btnResetView = document.getElementById('schem-btn-reset');
    if (btnResetView) {
      btnResetView.addEventListener('click', () => {
        schemRotX = 0.38;
        schemRotY = -0.45;
        schemZoom = 1.0;
        autoOrbitSchem = false;
      });
    }

    const btnAutoOrbit = document.getElementById('schem-btn-orbit');
    if (btnAutoOrbit) {
      btnAutoOrbit.addEventListener('click', () => {
        autoOrbitSchem = !autoOrbitSchem;
        btnAutoOrbit.textContent = autoOrbitSchem ? '⏸ Pause' : '▶ Orbit';
      });
    }

    const btnTopView = document.getElementById('schem-btn-top');
    if (btnTopView) {
      btnTopView.addEventListener('click', () => {
        schemRotX = 1.57;
        schemRotY = 0;
        schemZoom = 1.0;
        autoOrbitSchem = false;
      });
    }

    // Rotary Knob Controls
    const btnCenter = document.getElementById('knob-center-btn');
    if (btnCenter) {
      btnCenter.addEventListener('click', (e) => {
        e.stopPropagation();
        handleEncoderSwitchClick();
      });
    }

    const btnStepLeft = document.getElementById('btn-step-left');
    if (btnStepLeft) {
      btnStepLeft.addEventListener('click', () => handleEncoderDelta(-1));
    }
    const btnStepRight = document.getElementById('btn-step-right');
    if (btnStepRight) {
      btnStepRight.addEventListener('click', () => handleEncoderDelta(1));
    }

    const knobOuter = document.getElementById('knob-outer');
    if (knobOuter) {
      knobOuter.addEventListener('wheel', (e) => {
        e.preventDefault();
        const delta = e.deltaY < 0 ? 1 : -1;
        handleEncoderDelta(delta);
      }, { passive: false });

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

    // Preset Buttons
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const trk = parseInt(btn.dataset.track, 10);
        if (!isNaN(trk)) {
          activeTrackIdx = trk;
          currentModeIdx = 6;
          isChiptuneSubMode = false;
          playChiptuneTrack(trk);
          updateTelemetryUi();
        }
      });
    });

    // Technical Architecture Tab Switcher
    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const targetId = btn.dataset.tab;
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        const targetPane = document.getElementById(targetId);
        if (targetPane) targetPane.classList.add('active');
      });
    });

    selectSchematicComponent('pico');
    updateTelemetryUi();
    updateAudioStatusPill();
    renderOledScreen();
    requestAnimationFrame(animate);
  });

})();

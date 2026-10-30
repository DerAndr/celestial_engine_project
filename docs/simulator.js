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

  // 21 Authentic 8-Bit Chiptune Melodies from firmware/src/audio.c (RP2350 16 kHz Synth)
  const TRACKS = [
    { // 0: DOOM E1M1
      name: 'DOOM E1M1 (AT DOOM\'S GATE)',
      anim: 14, // DOOM HELLFIRE
      notes: [
        { f: 82.41, d: 42 }, { f: 0, d: 84 }, { f: 82.41, d: 158 }, { f: 164.81, d: 116 },
        { f: 82.41, d: 42 }, { f: 0, d: 95 }, { f: 82.41, d: 148 }, { f: 146.83, d: 116 },
        { f: 82.41, d: 42 }, { f: 0, d: 95 }, { f: 82.41, d: 137 }, { f: 130.81, d: 126 },
        { f: 82.41, d: 42 }, { f: 0, d: 95 }, { f: 82.41, d: 148 }, { f: 116.54, d: 116 },
        { f: 82.41, d: 63 }, { f: 0, d: 74 }, { f: 82.41, d: 137 }, { f: 123.47, d: 137 },
        { f: 130.81, d: 158 }, { f: 0, d: 105 }, { f: 82.41, d: 137 }, { f: 164.81, d: 137 },
        { f: 82.41, d: 42 }, { f: 0, d: 84 }, { f: 82.41, d: 158 }, { f: 146.83, d: 116 },
        { f: 82.41, d: 42 }, { f: 0, d: 84 }, { f: 82.41, d: 137 }, { f: 130.81, d: 137 },
        { f: 82.41, d: 53 }, { f: 0, d: 84 }, { f: 82.41, d: 201 }, { f: 0, d: 84 },
        { f: 369.99, d: 74 }, { f: 329.63, d: 63 }, { f: 311.13, d: 63 }, { f: 369.99, d: 63 },
        { f: 440.00, d: 74 }, { f: 392.00, d: 63 }, { f: 369.99, d: 74 }, { f: 311.13, d: 63 },
        { f: 369.99, d: 63 }, { f: 392.00, d: 74 }, { f: 440.00, d: 63 }, { f: 493.88, d: 74 }
      ]
    },
    { // 1: TETRIS KOROBEINIKI
      name: 'TETRIS (KOROBEINIKI 1861)',
      anim: 12, // TETRIS BLOCKS
      notes: [
        { f: 659.25, d: 220 }, { f: 493.88, d: 110 }, { f: 523.25, d: 110 }, { f: 587.33, d: 220 },
        { f: 523.25, d: 110 }, { f: 493.88, d: 110 }, { f: 440.00, d: 220 }, { f: 440.00, d: 110 },
        { f: 523.25, d: 110 }, { f: 659.25, d: 220 }, { f: 587.33, d: 110 }, { f: 523.25, d: 110 },
        { f: 493.88, d: 330 }, { f: 523.25, d: 110 }, { f: 587.33, d: 220 }, { f: 659.25, d: 220 },
        { f: 523.25, d: 220 }, { f: 440.00, d: 220 }, { f: 440.00, d: 330 }, { f: 0, d: 110 },
        { f: 587.33, d: 220 }, { f: 698.46, d: 110 }, { f: 880.00, d: 220 }, { f: 783.99, d: 110 },
        { f: 698.46, d: 110 }, { f: 659.25, d: 330 }, { f: 523.25, d: 110 }, { f: 659.25, d: 220 },
        { f: 587.33, d: 110 }, { f: 523.25, d: 110 }, { f: 493.88, d: 220 }, { f: 493.88, d: 110 },
        { f: 523.25, d: 110 }, { f: 587.33, d: 220 }, { f: 659.25, d: 220 }, { f: 523.25, d: 220 },
        { f: 440.00, d: 220 }, { f: 440.00, d: 330 }
      ]
    },
    { // 2: STAR WARS IMPERIAL MARCH
      name: 'STAR WARS (IMPERIAL MARCH)',
      anim: 11, // STAR WARS DUEL
      notes: [
        { f: 392.00, d: 320 }, { f: 392.00, d: 320 }, { f: 392.00, d: 320 }, { f: 311.13, d: 240 },
        { f: 466.16, d: 80 },  { f: 392.00, d: 320 }, { f: 311.13, d: 240 }, { f: 466.16, d: 80 },
        { f: 392.00, d: 600 }, { f: 0, d: 120 },
        { f: 587.33, d: 320 }, { f: 587.33, d: 320 }, { f: 587.33, d: 320 }, { f: 622.25, d: 240 },
        { f: 466.16, d: 80 },  { f: 369.99, d: 320 }, { f: 311.13, d: 240 }, { f: 466.16, d: 80 },
        { f: 392.00, d: 600 }, { f: 0, d: 120 },
        { f: 783.99, d: 320 }, { f: 392.00, d: 240 }, { f: 392.00, d: 80 },  { f: 783.99, d: 320 },
        { f: 739.99, d: 240 }, { f: 698.46, d: 80 },  { f: 659.25, d: 90 },  { f: 622.25, d: 90 },
        { f: 659.25, d: 220 }, { f: 0, d: 120 },      { f: 415.30, d: 180 }, { f: 554.37, d: 320 },
        { f: 523.25, d: 240 }, { f: 493.88, d: 80 },  { f: 466.16, d: 90 },  { f: 440.00, d: 90 },
        { f: 466.16, d: 220 }, { f: 0, d: 120 },      { f: 311.13, d: 180 }, { f: 369.99, d: 320 },
        { f: 311.13, d: 240 }, { f: 466.16, d: 80 },  { f: 392.00, d: 650 }
      ]
    },
    { // 3: ZELDA OVERWORLD
      name: 'ZELDA (OVERWORLD HYMN)',
      anim: 13, // TRIFORCE HYMN
      notes: [
        { f: 466.16, d: 260 }, { f: 349.23, d: 180 }, { f: 0, d: 80 },    { f: 466.16, d: 140 },
        { f: 466.16, d: 90 },  { f: 523.25, d: 90 },  { f: 587.33, d: 90 },  { f: 622.25, d: 90 },
        { f: 698.46, d: 450 }, { f: 0, d: 120 },      { f: 698.46, d: 130 }, { f: 698.46, d: 90 },
        { f: 739.99, d: 90 },  { f: 830.61, d: 90 },  { f: 932.33, d: 450 }, { f: 0, d: 120 },
        { f: 932.33, d: 130 }, { f: 932.33, d: 90 },  { f: 830.61, d: 90 },  { f: 739.99, d: 90 },
        { f: 830.61, d: 260 }, { f: 739.99, d: 130 }, { f: 698.46, d: 450 }, { f: 0, d: 100 },
        { f: 622.25, d: 130 }, { f: 622.25, d: 90 },  { f: 698.46, d: 90 },  { f: 739.99, d: 260 },
        { f: 698.46, d: 130 }, { f: 622.25, d: 130 }, { f: 554.37, d: 130 }, { f: 523.25, d: 130 },
        { f: 587.33, d: 130 }, { f: 659.25, d: 260 }, { f: 783.99, d: 180 }, { f: 698.46, d: 400 }
      ]
    },
    { // 4: GHOSTBUSTERS 8-BIT
      name: 'GHOSTBUSTERS (ECTO GROOVE)',
      anim: 15, // ECTO GROOVE
      notes: [
        { f: 493.88, d: 120 }, { f: 0, d: 30 }, { f: 493.88, d: 120 }, { f: 0, d: 30 },
        { f: 622.25, d: 240 }, { f: 493.88, d: 120 }, { f: 554.37, d: 240 }, { f: 440.00, d: 360 }, { f: 0, d: 120 },
        { f: 493.88, d: 120 }, { f: 0, d: 30 }, { f: 493.88, d: 120 }, { f: 0, d: 30 },
        { f: 493.88, d: 120 }, { f: 493.88, d: 120 }, { f: 440.00, d: 240 }, { f: 493.88, d: 360 }, { f: 0, d: 120 },
        { f: 493.88, d: 180 }, { f: 587.33, d: 220 }, { f: 493.88, d: 180 }, { f: 587.33, d: 480 }, { f: 0, d: 160 },
        { f: 493.88, d: 150 }, { f: 493.88, d: 150 }, { f: 622.25, d: 240 }, { f: 493.88, d: 150 },
        { f: 659.25, d: 300 }, { f: 739.99, d: 750 }
      ]
    },
    { // 5: SUPER MARIO BROS
      name: 'SUPER MARIO BROS (OVERWORLD)',
      anim: 3, // RAINBOW SWIRL
      notes: [
        { f: 659.25, d: 120 }, { f: 659.25, d: 120 }, { f: 0, d: 120 },   { f: 659.25, d: 120 },
        { f: 0, d: 120 },      { f: 523.25, d: 120 }, { f: 659.25, d: 120 }, { f: 783.99, d: 240 },
        { f: 0, d: 240 },      { f: 392.00, d: 240 }, { f: 0, d: 240 },
        { f: 523.25, d: 180 }, { f: 0, d: 90 },       { f: 392.00, d: 180 }, { f: 0, d: 90 },
        { f: 329.63, d: 180 }, { f: 0, d: 90 },       { f: 440.00, d: 150 }, { f: 493.88, d: 150 },
        { f: 466.16, d: 120 }, { f: 440.00, d: 180 }, { f: 392.00, d: 140 }, { f: 659.25, d: 160 },
        { f: 783.99, d: 160 }, { f: 880.00, d: 180 }, { f: 698.46, d: 130 }, { f: 783.99, d: 150 },
        { f: 0, d: 90 },       { f: 659.25, d: 160 }, { f: 523.25, d: 130 }, { f: 587.33, d: 130 },
        { f: 493.88, d: 220 }
      ]
    },
    { // 6: SPOOKY SCARY SKELETONS
      name: 'SPOOKY SCARY SKELETONS',
      anim: 18, // SPOOKY BONES
      notes: [
        { f: 415.30, d: 176 }, { f: 0, d: 176 }, { f: 415.30, d: 352 },
        { f: 392.00, d: 176 }, { f: 0, d: 176 }, { f: 392.00, d: 352 },
        { f: 261.63, d: 352 }, { f: 311.13, d: 176 }, { f: 261.63, d: 352 }, { f: 0, d: 176 },
        { f: 261.63, d: 352 }, { f: 311.13, d: 352 }, { f: 349.23, d: 352 }, { f: 293.66, d: 352 },
        { f: 311.13, d: 352 }, { f: 261.63, d: 600 }, { f: 0, d: 200 },
        { f: 415.30, d: 176 }, { f: 0, d: 176 }, { f: 415.30, d: 352 },
        { f: 392.00, d: 176 }, { f: 0, d: 176 }, { f: 392.00, d: 352 },
        { f: 261.63, d: 352 }, { f: 311.13, d: 176 }, { f: 261.63, d: 352 }, { f: 0, d: 176 },
        { f: 261.63, d: 352 }, { f: 415.30, d: 176 }, { f: 415.30, d: 528 }, { f: 392.00, d: 352 },
        { f: 392.00, d: 352 }, { f: 261.63, d: 705 }
      ]
    },
    { // 7: DAFT PUNK DA FUNK
      name: 'DAFT PUNK (DA FUNK 303)',
      anim: 19, // CYBER VISOR
      notes: [
        { f: 392.00, d: 540 }, { f: 0, d: 135 }, { f: 349.23, d: 135 }, { f: 0, d: 90 },
        { f: 392.00, d: 135 }, { f: 0, d: 90 },  { f: 466.16, d: 135 }, { f: 0, d: 90 },
        { f: 293.66, d: 640 }, { f: 0, d: 135 }, { f: 261.63, d: 135 }, { f: 0, d: 90 },
        { f: 293.66, d: 135 }, { f: 0, d: 90 },  { f: 349.23, d: 135 }, { f: 0, d: 90 },
        { f: 233.08, d: 640 }, { f: 0, d: 135 }, { f: 220.00, d: 135 }, { f: 0, d: 90 },
        { f: 233.08, d: 135 }, { f: 0, d: 90 },  { f: 293.66, d: 135 }, { f: 0, d: 90 },
        { f: 196.00, d: 640 }, { f: 0, d: 135 }, { f: 220.00, d: 270 }, { f: 233.08, d: 270 },
        { f: 392.00, d: 600 }
      ]
    },
    { // 8: MORROWIND NEREVAR RISING
      name: 'MORROWIND (NEREVAR RISING)',
      anim: 17, // MORROWIND ASH
      notes: [
        { f: 261.63, d: 240 }, { f: 293.66, d: 240 }, { f: 311.13, d: 720 },
        { f: 311.13, d: 240 }, { f: 349.23, d: 240 }, { f: 392.00, d: 720 },
        { f: 392.00, d: 240 }, { f: 466.16, d: 240 }, { f: 349.23, d: 600 },
        { f: 392.00, d: 136 }, { f: 349.23, d: 136 }, { f: 311.13, d: 240 },
        { f: 293.66, d: 240 }, { f: 261.63, d: 720 },
        { f: 523.25, d: 360 }, { f: 587.33, d: 90 },  { f: 622.25, d: 450 },
        { f: 587.33, d: 450 }, { f: 523.25, d: 450 }, { f: 466.16, d: 450 },
        { f: 415.30, d: 450 }, { f: 392.00, d: 450 }, { f: 349.23, d: 720 },
        { f: 311.13, d: 240 }, { f: 293.66, d: 240 }, { f: 261.63, d: 900 }
      ]
    },
    { // 9: HARRY POTTER HEDWIG'S THEME
      name: 'HARRY POTTER (HEDWIG\'S WALTZ)',
      anim: 24, // HARRY POTTER
      notes: [
        { f: 246.94, d: 260 }, { f: 329.63, d: 390 }, { f: 392.00, d: 200 }, { f: 369.99, d: 200 },
        { f: 329.63, d: 390 }, { f: 493.88, d: 780 }, { f: 440.00, d: 580 }, { f: 369.99, d: 580 },
        { f: 329.63, d: 390 }, { f: 392.00, d: 200 }, { f: 369.99, d: 200 }, { f: 311.13, d: 390 },
        { f: 349.23, d: 390 }, { f: 246.94, d: 780 }, { f: 246.94, d: 260 }, { f: 329.63, d: 390 },
        { f: 392.00, d: 200 }, { f: 369.99, d: 200 }, { f: 329.63, d: 390 }, { f: 493.88, d: 780 },
        { f: 587.33, d: 520 }, { f: 554.37, d: 260 }, { f: 523.25, d: 520 }, { f: 415.30, d: 260 },
        { f: 523.25, d: 390 }, { f: 493.88, d: 200 }, { f: 466.16, d: 200 }, { f: 246.94, d: 390 },
        { f: 392.00, d: 390 }, { f: 329.63, d: 850 }
      ]
    },
    { // 10: POKEMON THEME
      name: 'POKÉMON (GOTTA CATCH \'EM ALL)',
      anim: 25, // POKEMON POKEBALL
      notes: [
        { f: 440.00, d: 240 }, { f: 523.25, d: 240 }, { f: 587.33, d: 480 }, { f: 0, d: 140 },
        { f: 440.00, d: 150 }, { f: 440.00, d: 150 }, { f: 523.25, d: 200 }, { f: 587.33, d: 200 },
        { f: 587.33, d: 200 }, { f: 523.25, d: 440 }, { f: 0, d: 140 },
        { f: 523.25, d: 180 }, { f: 587.33, d: 180 }, { f: 659.25, d: 180 }, { f: 698.46, d: 240 },
        { f: 659.25, d: 180 }, { f: 587.33, d: 180 }, { f: 523.25, d: 480 }, { f: 0, d: 160 },
        { f: 440.00, d: 240 }, { f: 523.25, d: 240 }, { f: 587.33, d: 480 }, { f: 0, d: 140 },
        { f: 587.33, d: 200 }, { f: 587.33, d: 200 }, { f: 523.25, d: 200 }, { f: 440.00, d: 200 },
        { f: 349.23, d: 440 }, { f: 349.23, d: 180 }, { f: 392.00, d: 180 }, { f: 440.00, d: 200 },
        { f: 440.00, d: 200 }, { f: 392.00, d: 180 }, { f: 349.23, d: 180 }, { f: 440.00, d: 580 }
      ]
    },
    { // 11: SAILOR MOON MOONLIGHT DENSETSU
      name: 'SAILOR MOON (MOONLIGHT DENSETSU)',
      anim: 23, // SAILOR MOON
      notes: [
        { f: 392.00, d: 214 }, { f: 523.25, d: 214 }, { f: 622.25, d: 214 }, { f: 783.99, d: 428 },
        { f: 783.99, d: 214 }, { f: 698.46, d: 428 }, { f: 698.46, d: 214 }, { f: 622.25, d: 214 },
        { f: 587.33, d: 428 }, { f: 698.46, d: 428 }, { f: 493.88, d: 214 }, { f: 587.33, d: 214 },
        { f: 698.46, d: 214 }, { f: 830.61, d: 428 }, { f: 830.61, d: 214 }, { f: 932.33, d: 428 },
        { f: 830.61, d: 214 }, { f: 783.99, d: 214 }, { f: 698.46, d: 214 }, { f: 622.25, d: 428 },
        { f: 783.99, d: 428 }, { f: 392.00, d: 214 }, { f: 523.25, d: 214 }, { f: 622.25, d: 214 },
        { f: 1046.50, d: 428 }, { f: 987.77, d: 428 }, { f: 932.33, d: 428 }, { f: 830.61, d: 428 },
        { f: 783.99, d: 214 }, { f: 698.46, d: 214 }, { f: 783.99, d: 428 }, { f: 698.46, d: 428 },
        { f: 622.25, d: 214 }, { f: 587.33, d: 428 }, { f: 523.25, d: 550 }
      ]
    },
    { // 12: MEGA MAN 2 DR. WILY STAGE 1
      name: 'MEGA MAN 2 (DR. WILY STAGE 1)',
      anim: 9, // RADAR RIPPLE
      notes: [
        { f: 293.66, d: 110 }, { f: 293.66, d: 110 }, { f: 293.66, d: 110 }, { f: 293.66, d: 110 },
        { f: 466.16, d: 180 }, { f: 440.00, d: 180 }, { f: 392.00, d: 180 }, { f: 349.23, d: 180 },
        { f: 392.00, d: 180 }, { f: 440.00, d: 220 },
        { f: 293.66, d: 110 }, { f: 293.66, d: 110 }, { f: 293.66, d: 110 }, { f: 293.66, d: 110 },
        { f: 466.16, d: 180 }, { f: 440.00, d: 180 }, { f: 392.00, d: 180 }, { f: 349.23, d: 180 },
        { f: 329.63, d: 180 }, { f: 293.66, d: 220 },
        { f: 349.23, d: 140 }, { f: 392.00, d: 140 }, { f: 440.00, d: 140 }, { f: 587.33, d: 260 },
        { f: 523.25, d: 180 }, { f: 466.16, d: 180 }, { f: 440.00, d: 180 }, { f: 392.00, d: 180 },
        { f: 440.00, d: 260 }
      ]
    },
    { // 13: EBAY CHAOS AUCTION
      name: 'EBAY CHAOS AUCTION',
      anim: 26, // EBAY AUCTION
      notes: [
        { f: 261.63, d: 110 }, { f: 329.63, d: 110 }, { f: 392.00, d: 110 }, { f: 523.25, d: 140 },
        { f: 493.88, d: 110 }, { f: 392.00, d: 110 }, { f: 440.00, d: 110 }, { f: 493.88, d: 110 }, { f: 523.25, d: 220 },
        { f: 587.33, d: 95 },  { f: 659.25, d: 95 },  { f: 698.46, d: 95 },  { f: 783.99, d: 130 },
        { f: 523.25, d: 70 },  { f: 554.37, d: 70 },  { f: 587.33, d: 70 },  { f: 622.25, d: 70 },
        { f: 659.25, d: 70 },  { f: 698.46, d: 70 },  { f: 739.99, d: 70 },  { f: 783.99, d: 70 },
        { f: 830.61, d: 70 },  { f: 880.00, d: 70 },  { f: 987.77, d: 70 },  { f: 1046.50, d: 180 },
        { f: 130.81, d: 80 },  { f: 0, d: 60 },       { f: 130.81, d: 80 },  { f: 0, d: 60 },       { f: 130.81, d: 120 },
        { f: 1864.66, d: 90 }, { f: 2349.32, d: 260 }, { f: 0, d: 50 },      { f: 2093.00, d: 420 }
      ]
    },
    { // 14: BELLA CIAO
      name: 'BELLA CIAO (FOLK ANTHEM)',
      anim: 5, // SOLAR PLASMA
      notes: [
        { f: 329.63, d: 150 }, { f: 440.00, d: 150 }, { f: 493.88, d: 150 }, { f: 523.25, d: 240 }, { f: 440.00, d: 240 },
        { f: 329.63, d: 150 }, { f: 440.00, d: 150 }, { f: 493.88, d: 150 }, { f: 523.25, d: 240 }, { f: 440.00, d: 240 },
        { f: 329.63, d: 140 }, { f: 440.00, d: 140 }, { f: 493.88, d: 140 }, { f: 523.25, d: 140 }, { f: 493.88, d: 140 },
        { f: 440.00, d: 140 }, { f: 523.25, d: 140 }, { f: 493.88, d: 140 }, { f: 440.00, d: 140 }, { f: 659.25, d: 380 },
        { f: 659.25, d: 180 }, { f: 659.25, d: 180 }, { f: 587.33, d: 140 }, { f: 659.25, d: 140 }, { f: 698.46, d: 240 },
        { f: 698.46, d: 140 }, { f: 659.25, d: 140 }, { f: 587.33, d: 140 }, { f: 698.46, d: 140 }, { f: 659.25, d: 320 },
        { f: 659.25, d: 140 }, { f: 587.33, d: 140 }, { f: 523.25, d: 140 }, { f: 493.88, d: 180 }, { f: 659.25, d: 180 },
        { f: 493.88, d: 180 }, { f: 523.25, d: 180 }, { f: 440.00, d: 450 }
      ]
    },
    { // 15: ODE TO JOY (BEETHOVEN)
      name: 'ODE TO JOY (BEETHOVEN NO. 9)',
      anim: 20, // CHROMA PRISMATIC
      notes: [
        { f: 329.63, d: 220 }, { f: 329.63, d: 220 }, { f: 349.23, d: 220 }, { f: 392.00, d: 220 },
        { f: 392.00, d: 220 }, { f: 349.23, d: 220 }, { f: 329.63, d: 220 }, { f: 293.66, d: 220 },
        { f: 261.63, d: 220 }, { f: 261.63, d: 220 }, { f: 293.66, d: 220 }, { f: 329.63, d: 220 },
        { f: 329.63, d: 330 }, { f: 293.66, d: 110 }, { f: 293.66, d: 440 }, { f: 0, d: 110 },
        { f: 329.63, d: 220 }, { f: 329.63, d: 220 }, { f: 349.23, d: 220 }, { f: 392.00, d: 220 },
        { f: 392.00, d: 220 }, { f: 349.23, d: 220 }, { f: 329.63, d: 220 }, { f: 293.66, d: 220 },
        { f: 261.63, d: 220 }, { f: 261.63, d: 220 }, { f: 293.66, d: 220 }, { f: 329.63, d: 220 },
        { f: 293.66, d: 330 }, { f: 261.63, d: 110 }, { f: 261.63, d: 440 }
      ]
    },
    { // 16: GREENSLEEVES
      name: 'GREENSLEEVES (CELTIC BALLAD)',
      anim: 27, // TERRA PLANET
      notes: [
        { f: 349.23, d: 180 }, { f: 415.30, d: 360 }, { f: 466.16, d: 180 }, { f: 523.25, d: 280 }, { f: 554.37, d: 100 },
        { f: 523.25, d: 180 }, { f: 466.16, d: 360 }, { f: 392.00, d: 180 }, { f: 311.13, d: 280 }, { f: 349.23, d: 100 },
        { f: 392.00, d: 180 }, { f: 415.30, d: 360 }, { f: 349.23, d: 180 }, { f: 349.23, d: 280 }, { f: 329.63, d: 100 },
        { f: 349.23, d: 180 }, { f: 392.00, d: 360 }, { f: 329.63, d: 180 }, { f: 261.63, d: 450 }
      ]
    },
    { // 17: SAKURA SAKURA
      name: 'SAKURA SAKURA (JAPANESE FOLK)',
      anim: 6, // DIAMOND STARS
      notes: [
        { f: 440.00, d: 260 }, { f: 440.00, d: 260 }, { f: 493.88, d: 520 },
        { f: 440.00, d: 260 }, { f: 440.00, d: 260 }, { f: 493.88, d: 520 },
        { f: 440.00, d: 260 }, { f: 493.88, d: 260 }, { f: 523.25, d: 260 }, { f: 493.88, d: 260 },
        { f: 440.00, d: 260 }, { f: 493.88, d: 260 }, { f: 440.00, d: 260 }, { f: 349.23, d: 520 },
        { f: 329.63, d: 260 }, { f: 261.63, d: 260 }, { f: 329.63, d: 260 }, { f: 349.23, d: 260 },
        { f: 329.63, d: 260 }, { f: 329.63, d: 260 }, { f: 261.63, d: 260 }, { f: 246.94, d: 520 }
      ]
    },
    { // 18: MO LI HUA (JASMINE FLOWER)
      name: 'MO LI HUA (JASMINE FLOWER)',
      anim: 4, // STELLAR AURORA
      notes: [
        { f: 329.63, d: 300 }, { f: 329.63, d: 150 }, { f: 392.00, d: 150 }, { f: 440.00, d: 300 }, { f: 523.25, d: 300 },
        { f: 440.00, d: 150 }, { f: 392.00, d: 150 }, { f: 440.00, d: 300 }, { f: 392.00, d: 450 }, { f: 0, d: 150 },
        { f: 392.00, d: 300 }, { f: 392.00, d: 150 }, { f: 440.00, d: 150 }, { f: 392.00, d: 300 }, { f: 329.63, d: 300 },
        { f: 293.66, d: 300 }, { f: 329.63, d: 150 }, { f: 392.00, d: 150 }, { f: 329.63, d: 300 }, { f: 293.66, d: 450 }
      ]
    },
    { // 19: DU LIEGST MIR IM HERZEN
      name: 'DU LIEGST MIR IM HERZEN',
      anim: 8, // SOLAR ORRERY
      notes: [
        { f: 493.88, d: 220 }, { f: 392.00, d: 220 }, { f: 293.66, d: 220 }, { f: 392.00, d: 440 }, { f: 369.99, d: 220 },
        { f: 440.00, d: 220 }, { f: 369.99, d: 220 }, { f: 293.66, d: 220 }, { f: 369.99, d: 440 }, { f: 329.63, d: 220 },
        { f: 293.66, d: 220 }, { f: 392.00, d: 220 }, { f: 493.88, d: 220 }, { f: 587.33, d: 440 }, { f: 523.25, d: 220 },
        { f: 493.88, d: 220 }, { f: 392.00, d: 220 }, { f: 440.00, d: 220 }, { f: 392.00, d: 500 }
      ]
    },
    { // 20: LA CUCARACHA
      name: 'LA CUCARACHA (TRADITIONAL)',
      anim: 22, // SACRED POLYGON
      notes: [
        { f: 261.63, d: 110 }, { f: 261.63, d: 110 }, { f: 261.63, d: 110 }, { f: 349.23, d: 320 }, { f: 440.00, d: 240 },
        { f: 261.63, d: 110 }, { f: 261.63, d: 110 }, { f: 261.63, d: 110 }, { f: 349.23, d: 320 }, { f: 440.00, d: 240 },
        { f: 349.23, d: 140 }, { f: 349.23, d: 110 }, { f: 329.63, d: 110 }, { f: 329.63, d: 110 }, { f: 293.66, d: 110 }, { f: 293.66, d: 110 }, { f: 261.63, d: 420 }
      ]
    }
  ];

  // 14 Authentic Cosmic & Astromech Sound FX from firmware/src/audio.c
  const COSMIC_SFX = [
    { name: 'WARP DRIVE', anim: 1, notes: [
      { f: 110.0, d: 60 }, { f: 146.83, d: 50 }, { f: 196.00, d: 45 }, { f: 261.63, d: 40 },
      { f: 349.23, d: 35 }, { f: 466.16, d: 30 }, { f: 622.25, d: 25 }, { f: 830.61, d: 25 },
      { f: 1108.73, d: 30 }, { f: 1479.98, d: 40 }, { f: 1975.53, d: 60 }, { f: 2489.02, d: 120 }
    ]},
    { name: 'PULSAR BEACON', anim: 9, notes: [
      { f: 880.0, d: 40 }, { f: 0, d: 60 }, { f: 880.0, d: 40 }, { f: 0, d: 60 },
      { f: 1174.66, d: 60 }, { f: 0, d: 80 }, { f: 880.0, d: 40 }, { f: 0, d: 60 }, { f: 1318.51, d: 100 }
    ]},
    { name: 'ALIEN SCANNER', anim: 2, notes: [
      { f: 587.33, d: 35 }, { f: 880.00, d: 35 }, { f: 1174.66, d: 35 }, { f: 783.99, d: 35 },
      { f: 1046.50, d: 35 }, { f: 1396.91, d: 35 }, { f: 932.33, d: 35 }, { f: 1244.51, d: 35 }, { f: 1661.22, d: 70 }
    ]},
    { name: 'LASER BLASTER', anim: 11, notes: [
      { f: 1567.98, d: 25 }, { f: 1174.66, d: 25 }, { f: 880.00, d: 25 }, { f: 659.25, d: 25 },
      { f: 493.88, d: 30 }, { f: 369.99, d: 35 }, { f: 277.18, d: 45 }, { f: 185.00, d: 60 }
    ]},
    { name: 'TRACTOR BEAM', anim: 8, notes: [
      { f: 110.00, d: 90 }, { f: 130.81, d: 90 }, { f: 110.00, d: 90 }, { f: 130.81, d: 90 },
      { f: 110.00, d: 90 }, { f: 146.83, d: 110 }, { f: 110.00, d: 140 }
    ]},
    { name: 'STARGATE PORTAL', anim: 10, notes: [
      { f: 329.63, d: 90 }, { f: 493.88, d: 90 }, { f: 659.25, d: 100 }, { f: 880.00, d: 110 },
      { f: 1108.73, d: 130 }, { f: 1318.51, d: 160 }, { f: 1661.22, d: 250 }
    ]},
    { name: 'SUPERNOVA RUMBLE', anim: 5, notes: [
      { f: 65.41, d: 140 }, { f: 82.41, d: 120 }, { f: 110.00, d: 100 }, { f: 146.83, d: 80 },
      { f: 98.00, d: 120 }, { f: 73.42, d: 160 }, { f: 55.00, d: 220 }, { f: 41.20, d: 350 }
    ]},
    { name: 'R2-D2 HAPPY CHIRP', anim: 4, notes: [
      { f: 1046.50, d: 35 }, { f: 1318.51, d: 35 }, { f: 1567.98, d: 40 }, { f: 2093.00, d: 45 },
      { f: 2793.83, d: 55 }, { f: 0, d: 20 },
      { f: 3135.96, d: 30 }, { f: 2793.83, d: 30 }, { f: 3135.96, d: 30 }, { f: 3520.00, d: 85 }
    ]},
    { name: 'R2-D2 DROID CHATTER', anim: 7, notes: [
      { f: 2200.00, d: 30 }, { f: 1640.00, d: 35 }, { f: 2480.00, d: 25 }, { f: 1860.00, d: 40 },
      { f: 2920.00, d: 30 }, { f: 2100.00, d: 45 }, { f: 1480.00, d: 35 }, { f: 2600.00, d: 50 }, { f: 3300.00, d: 75 }
    ]},
    { name: 'R2-D2 PANIC SCREAM', anim: 14, notes: [
      { f: 2793.83, d: 20 }, { f: 3520.00, d: 20 }, { f: 3135.96, d: 20 }, { f: 3951.07, d: 20 },
      { f: 2793.83, d: 20 }, { f: 3520.00, d: 20 }, { f: 3135.96, d: 20 }, { f: 3951.07, d: 20 },
      { f: 2637.02, d: 35 }, { f: 2093.00, d: 45 }, { f: 1567.98, d: 65 }
    ]},
    { name: 'CHA-CHING AUCTION', anim: 26, notes: [
      { f: 1864.66, d: 80 }, { f: 2349.32, d: 220 }, { f: 0, d: 40 }, { f: 2093.00, d: 320 }
    ]},
    { name: 'POKÉCENTER HEAL', anim: 25, notes: [
      { f: 493.88, d: 140 }, { f: 659.25, d: 140 }, { f: 830.61, d: 140 }, { f: 987.77, d: 220 },
      { f: 830.61, d: 140 }, { f: 987.77, d: 380 }
    ]},
    { name: 'LUMOS WAND FLARE', anim: 24, notes: [
      { f: 1046.50, d: 40 }, { f: 1318.51, d: 40 }, { f: 1567.98, d: 50 }, { f: 2093.00, d: 60 }, { f: 3135.96, d: 140 }
    ]},
    { name: 'EXPECTO PATRONUM', anim: 23, notes: [
      { f: 220.00, d: 60 }, { f: 329.63, d: 60 }, { f: 440.00, d: 70 }, { f: 659.25, d: 80 },
      { f: 880.00, d: 100 }, { f: 1318.51, d: 180 }
    ]}
  ];

  function syncActiveEffectButtons(fxIdx) {
    document.querySelectorAll('.fx-mode-btn').forEach(b => {
      b.classList.toggle('active', b.dataset.fxId === String(fxIdx));
    });
  }

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
    document.querySelectorAll('.preset-btn, .sfx-btn').forEach(b => b.classList.remove('playing'));
    updateAudioStatusPill();
  }

  function playNoteSequence(notes, animIdx, onDone) {
    stopChiptunePlayback();
    initAudioContext();
    if (!notes || !notes.length) return;

    isChiptunePlaying = true;
    updateAudioStatusPill();

    if (animIdx !== undefined) {
      currentAnimIdx = animIdx;
      syncActiveEffectButtons(animIdx);
    }

    let noteIdx = 0;
    function playNextNote() {
      if (!isChiptunePlaying) return;
      if (noteIdx >= notes.length) {
        stopChiptunePlayback();
        renderOledScreen();
        if (onDone) onDone();
        return;
      }

      const note = notes[noteIdx];
      if (note.f > 0) {
        playTone(note.f, note.d, 'retro');
      }

      noteIdx++;
      chiptuneTimeoutId = setTimeout(playNextNote, note.d);
    }

    playNextNote();
    renderOledScreen();
  }

  function playChiptuneTrack(trackIdx) {
    if (trackIdx < 0 || trackIdx >= TRACKS.length) return;
    const track = TRACKS[trackIdx];
    playNoteSequence(track.notes, track.anim);
    document.querySelectorAll('.preset-btn').forEach(b => {
      b.classList.toggle('playing', parseInt(b.dataset.track, 10) === trackIdx);
    });
  }

  function playCosmicSfx(sfxIdx) {
    if (sfxIdx < 0 || sfxIdx >= COSMIC_SFX.length) return;
    const sfx = COSMIC_SFX[sfxIdx];
    playNoteSequence(sfx.notes, sfx.anim);
    document.querySelectorAll('.sfx-btn').forEach(b => {
      b.classList.toggle('playing', parseInt(b.dataset.sfx, 10) === sfxIdx);
    });
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
  let activeSfxIdx = 0;     // WARP DRIVE
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
    'COSMIC BREATHE', 'GALAXY VORTEX', 'OUROBOROS SNAKE', 'RAINBOW SWIRL',
    'STELLAR AURORA', 'SOLAR PLASMA', 'DIAMOND STARS', 'VOICE HALO VU',
    'SOLAR ORRERY', 'RADAR RIPPLE', 'SONIC EXPAND', 'STAR WARS DUEL',
    'TETRIS BLOCKS', 'TRIFORCE HYMN', 'DOOM HELLFIRE', 'ECTO GROOVE',
    'HALLOWEEN PUMPKIN', 'MORROWIND ASH', 'SPOOKY BONES', 'CYBER VISOR',
    'CHROMA PRISMATIC', 'AETHER HOURGLASS', 'SACRED POLYGON', 'SAILOR MOON',
    'HARRY POTTER', 'POKEMON POKEBALL', 'EBAY AUCTION', 'TERRA PLANET'
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
      LED_NODES.forEach((n) => {
        const diff = Math.sin((n.angle || 0) - rot);
        const b = Math.max(0, diff);
        n.r = Math.floor(255 * b);
        n.g = Math.floor(170 * b);
        n.b = 20;
      });
      return;
    } else if (currentState === STATE_SPEAKING) {
      const wave = Math.sin(sec * 12);
      LED_NODES.forEach((n) => {
        n.r = Math.floor(160 + 60 * wave);
        n.g = 40;
        n.b = Math.floor(220 + 35 * wave);
      });
      return;
    }

    switch (currentAnimIdx) {
      case 0: { // COSMIC BREATHE
        const bPulse = 0.25 + 0.75 * (0.5 + 0.5 * Math.sin(sec * 2.0));
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = Math.floor(255 * bPulse); n.g = Math.floor(230 * bPulse); n.b = 255;
          } else {
            n.r = Math.floor(20 * bPulse);
            n.g = Math.floor(195 * bPulse);
            n.b = Math.floor(255 * bPulse);
          }
        });
        break;
      }

      case 1: { // GALAXY VORTEX
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 240; n.b = 180;
          } else {
            const speed = (4 - n.ring) * 1.8;
            const arm = Math.sin((n.angle || 0) * 2 - sec * speed + n.ring * 0.9);
            const v = Math.max(0.08, (arm + 1) * 0.5);
            n.r = Math.floor(130 * v * (n.ring === 3 ? 1.2 : 0.6));
            n.g = Math.floor(190 * v);
            n.b = Math.floor(255 * v);
          }
        });
        break;
      }

      case 2: { // OUROBOROS SNAKE
        const head = Math.floor(sec * 18) % 43;
        LED_NODES.forEach((n, idx) => {
          const dist = (head - idx + 43) % 43;
          if (dist === 0) {
            n.r = 255; n.g = 255; n.b = 180;
          } else if (dist < 14) {
            const fade = 1 - dist / 14;
            n.r = Math.floor(20 * fade);
            n.g = Math.floor(255 * fade);
            n.b = Math.floor(130 * fade);
          } else {
            n.r = 4; n.g = 18; n.b = 12;
          }
        });
        break;
      }

      case 3: { // RAINBOW SWIRL
        LED_NODES.forEach((n) => {
          const hue = (sec * 65 + ((n.angle || 0) * 180) / Math.PI + n.ring * 45 + 360) % 360;
          const rgb = hsvToRgb(hue, 1, 1);
          n.r = rgb[0]; n.g = rgb[1]; n.b = rgb[2];
        });
        break;
      }

      case 4: { // STELLAR AURORA
        LED_NODES.forEach((n) => {
          const wave1 = Math.sin(sec * 1.6 + (n.angle || 0) * 2 + n.ring * 0.8);
          const wave2 = Math.cos(sec * 1.2 - n.ring * 1.1);
          const blend = (wave1 + wave2 + 2) / 4;
          if (n.ring === 0) {
            n.r = 255; n.g = 225; n.b = 100;
          } else {
            n.r = Math.floor(20 + 80 * blend);
            n.g = Math.floor(160 + 90 * Math.sin(sec * 1.5 + n.ring));
            n.b = Math.floor(215 + 40 * Math.cos(sec * 1.3 + (n.angle || 0)));
          }
        });
        break;
      }

      case 5: { // SOLAR PLASMA
        LED_NODES.forEach((n, i) => {
          const heat = 0.5 + 0.5 * Math.sin(sec * 6 - n.ring * 1.3 + Math.cos(i + sec * 3));
          n.r = 255;
          n.g = Math.floor(40 + 185 * heat * (1 - n.ring * 0.18));
          n.b = n.ring === 0 ? 120 : Math.floor(25 * heat);
        });
        break;
      }

      case 6: { // DIAMOND STARS
        LED_NODES.forEach((n, i) => {
          const twinkle = Math.sin(sec * 5.5 + i * 7.13) * Math.cos(sec * 3.1 - i * 3.7);
          if (twinkle > 0.72) {
            n.r = 255; n.g = 255; n.b = 255;
          } else {
            const base = Math.max(0.15, (twinkle + 1) * 0.35);
            n.r = Math.floor(25 * base);
            n.g = Math.floor(70 * base);
            n.b = Math.floor(180 * base);
          }
        });
        break;
      }

      case 7: { // VOICE HALO VU
        const vu = 0.5 + 0.5 * Math.sin(sec * 9) * Math.cos(sec * 4);
        LED_NODES.forEach((n) => {
          const active = (n.ring / 3) <= (0.25 + vu * 0.85);
          if (active) {
            n.r = n.ring === 3 ? 255 : 30;
            n.g = n.ring === 3 ? 90 : 230;
            n.b = n.ring === 1 ? 255 : 140;
          } else {
            n.r = 8; n.g = 24; n.b = 38;
          }
        });
        break;
      }

      case 8: { // SOLAR ORRERY
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 210; n.b = 40; // Sun
          } else {
            const pAngle = (sec * (4.2 - n.ring * 1.1)) % (Math.PI * 2);
            const diff = Math.abs(Math.atan2(Math.sin((n.angle || 0) - pAngle), Math.cos((n.angle || 0) - pAngle)));
            if (diff < 0.38) {
              if (n.ring === 1) { n.r = 255; n.g = 120; n.b = 40; }
              else if (n.ring === 2) { n.r = 40; n.g = 200; n.b = 255; }
              else { n.r = 170; n.g = 110; n.b = 255; }
            } else {
              n.r = 10; n.g = 18; n.b = 36;
            }
          }
        });
        break;
      }

      case 9: { // RADAR RIPPLE
        const sweepA = (sec * 3.6) % (Math.PI * 2);
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 0; n.g = 255; n.b = 140;
          } else {
            const d = ((n.angle || 0) - sweepA + Math.PI * 4) % (Math.PI * 2);
            const tail = Math.max(0, 1 - d / 1.8);
            n.r = 0;
            n.g = Math.floor(40 + 215 * tail);
            n.b = Math.floor(30 + 130 * tail);
          }
        });
        break;
      }

      case 10: { // SONIC EXPAND
        const waveR = (sec * 3.8) % 4.0;
        LED_NODES.forEach((n) => {
          const dist = Math.abs(n.ring - waveR);
          const v = Math.max(0.08, 1 - dist * 0.75);
          n.r = Math.floor(56 * v);
          n.g = Math.floor(189 * v);
          n.b = Math.floor(255 * v);
        });
        break;
      }

      case 11: { // STAR WARS DUEL (Sith Crimson vs Jedi Cyan Clash)
        const clash = Math.sin(sec * 8) > 0.78;
        LED_NODES.forEach((n) => {
          if (clash && n.ring <= 1) {
            n.r = 255; n.g = 255; n.b = 255;
          } else if (Math.sin((n.angle || 0) + sec * 2.2) >= 0) {
            n.r = 255; n.g = 15; n.b = 15; // Sith Crimson
          } else {
            n.r = 15; n.g = 185; n.b = 255; // Jedi Cyan
          }
        });
        break;
      }

      case 12: { // TETRIS BLOCKS
        const cycle = Math.floor(sec * 2.5) % 4;
        const tetrisColors = [
          [0, 240, 240], [240, 240, 0], [160, 0, 240], [0, 240, 60]
        ];
        LED_NODES.forEach((n, i) => {
          const c = tetrisColors[(cycle + n.ring + (i % 2)) % 4];
          n.r = c[0]; n.g = c[1]; n.b = c[2];
        });
        break;
      }

      case 13: { // TRIFORCE HYMN
        const zPulse = 0.55 + 0.45 * Math.sin(sec * 3.2);
        LED_NODES.forEach((n, i) => {
          if (n.ring <= 1 || (i % 3 === 0)) {
            n.r = Math.floor(255 * zPulse); n.g = Math.floor(215 * zPulse); n.b = 15;
          } else {
            n.r = 15; n.g = Math.floor(185 + 70 * zPulse); n.b = 50;
          }
        });
        break;
      }

      case 14: { // DOOM HELLFIRE
        LED_NODES.forEach((n, i) => {
          const bfg = Math.sin(sec * 3.5) > 0.88 && n.ring <= 1;
          const flicker = Math.sin(sec * 14 + i * 1.5) * 0.3 + 0.7;
          if (bfg) {
            n.r = 80; n.g = 255; n.b = 60; // BFG9000 plasma core flash
          } else if (n.ring === 0) {
            n.r = 255; n.g = Math.floor(220 * flicker); n.b = 80;
          } else if (n.ring === 1) {
            n.r = 255; n.g = Math.floor(120 * flicker); n.b = 10;
          } else if (n.ring === 2) {
            n.r = Math.floor(240 * flicker); n.g = Math.floor(35 * flicker); n.b = 0;
          } else {
            n.r = Math.floor(195 * flicker); n.g = 8; n.b = 0;
          }
        });
        break;
      }

      case 15: { // ECTO GROOVE (Ghostbusters Proton Streams & Slimer Green)
        LED_NODES.forEach((n, i) => {
          const stream = Math.sin((n.angle || 0) * 2 + sec * 7);
          if (n.ring <= 1) {
            n.r = 30; n.g = 255; n.b = 50; // Slimer Ectoplasm Core
          } else if (stream > 0.55) {
            n.r = 255; n.g = 70; n.b = 20; // Orange Proton Stream
          } else if (stream < -0.55) {
            n.r = 60; n.g = 170; n.b = 255; // Blue Proton Arc
          } else {
            n.r = 15; n.g = Math.floor(150 + 60 * Math.sin(sec * 5 + i)); n.b = 30;
          }
        });
        break;
      }

      case 16: { // HALLOWEEN PUMPKIN
        LED_NODES.forEach((n, i) => {
          const flicker = 0.65 + 0.35 * Math.sin(sec * 9 + i * 0.9);
          if (n.ring <= 2) {
            n.r = Math.floor(255 * flicker);
            n.g = Math.floor(105 * flicker);
            n.b = 5;
          } else {
            n.r = Math.floor(150 * flicker);
            n.g = 20;
            n.b = Math.floor(235 * flicker);
          }
        });
        break;
      }

      case 17: { // MORROWIND ASH (Red Mountain & Vivec Aurora)
        LED_NODES.forEach((n, i) => {
          if (n.ring <= 1) {
            const pulse = 0.6 + 0.4 * Math.sin(sec * 3 + i);
            n.r = Math.floor(255 * pulse); n.g = Math.floor(75 * pulse); n.b = 15;
          } else {
            const wave = 0.5 + 0.5 * Math.sin(sec * 2 + (n.angle || 0) * 2);
            n.r = Math.floor(40 + 80 * (1 - wave));
            n.g = Math.floor(170 * wave + 35);
            n.b = Math.floor(195 * wave + 40);
          }
        });
        break;
      }

      case 18: { // SPOOKY BONES
        const beat = Math.floor(sec * 6) % 2;
        LED_NODES.forEach((n, i) => {
          if ((i + beat) % 3 === 0) {
            n.r = 245; n.g = 245; n.b = 255; // Bone white strobe
          } else if ((i + beat) % 3 === 1) {
            n.r = 255; n.g = 115; n.b = 15; // Pumpkin orange
          } else {
            n.r = 140; n.g = 30; n.b = 220; // Crypt violet
          }
        });
        break;
      }

      case 19: { // CYBER VISOR (Daft Punk Gold & Crimson Visor Sweep)
        const sweep = Math.sin(sec * 3.8) * Math.PI;
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 200; n.b = 40;
          } else {
            const diff = Math.cos((n.angle || 0) - sweep);
            const val = Math.max(0, diff);
            n.r = Math.floor(255 * val);
            n.g = Math.floor((n.ring === 2 ? 190 : 35) * val);
            n.b = Math.floor(20 * val);
          }
        });
        break;
      }

      case 20: { // CHROMA PRISMATIC
        LED_NODES.forEach((n) => {
          const hue = (n.ring * 85 + sec * 45) % 360;
          const rgb = hsvToRgb(hue, 0.9, 1);
          n.r = rgb[0]; n.g = rgb[1]; n.b = rgb[2];
        });
        break;
      }

      case 21: { // AETHER HOURGLASS
        const fillPhase = (sec * 0.7) % 3;
        LED_NODES.forEach((n) => {
          const lit = n.ring <= fillPhase;
          n.r = lit ? 255 : 20;
          n.g = lit ? 195 : 40;
          n.b = lit ? 70 : 85;
        });
        break;
      }

      case 22: { // SACRED POLYGON (Triquetra 3x, Hexagram 6x, Dodecagram 12x)
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 230; n.b = 140;
          } else {
            const sym = n.ring === 1 ? 3 : (n.ring === 2 ? 6 : 12);
            const dir = n.ring === 2 ? -1 : 1;
            const v = 0.5 + 0.5 * Math.cos(((n.angle || 0) + sec * 1.6 * dir) * sym);
            n.r = Math.floor(190 * v);
            n.g = Math.floor(120 + 130 * v);
            n.b = 255;
          }
        });
        break;
      }

      case 23: { // SAILOR MOON (Moon Prism Pastel Pink, Gold & Cyan Spiral)
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 230; n.b = 90; // Golden Moon Crystal
          } else {
            const s = Math.sin((n.angle || 0) * 2 - sec * 3.5 + n.ring);
            if (s > 0.25) {
              n.r = 255; n.g = 105; n.b = 195; // Prism Pink
            } else if (s < -0.25) {
              n.r = 90; n.g = 215; n.b = 255;  // Mercury Cyan
            } else {
              n.r = 255; n.g = 215; n.b = 95;  // Crescent Gold
            }
          }
        });
        break;
      }

      case 24: { // HARRY POTTER (Hogwarts 4 Houses + Lumos Core Flash)
        const lumos = Math.sin(sec * 2.8) > 0.65;
        const houses = [
          [220, 35, 35],  // Gryffindor Crimson
          [25, 195, 75],  // Slytherin Emerald
          [245, 200, 35], // Hufflepuff Gold
          [35, 115, 245]  // Ravenclaw Sapphire
        ];
        LED_NODES.forEach((n) => {
          if (n.ring <= 1 && lumos) {
            n.r = 255; n.g = 255; n.b = 240; // Lumos Maxima!
          } else {
            const q = Math.floor((((n.angle || 0) + Math.PI + sec * 0.8) / (Math.PI / 2))) % 4;
            const hc = houses[(q + 4) % 4];
            n.r = hc[0]; n.g = hc[1]; n.b = hc[2];
          }
        });
        break;
      }

      case 25: { // POKEMON POKEBALL (Red Top, White Bottom, Pulsing Center Button)
        const catchPulse = 0.5 + 0.5 * Math.sin(sec * 7);
        LED_NODES.forEach((n) => {
          if (n.ring === 0) {
            n.r = 255; n.g = Math.floor(80 + 175 * catchPulse); n.b = Math.floor(80 + 175 * catchPulse);
          } else if (n.ring === 1) {
            n.r = 220; n.g = 235; n.b = 255;
          } else {
            const sinA = Math.sin(n.angle || 0);
            if (Math.abs(sinA) < 0.22) {
              n.r = 15; n.g = 20; n.b = 30; // Equatorial belt
            } else if (sinA < 0) {
              n.r = 245; n.g = 25; n.b = 25; // Top Red Hemisphere
            } else {
              n.r = 235; n.g = 240; n.b = 250; // Bottom White Hemisphere
            }
          }
        });
        break;
      }

      case 26: { // EBAY AUCTION (4-Color Bidding Strobe & Golden Gavel Shockwave)
        const ebayCols = [
          [229, 50, 56],  // Red
          [0, 100, 210],  // Blue
          [245, 175, 2],  // Yellow
          [134, 184, 23]  // Green
        ];
        const step = Math.floor(sec * 7);
        LED_NODES.forEach((n, i) => {
          if (n.ring === 0) {
            n.r = 255; n.g = 215; n.b = 0;
          } else {
            const c = ebayCols[(i + step) % 4];
            n.r = c[0]; n.g = c[1]; n.b = c[2];
          }
        });
        break;
      }

      default: { // 27: TERRA PLANET
        LED_NODES.forEach((n) => {
          const continent = Math.sin((n.angle || 0) * 2 + sec * 1.4) + Math.cos(n.ring * 1.5 - sec);
          if (n.ring === 3 && Math.abs(Math.sin(n.angle || 0)) > 0.86) {
            n.r = 120; n.g = 255; n.b = 220; // Polar aurora
          } else if (continent > 0.35) {
            n.r = 25; n.g = 210; n.b = 95;   // Emerald landmass
          } else {
            n.r = 15; n.g = 115; n.b = 245;  // Deep azure ocean
          }
        });
        break;
      }
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
          oledCtx.font = 'bold 21px "JetBrains Mono", monospace';
          oledCtx.fillText((TRACKS[activeTrackIdx] ? TRACKS[activeTrackIdx].name : 'UNKNOWN').slice(0, 26), 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('SW: JUKEBOX / STOP | 16kHz PIO1', 24, 168);
          break;

        case 7: // COSMIC SFX
          oledCtx.fillText(`COSMIC SFX: ${activeSfxIdx + 1}/${COSMIC_SFX.length} ${isChiptunePlaying ? '[ACTIVE]' : '[READY]'}`, 24, 80);
          oledCtx.font = 'bold 22px "JetBrains Mono", monospace';
          oledCtx.fillText(COSMIC_SFX[activeSfxIdx] ? COSMIC_SFX[activeSfxIdx].name : 'WARP DRIVE', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText('SW: TRIGGER SFX | R2-D2 & SPELLS', 24, 168);
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
          oledCtx.fillText(window.__celestialTwinBridge && window.__celestialTwinBridge.oracleMode === 'local-ai' ? 'ACTIVE: LOCAL WEB AI' : 'ACTIVE: SCRIPTED / GEMINI', 24, 126);
          oledCtx.font = 'bold 18px "JetBrains Mono", monospace';
          oledCtx.fillText(`PERSONA: ${(window.__celestialTwinBridge && window.__celestialTwinBridge.personaTitle) || 'MACHINE GOD'}`, 24, 168);
          break;

        case 8: // VOICE ORACLE
          {
            const bridge = window.__celestialTwinBridge || {};
            const pTitle = (bridge.personaTitle || 'MACHINE GOD').toUpperCase();
            const stNames = ['IDLE', 'LISTENING', 'PROCESSING', 'SPEAKING'];
            const stLabel = stNames[currentState] || 'IDLE';
            oledCtx.fillText(`ORACLE: ${pTitle.slice(0, 14)} [${stLabel}]`, 24, 78);
            oledCtx.font = 'bold 19px "JetBrains Mono", monospace';
            const resp = String(bridge.lastResponse || 'Press QUERY or type an inquiry below.');
            oledCtx.fillText(resp.slice(0, 26), 24, 122);
            oledCtx.font = 'bold 17px "JetBrains Mono", monospace';
            oledCtx.fillText(resp.slice(26, 54) || 'TURN: TUNE | SW: OK | GP9: PTT', 24, 164);
          }
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
        syncActiveEffectButtons(currentAnimIdx);
        playConfirmChime();
      } else if (currentModeIdx === 7) { // COSMIC SFX
        playCosmicSfx(activeSfxIdx);
        activeSfxIdx = (activeSfxIdx + 1) % COSMIC_SFX.length;
      } else {
        playConfirmChime();
      }
    }

    updateTelemetryUi();
    renderOledScreen();
  }

  let queryPressStartMs = 0;
  let queryWasAlreadyRecording = false;

  function handleQueryButtonDown() {
    initAudioContext();
    playTone(587.33, 40, 'sine');
    queryPressStartMs = Date.now();

    if (isChiptuneSubMode) {
      currentState = STATE_LISTENING;
      updateTelemetryUi();
      renderOledScreen();
      return;
    }

    if (typeof window.__celestialIsVoiceCapturing === 'function' && window.__celestialIsVoiceCapturing()) {
      queryWasAlreadyRecording = true;
      return;
    }
    queryWasAlreadyRecording = false;

    if (typeof window.__celestialStartVoiceCapture === 'function') {
      window.__celestialStartVoiceCapture();
      return;
    }

    currentState = STATE_LISTENING;
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

    const heldDuration = Date.now() - queryPressStartMs;
    if (typeof window.__celestialStopVoiceCapture === 'function') {
      // If user clicked a second time while recording, or held PTT for > 450ms and released, stop & send!
      // If it was a quick click (< 450ms) to start recording, keep recording open so user can speak!
      if (queryWasAlreadyRecording || heldDuration > 450) {
        window.__celestialStopVoiceCapture();
      }
      return;
    }

    if (typeof window.__celestialTriggerOracleQuery === 'function') {
      window.__celestialTriggerOracleQuery();
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

  // Expose bridge hooks for twin/controls.js
  window.__celestialRenderOled = renderOledScreen;
  window.__celestialSyncLegacyState = function (st) {
    currentState = st;
    updateTelemetryUi();
    renderOledScreen();
  };
  window.__celestialSetAnimIdx = function (idx) {
    currentAnimIdx = idx % ANIM_NAMES.length;
    currentModeIdx = 5; // CORE FX
    updateTelemetryUi();
    renderOledScreen();
  };

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
  let schemMode = 'exploded'; // 'exploded' or 'assembled'
  let schemModeProgress = 0.0; // 0.0 = exploded, 1.0 = assembled

  const SCHEMATIC_COMPONENTS = [
    {
      id: 'pico',
      name: 'Raspberry Pi Pico 2 W',
      chip: 'RP2350 (Dual ARM Cortex-M33 @ 150MHz)',
      category: 'MCU Central Node',
      voltage: '3.3V Logic (5V VBUS In)',
      x: 0, y: 0, z: 0,
      expX: 0, expY: 0, expZ: 0,
      asmX: 0, asmY: 20, asmZ: 0,
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
      expX: -210, expY: -90, expZ: 30,
      asmX: 0, asmY: -115, asmZ: 0,
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
      expX: -60, expY: -160, expZ: 40,
      asmX: 42, asmY: 55, asmZ: 15,
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
      expX: 180, expY: -100, expZ: 25,
      asmX: 0, asmY: 120, asmZ: 0,
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
      name: 'Side Query Button (PTT)',
      chip: 'Compact Momentary Push-To-Talk',
      category: 'AI Voice Trigger',
      voltage: '3.3V Logic',
      x: 210, y: 10, z: 15,
      expX: 210, expY: 10, expZ: 15,
      asmX: -55, asmY: 20, asmZ: 15,
      w: 56, h: 40,
      color: '#22c55e',
      desc: 'Momentary push-button mounted on the side of the enclosure. Hold to activate 16 kHz I2S microphone streaming; release to trigger Oracle response.',
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
      expX: -140, expY: 130, expZ: 30,
      asmX: -45, asmY: 45, asmZ: 15,
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
      name: 'INMP441 I2S Microphone',
      chip: 'Omnidirectional MEMS + Acoustic Capsule',
      category: 'Audio Input',
      voltage: '3.3V (Strict)',
      x: -180, y: 20, z: 20,
      expX: -180, expY: 20, expZ: 20,
      asmX: 0, asmY: 120, asmZ: 10,
      w: 64, h: 44,
      color: '#8b5cf6',
      desc: 'High-sensitivity omnidirectional digital MEMS microphone mounted at the bottom of the enclosure for 16 kHz voice queries.',
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
      expX: 180, expY: 150, expZ: 45,
      asmX: 160, asmY: 130, asmZ: 45,
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
      expX: 30, expY: 180, expZ: 35,
      asmX: -160, asmY: 130, asmZ: 45,
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

  let schemCircuitMode = 'all'; // Default to full direct pinout map

  let schemHitBoxes = [];



  function renderSchematic(t) {
    if (!schematicCtx || !schematicCanvas) return;
    if (typeof window.__celestialRenderBreadboard2D === 'function') {
      const cssW = schematicCanvas.clientWidth || (schematicCanvas.parentElement && schematicCanvas.parentElement.clientWidth) || 720;
      const cssH = 460;
      const dpr = Math.min(window.devicePixelRatio || 1, 2.5);
      const targetW = Math.round(cssW * dpr);
      const targetH = Math.round(cssH * dpr);
      if (schematicCanvas.width !== targetW || schematicCanvas.height !== targetH) {
        schematicCanvas.width = targetW;
        schematicCanvas.height = targetH;
      }
      schematicCtx.save();
      schematicCtx.scale(dpr, dpr);
      schemHitBoxes = window.__celestialRenderBreadboard2D(
        schematicCtx,
        cssW,
        cssH,
        {
          circuitMode: schemCircuitMode,
          selectedId: selectedComponentId,
          ledNodes: LED_NODES,
          staffState: currentState,
          timestamp: t
        }
      );
      schematicCtx.restore();
    }
  }

  function selectSchematicComponent(id, keepOverviewHighlight = false) {
    const comp = SCHEMATIC_COMPONENTS.find(c => c.id === id) || SCHEMATIC_COMPONENTS[0];
    if (!comp) return;

    selectedComponentId = keepOverviewHighlight ? 'pico' : id;
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
    if (!schematicCanvas) return;
    const rect = schematicCanvas.getBoundingClientRect();
    const clientX = e.clientX || (e.touches && e.touches[0] && e.touches[0].clientX) || 0;
    const clientY = e.clientY || (e.touches && e.touches[0] && e.touches[0].clientY) || 0;
    const clickX = clientX - rect.left;
    const clickY = clientY - rect.top;

    let hitId = null;
    for (let i = schemHitBoxes.length - 1; i >= 0; i--) {
      const box = schemHitBoxes[i];
      if (clickX >= box.x && clickX <= box.x + box.w && clickY >= box.y && clickY <= box.y + box.h) {
        hitId = box.id === 'wifi' ? 'pico' : box.id;
        break;
      }
    }

    if (hitId) {
      const nextId = (selectedComponentId === hitId && hitId !== 'pico') ? 'pico' : hitId;
      selectSchematicComponent(nextId);
      playTone(659.25, 20, 'sine');
    } else {
      selectSchematicComponent('pico');
    }
  }



  // --- MAIN LOOP ANIMATION ---

  let lastMarqueeTime = 0;
  let lastMobileLlmThrottleTime = 0;

  const prefersReducedMotion = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const isMobileDevice = /Android|iPhone|iPad|iPod|Mobile/i.test((typeof navigator !== 'undefined' && navigator.userAgent) || '');



  function animate(t) {

    if (document.hidden) {

      requestAnimationFrame(animate);

      return;

    }

    // Mobile-only optimization: yield mobile GPU/CPU to WebGPU/WASM Local LLM worker while generating
    if (isMobileDevice && window.__celestialIsLocalLlmGenerating) {
      if (t - lastMobileLlmThrottleTime < 600) {
        requestAnimationFrame(animate);
        return;
      }
      lastMobileLlmThrottleTime = t;
    }



    if (!prefersReducedMotion) {

      renderStarfield(t);

    }

    updateLedColors(t);

    renderLeds();



    if (typeof window.__celestialRenderReplicaFrame === 'function') {

      window.__celestialRenderReplicaFrame(t, LED_NODES);

    }



    if (t - lastMarqueeTime >= (prefersReducedMotion ? 250 : 50)) {

      if (!prefersReducedMotion) marqueeOffset += 2;

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



    // 2D Breadboard Schematic Click Listener

    if (schematicCanvas) {

      schematicCanvas.addEventListener('mousedown', handleSchematicPointerDown);

      schematicCanvas.addEventListener('touchstart', handleSchematicPointerDown, { passive: true });

    }



    // 2D Breadboard Wiring Example Buttons

    const circuitMap = {

      all: 'pico',

      leds: 'leds',

      oled: 'oled',

      controls: 'query',

      audio: 'mic'

    };

    document.querySelectorAll('.schematic-tool-btn[data-circuit]').forEach(btn => {

      btn.addEventListener('click', () => {

        const mode = btn.dataset.circuit || 'all';

        schemCircuitMode = mode;

        document.querySelectorAll('.schematic-tool-btn[data-circuit]').forEach(b => b.classList.remove('active'));

        btn.classList.add('active');

        if (circuitMap[mode]) {
          selectSchematicComponent(circuitMap[mode], true);
        }

      });

    });



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

    // Preset Chiptune Buttons (21 Staff Melodies)
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const trk = parseInt(btn.dataset.track, 10);
        if (!isNaN(trk)) {
          if (isChiptunePlaying && activeTrackIdx === trk && currentModeIdx === 6) {
            stopChiptunePlayback();
            playStopTone();
          } else {
            activeTrackIdx = trk;
            currentModeIdx = 6;
            isChiptuneSubMode = false;
            playChiptuneTrack(trk);
          }
          updateTelemetryUi();
          renderOledScreen();
        }
      });
    });

    // Cosmic & Astromech Sound FX Buttons (14 Staff SFX)
    document.querySelectorAll('.sfx-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const sfxIdx = parseInt(btn.dataset.sfx, 10);
        if (!isNaN(sfxIdx)) {
          activeSfxIdx = sfxIdx;
          currentModeIdx = 7;
          isChiptuneSubMode = false;
          playCosmicSfx(sfxIdx);
          updateTelemetryUi();
          renderOledScreen();
        }
      });
    });

    // Stop Audio Button
    const btnStopAudio = document.getElementById('btn-stop-audio');
    if (btnStopAudio) {
      btnStopAudio.addEventListener('click', () => {
        stopChiptunePlayback();
        playStopTone();
        updateTelemetryUi();
        renderOledScreen();
      });
    }

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

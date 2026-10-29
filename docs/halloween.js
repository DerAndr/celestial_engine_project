/**
 * Celestial Engine Mk.V — Cyber-Arcane Halloween Edition
 * Interactive Web Audio Chiptune Synthesizer, 3D Exploded/Assembled Flask Canvas,
 * Rotary Knob Physics & Ethereal Starfield/Bead Simulation.
 *
 * (c) 2026 DerAndr — Open Source Under MIT License
 */

(function () {
  'use strict';

  // ============================================================================
  // 1. SPOOKY AMBIENT BACKGROUND CANVAS (Ember Dust + Foam Beads + Ghosts)
  // ============================================================================
  const bgCanvas = document.getElementById('spooky-canvas');
  const bgCtx = bgCanvas ? bgCanvas.getContext('2d') : null;

  let bgWidth = 0;
  let bgHeight = 0;
  const particles = [];
  const PARTICLE_COUNT = 65;

  function resizeBg() {
    if (!bgCanvas) return;
    bgWidth = bgCanvas.width = window.innerWidth;
    bgHeight = bgCanvas.height = window.innerHeight;
  }
  window.addEventListener('resize', resizeBg);
  resizeBg();

  for (let i = 0; i < PARTICLE_COUNT; i++) {
    particles.push({
      x: Math.random() * (bgWidth || 1000),
      y: Math.random() * (bgHeight || 1000),
      radius: Math.random() * 2.5 + 0.8,
      speedX: (Math.random() - 0.5) * 0.4,
      speedY: -Math.random() * 0.6 - 0.2, // drifting upwards
      alpha: Math.random() * 0.7 + 0.2,
      color: Math.random() > 0.4 ? '#f97316' : (Math.random() > 0.5 ? '#a855f7' : '#38bdf8'),
      pulseSpeed: Math.random() * 0.03 + 0.01,
      angle: Math.random() * Math.PI * 2
    });
  }

  function renderBg() {
    if (!bgCtx || !bgCanvas) return;
    bgCtx.clearRect(0, 0, bgWidth, bgHeight);

    particles.forEach(p => {
      p.x += p.speedX;
      p.y += p.speedY;
      p.angle += p.pulseSpeed;

      if (p.y < -10) p.y = bgHeight + 10;
      if (p.x < -10) p.x = bgWidth + 10;
      if (p.x > bgWidth + 10) p.x = -10;

      const currentAlpha = p.alpha * (0.6 + 0.4 * Math.sin(p.angle));

      bgCtx.fillStyle = p.color;
      bgCtx.globalAlpha = currentAlpha;
      bgCtx.shadowColor = p.color;
      bgCtx.shadowBlur = p.radius * 3;
      bgCtx.beginPath();
      bgCtx.arc(p.x, p.y, p.radius, 0, Math.PI * 2);
      bgCtx.fill();
    });

    bgCtx.globalAlpha = 1.0;
    bgCtx.shadowBlur = 0;
    requestAnimationFrame(renderBg);
  }
  if (bgCtx) requestAnimationFrame(renderBg);


  // ============================================================================
  // 2. WEB AUDIO 8-BIT CHIPTUNE SYNTHESIZER (Authentic Square Waves & ADSR)
  // ============================================================================
  let audioCtx = null;
  let activeSongTimer = null;
  let currentTrackIndex = -1;
  let isPlaying = false;

  function initAudio() {
    if (!audioCtx) {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (AudioContextClass) {
        audioCtx = new AudioContextClass();
      }
    }
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  // Musical Note Frequencies (Hz)
  const NOTES = {
    REST: 0,
    C3: 130.81, D3: 146.83, E3: 164.81, F3: 174.61, G3: 196.00, A3: 220.00, B3: 246.94,
    C4: 261.63, CS4: 277.18, D4: 293.66, DS4: 311.13, E4: 329.63, F4: 349.23,
    FS4: 369.99, G4: 392.00, GS4: 415.30, A4: 440.00, AS4: 466.16, B4: 493.88,
    C5: 523.25, CS5: 554.37, D5: 587.33, DS5: 622.25, E5: 659.25, F5: 698.46,
    FS5: 739.99, G5: 783.99, GS5: 830.61, A5: 880.00, AS5: 932.33, B5: 987.77,
    C6: 1046.50
  };

  // Iconic Authentic Melodies
  const CHIPTUNES = {
    0: {
      title: 'DOOM E1M1 HELLFIRE',
      tempo: 125,
      notes: [
        { f: NOTES.E3, d: 0.12 }, { f: NOTES.E3, d: 0.12 }, { f: NOTES.E4, d: 0.18 },
        { f: NOTES.E3, d: 0.12 }, { f: NOTES.E3, d: 0.12 }, { f: NOTES.D4, d: 0.18 },
        { f: NOTES.E3, d: 0.12 }, { f: NOTES.E3, d: 0.12 }, { f: NOTES.C4, d: 0.18 },
        { f: NOTES.E3, d: 0.12 }, { f: NOTES.E3, d: 0.12 }, { f: NOTES.AS3, d: 0.18 },
        { f: NOTES.B3, d: 0.22 }, { f: NOTES.C4, d: 0.22 }
      ]
    },
    1: {
      title: 'KOROBEINIKI (TETRIS)',
      tempo: 140,
      notes: [
        { f: NOTES.E4, d: 0.35 }, { f: NOTES.B3, d: 0.18 }, { f: NOTES.C4, d: 0.18 },
        { f: NOTES.D4, d: 0.35 }, { f: NOTES.C4, d: 0.18 }, { f: NOTES.B3, d: 0.18 },
        { f: NOTES.A3, d: 0.35 }, { f: NOTES.A3, d: 0.18 }, { f: NOTES.C4, d: 0.18 },
        { f: NOTES.E4, d: 0.35 }, { f: NOTES.D4, d: 0.18 }, { f: NOTES.C4, d: 0.18 },
        { f: NOTES.B3, d: 0.50 }, { f: NOTES.C4, d: 0.18 }, { f: NOTES.D4, d: 0.35 },
        { f: NOTES.E4, d: 0.35 }, { f: NOTES.C4, d: 0.35 }, { f: NOTES.A3, d: 0.35 },
        { f: NOTES.A3, d: 0.50 }
      ]
    },
    2: {
      title: 'IMPERIAL MARCH',
      tempo: 108,
      notes: [
        { f: NOTES.G4, d: 0.45 }, { f: NOTES.G4, d: 0.45 }, { f: NOTES.G4, d: 0.45 },
        { f: NOTES.DS4, d: 0.32 }, { f: NOTES.AS4, d: 0.14 },
        { f: NOTES.G4, d: 0.45 }, { f: NOTES.DS4, d: 0.32 }, { f: NOTES.AS4, d: 0.14 },
        { f: NOTES.G4, d: 0.80 },
        { f: NOTES.D5, d: 0.45 }, { f: NOTES.D5, d: 0.45 }, { f: NOTES.D5, d: 0.45 },
        { f: NOTES.DS5, d: 0.32 }, { f: NOTES.AS4, d: 0.14 },
        { f: NOTES.FS4, d: 0.45 }, { f: NOTES.DS4, d: 0.32 }, { f: NOTES.AS4, d: 0.14 },
        { f: NOTES.G4, d: 0.80 }
      ]
    },
    3: {
      title: 'ZELDA TRIFORCE HYMN',
      tempo: 125,
      notes: [
        { f: NOTES.AS3, d: 0.50 }, { f: NOTES.F4, d: 0.30 }, { f: NOTES.AS4, d: 0.50 },
        { f: NOTES.C5, d: 0.18 }, { f: NOTES.D5, d: 0.18 }, { f: NOTES.DS5, d: 0.18 },
        { f: NOTES.F5, d: 0.80 }, { f: NOTES.F5, d: 0.25 }, { f: NOTES.FS5, d: 0.25 },
        { f: NOTES.GS5, d: 0.25 }, { f: NOTES.AS5, d: 0.90 }
      ]
    },
    4: {
      title: 'ECTO GROOVE (GHOSTBUSTERS)',
      tempo: 116,
      notes: [
        { f: NOTES.C4, d: 0.25 }, { f: NOTES.C4, d: 0.12 }, { f: NOTES.E4, d: 0.25 },
        { f: NOTES.C4, d: 0.18 }, { f: NOTES.D4, d: 0.18 }, { f: NOTES.AS3, d: 0.35 },
        { f: NOTES.C4, d: 0.25 }, { f: NOTES.C4, d: 0.12 }, { f: NOTES.E4, d: 0.25 },
        { f: NOTES.C4, d: 0.18 }, { f: NOTES.G4, d: 0.30 }, { f: NOTES.F4, d: 0.45 }
      ]
    },
    7: {
      title: 'DAFT PUNK CYBER FUNK',
      tempo: 126,
      notes: [
        { f: NOTES.A3, d: 0.22 }, { f: NOTES.C4, d: 0.22 }, { f: NOTES.E4, d: 0.22 },
        { f: NOTES.G4, d: 0.22 }, { f: NOTES.A4, d: 0.40 }, { f: NOTES.G4, d: 0.22 },
        { f: NOTES.E4, d: 0.22 }, { f: NOTES.D4, d: 0.30 }, { f: NOTES.C4, d: 0.22 },
        { f: NOTES.A3, d: 0.45 }
      ]
    }
  };

  let audioEnergy = 0.0;

  function playTone(freq, duration) {
    if (!audioCtx || freq <= 0) return;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();

    osc.type = 'square'; // 8-bit NES/Arcade classic tone
    osc.frequency.setValueAtTime(freq, audioCtx.currentTime);

    // Fast ADSR Envelope
    const now = audioCtx.currentTime;
    const attack = 0.015;
    const decay = 0.04;
    const sustain = 0.7;
    const release = 0.03;

    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.linearRampToValueAtTime(0.22, now + attack);
    gain.gain.linearRampToValueAtTime(0.22 * sustain, now + attack + decay);
    gain.gain.setValueAtTime(0.22 * sustain, now + duration - release);
    gain.gain.linearRampToValueAtTime(0.0001, now + duration);

    osc.connect(gain);
    gain.connect(audioCtx.destination);

    osc.start(now);
    osc.stop(now + duration);

    audioEnergy = 1.0;
  }

  function playRotaryClick() {
    initAudio();
    if (!audioCtx) return;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'triangle';
    osc.frequency.setValueAtTime(600, audioCtx.currentTime);
    osc.frequency.exponentialRampToValueAtTime(200, audioCtx.currentTime + 0.025);

    gain.gain.setValueAtTime(0.12, audioCtx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.025);

    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    osc.stop(audioCtx.currentTime + 0.025);
  }

  function stopChiptune() {
    if (activeSongTimer) {
      clearTimeout(activeSongTimer);
      activeSongTimer = null;
    }
    isPlaying = false;
    currentTrackIndex = -1;
    audioEnergy = 0;

    const statusPill = document.getElementById('music-status-pill');
    if (statusPill) {
      statusPill.textContent = '○ STOPPED';
      statusPill.style.color = 'var(--text-dim)';
    }
    const stateLabel = document.getElementById('oled-state-label');
    if (stateLabel) stateLabel.textContent = 'STATE: IDLE';

    document.querySelectorAll('.music-btn').forEach(btn => btn.classList.remove('active'));
  }

  function playChiptune(trackId) {
    initAudio();
    stopChiptune();

    const track = CHIPTUNES[trackId];
    if (!track) return;

    isPlaying = true;
    currentTrackIndex = trackId;

    const statusPill = document.getElementById('music-status-pill');
    if (statusPill) {
      statusPill.textContent = '● PLAYING: ' + track.title;
      statusPill.style.color = 'var(--pumpkin-orange)';
    }

    const oledMain = document.getElementById('oled-main-text');
    if (oledMain) oledMain.textContent = track.title;

    const oledTrack = document.getElementById('oled-track-label');
    if (oledTrack) oledTrack.textContent = track.title;

    const oledState = document.getElementById('oled-state-label');
    if (oledState) oledState.textContent = 'SYNTH: 16kHz';

    document.querySelectorAll('.music-btn').forEach(btn => {
      if (parseInt(btn.getAttribute('data-track')) === trackId) {
        btn.classList.add('active');
      } else {
        btn.classList.remove('active');
      }
    });

    let noteIdx = 0;
    function scheduleNext() {
      if (!isPlaying) return;
      const n = track.notes[noteIdx];
      playTone(n.f, n.d);
      noteIdx = (noteIdx + 1) % track.notes.length;
      activeSongTimer = setTimeout(scheduleNext, n.d * 1000 + 35);
    }
    scheduleNext();
  }


  // ============================================================================
  // 3. FLASK CANVAS VISUALIZER (Assembled Flask vs. Exploded 3D Parts)
  // ============================================================================
  const flaskCanvas = document.getElementById('flask-canvas');
  const flaskCtx = flaskCanvas ? flaskCanvas.getContext('2d') : null;

  let viewMode = 'assembled'; // 'assembled' or 'exploded'
  let transitionProgress = 0.0; // 0 = assembled, 1 = exploded
  let knobAngle = 0; // rotation angle in degrees

  // Generate Styrofoam Diffuser Beads (Foam Balls)
  const BEAD_COUNT = 38;
  const beads = [];
  for (let i = 0; i < BEAD_COUNT; i++) {
    const angle = Math.random() * Math.PI * 2;
    const r = Math.random() * 52;
    beads.push({
      baseX: 150 + Math.cos(angle) * r,
      baseY: 90 + Math.sin(angle) * (r * 0.75),
      size: Math.random() * 4.5 + 3.0,
      color: Math.random() > 0.25 ? '#f8fafc' : (Math.random() > 0.5 ? '#e0f2fe' : '#fed7aa'),
      driftAngle: Math.random() * Math.PI * 2,
      explodeOffsetX: (Math.random() - 0.5) * 140,
      explodeOffsetY: -Math.random() * 90 - 40
    });
  }

  // 43 LED Rings Geometry
  const LED_NODES = [];
  // Center LED (1)
  LED_NODES.push({ ring: 0, idx: 0, r: 0, angle: 0 });
  // Inner Ring (6 LEDs)
  for (let i = 0; i < 6; i++) {
    LED_NODES.push({ ring: 1, idx: i, r: 18, angle: (i / 6) * Math.PI * 2 });
  }
  // Mid Ring (12 LEDs)
  for (let i = 0; i < 12; i++) {
    LED_NODES.push({ ring: 2, idx: i, r: 34, angle: (i / 12) * Math.PI * 2 });
  }
  // Outer Ring (24 LEDs)
  for (let i = 0; i < 24; i++) {
    LED_NODES.push({ ring: 3, idx: i, r: 52, angle: (i / 24) * Math.PI * 2 });
  }

  function renderFlask(timestamp) {
    if (!flaskCtx || !flaskCanvas) return;
    const w = flaskCanvas.width;
    const h = flaskCanvas.height;
    const t = timestamp * 0.002;

    // Smooth transition between assembled and exploded
    const targetProgress = viewMode === 'exploded' ? 1.0 : 0.0;
    transitionProgress += (targetProgress - transitionProgress) * 0.08;

    flaskCtx.clearRect(0, 0, w, h);

    // Audio energy decay
    audioEnergy *= 0.94;
    const pulseFactor = 1.0 + audioEnergy * 0.35 + 0.08 * Math.sin(t * 3);

    // Component Vertical Offsets in Exploded Mode
    const domeLift = transitionProgress * -75;
    const ledLift = transitionProgress * -38;
    const beadDisp = transitionProgress;
    const chassisSlide = transitionProgress * 30;
    const oledSlide = transitionProgress * 55;
    const knobDrop = transitionProgress * 55;

    // Center Coordinate Base
    const cx = w / 2;
    const domeCenterY = 90 + domeLift;

    // ------------------------------------------------------------------------
    // A. EXPLO-MODE LEADER ALIGNMENT LINES
    // ------------------------------------------------------------------------
    if (transitionProgress > 0.05) {
      flaskCtx.save();
      flaskCtx.strokeStyle = `rgba(56, 189, 248, ${0.35 * transitionProgress})`;
      flaskCtx.setLineDash([3, 4]);
      flaskCtx.lineWidth = 1;

      // Central Axis Line
      flaskCtx.beginPath();
      flaskCtx.moveTo(cx, 20 + domeLift);
      flaskCtx.lineTo(cx, 370 + knobDrop);
      flaskCtx.stroke();

      // Horizontal leader tags
      if (transitionProgress > 0.6) {
        flaskCtx.font = '9px "JetBrains Mono", monospace';
        flaskCtx.fillStyle = 'rgba(148, 163, 184, 0.8)';

        // Dome Tag
        flaskCtx.fillText('① ACRYLIC BELL DOME', cx + 65, domeCenterY - 30);
        // LEDs Tag
        flaskCtx.fillText('② 43 CONCENTRIC LEDS', cx - 130, 90 + ledLift);
        // Breadboard Tag
        flaskCtx.fillText('③ PICO 2 W / BREADBOARD', cx - 140, 200);
        // OLED Tag
        flaskCtx.fillText('④ SH1106 1.3" OLED', cx + 75, 270 + oledSlide * 0.2);
        // Knob Tag
        flaskCtx.fillText('⑤ YELLOW ROTARY KNOB', cx + 45, 345 + knobDrop);
      }
      flaskCtx.restore();
    }

    // ------------------------------------------------------------------------
    // B. TRANSPARENT ACRYLIC DOME & FLASK BODY
    // ------------------------------------------------------------------------
    flaskCtx.save();
    const glassAlpha = 1.0 - transitionProgress * 0.45;

    // Dome Glass Arc
    flaskCtx.beginPath();
    flaskCtx.arc(cx, domeCenterY, 68, Math.PI, 0, false);
    flaskCtx.lineTo(cx + 68, domeCenterY + 40);
    flaskCtx.lineTo(cx - 68, domeCenterY + 40);
    flaskCtx.closePath();

    const domeGrad = flaskCtx.createRadialGradient(cx - 20, domeCenterY - 20, 10, cx, domeCenterY, 70);
    domeGrad.addColorStop(0, `rgba(255, 255, 255, ${0.15 * glassAlpha})`);
    domeGrad.addColorStop(0.7, `rgba(56, 189, 248, ${0.08 * glassAlpha})`);
    domeGrad.addColorStop(1, `rgba(56, 189, 248, ${0.25 * glassAlpha})`);
    flaskCtx.fillStyle = domeGrad;
    flaskCtx.fill();

    flaskCtx.strokeStyle = `rgba(255, 255, 255, ${0.4 * glassAlpha})`;
    flaskCtx.lineWidth = 2;
    flaskCtx.stroke();

    // Cylindrical Jar Body
    const bodyTopY = 130 + domeLift * 0.3;
    const bodyBottomY = 320;
    const bodyWidth = 136;
    const bodyLeft = cx - bodyWidth / 2 + (transitionProgress > 0 ? chassisSlide * 0.4 : 0);

    flaskCtx.beginPath();
    if (flaskCtx.roundRect) {
      flaskCtx.roundRect(bodyLeft, bodyTopY, bodyWidth, bodyBottomY - bodyTopY, [0, 0, 16, 16]);
    } else {
      flaskCtx.rect(bodyLeft, bodyTopY, bodyWidth, bodyBottomY - bodyTopY);
    }
    const bodyGrad = flaskCtx.createLinearGradient(bodyLeft, bodyTopY, bodyLeft + bodyWidth, bodyBottomY);
    bodyGrad.addColorStop(0, `rgba(255, 255, 255, ${0.05 * glassAlpha})`);
    bodyGrad.addColorStop(0.5, `rgba(56, 189, 248, ${0.03 * glassAlpha})`);
    bodyGrad.addColorStop(1, `rgba(255, 255, 255, ${0.08 * glassAlpha})`);
    flaskCtx.fillStyle = bodyGrad;
    flaskCtx.fill();
    flaskCtx.strokeStyle = `rgba(255, 255, 255, ${0.25 * glassAlpha})`;
    flaskCtx.lineWidth = 1.5;
    flaskCtx.stroke();

    // Specular Highlight Glare on Glass
    flaskCtx.beginPath();
    flaskCtx.ellipse(bodyLeft + 14, bodyTopY + 70, 4, 50, 0, 0, Math.PI * 2);
    flaskCtx.fillStyle = `rgba(255, 255, 255, ${0.18 * glassAlpha})`;
    flaskCtx.fill();
    flaskCtx.restore();

    // ------------------------------------------------------------------------
    // C. 43 WS2812B CONCENTRIC LED RINGS & GLOW
    // ------------------------------------------------------------------------
    const ledCenterY = 90 + ledLift;
    flaskCtx.save();

    // Ambient Ethereal Light Blob
    const glowGrad = flaskCtx.createRadialGradient(cx, ledCenterY, 5, cx, ledCenterY, 65 * pulseFactor);
    glowGrad.addColorStop(0, 'rgba(56, 189, 248, 0.45)');
    glowGrad.addColorStop(0.5, 'rgba(168, 85, 247, 0.25)');
    glowGrad.addColorStop(1, 'rgba(0, 0, 0, 0)');
    flaskCtx.fillStyle = glowGrad;
    flaskCtx.beginPath();
    flaskCtx.arc(cx, ledCenterY, 70 * pulseFactor, 0, Math.PI * 2);
    flaskCtx.fill();

    // Draw Individual Concentric Ring PCBs (Black/Dark ring substrates)
    [18, 34, 52].forEach(radius => {
      flaskCtx.strokeStyle = 'rgba(20, 30, 45, 0.85)';
      flaskCtx.lineWidth = 6;
      flaskCtx.beginPath();
      flaskCtx.arc(cx, ledCenterY, radius, 0, Math.PI * 2);
      flaskCtx.stroke();
    });

    // Draw 43 Active Glowing LEDs
    LED_NODES.forEach((node, i) => {
      const lx = cx + Math.cos(node.angle + t * 0.4) * node.r;
      const ly = ledCenterY + Math.sin(node.angle + t * 0.4) * (node.r * 0.7);

      // Color animation (Cyber Blue, Spectral Purple, Emerald Glow)
      const colorHue = 195 + 40 * Math.sin(t * 2 + i * 0.2);
      const isBeat = (i % 4 === 0 && audioEnergy > 0.4);

      flaskCtx.fillStyle = isBeat ? '#ffffff' : `hsl(${colorHue}, 95%, 65%)`;
      flaskCtx.shadowColor = `hsl(${colorHue}, 90%, 60%)`;
      flaskCtx.shadowBlur = isBeat ? 14 : 7;

      flaskCtx.beginPath();
      flaskCtx.arc(lx, ly, isBeat ? 3.8 : 2.5, 0, Math.PI * 2);
      flaskCtx.fill();
    });
    flaskCtx.shadowBlur = 0;
    flaskCtx.restore();

    // ------------------------------------------------------------------------
    // D. FOAM DIFFUSER BEADS (Styrofoam Balls in the Dome)
    // ------------------------------------------------------------------------
    flaskCtx.save();
    beads.forEach(b => {
      const bx = b.baseX + b.explodeOffsetX * beadDisp;
      const by = b.baseY + domeLift * 0.7 + b.explodeOffsetY * beadDisp + Math.sin(t + b.driftAngle) * 2;

      flaskCtx.fillStyle = b.color;
      flaskCtx.shadowColor = 'rgba(56, 189, 248, 0.4)';
      flaskCtx.shadowBlur = 4;
      flaskCtx.globalAlpha = 0.85 - transitionProgress * 0.2;

      flaskCtx.beginPath();
      flaskCtx.arc(bx, by, b.size, 0, Math.PI * 2);
      flaskCtx.fill();
    });
    flaskCtx.restore();

    // ------------------------------------------------------------------------
    // E. WHITE BREADBOARD + PICO 2 W (RP2350) + DUPONT WIRES
    // ------------------------------------------------------------------------
    flaskCtx.save();
    const bbX = cx - 40;
    const bbY = 150;
    const bbW = 80;
    const bbH = 110;

    // Breadboard body
    flaskCtx.fillStyle = '#f1f5f9';
    flaskCtx.shadowColor = 'rgba(0, 0, 0, 0.4)';
    flaskCtx.shadowBlur = 8;
    if (flaskCtx.roundRect) {
      flaskCtx.roundRect(bbX, bbY, bbW, bbH, 6);
    } else {
      flaskCtx.rect(bbX, bbY, bbW, bbH);
    }
    flaskCtx.fill();
    flaskCtx.shadowBlur = 0;

    // Breadboard tie point rails
    flaskCtx.fillStyle = '#94a3b8';
    for (let row = 0; row < 10; row++) {
      for (let col = 0; col < 6; col++) {
        flaskCtx.fillRect(bbX + 8 + col * 11, bbY + 8 + row * 9.5, 2, 2);
      }
    }

    // Green Raspberry Pi Pico 2 W PCB
    const picoX = cx - 22;
    const picoY = bbY + 12;
    const picoW = 44;
    const picoH = 75;

    flaskCtx.fillStyle = '#059669'; // RP green PCB
    flaskCtx.strokeStyle = '#34d399';
    flaskCtx.lineWidth = 1;
    if (flaskCtx.roundRect) {
      flaskCtx.roundRect(picoX, picoY, picoW, picoH, 4);
    } else {
      flaskCtx.rect(picoX, picoY, picoW, picoH);
    }
    flaskCtx.fill();
    flaskCtx.stroke();

    // RP2350 Black Chip
    flaskCtx.fillStyle = '#1e293b';
    flaskCtx.fillRect(cx - 10, picoY + 22, 20, 20);

    // Silver CYW43439 Wi-Fi Can
    flaskCtx.fillStyle = '#94a3b8';
    flaskCtx.fillRect(cx - 12, picoY + 48, 24, 18);

    // Onboard Green LED (Blinks on telemetry/audio)
    flaskCtx.fillStyle = (Math.sin(t * 8) > 0 || isPlaying) ? '#4ade80' : '#166534';
    flaskCtx.shadowColor = '#4ade80';
    flaskCtx.shadowBlur = 6;
    flaskCtx.fillRect(cx + 12, picoY + 6, 3, 3);
    flaskCtx.shadowBlur = 0;

    // Colorful DuPont jumper wires running downwards
    const wireColors = ['#ef4444', '#3b82f6', '#f59e0b', '#10b981', '#a855f7'];
    wireColors.forEach((wc, wi) => {
      flaskCtx.strokeStyle = wc;
      flaskCtx.lineWidth = 1.5;
      flaskCtx.beginPath();
      flaskCtx.moveTo(cx - 15 + wi * 7, bbY + bbH - 6);
      flaskCtx.bezierCurveTo(
        cx - 20 + wi * 10, bbY + bbH + 20,
        cx + (wi - 2) * 12, 290,
        cx + 18, 295
      );
      flaskCtx.stroke();
    });
    flaskCtx.restore();

    // ------------------------------------------------------------------------
    // F. SH1106 1.3" OLED DISPLAY (Mounted angled on the right)
    // ------------------------------------------------------------------------
    flaskCtx.save();
    const oledX = cx + 8 + oledSlide;
    const oledY = 270;
    const oledW = 54;
    const oledH = 34;

    // Blue PCB carrier
    flaskCtx.fillStyle = '#1e3a8a';
    flaskCtx.fillRect(oledX, oledY, oledW, oledH);

    // Dark Glass Panel
    flaskCtx.fillStyle = '#020617';
    flaskCtx.fillRect(oledX + 3, oledY + 4, oledW - 6, oledH - 8);

    // Cyan OLED Pixels ("TURN: TUNE | OK")
    flaskCtx.fillStyle = '#38bdf8';
    flaskCtx.font = '6px "JetBrains Mono", monospace';
    flaskCtx.fillText('TURN: TUNE', oledX + 6, oledY + 14);
    flaskCtx.fillText('OK // PTT', oledX + 6, oledY + 23);

    flaskCtx.restore();

    // ------------------------------------------------------------------------
    // G. YELLOW ROTARY KNOB (Bourns PEC11R mounted on base)
    // ------------------------------------------------------------------------
    flaskCtx.save();
    const knobX = cx;
    const knobY = 338 + knobDrop;
    const knobR = 18;

    // Base Nut & Washer
    flaskCtx.fillStyle = '#475569';
    flaskCtx.beginPath();
    flaskCtx.arc(knobX, knobY, knobR + 4, 0, Math.PI * 2);
    flaskCtx.fill();

    // Bright Tactile Yellow Knob Cap
    const knobGrad = flaskCtx.createRadialGradient(knobX - 4, knobY - 4, 2, knobX, knobY, knobR);
    knobGrad.addColorStop(0, '#fef08a');
    knobGrad.addColorStop(0.7, '#facc15');
    knobGrad.addColorStop(1, '#ca8a04');

    flaskCtx.fillStyle = knobGrad;
    flaskCtx.shadowColor = 'rgba(234, 179, 8, 0.5)';
    flaskCtx.shadowBlur = 10;
    flaskCtx.beginPath();
    flaskCtx.arc(knobX, knobY, knobR, 0, Math.PI * 2);
    flaskCtx.fill();
    flaskCtx.shadowBlur = 0;

    // Ribbed Texture Marks
    const radKnob = (knobAngle * Math.PI) / 180;
    flaskCtx.strokeStyle = '#854d0e';
    flaskCtx.lineWidth = 1.2;
    for (let a = 0; a < 12; a++) {
      const notchAngle = radKnob + (a / 12) * Math.PI * 2;
      flaskCtx.beginPath();
      flaskCtx.moveTo(knobX + Math.cos(notchAngle) * (knobR - 4), knobY + Math.sin(notchAngle) * (knobR - 4));
      flaskCtx.lineTo(knobX + Math.cos(notchAngle) * knobR, knobY + Math.sin(notchAngle) * knobR);
      flaskCtx.stroke();
    }

    // Center Indicator Dot/Line
    flaskCtx.strokeStyle = '#451a03';
    flaskCtx.lineWidth = 2.5;
    flaskCtx.beginPath();
    flaskCtx.moveTo(knobX, knobY);
    flaskCtx.lineTo(knobX + Math.cos(radKnob) * (knobR - 3), knobY + Math.sin(radKnob) * (knobR - 3));
    flaskCtx.stroke();

    flaskCtx.restore();

    requestAnimationFrame(renderFlask);
  }
  if (flaskCtx) requestAnimationFrame(renderFlask);


  // ============================================================================
  // 4. ROTARY KNOB INTERACTION (PHYSICAL TURNING & OLED TIKER)
  // ============================================================================
  const yellowKnobEl = document.getElementById('yellow-knob');
  const oledMainText = document.getElementById('oled-main-text');
  const oledTrackLabel = document.getElementById('oled-track-label');

  const KNOB_MENUS = [
    { title: 'TURN: TUNE | OK', subtitle: 'DOOM E1M1 HELLFIRE', trackId: 0 },
    { title: 'KOROBEINIKI 1861', subtitle: 'RUSSIAN FOLK / TETRIS', trackId: 1 },
    { title: 'IMPERIAL MARCH', subtitle: 'GALACTIC EMPIRE THEME', trackId: 2 },
    { title: 'TRIFORCE FANFARE', subtitle: 'HYRULE ARCANE CHIME', trackId: 3 },
    { title: 'ECTO GROOVE', subtitle: 'SPECTRAL PROTON PULSE', trackId: 4 },
    { title: 'DAFT FUNK', subtitle: 'RETRO CYBER VISOR', trackId: 7 },
    { title: 'AI ORACLE READY', subtitle: 'STANDALONE FIRMWARE 42', trackId: -1 },
    { title: 'BATTERY: 4.18V', subtitle: 'STAFF 18650 POWER OK', trackId: -1 },
    { title: 'RADAR: WATCHDOG', subtitle: 'PING: 18ms UDP PORT 5005', trackId: -1 }
  ];
  let menuIndex = 0;

  function rotateKnobStep(direction) {
    knobAngle = (knobAngle + direction * 30) % 360;
    if (yellowKnobEl) {
      yellowKnobEl.style.transform = `rotate(${knobAngle}deg)`;
    }
    playRotaryClick();

    menuIndex = (menuIndex + direction + KNOB_MENUS.length) % KNOB_MENUS.length;
    const item = KNOB_MENUS[menuIndex];

    if (oledMainText) {
      oledMainText.textContent = item.title;
      oledMainText.style.animation = 'none';
      void oledMainText.offsetWidth; // re-trigger animation
      oledMainText.style.animation = 'marquee 5s linear infinite';
    }
    if (oledTrackLabel) {
      oledTrackLabel.textContent = item.subtitle;
    }

    // If it corresponds to a musical track, switch to it if playing
    if (isPlaying && item.trackId >= 0 && item.trackId !== currentTrackIndex) {
      playChiptune(item.trackId);
    }
  }

  if (yellowKnobEl) {
    yellowKnobEl.addEventListener('click', () => rotateKnobStep(1));
    yellowKnobEl.addEventListener('wheel', (e) => {
      e.preventDefault();
      rotateKnobStep(e.deltaY > 0 ? 1 : -1);
    });
  }

  // Clock Ticker in OLED
  function updateClock() {
    const clockEl = document.getElementById('oled-clock');
    if (!clockEl) return;
    const now = new Date();
    const hh = String(now.getHours()).padStart(2, '0');
    const mm = String(now.getMinutes()).padStart(2, '0');
    const ss = String(now.getSeconds()).padStart(2, '0');
    clockEl.textContent = `${hh}:${mm}:${ss}`;
  }
  setInterval(updateClock, 1000);
  updateClock();


  // ============================================================================
  // 5. QUERY / ORACLE BUTTON (HALLOWEEN DOUGLAS ADAMS PROPHECY)
  // ============================================================================
  const queryBtn = document.getElementById('query-btn');
  const HALLOWEEN_PROPHECIES_EN = [
    'The Celestial Staff detects an ectoplasm index of exactly 42 percent. Spooky vibes confirmed.',
    'The answer to the ultimate question of life, the universe, and Halloween is: 42 pumpkins.',
    'Neural oracle reports: a benevolent ghost has possessed the breadboard wiring.',
    'All 43 concentric light runes are aligned. Have a thrilling and joyful Halloween!',
    'Warning: excessive candy detected within a 5-meter radius. All systems nominal.'
  ];

  if (queryBtn) {
    queryBtn.addEventListener('click', () => {
      initAudio();
      audioEnergy = 1.5;

      if (oledMainText) oledMainText.textContent = 'ORACLE: LISTENING...';
      const stateLabel = document.getElementById('oled-state-label');
      if (stateLabel) stateLabel.textContent = 'STATE: QUERY';

      // Pick random prophecy
      const phrase = HALLOWEEN_PROPHECIES_EN[Math.floor(Math.random() * HALLOWEEN_PROPHECIES_EN.length)];

      setTimeout(() => {
        if (oledMainText) oledMainText.textContent = 'ORACLE: 42 SPOOKY';
        if (stateLabel) stateLabel.textContent = 'TTS: SPEAKING';

        // Speak via Web Speech API if supported
        if ('speechSynthesis' in window) {
          window.speechSynthesis.cancel();
          const utterance = new SpeechSynthesisUtterance(phrase);
          utterance.lang = 'en-US';
          utterance.rate = 1.05;
          utterance.pitch = 0.95;
          utterance.onend = () => {
            if (stateLabel) stateLabel.textContent = 'STATE: IDLE';
            if (oledMainText) oledMainText.textContent = 'TURN: TUNE | OK';
          };
          window.speechSynthesis.speak(utterance);
        } else {
          setTimeout(() => {
            if (stateLabel) stateLabel.textContent = 'STATE: IDLE';
            if (oledMainText) oledMainText.textContent = 'TURN: TUNE | OK';
          }, 3000);
        }
      }, 700);
    });
  }


  // ============================================================================
  // 6. TOGGLE ASSEMBLED VS. EXPLODED FLASK VIEW
  // ============================================================================
  const toggleAssembledBtn = document.getElementById('toggle-assembled');
  const toggleExplodedBtn = document.getElementById('toggle-exploded');
  const flaskModeLabel = document.getElementById('flask-mode-label');
  const explodedSpecCard = document.getElementById('exploded-spec-card');

  function setViewMode(mode) {
    viewMode = mode;
    if (mode === 'assembled') {
      if (toggleAssembledBtn) toggleAssembledBtn.classList.add('active');
      if (toggleExplodedBtn) toggleExplodedBtn.classList.remove('active');
      if (flaskModeLabel) flaskModeLabel.textContent = 'ASSEMBLED FLASK // 43 WS2812B';
      if (explodedSpecCard) explodedSpecCard.style.display = 'none';
    } else {
      if (toggleExplodedBtn) toggleExplodedBtn.classList.add('active');
      if (toggleAssembledBtn) toggleAssembledBtn.classList.remove('active');
      if (flaskModeLabel) flaskModeLabel.textContent = 'EXPLODED 3D SCHEMATIC // BLUEPRINT';
      if (explodedSpecCard) explodedSpecCard.style.display = 'block';
    }
  }

  if (toggleAssembledBtn) {
    toggleAssembledBtn.addEventListener('click', () => setViewMode('assembled'));
  }
  if (toggleExplodedBtn) {
    toggleExplodedBtn.addEventListener('click', () => setViewMode('exploded'));
  }


  // ============================================================================
  // 7. CHIPTUNE JUKEBOX BUTTON BINDINGS
  // ============================================================================
  document.querySelectorAll('.music-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const trackId = parseInt(btn.getAttribute('data-track'));
      if (isPlaying && currentTrackIndex === trackId) {
        stopChiptune();
      } else {
        playChiptune(trackId);
      }
    });
  });

})();

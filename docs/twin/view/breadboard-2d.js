/**
 * Celestial Engine Mk.V — Crisp High-DPI 2D RP2350 Pinout & Equipment Wiring Diagram
 * Shows ONLY the actual Celestial Engine Mk.V hardware connected directly to
 * Raspberry Pi Pico 2 W physical pins:
 *  - Center: Raspberry Pi Pico 2 W (RP2350 + CYW43439) with Pins 1..20 (left) & Pins 40..21 (right)
 *  - Left Column:
 *      1. SH1106 1.3" I2C1 OLED Display   -> GP2 (Pin 4), GP3 (Pin 5), 3V3 (Pin 36), GND (Pin 3)
 *      2. PEC11R Rotary Encoder           -> GP6 (Pin 9), GP7 (Pin 10), GP8 (Pin 11), GND (Pin 8)
 *      3. Side QUERY Button (PTT)         -> GP9 (Pin 12), GND (Pin 13)
 *      4. INMP441 I2S Microphone          -> GP10 (Pin 14), GP11 (Pin 15), GP12 (Pin 16), 3V3 (Pin 36)
 *  - Right Column:
 *      5. 43 WS2812B LED Rings (7+12+24)  -> GP15 (Pin 20) via 74AHCT125, VBUS 5V (Pin 40), GND (Pin 38)
 *      6. 3.3V Logic & CYW43439 Wi-Fi     -> 3V3 OUT (Pin 36), UDP Port 5005
 *      7. MAX98357A I2S Amp + 3W Speaker  -> GP16 (Pin 21), GP17 (Pin 22), GP18 (Pin 24), GND (Pin 23)
 */

const PICO_LEFT_PINS = [
  { pin: 1, label: 'GP0' },
  { pin: 2, label: 'GP1' },
  { pin: 3, label: 'GND', type: 'gnd', active: 'oled', color: '#94a3b8' },
  { pin: 4, label: 'GP2 SDA', active: 'oled', color: '#f59e0b' },
  { pin: 5, label: 'GP3 SCL', active: 'oled', color: '#fbbf24' },
  { pin: 6, label: 'GP4' },
  { pin: 7, label: 'GP5' },
  { pin: 8, label: 'GND', type: 'gnd', active: 'encoder', color: '#94a3b8' },
  { pin: 9, label: 'GP6 CLK', active: 'encoder', color: '#a855f7' },
  { pin: 10, label: 'GP7 DT', active: 'encoder', color: '#c084fc' },
  { pin: 11, label: 'GP8 SW', active: 'encoder', color: '#e879f9' },
  { pin: 12, label: 'GP9 PTT', active: 'query', color: '#22c55e' },
  { pin: 13, label: 'GND', type: 'gnd', active: 'query', color: '#94a3b8' },
  { pin: 14, label: 'GP10 SCK', active: 'mic', color: '#fde047' },
  { pin: 15, label: 'GP11 WS', active: 'mic', color: '#facc15' },
  { pin: 16, label: 'GP12 SD', active: 'mic', color: '#eab308' },
  { pin: 17, label: 'GP13' },
  { pin: 18, label: 'GND', type: 'gnd' },
  { pin: 19, label: 'GP14' },
  { pin: 20, label: 'GP15 DIN', active: 'leds', color: '#d946ef' }
];

const PICO_RIGHT_PINS = [
  { pin: 40, label: 'VBUS 5V', type: '5v', active: 'leds', color: '#ef4444' },
  { pin: 39, label: 'VSYS' },
  { pin: 38, label: 'GND', type: 'gnd', active: 'leds', color: '#94a3b8' },
  { pin: 37, label: '3V3_EN' },
  { pin: 36, label: '3V3 OUT', type: '3v3', active: 'wifi', color: '#fb7185' },
  { pin: 35, label: 'ADC_VREF' },
  { pin: 34, label: 'GP28' },
  { pin: 33, label: 'AGND', type: 'gnd' },
  { pin: 32, label: 'GP27' },
  { pin: 31, label: 'GP26' },
  { pin: 30, label: 'RUN' },
  { pin: 29, label: 'GP22' },
  { pin: 28, label: 'GND', type: 'gnd' },
  { pin: 27, label: 'GP21' },
  { pin: 26, label: 'GP20' },
  { pin: 25, label: 'GP19' },
  { pin: 24, label: 'GP18 DIN', active: 'amp', color: '#38bdf8' },
  { pin: 23, label: 'GND', type: 'gnd', active: 'amp', color: '#94a3b8' },
  { pin: 22, label: 'GP17 LRC', active: 'amp', color: '#0ea5e9' },
  { pin: 21, label: 'GP16 BCK', active: 'amp', color: '#7dd3fc' }
];

function drawOrthogonalTrace(ctx, x1, y1, midX, x2, y2, color, width = 2.4, active = true) {
  ctx.save();
  ctx.globalAlpha = active ? 0.98 : 0.12;
  ctx.strokeStyle = '#020617';
  ctx.lineWidth = width + 2.2;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(midX, y1);
  ctx.lineTo(midX, y2);
  ctx.lineTo(x2, y2);
  ctx.stroke();

  ctx.strokeStyle = color;
  ctx.lineWidth = active ? width : 1.5;
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(midX, y1);
  ctx.lineTo(midX, y2);
  ctx.lineTo(x2, y2);
  ctx.stroke();

  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(x1, y1, active ? 3.0 : 2.0, 0, Math.PI * 2);
  ctx.arc(x2, y2, active ? 3.0 : 2.0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

export function renderFritzingBreadboard2D(ctx, w, h, options = {}) {
  const {
    circuitMode = 'all', // 'all' | 'leds' | 'oled' | 'controls' | 'audio'
    selectedId = 'pico',
    ledNodes = [],
    staffState = 0
  } = options;

  const hitBoxes = [];
  ctx.save();

  // Determine which component(s) are highlighted vs dimmed:
  // - If user clicked a specific peripheral (selectedId !== 'pico'), dim all other peripherals!
  // - Or if a subsystem filter tab (circuitMode !== 'all') is active, highlight that subsystem.
  const isModuleHighlighted = (modId, groupName) => {
    if (selectedId && selectedId !== 'pico') {
      return selectedId === modId;
    }
    if (circuitMode !== 'all') {
      return circuitMode === groupName;
    }
    return true;
  };

  // 1. Clean Dark Engineering Blueprint Background
  ctx.fillStyle = '#060a12';
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = 'rgba(56, 189, 248, 0.04)';
  ctx.lineWidth = 1;
  for (let gx = 0; gx < w; gx += 24) {
    ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, h); ctx.stroke();
  }
  for (let gy = 0; gy < h; gy += 24) {
    ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(w, gy); ctx.stroke();
  }

  const scale = Math.min(w / 720, h / 460);
  const offsetX = (w - 720 * scale) / 2;
  const offsetY = (h - 460 * scale) / 2;
  ctx.translate(offsetX, offsetY);
  ctx.scale(scale, scale);

  const registerHit = (id, rx, ry, rw, rh) => {
    hitBoxes.push({
      id,
      x: offsetX + rx * scale,
      y: offsetY + ry * scale,
      w: rw * scale,
      h: rh * scale
    });
  };

  // Top Header Title
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 11.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'center';
  const titles = {
    all: 'RP2350 DIRECT PINOUT MAP — CLICK ANY MODULE TO HIGHLIGHT ITS PINS',
    leds: 'PINOUT GROUP: 43 WS2812B LED RINGS + 74AHCT125 LEVEL SHIFTER (GP15 / VBUS 5V)',
    oled: 'PINOUT GROUP: SH1106 1.3" I2C1 OLED DISPLAY (GP2 SDA • GP3 SCL • 3.3V)',
    controls: 'PINOUT GROUP: PEC11R ROTARY ENCODER (GP6/7/8) & SIDE QUERY BUTTON (GP9)',
    audio: 'PINOUT GROUP: INMP441 I2S MIC (GP10/11/12) & MAX98357A AMP (GP16/17/18)'
  };
  ctx.fillText(titles[circuitMode] || titles.all, 360, 20);

  // ==========================================================================
  // 2. CENTER: RASPBERRY PI PICO 2 W (RP2350) BOARD (Wide enough for zero text overlap!)
  // ==========================================================================
  const picoW = 184;
  const picoH = 382;
  const picoX = 360 - picoW / 2; // 268..452
  const picoY = 38;

  ctx.save();
  ctx.fillStyle = '#095c32';
  ctx.strokeStyle = selectedId === 'pico' ? '#38bdf8' : '#10b981';
  ctx.lineWidth = selectedId === 'pico' ? 2.6 : 1.6;
  ctx.beginPath();
  ctx.roundRect(picoX, picoY, picoW, picoH, 8);
  ctx.fill();
  ctx.stroke();

  // Top Micro-USB Port
  ctx.fillStyle = '#cbd5e1';
  ctx.strokeStyle = '#475569';
  ctx.lineWidth = 1.2;
  ctx.beginPath();
  ctx.roundRect(360 - 18, picoY - 5, 36, 12, 2);
  ctx.fill();
  ctx.stroke();

  // Board Title cleanly in the top center channel above the chips
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'center';
  ctx.fillText('Pico 2 W', 360, picoY + 22);

  // Center Spine: RP2350 Dual-Core Processor (narrowed to 42px so it never touches pin labels!)
  ctx.fillStyle = '#0f172a';
  ctx.strokeStyle = '#334155';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.roundRect(360 - 21, picoY + 132, 42, 42, 3);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 7.2px "JetBrains Mono", monospace';
  ctx.fillText('RP2350', 360, picoY + 151);
  ctx.fillStyle = '#94a3b8';
  ctx.font = '5.8px "JetBrains Mono", monospace';
  ctx.fillText('M33', 360, picoY + 162);

  // Center Bottom Spine: CYW43439 Wi-Fi Shield (narrowed to 42px so it never touches pin labels!)
  ctx.fillStyle = '#cbd5e1';
  ctx.strokeStyle = '#64748b';
  ctx.beginPath();
  ctx.roundRect(360 - 21, picoY + 258, 42, 54, 3);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#0f172a';
  ctx.font = 'bold 6.2px "JetBrains Mono", monospace';
  ctx.fillText('CYW43', 360, picoY + 282);
  ctx.font = '5.5px "JetBrains Mono", monospace';
  ctx.fillText('Wi-Fi', 360, picoY + 294);
  ctx.restore();

  const pinStartY = picoY + 22;
  const pinSpacingY = 17.8;
  const leftPinCoords = {};
  const rightPinCoords = {};

  // Draw Left Header Pins (1..20)
  PICO_LEFT_PINS.forEach((p, idx) => {
    const py = pinStartY + idx * pinSpacingY;
    const px = picoX + 6;
    leftPinCoords[p.pin] = { x: picoX, y: py };

    const pinHighlighted = p.active
      ? isModuleHighlighted(p.active, p.active === 'oled' ? 'oled' : (p.active === 'encoder' || p.active === 'query' ? 'controls' : (p.active === 'mic' ? 'audio' : 'leds')))
      : (selectedId === 'pico' && circuitMode === 'all');

    ctx.save();
    ctx.globalAlpha = pinHighlighted ? 1.0 : (p.active ? 0.28 : 0.35);
    ctx.fillStyle = p.active ? p.color : (p.type === 'gnd' ? '#64748b' : '#facc15');
    ctx.beginPath();
    ctx.arc(px, py, p.active && pinHighlighted ? 3.6 : 2.8, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = p.active ? '#ffffff' : '#cbd5e1';
    ctx.font = p.active ? 'bold 7px "JetBrains Mono", monospace' : '6px "JetBrains Mono", monospace';
    ctx.textAlign = 'left';
    ctx.fillText(`${String(p.pin).padStart(2, ' ')} ${p.label}`, px + 6, py + 2.4);
    ctx.restore();
  });

  // Draw Right Header Pins (40..21)
  PICO_RIGHT_PINS.forEach((p, idx) => {
    const py = pinStartY + idx * pinSpacingY;
    const px = picoX + picoW - 6;
    rightPinCoords[p.pin] = { x: picoX + picoW, y: py };

    const pinHighlighted = p.active
      ? isModuleHighlighted(p.active, p.active === 'leds' ? 'leds' : (p.active === 'amp' ? 'audio' : 'all'))
      : (selectedId === 'pico' && circuitMode === 'all');

    ctx.save();
    ctx.globalAlpha = pinHighlighted ? 1.0 : (p.active ? 0.28 : 0.35);
    ctx.fillStyle = p.active ? p.color : (p.type === 'gnd' ? '#64748b' : '#facc15');
    ctx.beginPath();
    ctx.arc(px, py, p.active && pinHighlighted ? 3.6 : 2.8, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = p.active ? '#ffffff' : '#cbd5e1';
    ctx.font = p.active ? 'bold 7px "JetBrains Mono", monospace' : '6px "JetBrains Mono", monospace';
    ctx.textAlign = 'right';
    ctx.fillText(`${p.label} ${String(p.pin).padStart(2, ' ')}`, px - 6, py + 2.4);
    ctx.restore();
  });

  registerHit('pico', picoX, picoY, picoW, picoH);

  // ==========================================================================
  // 3. LEFT COLUMN EQUIPMENT CARDS (OLED, ENCODER, QUERY BUTTON, INMP441 MIC)
  // ==========================================================================
  const leftCardX = 14;
  const leftCardW = 202;

  // --- CARD L1: SH1106 1.3" I2C1 OLED Display ---
  const oledY = 38;
  const oledH = 98;
  const oledActive = isModuleHighlighted('oled', 'oled');
  ctx.save();
  ctx.globalAlpha = oledActive ? 1.0 : 0.20;
  ctx.fillStyle = '#0c274e';
  ctx.strokeStyle = selectedId === 'oled' ? '#38bdf8' : '#f59e0b';
  ctx.lineWidth = selectedId === 'oled' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, oledY, leftCardW, oledH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#020617';
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 1;
  ctx.fillRect(leftCardX + 10, oledY + 26, 76, 48);
  ctx.strokeRect(leftCardX + 10, oledY + 26, 76, 48);
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 6.2px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('SH1106 128x64', leftCardX + 14, oledY + 39);
  ctx.fillStyle = '#7dd3fc';
  ctx.font = '5.8px "JetBrains Mono", monospace';
  ctx.fillText('I2C1 0x3C', leftCardX + 14, oledY + 51);
  ctx.fillText(staffState === 1 ? '● REC MIC' : 'AI ORACLE', leftCardX + 14, oledY + 63);

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.fillText('④ 1.3" SH1106 OLED', leftCardX + 10, oledY + 16);

  const oledPins = [
    { name: 'GND -> Pin 3', y: oledY + 32, picoPin: 3, side: 'left', color: '#94a3b8', midX: 248 },
    { name: 'SDA -> GP2 (Pin 4)', y: oledY + 50, picoPin: 4, side: 'left', color: '#f59e0b', midX: 238 },
    { name: 'SCL -> GP3 (Pin 5)', y: oledY + 68, picoPin: 5, side: 'left', color: '#fbbf24', midX: 228 },
    { name: 'VCC -> 3V3 (Pin 36)', y: oledY + 84, picoPin: 36, side: '3v3_note', color: '#fb7185', midX: 220 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  oledPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    if (t.side === 'left') {
      const target = leftPinCoords[t.picoPin];
      drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.3, oledActive);
    }
  });
  ctx.restore();
  registerHit('oled', leftCardX, oledY, leftCardW, oledH);

  // --- CARD L2: Bourns PEC11R Rotary Encoder ---
  const encY = 148;
  const encH = 100;
  const encActive = isModuleHighlighted('encoder', 'controls');
  ctx.save();
  ctx.globalAlpha = encActive ? 1.0 : 0.20;
  ctx.fillStyle = '#131c31';
  ctx.strokeStyle = selectedId === 'encoder' ? '#38bdf8' : '#a855f7';
  ctx.lineWidth = selectedId === 'encoder' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, encY, leftCardW, encH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑤ PEC11R Rotary Encoder', leftCardX + 10, encY + 16);

  ctx.fillStyle = '#334155';
  ctx.strokeStyle = '#94a3b8';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(leftCardX + 36, encY + 56, 20, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#e2e8f0';
  ctx.beginPath();
  ctx.arc(leftCardX + 36, encY + 56, 8, 0, Math.PI * 2);
  ctx.fill();

  const encPins = [
    { name: 'GND -> Pin 8', y: encY + 32, picoPin: 8, color: '#94a3b8', midX: 252 },
    { name: 'CLK -> GP6 (Pin 9)', y: encY + 50, picoPin: 9, color: '#a855f7', midX: 242 },
    { name: 'DT  -> GP7 (Pin 10)', y: encY + 68, picoPin: 10, color: '#c084fc', midX: 232 },
    { name: 'SW  -> GP8 (Pin 11)', y: encY + 86, picoPin: 11, color: '#e879f9', midX: 222 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  encPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.3, encActive);
  });
  ctx.restore();
  registerHit('encoder', leftCardX, encY, leftCardW, encH);

  // --- CARD L3: Side QUERY Button (PTT) ---
  const qY = 260;
  const qH = 74;
  const qActive = isModuleHighlighted('query', 'controls');
  ctx.save();
  ctx.globalAlpha = qActive ? 1.0 : 0.20;
  ctx.fillStyle = '#0f291e';
  ctx.strokeStyle = selectedId === 'query' ? '#38bdf8' : '#22c55e';
  ctx.lineWidth = selectedId === 'query' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, qY, leftCardW, qH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑥ Side QUERY Button (PTT)', leftCardX + 10, qY + 16);

  ctx.fillStyle = '#22c55e';
  ctx.strokeStyle = '#bbf7d0';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(leftCardX + 34, qY + 45, 13, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();

  const qPins = [
    { name: 'PTT -> GP9 (Pin 12)', y: qY + 36, picoPin: 12, color: '#22c55e', midX: 242 },
    { name: 'GND -> Pin 13', y: qY + 56, picoPin: 13, color: '#94a3b8', midX: 252 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  qPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.3, qActive);
  });
  ctx.restore();
  registerHit('query', leftCardX, qY, leftCardW, qH);

  // --- CARD L4: INMP441 I2S Microphone ---
  const micY = 346;
  const micH = 82;
  const micActive = isModuleHighlighted('mic', 'audio');
  ctx.save();
  ctx.globalAlpha = micActive ? 1.0 : 0.20;
  ctx.fillStyle = '#261e0a';
  ctx.strokeStyle = selectedId === 'mic' ? '#38bdf8' : '#eab308';
  ctx.lineWidth = selectedId === 'mic' ? 2.6 : 1.6;
  ctx.beginPath();
  ctx.roundRect(leftCardX, micY, leftCardW, micH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#fef08a';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑦ INMP441 I2S Microphone', leftCardX + 10, micY + 16);

  ctx.fillStyle = '#eab308';
  ctx.strokeStyle = staffState === 1 ? '#ef4444' : '#fde047';
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  ctx.arc(leftCardX + 34, micY + 48, 16, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#0f172a';
  ctx.beginPath();
  ctx.arc(leftCardX + 34, micY + 48, 7, 0, Math.PI * 2);
  ctx.fill();

  const micPins = [
    { name: 'SCK -> GP10 (Pin 14)', y: micY + 32, picoPin: 14, color: '#fde047', midX: 232 },
    { name: 'WS  -> GP11 (Pin 15)', y: micY + 50, picoPin: 15, color: '#facc15', midX: 242 },
    { name: 'SD  -> GP12 (Pin 16)', y: micY + 68, picoPin: 16, color: '#eab308', midX: 252 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  micPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.3, micActive);
  });
  ctx.restore();
  registerHit('mic', leftCardX, micY, leftCardW, micH);

  // ==========================================================================
  // 4. RIGHT COLUMN EQUIPMENT CARDS (43 LED RINGS, 3.3V/WIFI, MAX98357A AMP)
  // ==========================================================================
  const rightCardX = 504;
  const rightCardW = 202;

  // --- CARD R1: 43 Concentric WS2812B Rings + SN74AHCT125N ---
  const ledY = 38;
  const ledH = 142;
  const ledsActive = isModuleHighlighted('leds', 'leds');
  ctx.save();
  ctx.globalAlpha = ledsActive ? 1.0 : 0.20;
  ctx.fillStyle = '#141829';
  ctx.strokeStyle = selectedId === 'leds' ? '#38bdf8' : '#d946ef';
  ctx.lineWidth = selectedId === 'leds' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, ledY, rightCardW, ledH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('② 43 WS2812B Rings (7+12+24)', rightCardX + 10, ledY + 16);
  ctx.fillStyle = '#e879f9';
  ctx.font = '6.2px "JetBrains Mono", monospace';
  ctx.fillText('+ SN74AHCT125N Level Shifter & 1000µF', rightCardX + 10, ledY + 27);

  const ringCx = rightCardX + 160;
  const ringCy = ledY + 86;
  [28, 19, 9].forEach((r) => {
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.35)';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(ringCx, ringCy, r, 0, Math.PI * 2);
    ctx.stroke();
  });
  for (let d = 0; d < 12; d++) {
    const a = (d / 12) * Math.PI * 2;
    const node = ledNodes[d * 3];
    ctx.fillStyle = node ? `rgb(${node.r},${node.g},${node.b})` : '#38bdf8';
    ctx.beginPath();
    ctx.arc(ringCx + Math.cos(a) * 28, ringCy + Math.sin(a) * 28, 2.2, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.fillStyle = '#fde047';
  ctx.beginPath();
  ctx.arc(ringCx, ringCy, 3, 0, Math.PI * 2);
  ctx.fill();

  const ledPins = [
    { name: 'Pin 40 (VBUS 5V) <- 5V VCC', y: ledY + 48, picoPin: 40, side: 'right', color: '#ef4444', midX: 468 },
    { name: 'Pin 38 (GND)     <- Star GND', y: ledY + 74, picoPin: 38, side: 'right', color: '#94a3b8', midX: 478 },
    { name: 'Pin 20 (GP15)    <- DIN (PIO0)', y: ledY + 118, picoPin: 20, side: 'left_bottom', color: '#d946ef', midX: 488 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ledPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, rightCardX + 8, t.y + 2);
    if (t.side === 'right') {
      const target = rightPinCoords[t.picoPin];
      drawOrthogonalTrace(ctx, rightCardX, t.y, t.midX, target.x, target.y, t.color, 2.3, ledsActive);
    } else {
      const target = leftPinCoords[20];
      ctx.save();
      ctx.globalAlpha = ledsActive ? 0.98 : 0.12;
      ctx.strokeStyle = '#d946ef';
      ctx.lineWidth = ledsActive ? 2.3 : 1.5;
      ctx.beginPath();
      ctx.moveTo(target.x, target.y);
      ctx.lineTo(target.x - 10, target.y);
      ctx.lineTo(target.x - 10, picoY + picoH + 10);
      ctx.lineTo(488, picoY + picoH + 10);
      ctx.lineTo(488, t.y);
      ctx.lineTo(rightCardX, t.y);
      ctx.stroke();
      ctx.restore();
    }
  });
  ctx.restore();
  registerHit('leds', rightCardX, ledY, rightCardW, ledH);

  // --- CARD R2: 3.3V Logic Rail & CYW43439 Wi-Fi ---
  const netY = 194;
  const netH = 92;
  const wifiActive = isModuleHighlighted('wifi', 'all');
  ctx.save();
  ctx.globalAlpha = wifiActive ? 1.0 : 0.20;
  ctx.fillStyle = '#0d2229';
  ctx.strokeStyle = selectedId === 'wifi' ? '#38bdf8' : '#10b981';
  ctx.lineWidth = selectedId === 'wifi' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, netY, rightCardW, netH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('📡 3.3V Rail & CYW43439 Wi-Fi', rightCardX + 10, netY + 18);
  ctx.fillStyle = '#6ee7b7';
  ctx.font = '6.5px "JetBrains Mono", monospace';
  ctx.fillText('• Pin 36 (3V3 OUT): OLED + INMP441', rightCardX + 10, netY + 36);
  ctx.fillText('• UDP v2 Port 5005 (0x811C9DC5)', rightCardX + 10, netY + 52);
  ctx.fillText('• Dual-Brain: Pixel 9 Pro / Pi 5', rightCardX + 10, netY + 66);
  ctx.fillStyle = '#fb7185';
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.fillText('Pin 36 (3V3) <- Logic Rail', rightCardX + 8, netY + 82);
  const p36 = rightPinCoords[36];
  drawOrthogonalTrace(ctx, rightCardX, netY + 80, 468, p36.x, p36.y, '#fb7185', 2.2, wifiActive);
  ctx.restore();
  registerHit('wifi', rightCardX, netY, rightCardW, netH);

  // --- CARD R3: MAX98357A I2S Amp + 4Ω 3W Speaker ---
  const ampY = 298;
  const ampH = 130;
  const ampActive = isModuleHighlighted('amp', 'audio');
  ctx.save();
  ctx.globalAlpha = ampActive ? 1.0 : 0.20;
  ctx.fillStyle = '#1b1636';
  ctx.strokeStyle = selectedId === 'amp' ? '#38bdf8' : '#38bdf8';
  ctx.lineWidth = selectedId === 'amp' ? 2.6 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, ampY, rightCardW, ampH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑧ MAX98357A Amp + 3W Speaker', rightCardX + 10, ampY + 16);
  ctx.fillStyle = '#93c5fd';
  ctx.font = '6.2px "JetBrains Mono", monospace';
  ctx.fillText('PIO1 SM0 • 16kHz PCM + 24 Chiptunes', rightCardX + 10, ampY + 27);

  const spkCx = rightCardX + 166;
  const spkCy = ampY + 80;
  ctx.fillStyle = '#0f172a';
  ctx.strokeStyle = '#64748b';
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  ctx.arc(spkCx, spkCy, 20, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#334155';
  ctx.beginPath();
  ctx.arc(spkCx, spkCy, 7, 0, Math.PI * 2);
  ctx.fill();

  const ampPins = [
    { name: 'Pin 24 (GP18) <- DIN', y: ampY + 48, picoPin: 24, color: '#38bdf8', midX: 466 },
    { name: 'Pin 23 (GND)  <- GND', y: ampY + 70, picoPin: 23, color: '#94a3b8', midX: 474 },
    { name: 'Pin 22 (GP17) <- LRC', y: ampY + 92, picoPin: 22, color: '#0ea5e9', midX: 482 },
    { name: 'Pin 21 (GP16) <- BCK', y: ampY + 114, picoPin: 21, color: '#7dd3fc', midX: 490 }
  ];
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ampPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, rightCardX + 8, t.y + 2);
    const target = rightPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, rightCardX, t.y, t.midX, target.x, target.y, t.color, 2.3, ampActive);
  });
  ctx.restore();
  registerHit('amp', rightCardX, ampY, rightCardW, ampH);

  ctx.restore();
  return hitBoxes;
}

if (typeof window !== 'undefined') {
  window.__celestialRenderBreadboard2D = renderFritzingBreadboard2D;
}

/**
 * Celestial Engine Mk.V — Clean 2D RP2350 Pinout & Equipment Wiring Diagram
 * Shows ONLY the actual Celestial Engine Mk.V hardware connected directly to
 * Raspberry Pi Pico 2 W physical pins (no breadboard clutter, no extra LEDs):
 *  - Center: Raspberry Pi Pico 2 W (RP2350 + CYW43439) with Pins 1..20 (left) & Pins 40..21 (right)
 *  - Left Side:
 *      1. SH1106 1.3" 128x64 I2C1 OLED Display -> GP2 (Pin 4), GP3 (Pin 5), 3V3 (Pin 36), GND (Pin 3)
 *      2. Bourns PEC11R Rotary Encoder         -> GP6 (Pin 9), GP7 (Pin 10), GP8 (Pin 11), GND (Pin 8)
 *      3. Green Side QUERY Button (PTT)        -> GP9 (Pin 12), GND (Pin 13)
 *  - Right Side:
 *      4. 43 WS2812B LED Rings (7+12+24)       -> GP15 (Pin 20) via 74AHCT125, VBUS 5V (Pin 40), GND (Pin 38)
 *      5. Yellow INMP441 I2S Microphone        -> GP10 (Pin 14), GP11 (Pin 15), GP12 (Pin 16), 3V3 (Pin 36)
 *      6. MAX98357A I2S Amp + 4Ω 3W Speaker    -> GP16 (Pin 21), GP17 (Pin 22), GP18 (Pin 24), VBUS 5V (Pin 40)
 */

// Physical Pico 2 W pin table (Left: 1..20, Right: 40..21)
const PICO_LEFT_PINS = [
  { pin: 1, label: 'GP0' },
  { pin: 2, label: 'GP1' },
  { pin: 3, label: 'GND', type: 'gnd' },
  { pin: 4, label: 'GP2 (SDA)', active: 'oled', color: '#f59e0b' },
  { pin: 5, label: 'GP3 (SCL)', active: 'oled', color: '#fbbf24' },
  { pin: 6, label: 'GP4' },
  { pin: 7, label: 'GP5' },
  { pin: 8, label: 'GND', type: 'gnd', active: 'encoder', color: '#64748b' },
  { pin: 9, label: 'GP6 (CLK)', active: 'encoder', color: '#a855f7' },
  { pin: 10, label: 'GP7 (DT)', active: 'encoder', color: '#c084fc' },
  { pin: 11, label: 'GP8 (SW)', active: 'encoder', color: '#e879f9' },
  { pin: 12, label: 'GP9 (PTT)', active: 'query', color: '#22c55e' },
  { pin: 13, label: 'GND', type: 'gnd', active: 'query', color: '#64748b' },
  { pin: 14, label: 'GP10 (SCK)', active: 'mic', color: '#fde047' },
  { pin: 15, label: 'GP11 (WS)', active: 'mic', color: '#facc15' },
  { pin: 16, label: 'GP12 (SD)', active: 'mic', color: '#eab308' },
  { pin: 17, label: 'GP13' },
  { pin: 18, label: 'GND', type: 'gnd' },
  { pin: 19, label: 'GP14' },
  { pin: 20, label: 'GP15 (DIN)', active: 'leds', color: '#d946ef' }
];

const PICO_RIGHT_PINS = [
  { pin: 40, label: 'VBUS (5V)', type: '5v', active: 'leds', color: '#ef4444' },
  { pin: 39, label: 'VSYS' },
  { pin: 38, label: 'GND', type: 'gnd', active: 'leds', color: '#64748b' },
  { pin: 37, label: '3V3_EN' },
  { pin: 36, label: '3V3 (OUT)', type: '3v3', active: 'oled', color: '#fb7185' },
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
  { pin: 24, label: 'GP18 (DIN)', active: 'amp', color: '#38bdf8' },
  { pin: 23, label: 'GND', type: 'gnd', active: 'amp', color: '#64748b' },
  { pin: 22, label: 'GP17 (LRC)', active: 'amp', color: '#0ea5e9' },
  { pin: 21, label: 'GP16 (BCK)', active: 'amp', color: '#7dd3fc' }
];

function drawOrthogonalTrace(ctx, x1, y1, midX, x2, y2, color, width = 2.2, dimmed = false) {
  ctx.save();
  ctx.globalAlpha = dimmed ? 0.16 : 0.95;
  ctx.strokeStyle = '#020617';
  ctx.lineWidth = width + 2;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(midX, y1);
  ctx.lineTo(midX, y2);
  ctx.lineTo(x2, y2);
  ctx.stroke();

  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(midX, y1);
  ctx.lineTo(midX, y2);
  ctx.lineTo(x2, y2);
  ctx.stroke();

  // Terminal dots
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(x1, y1, 2.6, 0, Math.PI * 2);
  ctx.arc(x2, y2, 2.6, 0, Math.PI * 2);
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

  // 1. Clean Dark Engineering Blueprint Background
  ctx.fillStyle = '#070b14';
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = 'rgba(56, 189, 248, 0.045)';
  ctx.lineWidth = 1;
  for (let gx = 0; gx < w; gx += 24) {
    ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, h); ctx.stroke();
  }
  for (let gy = 0; gy < h; gy += 24) {
    ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(w, gy); ctx.stroke();
  }

  // Responsive scale so the 700x460 coordinate layout fits any canvas cleanly
  const scale = Math.min(w / 700, h / 460);
  const offsetX = (w - 700 * scale) / 2;
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

  const isGroupActive = (group) => {
    if (circuitMode === 'all') return true;
    return circuitMode === group;
  };

  // Title Header
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 11px "JetBrains Mono", monospace';
  ctx.textAlign = 'center';
  const titles = {
    all: 'RP2350 DIRECT PINOUT MAP — ALL CELESTIAL ENGINE Mk.V EQUIPMENT',
    leds: 'PINOUT GROUP 1: 43 WS2812B LED RINGS + 74AHCT125 LEVEL SHIFTER (GP15 / VBUS 5V)',
    oled: 'PINOUT GROUP 2: SH1106 1.3" I2C1 OLED DISPLAY (GP2 SDA • GP3 SCL • 3.3V)',
    controls: 'PINOUT GROUP 3: PEC11R ROTARY ENCODER (GP6/7/8) & GREEN QUERY BUTTON (GP9)',
    audio: 'PINOUT GROUP 4: YELLOW INMP441 I2S MIC (GP10/11/12) & MAX98357A AMP (GP16/17/18)'
  };
  ctx.fillText(titles[circuitMode] || titles.all, 350, 20);

  // ==========================================================================
  // 2. CENTER: RASPBERRY PI PICO 2 W (RP2350) VERTICAL PINOUT BOARD
  // ==========================================================================
  const picoW = 148;
  const picoH = 368;
  const picoX = 350 - picoW / 2; // 276..424
  const picoY = 42;

  ctx.save();
  ctx.fillStyle = '#0b6b3a';
  ctx.strokeStyle = selectedId === 'pico' ? '#38bdf8' : '#10b981';
  ctx.lineWidth = selectedId === 'pico' ? 2.5 : 1.5;
  ctx.beginPath();
  ctx.roundRect(picoX, picoY, picoW, picoH, 8);
  ctx.fill();
  ctx.stroke();

  // Top Micro-USB Connector
  ctx.fillStyle = '#cbd5e1';
  ctx.strokeStyle = '#475569';
  ctx.lineWidth = 1.2;
  ctx.beginPath();
  ctx.roundRect(350 - 18, picoY - 6, 36, 14, 2);
  ctx.fill();
  ctx.stroke();

  // Onboard Status LED (GP25 / WL_GPIO0)
  ctx.fillStyle = '#4ade80';
  ctx.shadowColor = '#22c55e';
  ctx.shadowBlur = 8;
  ctx.beginPath();
  ctx.arc(picoX + 34, picoY + 18, 3.5, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;

  // BOOTSEL Button
  ctx.fillStyle = '#f8fafc';
  ctx.strokeStyle = '#64748b';
  ctx.fillRect(350 + 14, picoY + 26, 16, 11);
  ctx.strokeRect(350 + 14, picoY + 26, 16, 11);
  ctx.fillStyle = '#cbd5e1';
  ctx.font = '5.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'center';
  ctx.fillText('BOOT', 350 + 22, picoY + 45);

  // RP2350 Dual-Core Processor Package in Center
  ctx.fillStyle = '#0f172a';
  ctx.strokeStyle = '#334155';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.roundRect(350 - 22, picoY + 128, 44, 44, 3);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 7.5px "JetBrains Mono", monospace';
  ctx.fillText('RP2350', 350, picoY + 148);
  ctx.fillStyle = '#94a3b8';
  ctx.font = '5.8px "JetBrains Mono", monospace';
  ctx.fillText('M33 Dual', 350, picoY + 159);

  // CYW43439 Wi-Fi / BLE Shield at Bottom of Pico 2 W
  ctx.fillStyle = '#cbd5e1';
  ctx.strokeStyle = '#64748b';
  ctx.beginPath();
  ctx.roundRect(350 - 26, picoY + 258, 52, 64, 3);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#0f172a';
  ctx.font = 'bold 6.8px "JetBrains Mono", monospace';
  ctx.fillText('CYW43439', 350, picoY + 286);
  ctx.font = '5.8px "JetBrains Mono", monospace';
  ctx.fillText('Wi-Fi UDP', 350, picoY + 298);

  // Vertical Board Title
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.fillText('Raspberry Pi', 350, picoY + 95);
  ctx.fillText('Pico 2 W', 350, picoY + 107);
  ctx.restore();

  // Compute Y coordinates for the 20 pins on each side
  const pinStartY = picoY + 22;
  const pinSpacingY = 17.2;
  const leftPinCoords = {};
  const rightPinCoords = {};

  // Draw Left Header Pins (1..20)
  PICO_LEFT_PINS.forEach((p, idx) => {
    const py = pinStartY + idx * pinSpacingY;
    const px = picoX + 6;
    leftPinCoords[p.pin] = { x: picoX, y: py };

    // Gold castellated pad
    ctx.fillStyle = p.active ? p.color : (p.type === 'gnd' ? '#475569' : '#facc15');
    ctx.beginPath();
    ctx.arc(px, py, 3.2, 0, Math.PI * 2);
    ctx.fill();

    // Pin number & label inside board
    ctx.fillStyle = p.active ? '#ffffff' : 'rgba(226, 232, 240, 0.55)';
    ctx.font = p.active ? 'bold 6.5px "JetBrains Mono", monospace' : '5.8px "JetBrains Mono", monospace';
    ctx.textAlign = 'left';
    ctx.fillText(`${String(p.pin).padStart(2, ' ')} ${p.label}`, px + 6, py + 2.2);
  });

  // Draw Right Header Pins (40..21)
  PICO_RIGHT_PINS.forEach((p, idx) => {
    const py = pinStartY + idx * pinSpacingY;
    const px = picoX + picoW - 6;
    rightPinCoords[p.pin] = { x: picoX + picoW, y: py };

    ctx.fillStyle = p.active ? p.color : (p.type === 'gnd' ? '#475569' : (p.type === '5v' || p.type === '3v3' ? '#ef4444' : '#facc15'));
    ctx.beginPath();
    ctx.arc(px, py, 3.2, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = p.active ? '#ffffff' : 'rgba(226, 232, 240, 0.55)';
    ctx.font = p.active ? 'bold 6.5px "JetBrains Mono", monospace' : '5.8px "JetBrains Mono", monospace';
    ctx.textAlign = 'right';
    ctx.fillText(`${p.label} ${String(p.pin).padStart(2, ' ')}`, px - 6, py + 2.2);
  });

  registerHit('pico', picoX, picoY, picoW, picoH);

  // ==========================================================================
  // 3. LEFT COLUMN EQUIPMENT CARDS (OLED, ENCODER, GREEN QUERY BUTTON)
  // ==========================================================================
  const leftCardX = 16;
  const leftCardW = 198;

  // --- CARD L1: SH1106 1.3" I2C1 OLED Display (y: 44..148) ---
  const oledY = 44;
  const oledH = 104;
  const oledActive = isGroupActive('oled');
  ctx.save();
  ctx.globalAlpha = oledActive ? 1.0 : 0.25;
  ctx.fillStyle = '#0c2d5e';
  ctx.strokeStyle = selectedId === 'oled' ? '#38bdf8' : '#f59e0b';
  ctx.lineWidth = selectedId === 'oled' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, oledY, leftCardW, oledH, 7);
  ctx.fill();
  ctx.stroke();

  // Active OLED screen preview on left of card
  ctx.fillStyle = '#020617';
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 1;
  ctx.fillRect(leftCardX + 10, oledY + 26, 86, 52);
  ctx.strokeRect(leftCardX + 10, oledY + 26, 86, 52);
  ctx.fillStyle = '#38bdf8';
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('CELESTIAL ENGINE', leftCardX + 14, oledY + 39);
  ctx.fillStyle = '#7dd3fc';
  ctx.font = '6px "JetBrains Mono", monospace';
  ctx.fillText('I2C1 0x3C 400kHz', leftCardX + 14, oledY + 51);
  ctx.fillText(staffState === 1 ? 'STATE: ● REC MIC' : 'MODE: AI ORACLE', leftCardX + 14, oledY + 63);

  // OLED Title & Pin Terminals on right edge of card
  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.fillText('④ 1.3" SH1106 OLED', leftCardX + 10, oledY + 16);

  const oledPins = [
    { name: 'GND -> Pin 3', y: oledY + 32, picoPin: 3, side: 'left', color: '#64748b', midX: 256 },
    { name: 'SDA -> GP2 (Pin 4)', y: oledY + 50, picoPin: 4, side: 'left', color: '#f59e0b', midX: 244 },
    { name: 'SCL -> GP3 (Pin 5)', y: oledY + 68, picoPin: 5, side: 'left', color: '#fbbf24', midX: 234 },
    { name: 'VCC -> 3V3 (Pin 36)', y: oledY + 86, picoPin: 36, side: 'left_3v3', color: '#fb7185', midX: 224 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  oledPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    if (t.side === 'left') {
      const target = leftPinCoords[t.picoPin];
      drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.2, !oledActive);
    }
  });
  ctx.restore();
  registerHit('oled', leftCardX, oledY, leftCardW, oledH);

  // --- CARD L2: Bourns PEC11R Rotary Encoder (y: 164..268) ---
  const encY = 164;
  const encH = 104;
  const ctrlActive = isGroupActive('controls');
  ctx.save();
  ctx.globalAlpha = ctrlActive ? 1.0 : 0.25;
  ctx.fillStyle = '#131c31';
  ctx.strokeStyle = selectedId === 'encoder' ? '#38bdf8' : '#a855f7';
  ctx.lineWidth = selectedId === 'encoder' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, encY, leftCardW, encH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑤ PEC11R Rotary Encoder', leftCardX + 10, encY + 16);

  // Encoder dial icon
  ctx.fillStyle = '#334155';
  ctx.strokeStyle = '#94a3b8';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(leftCardX + 38, encY + 58, 22, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#e2e8f0';
  ctx.beginPath();
  ctx.arc(leftCardX + 38, encY + 58, 9, 0, Math.PI * 2);
  ctx.fill();

  const encPins = [
    { name: 'GND -> Pin 8', y: encY + 34, picoPin: 8, color: '#64748b', midX: 258 },
    { name: 'CLK -> GP6 (Pin 9)', y: encY + 52, picoPin: 9, color: '#a855f7', midX: 248 },
    { name: 'DT  -> GP7 (Pin 10)', y: encY + 70, picoPin: 10, color: '#c084fc', midX: 238 },
    { name: 'SW  -> GP8 (Pin 11)', y: encY + 88, picoPin: 11, color: '#e879f9', midX: 228 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  encPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.2, !ctrlActive);
  });
  ctx.restore();
  registerHit('encoder', leftCardX, encY, leftCardW, encH);

  // --- CARD L3: Green Side QUERY Button (y: 284..364) ---
  const qY = 284;
  const qH = 74;
  ctx.save();
  ctx.globalAlpha = ctrlActive ? 1.0 : 0.25;
  ctx.fillStyle = '#0f291e';
  ctx.strokeStyle = selectedId === 'query' ? '#38bdf8' : '#22c55e';
  ctx.lineWidth = selectedId === 'query' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(leftCardX, qY, leftCardW, qH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑥ Green Side QUERY Btn', leftCardX + 10, qY + 16);

  // Green tactile button icon
  ctx.fillStyle = '#22c55e';
  ctx.strokeStyle = '#bbf7d0';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(leftCardX + 36, qY + 45, 14, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();

  const qPins = [
    { name: 'PTT -> GP9 (Pin 12)', y: qY + 36, picoPin: 12, color: '#22c55e', midX: 248 },
    { name: 'GND -> Pin 13', y: qY + 56, picoPin: 13, color: '#64748b', midX: 258 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  qPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.2, !ctrlActive);
  });
  ctx.restore();
  registerHit('query', leftCardX, qY, leftCardW, qH);

  // --- CARD L4: Yellow INMP441 I2S Microphone (y: 370..446) on Left Side (Pins 14, 15, 16!) ---
  // Notice GP10 (Pin 14), GP11 (Pin 15), GP12 (Pin 16) are physically on the LEFT header of Pico 2 W!
  // Placing INMP441 right below Pin 14/15/16 makes the wiring 100% direct without crossing the board!
  const micY = 370;
  const micH = 78;
  const audioActive = isGroupActive('audio');
  ctx.save();
  ctx.globalAlpha = audioActive ? 1.0 : 0.25;
  ctx.fillStyle = '#2a2008';
  ctx.strokeStyle = selectedId === 'mic' ? '#38bdf8' : '#eab308';
  ctx.lineWidth = selectedId === 'mic' ? 2.4 : 1.6;
  ctx.beginPath();
  ctx.roundRect(leftCardX, micY, leftCardW, micH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#fef08a';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑦ Yellow INMP441 I2S Mic', leftCardX + 10, micY + 15);

  // Yellow INMP441 circular module icon
  ctx.fillStyle = '#eab308';
  ctx.strokeStyle = staffState === 1 ? '#ef4444' : '#fde047';
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  ctx.arc(leftCardX + 34, micY + 46, 17, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#1e293b';
  ctx.beginPath();
  ctx.arc(leftCardX + 34, micY + 46, 7.5, 0, Math.PI * 2);
  ctx.fill();

  const micPins = [
    { name: 'SCK -> GP10 (Pin 14)', y: micY + 30, picoPin: 14, color: '#fde047', midX: 236 },
    { name: 'WS  -> GP11 (Pin 15)', y: micY + 48, picoPin: 15, color: '#facc15', midX: 246 },
    { name: 'SD  -> GP12 (Pin 16)', y: micY + 66, picoPin: 16, color: '#eab308', midX: 256 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'right';
  micPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, leftCardX + leftCardW - 8, t.y + 2);
    const target = leftPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, leftCardX + leftCardW, t.y, t.midX, target.x, target.y, t.color, 2.2, !audioActive);
  });
  ctx.restore();
  registerHit('mic', leftCardX, micY, leftCardW, micH);

  // ==========================================================================
  // 4. RIGHT COLUMN EQUIPMENT CARDS (43 LED RINGS + 74AHCT125, MAX98357A AMP, POWER/WIFI)
  // ==========================================================================
  const rightCardX = 486;
  const rightCardW = 198;

  // --- CARD R1: 43 Concentric WS2812B Rings + SN74AHCT125N (y: 44..186) ---
  const ledY = 44;
  const ledH = 142;
  const ledsActive = isGroupActive('leds');
  ctx.save();
  ctx.globalAlpha = ledsActive ? 1.0 : 0.25;
  ctx.fillStyle = '#141829';
  ctx.strokeStyle = selectedId === 'leds' ? '#38bdf8' : '#d946ef';
  ctx.lineWidth = selectedId === 'leds' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, ledY, rightCardW, ledH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('② 43 WS2812B Rings (7+12+24)', rightCardX + 10, ledY + 16);
  ctx.fillStyle = '#e879f9';
  ctx.font = '6.2px "JetBrains Mono", monospace';
  ctx.fillText('+ SN74AHCT125N Level Shifter & 1000µF', rightCardX + 10, ledY + 27);

  // Concentric 3-ring mini visualizer on right side of card
  const ringCx = rightCardX + 154;
  const ringCy = ledY + 86;
  [30, 20, 10].forEach((r) => {
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
    ctx.arc(ringCx + Math.cos(a) * 30, ringCy + Math.sin(a) * 30, 2.2, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.fillStyle = '#fde047';
  ctx.beginPath();
  ctx.arc(ringCx, ringCy, 3, 0, Math.PI * 2);
  ctx.fill();

  const ledPins = [
    { name: 'Pin 40 (VBUS 5V) <- 5V VCC', y: ledY + 48, picoPin: 40, side: 'right', color: '#ef4444', midX: 444 },
    { name: 'Pin 38 (GND)     <- Star GND', y: ledY + 74, picoPin: 38, side: 'right', color: '#64748b', midX: 456 },
    { name: 'Pin 20 (GP15)    <- DIN (PIO0)', y: ledY + 118, picoPin: 20, side: 'left_bottom', color: '#d946ef', midX: 468 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ledPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, rightCardX + 8, t.y + 2);
    if (t.side === 'right') {
      const target = rightPinCoords[t.picoPin];
      drawOrthogonalTrace(ctx, rightCardX, t.y, t.midX, target.x, target.y, t.color, 2.2, !ledsActive);
    } else {
      // Pin 20 is at the bottom-left corner of Pico 2 W: route cleanly underneath Pico bottom edge
      const target = leftPinCoords[20];
      ctx.save();
      ctx.globalAlpha = ledsActive ? 0.95 : 0.16;
      ctx.strokeStyle = '#d946ef';
      ctx.lineWidth = 2.2;
      ctx.beginPath();
      ctx.moveTo(target.x, target.y);
      ctx.lineTo(target.x - 12, target.y);
      ctx.lineTo(target.x - 12, picoY + picoH + 10);
      ctx.lineTo(468, picoY + picoH + 10);
      ctx.lineTo(468, t.y);
      ctx.lineTo(rightCardX, t.y);
      ctx.stroke();
      ctx.restore();
    }
  });
  ctx.restore();
  registerHit('leds', rightCardX, ledY, rightCardW, ledH);

  // --- CARD R2: Power Rail & CYW43439 Wi-Fi Info (y: 200..296) ---
  const netY = 200;
  const netH = 96;
  ctx.save();
  ctx.fillStyle = '#0d2229';
  ctx.strokeStyle = selectedId === 'wifi' ? '#38bdf8' : '#10b981';
  ctx.lineWidth = selectedId === 'wifi' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, netY, rightCardW, netH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('📡 3.3V Rail & CYW43439 Wi-Fi', rightCardX + 10, netY + 18);
  ctx.fillStyle = '#6ee7b7';
  ctx.font = '6.5px "JetBrains Mono", monospace';
  ctx.fillText('• Pin 36 (3V3 OUT): OLED + INMP441', rightCardX + 10, netY + 38);
  ctx.fillText('• UDP v2 Port 5005 (0x811C9DC5)', rightCardX + 10, netY + 54);
  ctx.fillText('• Dual-Brain: Pixel 9 Pro / Pi 5', rightCardX + 10, netY + 70);
  ctx.fillStyle = '#fb7185';
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.fillText('Pin 36 (3V3) <- Logic Rail', rightCardX + 8, netY + 86);
  const p36 = rightPinCoords[36];
  drawOrthogonalTrace(ctx, rightCardX, netY + 84, 444, p36.x, p36.y, '#fb7185', 2.0, false);
  ctx.restore();
  registerHit('wifi', rightCardX, netY, rightCardW, netH);

  // --- CARD R3: MAX98357A I2S Amp + 4Ω 3W Speaker (y: 312..446) ---
  const ampY = 312;
  const ampH = 134;
  ctx.save();
  ctx.globalAlpha = audioActive ? 1.0 : 0.25;
  ctx.fillStyle = '#1b1636';
  ctx.strokeStyle = selectedId === 'amp' ? '#38bdf8' : '#38bdf8';
  ctx.lineWidth = selectedId === 'amp' ? 2.4 : 1.5;
  ctx.beginPath();
  ctx.roundRect(rightCardX, ampY, rightCardW, ampH, 7);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = '#ffffff';
  ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ctx.fillText('⑧ MAX98357A Amp + 3W Spkr', rightCardX + 10, ampY + 16);
  ctx.fillStyle = '#93c5fd';
  ctx.font = '6.2px "JetBrains Mono", monospace';
  ctx.fillText('PIO1 SM0 • 16kHz PCM + 24 Chiptunes', rightCardX + 10, ampY + 27);

  // Speaker cone icon on right of card
  const spkCx = rightCardX + 162;
  const spkCy = ampY + 82;
  ctx.fillStyle = '#0f172a';
  ctx.strokeStyle = '#64748b';
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  ctx.arc(spkCx, spkCy, 22, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = '#334155';
  ctx.beginPath();
  ctx.arc(spkCx, spkCy, 8, 0, Math.PI * 2);
  ctx.fill();

  const ampPins = [
    { name: 'Pin 24 (GP18) <- DIN', y: ampY + 50, picoPin: 24, color: '#38bdf8', midX: 442 },
    { name: 'Pin 23 (GND)  <- GND', y: ampY + 72, picoPin: 23, color: '#64748b', midX: 452 },
    { name: 'Pin 22 (GP17) <- LRC', y: ampY + 94, picoPin: 22, color: '#0ea5e9', midX: 462 },
    { name: 'Pin 21 (GP16) <- BCK', y: ampY + 116, picoPin: 21, color: '#7dd3fc', midX: 472 }
  ];
  ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
  ctx.textAlign = 'left';
  ampPins.forEach((t) => {
    ctx.fillStyle = t.color;
    ctx.fillText(t.name, rightCardX + 8, t.y + 2);
    const target = rightPinCoords[t.picoPin];
    drawOrthogonalTrace(ctx, rightCardX, t.y, t.midX, target.x, target.y, t.color, 2.2, !audioActive);
  });
  ctx.restore();
  registerHit('amp', rightCardX, ampY, rightCardW, ampH);

  ctx.restore();
  return hitBoxes;
}

if (typeof window !== 'undefined') {
  window.__celestialRenderBreadboard2D = renderFritzingBreadboard2D;
}

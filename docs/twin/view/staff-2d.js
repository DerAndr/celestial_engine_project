/**
 * Celestial Engine Mk.V — Aesthetic 2D Physical Staff Replica & Exploded View Renderer
 * Renders a unified, cohesive hardware assembly (no floating detached parts in Assembled mode):
 *  1. Clear Tapered Bell-Jar Dome (~65 × 95 mm, flush on enclosure collar)
 *  2. 43 Concentric WS2812B LEDs (3 verified rings: 7 Core + 12 Inner + 24 Outer)
 *  3. Velleman VTBB3 Breadboard + Raspberry Pi Pico 2 W (RP2350 + CYW43439) + SN74AHCT125N + 1000µF
 *  4. SH1106 1.3" 128×64 Blue I2C1 OLED Display (GP2 SDA, GP3 SCL)
 *  5. Bourns PEC11R Rotary Encoder on left wall (GP6/GP7/GP8)
 *  6. Small Green Side Momentary QUERY Button on left wall (GP9 Push-To-Talk)
 *  7. Integrated Yellow INMP441 I2S MEMS Microphone at the bottom acoustic port inside the enclosure (GP10/11/12)
 *  8. MAX98357A I2S Mono Class-D Amplifier + 4Ω 3W Speaker inside the enclosure (GP16/17/18)
 *  9. Turned Wooden Staff Shaft (~28 mm) joined directly into the bottom collar of the enclosure
 */

import { FIRMWARE_EFFECTS, STAFF_STATE_NAMES } from '../state.js?v=1.2.2';
import { renderFritzingBreadboard2D } from './breadboard-2d.js?v=1.2.2';

export const HARDWARE_HOTSPOTS = [
  {
    id: 'dome',
    title: 'Clear Tapered Bell-Jar Dome (~65 × 95 mm)',
    badge: 'OPTICAL ENCLOSURE',
    summary: 'Transparent tapered acrylic bell-jar dome mounted flush on the upper collar of the main enclosure, protecting the 43-LED concentric star assembly.',
    specs: 'Dimensions: ~65 mm diameter × 95 mm height • Clear optical shell • M2.5 base coupling'
  },
  {
    id: 'leds',
    title: '43 Concentric WS2812B LED Rings (7 + 12 + 24)',
    badge: 'PIO0 SM0 • 800 kHz',
    summary: 'Three concentric WS2812B rings from the AZ-Delivery 5-ring set (7-LED disc + 12-LED ring + 24-LED outer ring = 43 LEDs; 16-LED ring removed). Driven via SN74AHCT125N level shifter.',
    specs: 'Data: GP15 (Pin 20) -> 74AHCT125N -> 470Ω -> DIN • Power: 5V VBUS + 1000 µF 16V cap'
  },
  {
    id: 'pico',
    title: 'Raspberry Pi Pico 2 W + VTBB3 Breadboard + 74AHCT125N',
    badge: 'RP2350 DUAL-CORE',
    summary: 'Dual-core ARM Cortex-M33 @ 150 MHz with onboard CYW43439 Wi-Fi on a Velleman VTBB3 breadboard alongside the DIP-14 SN74AHCT125N 5V level shifter.',
    specs: 'Core 0: Wi-Fi UDP (5005) & I2C1 OLED • Core 1: PIO0 WS2812B + PIO0/1 Full-Duplex I2S Audio'
  },
  {
    id: 'oled',
    title: '1.3" SH1106 Blue I2C1 OLED Display (128×64)',
    badge: 'I2C1 • GP2 / GP3',
    summary: 'Blue 4-pin PCB module mounted in the lower-right window of the transparent enclosure. Displays active AI core, persona, LED effect, and 20 FPS telemetry.',
    specs: 'SDA: GP2 (Pin 4) • SCL: GP3 (Pin 5) • Address: 0x3C @ 400 kHz • Logic: 3.3V (Pin 36)'
  },
  {
    id: 'encoder',
    title: 'Bourns PEC11R Rotary Encoder (Left Side)',
    badge: 'GP6 / GP7 / GP8',
    summary: '24-detent mechanical quadrature encoder with push switch protruding horizontally from the left wall of the transparent enclosure.',
    specs: 'CLK: GP6 (Pin 9) • DT: GP7 (Pin 10) • SW: GP8 (Pin 11) • Active-LOW with pull-ups'
  },
  {
    id: 'query',
    title: 'Small Green Side QUERY Button (Push-To-Talk)',
    badge: 'GP9 • PIN 12',
    summary: 'Compact green momentary push-button mounted on the left side wall below the rotary encoder. Triggers 16 kHz I2S voice capture and Oracle queries.',
    specs: 'Signal: GP9 (Pin 12) • Return: GND (Pin 13) • Internal Pull-Up (Active-LOW)'
  },
  {
    id: 'kinder_mic',
    title: 'Integrated Yellow INMP441 I2S MEMS Microphone',
    badge: 'PIO0 SM1 • GP10/11/12',
    summary: 'Omnidirectional digital MEMS microphone integrated at the bottom acoustic port of the enclosure directly above the shaft collar.',
    specs: 'SCK: GP10 (Pin 14) • WS: GP11 (Pin 15) • SD: GP12 (Pin 16) • L/R->GND • 3.3V Only'
  },
  {
    id: 'audio',
    title: 'MAX98357A Class-D I2S Amp + 4Ω 3W Speaker',
    badge: 'PIO1 SM0 • GP16/17/18',
    summary: 'Filterless 3.2W Class-D I2S mono amplifier driving the 4Ω 3W 44mm transducer inside the lower-left acoustic bay.',
    specs: 'BCLK: GP16 (Pin 21) • LRCLK: GP17 (Pin 22) • DIN: GP18 (Pin 24) • 5V VBUS (9 dB gain)'
  },
  {
    id: 'shaft',
    title: 'Turned Wooden Staff Shaft & 5V Power Conduit',
    badge: 'ISOLATED 5V USB',
    summary: 'Ergonomic ~28 mm diameter wooden staff shaft joined directly into the bottom collar of the enclosure, routing the isolated 5V USB power cable.',
    specs: 'Diameter: ~28 mm • Power: 5V USB Power Bank -> Pico VBUS (Pin 40)'
  }
];

function computeRingGeometry(cx, cy, rOuter = 44, rMid = 28, rInner = 14) {
  const pts = [];
  pts.push({ x: cx, y: cy, ring: 0 });
  for (let i = 0; i < 6; i++) {
    const a = (i / 6) * Math.PI * 2 - Math.PI / 2;
    pts.push({ x: cx + Math.cos(a) * rInner, y: cy + Math.sin(a) * rInner, ring: 1 });
  }
  for (let i = 0; i < 12; i++) {
    const a = (i / 12) * Math.PI * 2 - Math.PI / 2;
    pts.push({ x: cx + Math.cos(a) * rMid, y: cy + Math.sin(a) * rMid, ring: 2 });
  }
  for (let i = 0; i < 24; i++) {
    const a = (i / 24) * Math.PI * 2 - Math.PI / 2;
    pts.push({ x: cx + Math.cos(a) * rOuter, y: cy + Math.sin(a) * rOuter, ring: 3 });
  }
  return pts;
}

export class Staff2DRenderer {
  constructor(canvas, stateStore, onHotspotSelect) {
    this.canvas = canvas;
    this.ctx = canvas ? canvas.getContext('2d') : null;
    this.store = stateStore;
    this.onHotspotSelect = onHotspotSelect;
    this.hitRegions = [];
    this.explodeProgress = 0;
    this.hoveredId = null;

    if (this.canvas) {
      this._bindPointerEvents();
    }
  }

  _bindPointerEvents() {
    const getPos = (e) => {
      const rect = this.canvas.getBoundingClientRect();
      const scaleX = this.canvas.width / rect.width;
      const scaleY = this.canvas.height / rect.height;
      const clientX = e.clientX ?? (e.touches && e.touches[0] && e.touches[0].clientX) ?? 0;
      const clientY = e.clientY ?? (e.touches && e.touches[0] && e.touches[0].clientY) ?? 0;
      return {
        x: (clientX - rect.left) * scaleX,
        y: (clientY - rect.top) * scaleY
      };
    };

    const handleSelect = (e) => {
      const pos = getPos(e);
      for (let i = this.hitRegions.length - 1; i >= 0; i--) {
        const r = this.hitRegions[i];
        if (pos.x >= r.x && pos.x <= r.x + r.w && pos.y >= r.y && pos.y <= r.y + r.h) {
          this.store.update({ selectedHotspotId: r.id });
          if (this.onHotspotSelect) this.onHotspotSelect(r.id);
          return;
        }
      }
    };

    this.canvas.addEventListener('click', handleSelect);
    this.canvas.addEventListener('touchstart', (e) => {
      handleSelect(e);
    }, { passive: true });

    this.canvas.addEventListener('mousemove', (e) => {
      const pos = getPos(e);
      let found = null;
      for (let i = this.hitRegions.length - 1; i >= 0; i--) {
        const r = this.hitRegions[i];
        if (pos.x >= r.x && pos.x <= r.x + r.w && pos.y >= r.y && pos.y <= r.y + r.h) {
          found = r.id;
          break;
        }
      }
      this.hoveredId = found;
      this.canvas.style.cursor = found ? 'pointer' : 'default';
    });
  }

  render(t = 0) {
    if (!this.ctx || !this.canvas) return;
    const state = this.store.getState();
    const mode = state.viewMode2D || 'assembled';

    const targetExplode = mode === 'exploded' ? 1.0 : 0.0;
    this.explodeProgress += (targetExplode - this.explodeProgress) * 0.14;

    if (mode === 'breadboard') {
      this.renderBreadboardCad(t, state);
    } else {
      this.renderPhysicalReplica(t, state, this.explodeProgress);
    }
  }

  renderPhysicalReplica(t, state, exp) {
    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;
    this.hitRegions = [];

    ctx.clearRect(0, 0, w, h);

    // 1. Deep Atmospheric Studio Backdrop with Soft Radial Aura
    const bgGrad = ctx.createRadialGradient(w * 0.5, h * 0.36, 16, w * 0.5, h * 0.5, w * 0.72);
    bgGrad.addColorStop(0, '#0f1b33');
    bgGrad.addColorStop(0.55, '#070c18');
    bgGrad.addColorStop(1, '#04060d');
    ctx.fillStyle = bgGrad;
    ctx.fillRect(0, 0, w, h);

    // Subtle blueprint grid
    ctx.strokeStyle = 'rgba(56, 189, 248, 0.032)';
    ctx.lineWidth = 1;
    for (let x = 20; x < w; x += 24) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }
    for (let y = 20; y < h; y += 24) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }

    const cx = Math.round(w / 2);

    // Unified vertical coordinates (connected with zero air gaps when exp === 0!)
    const domeTopY = Math.round(28 - exp * 16);
    const domeBaseY = Math.round(184 - exp * 12);
    const ringsCenterY = Math.round(112 - exp * 8);
    const boxY = Math.round(184 + exp * 4);
    const boxH = 192; // Ends at 376
    const boxW = 166;
    const boxLeft = cx - boxW / 2;
    const boxRight = cx + boxW / 2;
    const shaftTopY = Math.round(boxY + boxH + exp * 18);

    // ========================================================================
    // LAYER 1: TURNED WOODEN STAFF SHAFT (Directly joined to bottom of enclosure)
    // ========================================================================
    ctx.save();
    const shaftW = 40;
    // Metallic / Brass Mounting Collar joining Enclosure to Wooden Shaft
    const collarGrad = ctx.createLinearGradient(cx - 26, shaftTopY - 4, cx + 26, shaftTopY - 4);
    collarGrad.addColorStop(0, '#334155');
    collarGrad.addColorStop(0.35, '#94a3b8');
    collarGrad.addColorStop(0.5, '#e2e8f0');
    collarGrad.addColorStop(0.65, '#64748b');
    collarGrad.addColorStop(1, '#1e293b');
    ctx.fillStyle = collarGrad;
    ctx.strokeStyle = '#475569';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.roundRect(cx - 26, shaftTopY - 3, 52, 12, 4);
    ctx.fill();
    ctx.stroke();

    // Warm Turned Hardwood Shaft
    const woodGrad = ctx.createLinearGradient(cx - shaftW / 2, shaftTopY, cx + shaftW / 2, shaftTopY);
    woodGrad.addColorStop(0, '#3e1f0d');
    woodGrad.addColorStop(0.22, '#7c3f1c');
    woodGrad.addColorStop(0.48, '#b46939');
    woodGrad.addColorStop(0.75, '#783b19');
    woodGrad.addColorStop(1, '#2e1608');
    ctx.fillStyle = woodGrad;
    ctx.strokeStyle = state.selectedHotspotId === 'shaft' ? '#38bdf8' : '#5c2d12';
    ctx.lineWidth = state.selectedHotspotId === 'shaft' ? 2.2 : 1.3;
    ctx.beginPath();
    ctx.roundRect(cx - shaftW / 2, shaftTopY + 7, shaftW, h - shaftTopY - 12, [2, 2, 8, 8]);
    ctx.fill();
    ctx.stroke();

    // Subtle wood grain rings
    ctx.strokeStyle = 'rgba(46, 22, 8, 0.35)';
    ctx.lineWidth = 1;
    for (let wy = shaftTopY + 24; wy < h - 18; wy += 18) {
      ctx.beginPath();
      ctx.moveTo(cx - shaftW / 2 + 4, wy);
      ctx.lineTo(cx + shaftW / 2 - 4, wy);
      ctx.stroke();
    }
    ctx.restore();
    this.hitRegions.push({ id: 'shaft', x: cx - 26, y: shaftTopY - 3, w: 52, h: h - shaftTopY });

    // ========================================================================
    // LAYER 2: CLEAR BELL-JAR DOME & 43 CONCENTRIC WS2812B RINGS (7 + 12 + 24)
    // ========================================================================
    ctx.save();
    const domeBaseHalfW = 72;
    const domeTopHalfW = 52;

    // Ambient optical aura behind the dome driven by live LED colors
    const centerLed = state.ledNodes[0] || { r: 56, g: 189, b: 248 };
    const auraGrad = ctx.createRadialGradient(cx, ringsCenterY, 6, cx, ringsCenterY, 74);
    auraGrad.addColorStop(0, `rgba(${centerLed.r}, ${centerLed.g}, ${centerLed.b}, 0.32)`);
    auraGrad.addColorStop(0.55, 'rgba(56, 189, 248, 0.12)');
    auraGrad.addColorStop(1, 'rgba(15, 23, 42, 0)');
    ctx.fillStyle = auraGrad;
    ctx.beginPath();
    ctx.arc(cx, ringsCenterY, 74, 0, Math.PI * 2);
    ctx.fill();

    // Bell-Jar Glass Dome Silhouette sitting flush on the enclosure top
    ctx.beginPath();
    ctx.moveTo(cx - domeBaseHalfW, domeBaseY);
    ctx.bezierCurveTo(
      cx - domeBaseHalfW + 2, domeTopY + 54,
      cx - domeTopHalfW - 4, domeTopY + 2,
      cx, domeTopY
    );
    ctx.bezierCurveTo(
      cx + domeTopHalfW + 4, domeTopY + 2,
      cx + domeBaseHalfW - 2, domeTopY + 54,
      cx + domeBaseHalfW, domeBaseY
    );
    ctx.closePath();

    const glassFill = ctx.createLinearGradient(cx - domeBaseHalfW, domeTopY, cx + domeBaseHalfW, domeBaseY);
    glassFill.addColorStop(0, 'rgba(186, 230, 253, 0.15)');
    glassFill.addColorStop(0.35, 'rgba(56, 189, 248, 0.05)');
    glassFill.addColorStop(0.75, 'rgba(15, 23, 42, 0.22)');
    glassFill.addColorStop(1, 'rgba(125, 211, 252, 0.18)');
    ctx.fillStyle = glassFill;
    ctx.fill();

    ctx.lineWidth = state.selectedHotspotId === 'dome' ? 2.4 : 1.6;
    ctx.strokeStyle = state.selectedHotspotId === 'dome'
      ? '#38bdf8'
      : 'rgba(186, 230, 253, 0.65)';
    ctx.stroke();

    // Left & right curved glass specular highlights
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.38)';
    ctx.lineWidth = 2.2;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(cx - domeBaseHalfW + 11, domeBaseY - 14);
    ctx.quadraticCurveTo(cx - domeBaseHalfW + 14, domeTopY + 42, cx - 22, domeTopY + 11);
    ctx.stroke();

    ctx.strokeStyle = 'rgba(125, 211, 252, 0.22)';
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.moveTo(cx + domeBaseHalfW - 10, domeBaseY - 16);
    ctx.quadraticCurveTo(cx + domeBaseHalfW - 12, domeTopY + 48, cx + 26, domeTopY + 16);
    ctx.stroke();

    // Acrylic Dome Base Flange Ring (flush with enclosure top)
    const flangeGrad = ctx.createLinearGradient(cx - domeBaseHalfW - 4, domeBaseY - 6, cx + domeBaseHalfW + 4, domeBaseY);
    flangeGrad.addColorStop(0, 'rgba(148, 163, 184, 0.35)');
    flangeGrad.addColorStop(0.5, 'rgba(226, 232, 240, 0.65)');
    flangeGrad.addColorStop(1, 'rgba(100, 116, 139, 0.35)');
    ctx.fillStyle = flangeGrad;
    ctx.strokeStyle = 'rgba(186, 230, 253, 0.55)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.roundRect(cx - domeBaseHalfW - 3, domeBaseY - 6, (domeBaseHalfW + 3) * 2, 7, 3);
    ctx.fill();
    ctx.stroke();

    // --- 43 Concentric WS2812B LED Rings (24 Outer + 12 Middle + 7 Center) ---
    // Dark matte circular PCB carrier plate
    ctx.fillStyle = 'rgba(9, 13, 24, 0.92)';
    ctx.strokeStyle = state.selectedHotspotId === 'leds' ? '#38bdf8' : 'rgba(148, 163, 184, 0.4)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(cx, ringsCenterY, 52, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // Concentric FR4 PCB ring tracks (Outer 24-LED r=44, Inner 12-LED r=28, Core 7-LED disc r=14)
    [44, 28, 14].forEach((r) => {
      ctx.strokeStyle = 'rgba(51, 65, 85, 0.85)';
      ctx.lineWidth = 6.5;
      ctx.beginPath();
      ctx.arc(cx, ringsCenterY, r, 0, Math.PI * 2);
      ctx.stroke();
    });

    const ringPts = computeRingGeometry(cx, ringsCenterY, 44, 28, 14);
    for (let i = 0; i < ringPts.length; i++) {
      const pt = ringPts[i];
      const c = state.ledNodes[i] || { r: 0, g: 180, b: 255 };
      const r = Math.min(255, c.r + 35);
      const g = Math.min(255, c.g + 35);
      const b = Math.min(255, c.b + 35);

      // Soft LED glow halo
      const halo = ctx.createRadialGradient(pt.x, pt.y, 0.5, pt.x, pt.y, 8);
      halo.addColorStop(0, `rgba(${r}, ${g}, ${b}, 0.95)`);
      halo.addColorStop(0.45, `rgba(${c.r}, ${c.g}, ${c.b}, 0.38)`);
      halo.addColorStop(1, `rgba(${c.r}, ${c.g}, ${c.b}, 0)`);
      ctx.fillStyle = halo;
      ctx.beginPath();
      ctx.arc(pt.x, pt.y, 8, 0, Math.PI * 2);
      ctx.fill();

      // Crisp 5050 SMD LED package + bright emitter core
      ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      ctx.beginPath();
      ctx.arc(pt.x, pt.y, pt.ring === 0 ? 3.2 : 2.3, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();

    this.hitRegions.push({ id: 'leds', x: cx - 52, y: ringsCenterY - 52, w: 104, h: 104 });
    this.hitRegions.push({ id: 'dome', x: cx - domeBaseHalfW, y: domeTopY, w: domeBaseHalfW * 2, h: domeBaseY - domeTopY });

    // ========================================================================
    // LAYER 3: TRANSPARENT LOWER ENCLOSURE (Unified Housing for All Electronics)
    // ========================================================================
    ctx.save();
    const encFill = ctx.createLinearGradient(boxLeft, boxY, boxRight, boxY + boxH);
    encFill.addColorStop(0, 'rgba(30, 41, 59, 0.62)');
    encFill.addColorStop(0.5, 'rgba(15, 23, 42, 0.78)');
    encFill.addColorStop(1, 'rgba(23, 37, 84, 0.68)');
    ctx.fillStyle = encFill;
    ctx.strokeStyle = 'rgba(186, 230, 253, 0.58)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.roundRect(boxLeft, boxY, boxW, boxH, 16);
    ctx.fill();
    ctx.stroke();

    // Inner polycarbonate bezel rim & 4 corner mounting bosses
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.22)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(boxLeft + 5, boxY + 5, boxW - 10, boxH - 10, 12);
    ctx.stroke();

    [
      [boxLeft + 11, boxY + 11],
      [boxRight - 11, boxY + 11],
      [boxLeft + 11, boxY + boxH - 11],
      [boxRight - 11, boxY + boxH - 11]
    ].forEach(([bx, by]) => {
      ctx.fillStyle = 'rgba(148, 163, 184, 0.35)';
      ctx.beginPath();
      ctx.arc(bx, by, 3, 0, Math.PI * 2);
      ctx.fill();
    });

    // --- BAY 1 (TOP OF ENCLOSURE): Velleman VTBB3 Breadboard + Pico 2 W + 74AHCT125N ---
    const bbX = boxLeft + 14;
    const bbY = boxY + 14;
    const bbW = boxW - 28; // 138px
    const bbH = 54;

    ctx.fillStyle = '#e2e8f0';
    ctx.strokeStyle = state.selectedHotspotId === 'pico' ? '#38bdf8' : '#94a3b8';
    ctx.lineWidth = state.selectedHotspotId === 'pico' ? 2 : 1.2;
    ctx.beginPath();
    ctx.roundRect(bbX, bbY, bbW, bbH, 5);
    ctx.fill();
    ctx.stroke();

    // Red (+) and Blue (-) power rail stripes on breadboard
    ctx.strokeStyle = 'rgba(239, 68, 68, 0.65)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(bbX + 5, bbY + 4); ctx.lineTo(bbX + bbW - 5, bbY + 4); ctx.stroke();
    ctx.strokeStyle = 'rgba(37, 99, 235, 0.65)';
    ctx.beginPath(); ctx.moveTo(bbX + 5, bbY + bbH - 4); ctx.lineTo(bbX + bbW - 5, bbY + bbH - 4); ctx.stroke();

    // Green Raspberry Pi Pico 2 W PCB mounted on the breadboard
    const picoX = bbX + 7;
    const picoY = bbY + 10;
    const picoW = 92;
    const picoH = 34;
    ctx.fillStyle = '#0b6b3a';
    ctx.strokeStyle = '#064e2b';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(picoX, picoY, picoW, picoH, 3);
    ctx.fill();
    ctx.stroke();

    // Gold header pins along top & bottom of Pico 2 W
    ctx.fillStyle = '#facc15';
    for (let p = 0; p < 20; p++) {
      ctx.fillRect(picoX + 4 + p * 4.3, picoY + 1, 2, 2.5);
      ctx.fillRect(picoX + 4 + p * 4.3, picoY + picoH - 3.5, 2, 2.5);
    }

    // Glowing green onboard LED
    ctx.fillStyle = '#4ade80';
    ctx.shadowColor = '#22c55e';
    ctx.shadowBlur = 7;
    ctx.beginPath();
    ctx.arc(picoX + 10, picoY + 17, 3, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowBlur = 0;

    // RP2350 chip + CYW43439 Wi-Fi shield
    ctx.fillStyle = '#0f172a';
    ctx.fillRect(picoX + 30, picoY + 10, 14, 14);
    ctx.fillStyle = '#cbd5e1';
    ctx.fillRect(picoX + 64, picoY + 7, 22, 20);
    ctx.fillStyle = '#f8fafc';
    ctx.font = 'bold 5.5px "JetBrains Mono", monospace';
    ctx.fillText('Pico 2 W', picoX + 20, picoY + 29);

    // SN74AHCT125N DIP-14 Level Shifter + 1000µF Capacitor on right side of breadboard
    const lsX = picoX + picoW + 5;
    const lsY = picoY + 6;
    ctx.fillStyle = '#1e293b';
    ctx.strokeStyle = '#94a3b8';
    ctx.lineWidth = 1;
    ctx.fillRect(lsX, lsY, 24, 22);
    ctx.strokeRect(lsX, lsY, 24, 22);
    ctx.fillStyle = '#e2e8f0';
    ctx.font = '4.8px "JetBrains Mono", monospace';
    ctx.fillText('74AHCT', lsX + 3, lsY + 13);

    // 1000µF Electrolytic Capacitor
    ctx.fillStyle = '#0284c7';
    ctx.strokeStyle = '#bae6fd';
    ctx.beginPath();
    ctx.arc(lsX + 12, picoY - 1, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    this.hitRegions.push({ id: 'pico', x: bbX, y: bbY, w: bbW, h: bbH });

    // Subtle internal ribbon buses connecting breadboard to Dome, Speaker, OLED, and Integrated Mic
    const drawInternalBus = (x1, y1, x2, y2, color) => {
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.8;
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x1, (y1 + y2) / 2);
      ctx.lineTo(x2, (y1 + y2) / 2);
      ctx.lineTo(x2, y2);
      ctx.stroke();
    };
    drawInternalBus(lsX + 12, bbY, cx, ringsCenterY + 50, 'rgba(16, 185, 129, 0.75)');
    drawInternalBus(picoX + 26, bbY + bbH, boxLeft + 46, boxY + 78, 'rgba(236, 72, 153, 0.75)');
    drawInternalBus(picoX + 70, bbY + bbH, cx + 42, boxY + 78, 'rgba(56, 189, 248, 0.75)');
    drawInternalBus(cx, bbY + bbH, cx, boxY + 144, 'rgba(250, 204, 21, 0.75)');

    // --- BAY 2 (MIDDLE-LEFT OF ENCLOSURE): MAX98357A Amp + 4Ω 3W Speaker ---
    const audX = boxLeft + 14 - exp * 12;
    const audY = boxY + 76;
    const audW = 64;
    const audH = 54;

    ctx.fillStyle = '#191638';
    ctx.strokeStyle = state.selectedHotspotId === 'audio' ? '#38bdf8' : 'rgba(236, 72, 153, 0.8)';
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.roundRect(audX, audY, audW, audH, 6);
    ctx.fill();
    ctx.stroke();

    // Speaker driver cone
    const spkCx = audX + 23;
    const spkCy = audY + 30;
    ctx.fillStyle = '#0f172a';
    ctx.strokeStyle = '#64748b';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(spkCx, spkCy, 15, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = '#334155';
    ctx.beginPath();
    ctx.arc(spkCx, spkCy, 5.5, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#f472b6';
    ctx.font = 'bold 5.4px "JetBrains Mono", monospace';
    ctx.fillText('MAX98357A', audX + 6, audY + 10);
    ctx.fillStyle = '#cbd5e1';
    ctx.font = '5px "JetBrains Mono", monospace';
    ctx.fillText('3W', audX + 43, audY + 27);
    ctx.fillText('4Ω', audX + 43, audY + 36);

    this.hitRegions.push({ id: 'audio', x: audX, y: audY, w: audW, h: audH });

    // --- BAY 3 (MIDDLE-RIGHT OF ENCLOSURE): SH1106 1.3" Blue I2C1 OLED ---
    const oledX = cx + 4 + exp * 12;
    const oledY = boxY + 76;
    const oledW = 68;
    const oledH = 54;

    ctx.fillStyle = '#0f3c78';
    ctx.strokeStyle = state.selectedHotspotId === 'oled' ? '#38bdf8' : '#60a5fa';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.roundRect(oledX, oledY, oledW, oledH, 5);
    ctx.fill();
    ctx.stroke();

    // Top 4-pin I2C header (VCC GND SCL SDA)
    ctx.fillStyle = '#facc15';
    for (let ip = 0; ip < 4; ip++) {
      ctx.fillRect(oledX + 20 + ip * 7, oledY + 2, 2.5, 3.5);
    }

    // Active OLED Screen
    ctx.fillStyle = '#020617';
    ctx.strokeStyle = 'rgba(56, 189, 248, 0.5)';
    ctx.lineWidth = 0.8;
    ctx.fillRect(oledX + 5, oledY + 9, oledW - 10, oledH - 14);
    ctx.strokeRect(oledX + 6, oledY + 10, oledW - 12, oledH - 16);

    const eff = FIRMWARE_EFFECTS[state.animIdx] || FIRMWARE_EFFECTS[4];
    ctx.fillStyle = '#38bdf8';
    ctx.font = 'bold 5.5px "JetBrains Mono", monospace';
    const headerLine = state.staffState === 0
      ? `[${String(state.animIdx + 1).padStart(2, '0')}] AI CORE(G4)`
      : `[${STAFF_STATE_NAMES[state.staffState]}]`;
    ctx.fillText(headerLine, oledX + 8, oledY + 20);

    ctx.fillStyle = '#7dd3fc';
    ctx.font = '5.2px "JetBrains Mono", monospace';
    ctx.fillText(`AI: ${state.personaId.slice(0, 9).toUpperCase()}`, oledX + 8, oledY + 29);

    ctx.strokeStyle = '#38bdf8';
    ctx.strokeRect(oledX + 8, oledY + 33, oledW - 16, 9);
    ctx.font = '4.8px "JetBrains Mono", monospace';
    ctx.fillText(`FX:${eff.name.slice(0, 8)}`, oledX + 10, oledY + 39.5);

    this.hitRegions.push({ id: 'oled', x: oledX, y: oledY, w: oledW, h: oledH });

    // --- BAY 4 (INTEGRATED BOTTOM OF ENCLOSURE): Yellow INMP441 I2S Microphone ---
    // Mounted right inside the lower center acoustic bay of the enclosure (not floating separately!)
    const micW = 86;
    const micH = 34;
    const micX = cx - micW / 2;
    const micY = boxY + 142; // 326..360 inside boxY..boxY+192 (ends at 376)
    const micCy = micY + micH / 2;

    if (state.staffState === 1) {
      for (let wv = 0; wv < 2; wv++) {
        const wavePhase = ((t * 0.003 + wv * 0.5) % 1);
        ctx.strokeStyle = `rgba(239, 68, 68, ${(1 - wavePhase) * 0.85})`;
        ctx.lineWidth = 1.8;
        ctx.beginPath();
        ctx.roundRect(micX - wavePhase * 8, micY - wavePhase * 5, micW + wavePhase * 16, micH + wavePhase * 10, 12);
        ctx.stroke();
      }
    }

    const micGrad = ctx.createLinearGradient(micX, micY, micX + micW, micY + micH);
    micGrad.addColorStop(0, '#fde047');
    micGrad.addColorStop(0.5, '#eab308');
    micGrad.addColorStop(1, '#ca8a04');
    ctx.fillStyle = micGrad;
    ctx.strokeStyle = state.staffState === 1 ? '#ef4444' : (state.selectedHotspotId === 'kinder_mic' ? '#38bdf8' : '#854d0e');
    ctx.lineWidth = state.staffState === 1 ? 2.2 : 1.5;
    ctx.beginPath();
    ctx.roundRect(micX, micY, micW, micH, 10);
    ctx.fill();
    ctx.stroke();

    // Circular MEMS acoustic mesh port on left-center of yellow INMP441 module
    const portCx = micX + 20;
    ctx.fillStyle = '#0f172a';
    ctx.strokeStyle = '#cbd5e1';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.arc(portCx, micCy, 9.5, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = '#94a3b8';
    [[0, 0], [-3.2, 0], [3.2, 0], [0, -3.2], [0, 3.2]].forEach(([dx, dy]) => {
      ctx.beginPath();
      ctx.arc(portCx + dx, micCy + dy, 0.9, 0, Math.PI * 2);
      ctx.fill();
    });

    // Clear INMP441 label on yellow housing
    ctx.fillStyle = '#1e293b';
    ctx.font = 'bold 6.5px "JetBrains Mono", monospace';
    ctx.textAlign = 'left';
    ctx.fillText('INMP441 MIC', micX + 34, micCy - 2);
    ctx.fillStyle = '#451a03';
    ctx.font = '5.5px "JetBrains Mono", monospace';
    ctx.fillText(state.staffState === 1 ? '● REC 16kHz' : 'I2S GP10-12', micX + 34, micCy + 7);
    ctx.restore();

    this.hitRegions.push({ id: 'kinder_mic', x: micX, y: micY, w: micW, h: micH });

    // ========================================================================
    // LAYER 4: LEFT-WALL CONTROLS (PEC11R ENCODER + SMALL GREEN QUERY BUTTON)
    // ========================================================================
    const encX = boxLeft - 22 - exp * 14;
    const encY = boxY + 42;
    ctx.save();
    ctx.fillStyle = '#64748b';
    ctx.fillRect(boxLeft - 6 - exp * 14, encY - 6, 6, 12);
    const shaftMetal = ctx.createLinearGradient(encX, encY - 4, encX, encY + 4);
    shaftMetal.addColorStop(0, '#f8fafc');
    shaftMetal.addColorStop(0.5, '#94a3b8');
    shaftMetal.addColorStop(1, '#334155');
    ctx.fillStyle = shaftMetal;
    ctx.strokeStyle = state.selectedHotspotId === 'encoder' ? '#38bdf8' : '#475569';
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.roundRect(encX, encY - 4, 18, 8, 2);
    ctx.fill();
    ctx.stroke();

    // Small Green Side QUERY Button (GP9) mounted flush on left wall
    const qBtnX = boxLeft - 13 - exp * 14;
    const qBtnY = boxY + 104;
    ctx.fillStyle = '#334155';
    ctx.fillRect(qBtnX + 7, qBtnY - 6, 6, 12);
    const greenGrad = ctx.createRadialGradient(qBtnX + 3, qBtnY - 2, 1, qBtnX + 3, qBtnY, 8);
    greenGrad.addColorStop(0, '#86efac');
    greenGrad.addColorStop(0.6, '#22c55e');
    greenGrad.addColorStop(1, '#15803d');
    ctx.fillStyle = greenGrad;
    ctx.strokeStyle = state.selectedHotspotId === 'query' ? '#ffffff' : '#166534';
    ctx.lineWidth = 1.4;
    if (state.staffState === 1) {
      ctx.shadowColor = '#ef4444';
      ctx.shadowBlur = 10;
    }
    ctx.beginPath();
    ctx.roundRect(qBtnX - 2, qBtnY - 6, 10, 12, 4);
    ctx.fill();
    ctx.stroke();
    ctx.shadowBlur = 0;
    ctx.restore();

    this.hitRegions.push({ id: 'encoder', x: encX - 6, y: encY - 12, w: 32, h: 24 });
    this.hitRegions.push({ id: 'query', x: qBtnX - 8, y: qBtnY - 12, w: 26, h: 24 });

    // ========================================================================
    // LAYER 5: SLEEK CALLOUT BADGES (LEFT & RIGHT MARGINS)
    // ========================================================================
    ctx.save();
    ctx.font = '600 9px "JetBrains Mono", monospace';
    const callouts = [
      // Left Column (top-to-bottom)
      { id: 'dome', label: '① Clear Bell Dome', tx: 10, ty: domeTopY + 30, px: cx - 54, py: domeTopY + 30, align: 'left' },
      { id: 'leds', label: '② 43 LEDs (7+12+24)', tx: 10, ty: ringsCenterY, px: cx - 52, py: ringsCenterY, align: 'left' },
      { id: 'encoder', label: '⑤ PEC11R Encoder', tx: 10, ty: encY, px: encX, py: encY, align: 'left' },
      { id: 'query', label: '⑥ Green Query Btn', tx: 10, ty: qBtnY, px: qBtnX - 2, py: qBtnY, align: 'left' },
      { id: 'audio', label: '⑧ MAX98357A + 3W', tx: 10, ty: audY + 38, px: audX, py: audY + 38, align: 'left' },
      { id: 'kinder_mic', label: '⑦ INMP441 Mic', tx: 10, ty: micCy, px: micX, py: micCy, align: 'left' },
      // Right Column (top-to-bottom)
      { id: 'pico', label: '③ Pico 2 W + VTBB3', tx: w - 10, ty: bbY + 26, px: bbX + bbW, py: bbY + 26, align: 'right' },
      { id: 'oled', label: '④ 1.3" SH1106 OLED', tx: w - 10, ty: oledY + 28, px: oledX + oledW, py: oledY + 28, align: 'right' },
      { id: 'shaft', label: '⑨ Wooden Shaft', tx: w - 10, ty: shaftTopY + 46, px: cx + 20, py: shaftTopY + 46, align: 'right' }
    ];

    callouts.forEach(c => {
      const isSelected = state.selectedHotspotId === c.id || this.hoveredId === c.id;
      ctx.strokeStyle = isSelected ? 'rgba(56, 189, 248, 0.9)' : 'rgba(148, 163, 184, 0.28)';
      ctx.lineWidth = isSelected ? 1.5 : 1;

      const textW = ctx.measureText(c.label).width;
      const lineStartX = c.align === 'left' ? c.tx + textW + 6 : c.tx - textW - 6;

      ctx.beginPath();
      ctx.moveTo(lineStartX, c.ty - 3);
      ctx.lineTo(c.px, c.py);
      ctx.stroke();

      ctx.fillStyle = isSelected ? '#38bdf8' : 'rgba(148, 163, 184, 0.6)';
      ctx.beginPath();
      ctx.arc(c.px, c.py, 2, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = isSelected ? '#38bdf8' : '#cbd5e1';
      ctx.textAlign = c.align;
      ctx.fillText(c.label, c.tx, c.ty);

      const hitX = c.align === 'left' ? c.tx - 4 : c.tx - textW - 4;
      this.hitRegions.push({ id: c.id, x: hitX, y: c.ty - 12, w: textW + 8, h: 16 });
    });
    ctx.restore();
  }

  // ==========================================================================
  // 2D DIRECT PINOUT MAP MODE (Inside top showcase when "2D Pinout Map" tab is clicked)
  // ==========================================================================
  renderBreadboardCad(t, state) {
    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;
    this.hitRegions = [];

    const idMap = {
      pico: 'pico',
      leds: 'leds',
      oled: 'oled',
      encoder: 'encoder',
      query: 'query',
      mic: 'kinder_mic',
      amp: 'audio',
      wifi: 'pico'
    };
    const reverseMap = {
      pico: 'pico',
      leds: 'leds',
      oled: 'oled',
      encoder: 'encoder',
      query: 'query',
      kinder_mic: 'mic',
      audio: 'amp'
    };

    const boxes = renderFritzingBreadboard2D(ctx, w, h, {
      circuitMode: 'all',
      selectedId: reverseMap[state.selectedHotspotId] || 'pico',
      ledNodes: state.ledNodes,
      staffState: state.staffState
    });

    boxes.forEach((b) => {
      this.hitRegions.push({
        id: idMap[b.id] || 'pico',
        x: b.x,
        y: b.y,
        w: b.w,
        h: b.h
      });
    });
  }
}

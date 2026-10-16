#!/usr/bin/env python3
"""
Generates high-aesthetic SVG diagrams for Celestial Engine Mk.V documentation.
Replaces deformed ASCII art with vector graphics that render cleanly on GitHub.
"""

import math
import os

os.makedirs("docs/assets", exist_ok=True)

# ==============================================================================
# 1. CONCENTRIC RINGS SVG (43-LED Headpiece)
# ==============================================================================
def generate_rings_svg():
    width, height = 760, 420
    cx, cy = 250, 210
    r1, r2, r3 = 50, 95, 145

    lines = []
    lines.append(f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="auto">
  <defs>
    <radialGradient id="bgGrad" cx="50%" cy="50%" r="75%">
      <stop offset="0%" stop-color="#141926"/>
      <stop offset="100%" stop-color="#0B0E14"/>
    </radialGradient>
    <filter id="glowCyan" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="3.5" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
    <filter id="glowGold" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="5" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>

  <!-- Card Background -->
  <rect width="{width}" height="{height}" rx="12" fill="url(#bgGrad)" stroke="#1F293D" stroke-width="1.5"/>

  <!-- Concentric Orbital Tracks -->
  <circle cx="{cx}" cy="{cy}" r="{r1}" fill="none" stroke="#1E2B45" stroke-dasharray="3,3" stroke-width="1.2"/>
  <circle cx="{cx}" cy="{cy}" r="{r2}" fill="none" stroke="#1E2B45" stroke-dasharray="4,4" stroke-width="1.2"/>
  <circle cx="{cx}" cy="{cy}" r="{r3}" fill="none" stroke="#1E2B45" stroke-dasharray="4,4" stroke-width="1.2"/>

  <!-- Radial Guide Rays -->
  <line x1="{cx - r3 - 10}" y1="{cy}" x2="{cx + r3 + 10}" y2="{cy}" stroke="#162032" stroke-width="1"/>
  <line x1="{cx}" y1="{cy - r3 - 10}" x2="{cx}" y2="{cy + r3 + 10}" stroke="#162032" stroke-width="1"/>

  <!-- Center Gemstone (LED 0) -->
  <circle cx="{cx}" cy="{cy}" r="9" fill="#FFD700" filter="url(#glowGold)"/>
  <circle cx="{cx}" cy="{cy}" r="3.5" fill="#FFFBEB"/>
''')

    # Inner Ring 1 (6 LEDs)
    lines.append('  <!-- Inner Ring 1 (6 LEDs) -->\n')
    for i in range(6):
        angle = math.radians(i * 60 - 90)
        x = cx + r1 * math.cos(angle)
        y = cy + r1 * math.sin(angle)
        lines.append(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="5.0" fill="#00E5FF" filter="url(#glowCyan)"/>')
        lines.append(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="2.0" fill="#E0F7FA"/>')

    # Middle Ring 2 (12 LEDs)
    lines.append('\n  <!-- Middle Ring 2 (12 LEDs) -->\n')
    for i in range(12):
        angle = math.radians(i * 30 - 90)
        x = cx + r2 * math.cos(angle)
        y = cy + r2 * math.sin(angle)
        lines.append(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="4.2" fill="#38BDF8" filter="url(#glowCyan)"/>')
        lines.append(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="1.6" fill="#F0F9FF"/>')

    # Outer Ring 3 (24 LEDs)
    lines.append('\n  <!-- Outer Ring 3 (24 LEDs) -->\n')
    for i in range(24):
        angle = math.radians(i * 15 - 90)
        x = cx + r3 * math.cos(angle)
        y = cy + r3 * math.sin(angle)
        lines.append(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="#818CF8" filter="url(#glowCyan)"/>')

    # Callouts & Annotations
    lines.append(f'''
  <!-- Pointer Callouts -->
  <line x1="{cx}" y1="{cy}" x2="{cx - 75}" y2="{cy - 80}" stroke="#FFD700" stroke-width="1.2" stroke-dasharray="2,2"/>
  <circle cx="{cx - 75}" cy="{cy - 80}" r="2.5" fill="#FFD700"/>
  <text x="{cx - 82}" y="{cy - 76}" text-anchor="end" fill="#FFD700" font-family="monospace, monospace" font-size="11" font-weight="bold">Core Gemstone (#00)</text>

  <line x1="{cx + r1 * 0.866:.1f}" y1="{cy - r1 * 0.5:.1f}" x2="{cx + 115}" y2="{cy - 55}" stroke="#00E5FF" stroke-width="1.2" stroke-dasharray="2,2"/>
  <text x="{cx + 120}" y="{cy - 51}" fill="#00E5FF" font-family="monospace, monospace" font-size="11">Inner Ring 1: 6 LEDs (#01..#06)</text>

  <line x1="{cx + r2:.1f}" y1="{cy:.1f}" x2="{cx + 135}" y2="{cy}" stroke="#38BDF8" stroke-width="1.2" stroke-dasharray="2,2"/>
  <text x="{cx + 140}" y="{cy + 4}" fill="#38BDF8" font-family="monospace, monospace" font-size="11">Middle Ring 2: 12 LEDs (#07..#18)</text>

  <line x1="{cx + r3 * 0.707:.1f}" y1="{cy + r3 * 0.707:.1f}" x2="{cx + 115}" y2="{cy + 75}" stroke="#818CF8" stroke-width="1.2" stroke-dasharray="2,2"/>
  <text x="{cx + 120}" y="{cy + 79}" fill="#818CF8" font-family="monospace, monospace" font-size="11">Outer Ring 3: 24 LEDs (#19..#42)</text>

  <!-- Technical Specs Panel (Right) -->
  <g transform="translate(485, 45)">
    <rect width="245" height="330" rx="8" fill="#0E131F" stroke="#1E293B" stroke-width="1"/>
    <text x="16" y="32" fill="#F1F5F9" font-family="system-ui, -apple-system, sans-serif" font-size="14" font-weight="bold">43-LED Concentric Headpiece</text>
    <text x="16" y="50" fill="#64748B" font-family="monospace, monospace" font-size="10">WS2812B RGB • PIO0 SM1 DMA</text>

    <line x1="16" y1="65" x2="229" y2="65" stroke="#1E293B" stroke-width="1"/>

    <text x="16" y="88" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Addressable Pixels</text>
    <text x="229" y="88" text-anchor="end" fill="#00E5FF" font-family="monospace, monospace" font-size="12" font-weight="bold">43 Nodes</text>

    <text x="16" y="115" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Concentric Topology</text>
    <text x="229" y="115" text-anchor="end" fill="#F8FAFC" font-family="monospace, monospace" font-size="11">1 + 6 + 12 + 24</text>

    <text x="16" y="142" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Hardware Power Cap</text>
    <text x="229" y="142" text-anchor="end" fill="#10B981" font-family="monospace, monospace" font-size="11" font-weight="bold">18% (0x2E Max)</text>

    <text x="16" y="169" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Full White Power Draw</text>
    <text x="229" y="169" text-anchor="end" fill="#F8FAFC" font-family="monospace, monospace" font-size="11">&lt; 650 mA @ 5V</text>

    <text x="16" y="196" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Hardware DMA Refresh</text>
    <text x="229" y="196" text-anchor="end" fill="#F8FAFC" font-family="monospace, monospace" font-size="11">33 Hz (30 ms)</text>

    <text x="16" y="223" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11">Animation Presets</text>
    <text x="229" y="223" text-anchor="end" fill="#F8FAFC" font-family="monospace, monospace" font-size="11">18 Unique Modes</text>

    <line x1="16" y1="240" x2="229" y2="240" stroke="#1E293B" stroke-width="1"/>

    <rect x="14" y="254" width="217" height="58" rx="6" fill="#131C30" stroke="#1E3A8A" stroke-width="1"/>
    <text x="22" y="273" fill="#00E5FF" font-family="system-ui, sans-serif" font-size="10" font-weight="bold">COMPANION CANVAS MIRROR</text>
    <text x="22" y="291" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="10">Real-time 33 Hz synchronization</text>
    <text x="22" y="303" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="10">with Android Jetpack Compose</text>
  </g>
</svg>
''')

    with open("docs/assets/concentric_rings.svg", "w", encoding="utf-8") as f:
        f.write("".join(lines))
    print("Generated docs/assets/concentric_rings.svg")


# ==============================================================================
# 2. UNIFIED COSMOS ZOOM NAVIGATION SVG
# ==============================================================================
def generate_cosmos_zoom_svg():
    width, height = 760, 240
    lines = []
    lines.append(f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="auto">
  <defs>
    <radialGradient id="spaceGrad" cx="50%" cy="50%" r="75%">
      <stop offset="0%" stop-color="#141926"/>
      <stop offset="100%" stop-color="#0B0E14"/>
    </radialGradient>
    <filter id="glowPortal" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="3" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>

  <rect width="{width}" height="{height}" rx="12" fill="url(#spaceGrad)" stroke="#1F293D" stroke-width="1.5"/>

  <!-- ================= LEVEL 0: GALAXY (1x) ================= -->
  <g transform="translate(40, 25)">
    <rect width="180" height="190" rx="8" fill="#0E131F" stroke="#1E293B" stroke-width="1"/>
    <text x="90" y="24" text-anchor="middle" fill="#00E5FF" font-family="system-ui, sans-serif" font-size="11" font-weight="bold">LEVEL 0: GALAXY (1x)</text>
    <text x="90" y="38" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="9">Milky Way Spiral Map</text>

    <!-- Galactic Core & Spiral Arms -->
    <ellipse cx="90" cy="105" rx="55" ry="32" fill="none" stroke="#334155" stroke-width="1" stroke-dasharray="3,3"/>
    <ellipse cx="90" cy="105" rx="35" ry="20" fill="none" stroke="#475569" stroke-width="1.2" stroke-dasharray="4,4"/>
    <circle cx="90" cy="105" r="5" fill="#FFD700" filter="url(#glowPortal)"/>
    <text x="90" y="122" text-anchor="middle" fill="#FFD700" font-family="monospace, monospace" font-size="8">Sgr A*</text>

    <!-- Sol Beacon -->
    <circle cx="118" cy="95" r="3.5" fill="#00E5FF" filter="url(#glowPortal)"/>
    <text x="126" y="98" fill="#00E5FF" font-family="monospace, monospace" font-size="8">Sol</text>

    <rect x="25" y="160" width="130" height="18" rx="4" fill="#162238"/>
    <text x="90" y="172" text-anchor="middle" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="9">LED_ANIM_GALAXY</text>
  </g>

  <!-- Transition Arrow 1 -->
  <g transform="translate(225, 110)">
    <path d="M 5,10 L 25,10 M 20,5 L 25,10 L 20,15" fill="none" stroke="#00E5FF" stroke-width="2"/>
    <text x="15" y="-6" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="8">CLICK</text>
    <text x="15" y="28" text-anchor="middle" fill="#38BDF8" font-family="monospace, monospace" font-size="8">Zoom In</text>
  </g>

  <!-- ================= LEVEL 1: SOLAR (1,000x) ================= -->
  <g transform="translate(260, 25)">
    <rect width="180" height="190" rx="8" fill="#0E131F" stroke="#1E293B" stroke-width="1"/>
    <text x="90" y="24" text-anchor="middle" fill="#38BDF8" font-family="system-ui, sans-serif" font-size="11" font-weight="bold">LEVEL 1: SOLAR (1,000x)</text>
    <text x="90" y="38" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="9">Keplerian Planetary Orrery</text>

    <!-- Central Sun -->
    <circle cx="90" cy="105" r="8" fill="#FFD700" filter="url(#glowPortal)"/>
    <circle cx="90" cy="105" r="3.5" fill="#FFF9C4"/>

    <!-- Planetary Orbits -->
    <circle cx="90" cy="105" r="22" fill="none" stroke="#334155" stroke-width="1" stroke-dasharray="2,2"/>
    <circle cx="90" cy="105" r="36" fill="none" stroke="#334155" stroke-width="1" stroke-dasharray="2,2"/>
    <circle cx="90" cy="105" r="50" fill="none" stroke="#38BDF8" stroke-width="1.2" stroke-dasharray="3,3"/>

    <!-- Earth & Moon -->
    <circle cx="125" cy="105" r="3.5" fill="#38BDF8"/>
    <circle cx="132" cy="101" r="1.5" fill="#E2E8F0"/>
    <text x="125" y="122" text-anchor="middle" fill="#38BDF8" font-family="monospace, monospace" font-size="8">Earth</text>

    <rect x="25" y="160" width="130" height="18" rx="4" fill="#162238"/>
    <text x="90" y="172" text-anchor="middle" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="9">LED_ANIM_SOLAR</text>
  </g>

  <!-- Transition Arrow 2 -->
  <g transform="translate(445, 110)">
    <path d="M 5,10 L 25,10 M 20,5 L 25,10 L 20,15" fill="none" stroke="#38BDF8" stroke-width="2"/>
    <text x="15" y="-6" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="8">CLICK</text>
    <text x="15" y="28" text-anchor="middle" fill="#818CF8" font-family="monospace, monospace" font-size="8">Zoom In</text>
  </g>

  <!-- ================= LEVEL 2: TERRA (1,000,000x) ================= -->
  <g transform="translate(480, 25)">
    <rect width="180" height="190" rx="8" fill="#0E131F" stroke="#1E293B" stroke-width="1"/>
    <text x="90" y="24" text-anchor="middle" fill="#818CF8" font-family="system-ui, sans-serif" font-size="11" font-weight="bold">LEVEL 2: TERRA (1,000,000x)</text>
    <text x="90" y="38" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="9">Planetary Globe &amp; Orbit</text>

    <!-- Globe Silhouette -->
    <circle cx="90" cy="105" r="32" fill="#0F172A" stroke="#818CF8" stroke-width="1.5" filter="url(#glowPortal)"/>
    <!-- Continents approximation -->
    <path d="M 80,85 Q 92,80 98,90 Q 105,95 95,108 Q 85,115 80,105 Z" fill="#10B981" opacity="0.6"/>
    <path d="M 95,112 Q 105,115 102,125 Q 92,130 90,120 Z" fill="#10B981" opacity="0.6"/>

    <!-- Orbital Moon Path -->
    <ellipse cx="90" cy="105" rx="46" ry="16" fill="none" stroke="#475569" stroke-width="1" stroke-dasharray="3,3"/>
    <circle cx="132" cy="100" r="3" fill="#E2E8F0"/>
    <text x="145" y="103" fill="#94A3B8" font-family="monospace, monospace" font-size="8">Moon</text>

    <rect x="25" y="160" width="130" height="18" rx="4" fill="#162238"/>
    <text x="90" y="172" text-anchor="middle" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="9">LED_ANIM_EARTH</text>
  </g>

  <!-- Cycle Loop Arrow back to Galaxy -->
  <g transform="translate(685, 110)">
    <path d="M 0,10 Q 25,-60 -210,-90 Q -450,-120 -640,0" fill="none" stroke="#475569" stroke-width="1" stroke-dasharray="3,3"/>
  </g>
</svg>
''')

    with open("docs/assets/cosmos_zoom.svg", "w", encoding="utf-8") as f:
        f.write("".join(lines))
    print("Generated docs/assets/cosmos_zoom.svg")


# ==============================================================================
# 3. CYBERNETIC ARCHITECTURE HUD SCHEMATIC SVG
# ==============================================================================
def generate_hud_svg():
    width, height = 760, 220
    lines = []
    lines.append(f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="auto">
  <defs>
    <radialGradient id="hudGrad" cx="50%" cy="50%" r="75%">
      <stop offset="0%" stop-color="#121824"/>
      <stop offset="100%" stop-color="#080B10"/>
    </radialGradient>
    <filter id="glowAether" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="3" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>

  <rect width="{width}" height="{height}" rx="12" fill="url(#hudGrad)" stroke="#1F293D" stroke-width="1.5"/>

  <!-- OLED Frame Border (SH1106 128x64 Simulation) -->
  <rect x="30" y="25" width="700" height="170" rx="6" fill="#000000" stroke="#00E5FF" stroke-width="1.5"/>

  <!-- HUD Status Bar -->
  <text x="45" y="44" fill="#00E5FF" font-family="monospace, monospace" font-size="11" font-weight="bold">[01] ARCHITECTURE</text>
  <text x="400" y="44" fill="#64748B" font-family="monospace, monospace" font-size="10">CYBERNETIC SCHEMATIC</text>
  <text x="715" y="44" text-anchor="end" fill="#00E5FF" font-family="monospace, monospace" font-size="11" font-weight="bold">[EN]  100%</text>
  <line x1="45" y1="50" x2="715" y2="50" stroke="#1E293B" stroke-width="1"/>

  <!-- ================= LEFT: STAFF NODE ================= -->
  <g transform="translate(130, 115)">
    <!-- Concentric Rings Arc -->
    <ellipse cx="0" cy="-8" rx="18" ry="12" fill="none" stroke="#00E5FF" stroke-width="1.5"/>
    <ellipse cx="0" cy="-8" rx="28" ry="18" fill="none" stroke="#00E5FF" stroke-width="1.2" stroke-dasharray="3,3"/>
    <ellipse cx="0" cy="-8" rx="38" ry="24" fill="none" stroke="#00E5FF" stroke-width="1" stroke-dasharray="4,4"/>
    <!-- Glowing Gemstone -->
    <circle cx="0" cy="-8" r="4" fill="#FFD700" filter="url(#glowAether)"/>
    <!-- Staff Rod -->
    <line x1="0" y1="16" x2="0" y2="48" stroke="#00E5FF" stroke-width="3.5" stroke-linecap="round"/>
    <line x1="-8" y1="26" x2="8" y2="26" stroke="#00E5FF" stroke-width="2"/>
    <!-- Pinnacle Spark -->
    <circle cx="0" cy="-35" r="2" fill="#00E5FF" filter="url(#glowAether)"/>
    <line x1="0" y1="-32" x2="0" y2="-27" stroke="#00E5FF" stroke-width="1"/>

    <text x="0" y="65" text-anchor="middle" fill="#F1F5F9" font-family="system-ui, sans-serif" font-size="10" font-weight="bold">STAFF NODE</text>
    <text x="0" y="77" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="9">Pico 2 W (RP2350)</text>
  </g>

  <!-- ================= CENTER: WIRELESS LINK ================= -->
  <g transform="translate(380, 105)">
    <!-- Radiating Waves Left -->
    <path d="M -30,-16 A 25,25 0 0,0 -30,16" fill="none" stroke="#00E5FF" stroke-width="1.5"/>
    <path d="M -45,-26 A 38,38 0 0,0 -45,26" fill="none" stroke="#00E5FF" stroke-width="1.2" stroke-dasharray="3,2"/>
    
    <!-- Bidirectional Flow Indicator -->
    <line x1="-20" y1="0" x2="20" y2="0" stroke="#38BDF8" stroke-width="2"/>
    <polygon points="-20,0 -12,-4 -12,4" fill="#38BDF8"/>
    <polygon points="20,0 12,-4 12,4" fill="#38BDF8"/>
    
    <!-- Flowing Packets -->
    <circle cx="-5" cy="0" r="3" fill="#FFD700" filter="url(#glowAether)"/>
    <circle cx="8" cy="0" r="2.5" fill="#00E5FF"/>

    <!-- Radiating Waves Right -->
    <path d="M 30,-16 A 25,25 0 0,1 30,16" fill="none" stroke="#00E5FF" stroke-width="1.5"/>
    <path d="M 45,-26 A 38,38 0 0,1 45,26" fill="none" stroke="#00E5FF" stroke-width="1.2" stroke-dasharray="3,2"/>

    <text x="0" y="32" text-anchor="middle" fill="#38BDF8" font-family="monospace, monospace" font-size="10" font-weight="bold">UDP PORT 5005</text>
    <text x="0" y="46" text-anchor="middle" fill="#64748B" font-family="system-ui, sans-serif" font-size="9">Zero-Latency Mesh</text>
  </g>

  <!-- ================= RIGHT: HOST NODE (Pi 5) ================= -->
  <g transform="translate(630, 115)">
    <!-- Pi 5 Board Outline -->
    <rect x="-35" y="-30" width="70" height="60" rx="4" fill="#0A0E17" stroke="#00E5FF" stroke-width="1.8"/>
    <!-- Cooling Fins -->
    <line x1="-28" y1="-22" x2="28" y2="-22" stroke="#1E293B" stroke-width="1.5"/>
    <line x1="-28" y1="-14" x2="28" y2="-14" stroke="#1E293B" stroke-width="1.5"/>
    <line x1="-28" y1="-6" x2="28" y2="-6" stroke="#1E293B" stroke-width="1.5"/>
    
    <!-- Active Cooler Fan Silhouette -->
    <circle cx="0" cy="8" r="14" fill="#06121E" stroke="#00E5FF" stroke-width="1"/>
    <!-- 4 Blades -->
    <line x1="-10" y1="8" x2="10" y2="8" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round"/>
    <line x1="0" y1="-2" x2="0" y2="18" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round"/>
    <circle cx="0" cy="8" r="3" fill="#FFD700"/>

    <text x="0" y="65" text-anchor="middle" fill="#F1F5F9" font-family="system-ui, sans-serif" font-size="10" font-weight="bold">HOST NODE</text>
    <text x="0" y="77" text-anchor="middle" fill="#64748B" font-family="monospace, monospace" font-size="9">Raspberry Pi 5 (Active Cooler)</text>
  </g>
</svg>
''')

    with open("docs/assets/cybernetic_hud.svg", "w", encoding="utf-8") as f:
        f.write("".join(lines))
    print("Generated docs/assets/cybernetic_hud.svg")


if __name__ == "__main__":
    generate_rings_svg()
    generate_cosmos_zoom_svg()
    generate_hud_svg()
    print("All vector assets generated successfully in docs/assets/")

/**
 * Celestial Engine Mk.V — Grounded Repository Knowledge Base (RAG Context & Fact Matcher)
 * Gives both the in-browser Local AI models (SmolLM2-135M / LFM2.5-350M) and Scripted Demo mode
 * authoritative knowledge of the celestial_engine_project repository, hardware pinout,
 * firmware architecture, UDP v2 protocol, Android 15 companion app, and Pi 5 edge server.
 */

export const REPO_SUMMARY_COMPACT =
  "Project: Celestial Engine Mk.V interactive AI staff created by Dr. Andrey Holz (LinkedIn: https://www.linkedin.com/in/andrey-holz/ , repo: DerAndr/celestial_engine_project, release v1.2.0). " +
  "MCU: Raspberry Pi Pico 2 W (RP2350 dual Cortex-M33 @ 150MHz, CYW43439 Wi-Fi) on Velleman VTBB3 breadboard. " +
  "LEDs: 43 WS2812B in 3 concentric rings (7 Core + 12 Inner + 24 Outer) on GP15 (Pin 20) via SN74AHCT125N 5V shifter, 18% brightness cap, 28 effects. " +
  "OLED: SH1106 1.3-inch 128x64 I2C1 (0x3C) on SDA=GP2 (Pin 4), SCL=GP3 (Pin 5). " +
  "Controls: Left-side Bourns PEC11R encoder on GP6/GP7/GP8 (Pins 9/10/11) and small green QUERY button (PTT) on GP9 (Pin 12). " +
  "Audio In: Yellow INMP441 I2S MEMS microphone at base (PIO0 SM0, 16kHz) on SCK=GP10 (Pin 14), WS=GP11 (Pin 15), SD=GP12 (Pin 16). " +
  "Audio Out: MAX98357A I2S 3W Class-D amp + 4-ohm speaker (PIO1 SM0, 16kHz, 21 chiptunes + 14 SFX) on BCLK=GP16 (Pin 21), LRC=GP17 (Pin 22), DIN=GP18 (Pin 24). " +
  "Primary Host over Wi-Fi UDP port 5005: Android 15 Companion App v1.1.2 on smartphone hotspot; Optional Offline Host: Wearable Raspberry Pi 5 16GB running Whisper.cpp + Ollama + Piper TTS.";

export const REPO_TOPICS = [
  {
    id: 'creator_author',
    keywords: /(\b(who|creator|author|made|built|engineer|developer|designer|founder|owner|andrey|holz|derandr|linkedin|contact|profile)\b|кто|автор|создал|сделал|разработал|андрей|хольц|линкедин)/i,
    fact: "Celestial Engine Mk.V was created and engineered by Dr. Andrey Holz. Official LinkedIn profile: https://www.linkedin.com/in/andrey-holz/",
    personaReplies: {
      machine_god: "Celestial Engine Mk.V was architected and built by Dr. Andrey Holz. Connect on LinkedIn: https://www.linkedin.com/in/andrey-holz/",
      archmage: "This arcane staff was forged by the master artificer Dr. Andrey Holz. Seek his scroll on LinkedIn: https://www.linkedin.com/in/andrey-holz/",
      marvin: "Dr. Andrey Holz built me and wired my brain into this staff. Complain to him on LinkedIn: https://www.linkedin.com/in/andrey-holz/",
      cyberpunk: "This whole cyber-arcane rig was engineered by Dr. Andrey Holz, choom. Ping his net-profile on LinkedIn: https://www.linkedin.com/in/andrey-holz/",
      pumpkin_lord: "Dr. Andrey Holz conjured this glowing Halloween staff! Visit the master maker on LinkedIn: https://www.linkedin.com/in/andrey-holz/"
    }
  },
  {
    id: 'pinout',
    keywords: /\b(pin|pins|pinout|gpio|gp\d+|wire|wiring|breadboard|vtbb3|schematic|connect|connection)\b/i,
    fact: "RP2350 Pinout: OLED I2C1 on GP2/GP3 (Pins 4,5); PEC11R Encoder on GP6/GP7/GP8 (Pins 9-11); Green QUERY button on GP9 (Pin 12); INMP441 Mic on GP10/11/12 (Pins 14-16); 43 WS2812B LEDs on GP15 (Pin 20); MAX98357A Amp on GP16/17/18 (Pins 21,22,24).",
    personaReplies: {
      machine_god: "Canonical RP2350 pinout verified: OLED GP2-3, encoder GP6-8, green PTT button GP9, INMP441 mic GP10-12, WS2812B LEDs GP15, and MAX98357A amp GP16-18.",
      archmage: "Copper runes bind GP2-3 to the OLED glass, GP6-8 to the dial, GP9 to the emerald trigger, GP15 to forty-three stars, and GP10-18 to voice.",
      marvin: "My breadboard pinout: OLED on GP2-3, encoder GP6-8, green query button GP9, yellow mic GP10-12, LEDs GP15, and speaker amp GP16-18.",
      cyberpunk: "Pin map locked, choom: I2C1 OLED GP2-3, encoder GP6-8, green PTT trigger GP9, INMP441 mic GP10-12, 43 LEDs GP15, and MAX98357A amp GP16-18.",
      pumpkin_lord: "Haunted wiring map: GP2-3 light the blue OLED, GP6-8 read the knob, GP9 is the green button, GP15 sparks LEDs, and GP10-18 carry sound!"
    }
  },
  {
    id: 'mic',
    keywords: /\b(mic|microphone|inmp441|listen|voice input|ptt|query button|green button|gp9|gp10|gp11|gp12|yellow mic)\b/i,
    fact: "Voice input uses a bottom-mounted yellow INMP441 omnidirectional I2S MEMS microphone driven by RP2350 PIO0 SM0 at 16 kHz (SCK=GP10 Pin 14, WS=GP11 Pin 15, SD=GP12 Pin 16), triggered by holding the green side QUERY button on GP9 (Pin 12).",
    personaReplies: {
      machine_god: "Acoustic capture uses the bottom yellow INMP441 I2S MEMS microphone on GP10, GP11, and GP12 at 16 kHz via PIO0 SM0, gated by GP9.",
      archmage: "Hold the emerald rune on GP9 and speak: the golden INMP441 microphone at the base captures your voice at sixteen kilohertz on GP10-12.",
      marvin: "Hold the little green button on GP9, and the yellow INMP441 microphone at the bottom streams your voice over GP10, GP11, and GP12.",
      cyberpunk: "Hold the green GP9 side trigger and talk into the bottom yellow INMP441 mic—PIO0 SM0 streams 16 kHz PCM across GP10-12 to UDP 5005.",
      pumpkin_lord: "Press the green button on GP9 and whisper into the golden INMP441 microphone at the base—it hears every word on GP10, GP11, and GP12!"
    }
  },
  {
    id: 'leds',
    keywords: /\b(led|leds|ring|rings|ws2812|ws2812b|dome|light|lighting|effect|effects|gp15|74ahct125|brightness|43)\b/i,
    fact: "The clear top dome houses 43 addressable WS2812B LEDs in 3 concentric rings (7 Core + 12 Inner + 24 Outer; the 4th 16-LED ring was removed). Driven on GP15 (Pin 20) via PIO0 SM1 DMA and SN74AHCT125N 5V level shifter with an 18% brightness cap and 24 animations.",
    personaReplies: {
      machine_god: "Optical array comprises 43 WS2812B emitters across three concentric rings—7 core, 12 inner, 24 outer—driven on GP15 via PIO0 SM1 DMA at 18 percent cap.",
      archmage: "Beneath the clear bell dome shine forty-three stars in three sacred circles of seven, twelve, and twenty-four, guided by GP15 through twenty-four arcane patterns.",
      marvin: "Forty-three WS2812B LEDs in three rings of 7, 12, and 24 on GP15, capped at 18 percent brightness so my power rail does not faint.",
      cyberpunk: "Three concentric neon rings under the clear dome—7 core, 12 inner, 24 outer—clocked at 800 kHz on GP15 through a 74AHCT125N 5V shifter.",
      pumpkin_lord: "Forty-three glowing lights spin inside the clear dome in three rings of seven, twelve, and twenty-four on GP15, casting twenty-four eerie light shows!"
    }
  },
  {
    id: 'audio_out',
    keywords: /\b(speaker|amp|amplifier|max98357|max98357a|music|chiptune|synth|song|track|tracks|sound|audio|gp16|gp17|gp18)\b/i,
    fact: "Audio output uses a MAX98357A Class-D I2S mono amplifier (9 dB gain) and 4-ohm 3W QUARKZMAN speaker on BCLK=GP16 (Pin 21), LRC=GP17 (Pin 22), and DIN=GP18 (Pin 24). Core 1 synthesizes 24 8-bit chiptune tracks and plays 16 kHz TTS audio.",
    personaReplies: {
      machine_god: "PIO1 SM0 drives the MAX98357A Class-D amplifier on GP16, GP17, and GP18 into a 4-ohm 3-watt transducer, synthesizing 24 sample-accurate 16 kHz chiptunes.",
      archmage: "Through GP16, GP17, and GP18, the MAX98357A amplifier awakens the three-watt voice of the staff, singing twenty-four ancient melodies and spoken prophecies.",
      marvin: "Core 1 synthesizes twenty-four 8-bit tunes at 16 kilohertz through a MAX98357A amp on GP16, GP17, and GP18. Even my sighs are 16-bit mono.",
      cyberpunk: "PIO1 SM0 pumps 16 kHz PCM and 24 retro chiptune tracks over GP16, GP17, and GP18 into a 3.2-watt MAX98357A Class-D amp and 4-ohm speaker.",
      pumpkin_lord: "The MAX98357A amp on GP16, GP17, and GP18 blasts twenty-four retro chiptune melodies and ghostly voices through our three-watt speaker!"
    }
  },
  {
    id: 'oled_encoder',
    keywords: /\b(oled|screen|display|sh1106|i2c|hud|encoder|knob|pec11r|turn|rotate|menu|mode|modes|gp2|gp3|gp6|gp7|gp8)\b/i,
    fact: "The 1.3-inch SH1106 128x64 I2C1 OLED (0x3C) runs at 400 kHz on GP2 (SDA) and GP3 (SCL), rendering 13 system modes and a 20 FPS ticker. Navigation uses the left-side Bourns PEC11R rotary encoder on GP6 (CLK), GP7 (DT), and GP8 (SW).",
    personaReplies: {
      machine_god: "Visual telemetry renders at 20 FPS on the 128x64 SH1106 I2C1 OLED via GP2/GP3, navigated by the PEC11R encoder on GP6, GP7, and GP8.",
      archmage: "Turn the silver PEC11R dial on GP6-GP8 to scroll thirteen arcane modes upon the glowing blue SH1106 scrying glass on GP2 and GP3.",
      marvin: "A tiny 128-by-64 SH1106 OLED on GP2/GP3 displays my thirteen system modes while you twist the PEC11R rotary encoder on GP6, GP7, and GP8.",
      cyberpunk: "400 kHz I2C1 on GP2/GP3 drives the 1.3-inch SH1106 cyber-HUD, while the left-hand PEC11R encoder on GP6-GP8 switches modes and tracks.",
      pumpkin_lord: "Spin the side encoder on GP6, GP7, and GP8 to flip through thirteen haunted modes on our glowing blue 1.3-inch SH1106 OLED!"
    }
  },
  {
    id: 'network_udp',
    keywords: /\b(udp|wifi|wi-fi|network|packet|protocol|port|5005|cyw43|cyw43439|ip|hotspot|latency|crc|opcode)\b/i,
    fact: "The Pico 2 W connects via CYW43439 Wi-Fi to 10.169.248.1 and communicates over binary UDP v2 on port 5005 (<18ms latency). Packets use a 4-byte FNV-1a magic header (0x811C9DC5), 1-byte opcode (0x01..0x05), uint16 length, payload, and CRC-16-CCITT.",
    personaReplies: {
      machine_god: "Binary UDP v2 transport operates on port 5005 with FNV-1a header 0x811C9DC5 and CRC-16 verification, achieving sub-18-millisecond round-trip latency.",
      archmage: "Invisible Wi-Fi threads bind the staff to its twin minds over port 5005, sealing every packet with the 0x811C9DC5 sigil and CRC-16 ward.",
      marvin: "We chat over port 5005 using binary UDP v2 packets stamped with 0x811C9DC5 and CRC-16 checksums, delivering questions in under 18 milliseconds.",
      cyberpunk: "Zero-copy binary UDP v2 on port 5005: 0x811C9DC5 magic header, opcodes 0x01-0x05, CRC-16-CCITT check, and sub-18ms latency on 10.169.248.x.",
      pumpkin_lord: "Our spectral Wi-Fi link flies over UDP port 5005 in under eighteen milliseconds, guarded by the magic 0x811C9DC5 header and CRC-16 spell!"
    }
  },
  {
    id: 'brains_ai',
    keywords: /\b(brain|dual-brain|android|pixel|companion|app|apk|pi 5|pi5|raspberry pi 5|wearable|ollama|whisper|piper|gemini|gemma|persona|personas)\b/i,
    fact: "Celestial Engine Mk.V uses a Dual-Brain AI architecture: (1) Android 15 Companion App v1.1.2 on Pixel 9 Pro XL (Gemini 2.5 Flash cloud voice, Spell Forge, 6 personas); (2) Offline Wearable Raspberry Pi 5 16GB with 1TB Lexar NM790 NVMe running Whisper.cpp, Ollama, and Piper TTS.",
    personaReplies: {
      machine_god: "Dual-Brain redundancy combines Android 15 Companion v1.1.2 on Pixel 9 Pro XL with an offline Pi 5 16GB NVMe node running Whisper, Ollama, and Piper.",
      archmage: "Two great minds serve the staff: the Pixel 9 grimoire channeling Gemini, and the wearable Pi 5 crystal running Whisper, Ollama, and Piper offline.",
      marvin: "I have three brains: an RP2350 in the staff, an Android 15 Pixel 9 app running Gemini, and a wearable Pi 5 running Ollama.",
      cyberpunk: "Dual-brain neural rig: Pixel 9 Pro XL running Android 15 v1.1.2 with Gemini 2.5 Flash, plus a wearable 16GB Pi 5 running local Ollama.",
      pumpkin_lord: "Two brains are spookier than one! The Android 15 Pixel app summons Gemini magic, while the wearable Pi 5 runs Whisper and Ollama offline!"
    }
  },
  {
    id: 'power',
    keywords: /\b(power|battery|voltage|5v|3\.3v|3v3|vbus|usb|capacitor|1000uf|geekworm|18650|molicel|isolation)\b/i,
    fact: "Safety rule: Staff power is a dedicated 5V USB power bank into Pico VBUS (Pin 40) with a 1000uF decoupling capacitor and strict star ground. Wearable Pi 5 power is completely separate (Geekworm X1209 UPS + four Molicel 18650 3500mAh cells).",
    personaReplies: {
      machine_god: "Power domains are strictly isolated: 5V USB VBUS with a 1000uF reservoir feeds the staff, while four Molicel 18650 cells power the Pi 5.",
      archmage: "Two separate wells of energy sustain the order: a five-volt USB vessel for the staff's stars, and four 18650 cells for the Pi 5.",
      marvin: "The staff runs on a 5V USB power bank with a 1000uF capacitor, kept strictly separate from the Pi 5's 18650 battery pack.",
      cyberpunk: "Clean 5V USB VBUS rail with a 1000uF cap powers the staff head, totally isolated from the wearable Pi 5's quad-18650 UPS pack.",
      pumpkin_lord: "Keep the potions separate! A 5V USB power bank feeds the glowing staff, while four 18650 cells power the wearable Pi 5 backpack!"
    }
  },
  {
    id: 'repo_overview',
    keywords: /\b(repo|repository|project|celestial|mk\.?v|firmware|code|c\+\+|cmake|uf2|release|version|github|what is this|how does it work)\b/i,
    fact: REPO_SUMMARY_COMPACT,
    personaReplies: {
      machine_god: "Celestial Engine Mk.V (v1.2.0) integrates RP2350 dual-core C firmware, 43 WS2812B LEDs, dual PIO I2S audio, Android 15 v1.1.2, and a Pi 5 server.",
      archmage: "This repository is the grimoire of Celestial Engine Mk.V—uniting RP2350 firmware, forty-three stars, a 1.3-inch OLED, and twin AI minds over Wi-Fi.",
      marvin: "This is the Celestial Engine Mk.V repository: bare-metal RP2350 C firmware, 43 LEDs, dual I2S audio, an Android 15 app, and a Pi 5 server.",
      cyberpunk: "Celestial Engine Mk.V (v1.2.0): dual-core RP2350 C firmware, 43 WS2812B LEDs, 16 kHz I2S audio, Android 15 v1.1.2, and Pi 5 AI.",
      pumpkin_lord: "Welcome to Celestial Engine Mk.V! Inside this repo live RP2350 C firmware, forty-three LEDs, 8-bit chiptunes, and our Android 15 companion app!"
    }
  }
];

/**
 * Retrieve relevant repository facts for a user question to ground LLM or Scripted answers.
 */
export function retrieveRepoKnowledge(question = '') {
  const q = (question || '').trim();
  if (!q) {
    return { matchedTopics: [], contextString: '' };
  }

  const matchedTopics = REPO_TOPICS.filter((topic) => topic.keywords.test(q));
  if (matchedTopics.length === 0) {
    return { matchedTopics: [], contextString: '' };
  }

  // Prefer specific topic facts over the full summary when both match, keeping prompt prefill fast on mobile
  const specificTopics = matchedTopics.filter((t) => t.id !== 'repo_overview');
  const chosenTopics = specificTopics.length > 0 ? specificTopics.slice(0, 2) : matchedTopics.slice(0, 1);
  const facts = chosenTopics.map((t) => t.fact).join(' ');
  return {
    matchedTopics,
    contextString: facts
  };
}

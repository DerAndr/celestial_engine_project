# Experiment 056: Extended 16-Bit Chiptune Arrangements & Pop-Culture Thematic Packs Suite

- **Date:** 2026-10-27
- **Target:** Raspberry Pi Pico 2 W (RP2350) + MAX98357A I2S DAC + 43 WS2812B LEDs + Android Companion App (Google Pixel 9 Pro XL)
- **Status:** **PASS (Hardware Flashed & In-App UI Integrated)**

---

## 1. Goal
1. Upgrade chiptune sound synthesis timbre from thin sine waves to authentic 16-bit arcade / Megadrive odd harmonics (`0.52 * sin + 0.22 * sin(3x) + 0.10 * sin(5x)`) with a punchy sub-octave bass harmonic (`0.16 * sin(0.5x)`).
2. Extend all melodic arrangements from 3–5 seconds to complete, extended 15–25 second iconic themes (45–65 notes each) with intros, hooks, verses, bridges, and basswalks.
3. Implement Daft Punk (*Harder, Better, Faster, Stronger*) and the 4 backlog pop-culture packs:
   - **eBay Chaos Auction**: 4-color racing strobe, cash register "cha-ching!" SFX, and accelerating arcade bidding war.
   - **Harry Potter Pack**: Hogwarts 4 House Runes, 16-bit Hedwig's Theme waltz, and Lumos / Expecto Patronum spells.
   - **Pokémon Pack**: Concentric Red & White Pokéball with pulsing button, Wild Pokémon Battle Theme, and Pokémon Center Healing Jingle.
   - **Sailor Moon Pack**: Moon Prism Power pastel transformation spiral and 8-bit *Moonlight Densetsu* (ムーンライト伝説).
4. Integrate a dedicated **Thematic Packs & Jukebox** tab in the Android Companion App (`SoundboardScreen.kt`) with direct "Play Theme & Runes" buttons and quick action SFX triggers.

---

## 2. Firmware Implementation Details

### Audio Synthesizer Upgrade (`firmware/src/audio.c`, `firmware/src/audio.h`)
- **Arcade Waveform Synthesis**:
  In `fill_audio_buffer()`, when `active_chiptune_track >= 0` or retro SFX is active:
  ```c
  float s3 = sinf(current_phase * 3.0f);
  float s5 = sinf(current_phase * 5.0f);
  float sub = sinf(current_phase * 0.5f);
  warm_wave = 0.52f * s1 + 0.22f * s3 + 0.10f * s5 + 0.16f * sub;
  ```
- **Extended & New Melodic Tracks**:
  - `DOOM_THEME`: Extended 64-note driving metal riff (Part 1, Part 2, Bridge in G, Bridge in C, Final Punch).
  - `TETRIS_THEME`: Extended 60-note Korobeiniki (Stanza A, Stanza B high octave, Russian folk bass walk, Cadence).
  - `ZELDA_THEME`: Extended 58-note Overworld March (Fanfare intro, Main theme A, Main theme B, Heroic resolution).
  - `IMPERIAL_THEME`: Extended 62-note Imperial March (Phrase A, Phrase B, Triplet marches, Minor modulation).
  - `GHOSTBUSTERS_THEME`: Extended 54-note Ray Parker Jr. groove (Slap bass, Vocal hook, Fanfare, Bass walk).
  - `MORROWIND_THEME`: Extended 52-note Jeremy Soule theme (Majestic phrase 1, Dorian phrase 2, Horn bridge, Grand climax).
  - `SPOOKY_SKELETONS_THEME`: Extended 54-note electro-swing (Hook 1, Hook 2, Xylophone bone-rattles, Swing reprise).
  - `DAFT_PUNK_THEME`: 52-note French touch electro (Intro groove, Vocoder hook, Verse, Arpeggio climax).
  - `EBAY_THEME`: 50-note accelerating bidding frenzy (Chant, Accelerando, Chromatic run, Gavel thuds, Cha-ching bell).
  - `HARRY_POTTER_THEME`: 56-note complete Hedwig's Theme waltz.
  - `POKEMON_THEME`: 58-note Wild Battle groove + chromatic descent + Center healing jingle.
  - `SAILOR_MOON_THEME`: 56-note Moonlight Densetsu opening verse & romantic chorus.
- **Dedicated Sound Effects**:
  - `COSMIC_SFX_CHA_CHING`: High bell ting (1864 Hz, 2349 Hz) + mechanical drawer slide.
  - `COSMIC_SFX_POKECENTER`: 6-note Pokémon Center heal jingle (B4, E5, G#5, B5, G#5, B5).
  - `COSMIC_SFX_LUMOS`: Shimmering high-pitch wand flare (1046 Hz -> 1567 Hz -> 2093 Hz -> 3135 Hz).
  - `COSMIC_SFX_PATRONUS`: Rising silver mist celestial wave (220 Hz -> 329 Hz -> 440 Hz -> 659 Hz -> 880 Hz -> 1318 Hz).

### Synchronized LED Runes (`firmware/src/leds.c`, `firmware/src/leds.h`)
- Added new animation modes:
  - `LED_ANIM_DAFT_PUNK`: Cyberpunk Chrome Gold & Silver Visor (Guy-Manuel gold EQ + Bangalter horizontal silver visor sweep + 125 BPM core kick).
  - `LED_ANIM_EBAY`: 4-Color Auction Strobe (eBay Red, Blue, Yellow, Green racing strobes with accelerating rotation + explosive white gavel strike).
  - `LED_ANIM_HARRY_POTTER`: Hogwarts 4 House Runes (Gryffindor Scarlet/Gold, Slytherin Emerald/Silver, Ravenclaw Azure/Bronze, Hufflepuff Canary/Charcoal + Lumos core flare + Golden Snitch orbit).
  - `LED_ANIM_POKEMON`: Concentric Pokéball (Top half Fire Red, Bottom half Ceramic White, Black equatorial divider latches, pulsing wild Pokémon center button).
  - `LED_ANIM_SAILOR_MOON`: Moon Prism Power Pastel Transformation (Rose Pink, Crescent Gold, Starlight Aqua spiral with shimmering Silver Crystal core).
- Updated `sync_chiptune_thematic_leds()` in `main.c` so playing any track automatically engages its matching LED animation.

---

## 3. Companion App Upgrade (`SoundboardScreen.kt`, `StaffModels.kt`)
1. **Model Expansion**:
   - Added `ThematicPack` data class with full metadata for all 13 thematic packs.
   - Added `LED_ANIM_DAFT_PUNK`, `LED_ANIM_EBAY`, `LED_ANIM_HARRY_POTTER`, `LED_ANIM_POKEMON`, `LED_ANIM_SAILOR_MOON` to `LedAnimation`.
   - Added `SfxItem(10..13)` for Cha-Ching, Pokécenter, Lumos, and Patronus.
2. **Interactive UI Redesign**:
   - Replaced basic SFX grid in `SoundboardScreen.kt` with a 3-tab tabbed interface:
     - **🎮 THEMES (13)**: Rich cards for Doom, Tetris, Zelda, Star Wars, Ghostbusters, Morrowind, Spooky, Daft Punk, eBay, Harry Potter, Pokémon, Sailor Moon, and Mario.
       - Each card features a direct **"Play Theme & Runes"** button commanding the staff to start music + lights simultaneously.
       - Dedicated quick-action chips for special SFX (e.g., eBay's "Cha-Ching! 💰", Harry Potter's "Lumos! 🪄" / "Patronus! 🦅", Pokémon's "PokéCenter Heal 🏥").
     - **🌌 SFX (14)**: Grid of all 14 cosmic & droid sound effects.
     - **🪄 COMBOS**: Arcane sound combos and preset spell sequences.
3. **Concentric Rings Visualizer**:
   - Added visual simulation in `ConcentricLedRingsView.kt` for all 5 new modes.

---

## 4. Verification
- Compiled Pico 2 W firmware with CMake: `celestial_firmware.uf2` generated and flashed to Pico 2 W via BOOTSEL `D:\`. Staff booted cleanly on `COM9`.
- Verified Kotlin compilation via GitHub Actions CI run `40417849817` (`app-debug.apk` built and packaged).
- Cleanly reinstalled APK on Google Pixel 9 Pro XL (`49051FDAS001YK`) resolving previous signature mismatch (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`).
- Screen capture verified:
  - App launched successfully and displays live telemetry from staff `10.169.248.64` (State: IDLE, Vol: 14%, Tick #14001+).
  - Audio Soundboard displays all 3 tabs: `[ 🎮 THEMES (13) ]`, `[ 🌌 SFX (14) ]`, `[ 🪄 COMBOS ]`.
  - All 13 thematic packs (Doom, Tetris, Zelda, Star Wars, Ghostbusters, Morrowind, Spooky Skeletons, Daft Punk, eBay Chaos Auction, Harry Potter, Pokémon, Sailor Moon, Super Mario) rendered with full cards, "Play Theme & Runes" buttons, and quick-action SFX chips.
  - Runes screen verified with 35 interactive LED presets and real-time concentric ring simulation.

# Experiment 061: Fix Jukebox Early Stop and Subsequent Track Playback

## Status: VERIFIED & FLASHED

## 1. Goal
Fix chiptune early stop via rotary switch and fix playback of subsequent tracks in Jukebox.

## 2. Root Cause Analysis
1. **Mode Hijacking in `sync_chiptune_thematic_leds()`**:
   Whenever a chiptune with thematic LEDs (Doom, Tetris, Zelda, Imperial March, Ghostbusters, etc.) was triggered, `sync_chiptune_thematic_leds()` executed:
   ```c
   current_mode_idx = MODE_LED_CORE;
   active_confirmed_mode = MODE_LED_CORE;
   led_edit_mode = false;
   ```
   This changed the global dashboard mode away from `MODE_CHIPTUNE` to `MODE_LED_CORE`.
2. **Inverted Switch Priority in `encoder_switch_clicked()`**:
   `else if (current_mode_idx == MODE_LED_CORE)` was evaluated **before** `else if (chiptune_mode)`.
   Because `current_mode_idx` was hijacked to `MODE_LED_CORE`, any subsequent press of the encoder switch hit `led_edit_mode = true;` instead of `chiptune_mode`. The user was trapped in the LED Effects menu and could not stop playback or select another track.
3. **No Dedicated Stop Button in Jukebox**:
   Inside Jukebox, Row 0 only toggled play modes (`SINGLE/ALL/CYCLE`). Stopping required navigating to the exact playing track.
4. **Tone Generator Clash**:
   `audio_play_blip()` at the start of chiptunes overwrote `current_freq` with 440 Hz, clashing with note 0.

## 3. Implemented Fixes
1. **Removed Mode Hijacking**: `sync_chiptune_thematic_leds()` now sets only the LED animation (`leds_set_anim_mode` and `active_anim_idx`). It never alters `current_mode_idx` or `active_confirmed_mode`.
2. **Reordered Switch Dispatch Priority**: Sub-menu states (`chiptune_mode`, `voice_mode`, `cosmic_mode`) are now evaluated **before** any base `current_mode_idx` checks.
3. **Interactive Stop Options**:
   - Inside Jukebox: Row 0 dynamically displays `[ STOP PLAYBACK ]` when audio is playing. Clicking it immediately stops music.
   - Inside Jukebox: Clicking the currently active track (`[STOP]`) stops playback. Clicking a different track cleanly aborts the current track and starts the new one.
   - On Dashboard (`MODE_CHIPTUNE`): Clicking the encoder switch stops playback immediately if music is playing, or enters Jukebox if idle. Marquee dynamically indicates `// CELESTIAL MUSIC PLAYING // CLICK SW TO STOP //`.
4. **Instant OLED Refresh**: Added playback edge detection (`cur_chiptune_playing != prev_chiptune_playing`) to refresh the OLED instantly upon playback start/stop.
5. **Clean Tone Initialization**: Removed `audio_play_blip()` on chiptune trigger to prevent sound generator clashing.

## 4. Hardware Verification
- Firmware compiled with `cmake --build firmware/build --target celestial_firmware`.
- Flashed via BOOTSEL to RP2350 (Pico 2 W).
- Verified via serial COM9:
  - Track triggering via `M 2` (Doom) and seamless switch to `M 1` (Tetris).
  - Stop command `S` halts audio without mode change.
  - Heartbeat confirms `Screen: [GUIDE]` (current mode) is preserved throughout music playback.

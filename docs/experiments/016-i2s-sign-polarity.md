# 016 — I2S sign-polarity isolation (ERR-AUDIO-02/03)

Date: 2026-10-10. Firmware **1.0.1-AUDIO-POLARITY**, built, flashed (picotool verify OK) by the agent. Status: **PENDING user listening result**.

## Goal
Determine whether loud, volume-independent output is caused by negative (sign-bit set) samples being mis-decoded on the I2S link.

## Evidence before this step
- User: still very loud at Vol 3%, encoder changes do not change loudness. V0 silent.
- Serial `A` after boot chime at 3%: `peak:7` (≈ −73 dBFS). Digital attenuation works; the PCM sent is tiny.
- Experiment 013: probe with **only non-negative** samples (0..16) → full silence. A signed signal was never tested in isolation.
- Hypothesis: small negative values (0xFFF8…) become full scale at the amplifier → wiring (e.g. BCLK/LRC swapped, bad DIN/GND contact) or frame misalignment. Not yet established.

## Parts / wiring
No change. Existing Pico 2 W, MAX98357A, one 4 Ω speaker at OUT+/OUT− only.

## Power off
No wiring in this step. Disconnect USB before touching any wire.

## Code/build changes
- `audio_play_polarity_tone()`: 1 s 440 Hz at current master volume, samples all ≥ 0 (`U`) or all ≤ 0 (`N`), same peak magnitude.
- Serial `U`, `N`; serial `A` (listed in help but previously unhandled) now prints diagnostics.

## Commands
```powershell
$p = [System.IO.Ports.SerialPort]::new('COM9', 115200); $p.DtrEnable = $true; $p.Open()
$p.Write("V3`n"); Start-Sleep 1; $p.Write("U`n"); Start-Sleep 2; $p.Write("N`n"); Start-Sleep 2; $p.Write("A`n"); Start-Sleep -Milliseconds 300; $p.ReadExisting(); $p.Close()
```

## Expected result
- Correct link: U and N both very quiet/inaudible, same level.
- Sign-decoding fault: U quiet, N loud buzz.

## If it fails
1. N loud, U quiet → photo of the amplifier pins + Pico pins 21–24 + wires; check BCLK↔GP16, LRC↔GP17, DIN↔GP18, GND↔pin 23 continuity.
2. Both loud → amplifier is not following PCM at all: check GND, VIN, module identity (A vs B variant) with photo.
3. Both silent → the link decodes correctly; investigate application sources instead.

## Observed result
2026-10-10 21:15, Vol 3%: **U quiet, N loud hum** (user). Serial: `peak:36`, tx_stalls 0, late_refills 0, restarts 0. Mic level rose 44% → 97% during N.

Conclusion: the digital PCM is correctly attenuated; negative samples are mis-decoded on the physical link. This explains ERR-AUDIO-02/03 (all real audio has negative half-waves → full-scale hum, volume-independent).

Analysis: a one-bit framing error (amp reads previous LSB as sign) would also make odd positive values loud, but U (values 0..36) was quiet, so pure frame misalignment is unlikely. Most consistent with swapped signal wires:
- **LRC ↔ DIN swapped** (Pico GP17/GP18): positive data keeps the amp's "LRCLK" line almost constant 0 → no frames → silence; negative data toggles it and the real LRCLK square wave is decoded as full-scale data → hum.
- **BCLK ↔ LRC swapped** (GP16/GP17): amp samples one high bit per frame → 0 for positive, 1 for negative.

Next step: wiring check / photo before any code change. ERR-AUDIO-02/03 remain OPEN.

### Follow-up 21:26
- User swapped signal wires: any swap → no working audio; only the original order produces sound. Order BCLK=GP16, LRC=GP17, DIN=GP18 is therefore taken as correct. U still quiet, N still loud.
- User: amplifier VIN is on the shared + rail (with other loads), not a dedicated wire.
- Revised suspicion: poor solder joint / missing ground path. If the amplifier GND is open or high-resistance, the chip can take its ground through the input protection diodes of logic pins held LOW. Positive small PCM keeps DIN mostly LOW (ground path present → quiet); negative PCM keeps DIN mostly HIGH (path lost → ground floats → loud hum). A DIN/GND joint fault can therefore give partial operation, not total silence.
- Requested: continuity of amp GND/BCLK/LRC/DIN to Pico pins 23/21/22/24, VIN–GND voltage measured at the amp pins, photo of header solder joints and rails.
- Rejected workaround: DC-biasing PCM so samples are never negative would put DC across the bridged speaker at higher volume and hide the hardware fault.

### Resolution 21:43 — PASS
- User re-soldered the amplifier header: **root cause was a bad solder joint**. After the fix, audio is clean and volume-dependent; at 3% it is very quiet.
- ERR-AUDIO-02 (loud hum) and ERR-AUDIO-03 (volume ineffective) **RESOLVED** by hardware repair. No firmware framing change was needed.
- Follow-up: `AUDIO_DEFAULT_VOLUME` 3 → 10 (+3.4 dB, ceiling 8000 unchanged). Encoder still adjusts 0..100% in 1% (~0.49 dB) steps.

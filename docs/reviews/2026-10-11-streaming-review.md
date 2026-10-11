# Review — voice response lifecycle and Pi 5 readiness

Reviewed baseline: `1e9ba65` (1.0.15 main), following `53d9764` voice capture. Scope: firmware audio/network/state/OLED paths, PC brain and mock service, current hardware/docs. User reports general stability, truncated audio, late/re-sent answers and missing full-screen text. Fourth ring discarded by user; no rewiring/redesign required.

## Findings fixed in 1.0.16 + protocol v2

| Priority | Finding / evidence | Change |
|---|---|---|
| P1 | Server sent 512 bytes every 13 ms for 16 ms of audio; 32768-sample ring had no full check. Unplayed samples overwritten; C reproduction failed before fix. | Reject full-ring writes, paced absolute sample clock, ordered ACK/retry without advancing on rejection. |
| P1 | Each redundant RESP_START reset counters and queued audio. No sequence checking; lost/repeated UDP packets silently altered speech. | Idempotent START, expected sample count, response sequence gate and stop-and-wait retransmission. END requires all samples. |
| P1 | After 45 s main returned IDLE without cancelling reception. Receiver auto-started arbitrary late chunks; worker ignored CANCEL. | Request IDs, random boot seed, explicit progress and bounded deadline; cancellation invalidates ID and suppresses old worker delivery. |
| P1 | RESP_END loss was treated as success after 1.5 s; main could then replay the one-second preview after streaming. | No invented successful EOF; stalled stream fails. Preview/replay code removed. |
| P1 | Receiver copied unchecked payload_len into a 512-byte stack array and trusted any sender. | Exact datagram length, payload limit, PCM alignment, configured address/port and request checks before copying. |
| P1 | Server speech inherited mic replay gain ×5 and clipped against 14400. C ramp reproduction failed before fix. | Separate full-scale stream gain; preserve mic replay boost and existing volume curve/peak ceiling. Speech may sound quieter and cleaner. |
| P1 | At V0 clocks/DMA stopped but queued stream stayed active forever. Native C reproduction failed. | Discard muted samples while acknowledging them, preserve text/transaction completion, never wake the DAC at V0. |
| P2 | Parallel workers wrote the same query.wav; duplicate QUERY_END launched duplicate workers. | Per-session PCM and unique temporary WAV, idempotent END, bounded workers and cancellation checks between generation stages. |
| P2 | Relative Windows sleep rounding accumulated per packet; transfer took longer than audio duration, causing starvation. | Pace against one absolute monotonic sample clock, not repeated relative sleeps. |
| P2 | Old 7-pixel marquee remained inside a header/equalizer layout. | Dedicated full-screen response drawing: 8× glyphs, 56 px high, clipped scrolling edges, elapsed time reset for each answer. |
| P2 | 85-token LLM cap could end a sentence; ffmpeg removed internal pauses and conversion failure was not checked. | 384-token budget, reject length/empty result into existing fallback, preserve PCM pauses, check conversion exit/timeout. |

## Validation and limits

Native C checks run actual production sources: existing 17 probes plus response overflow, server-PCM dynamics, full-screen frame motion and pure C request gate checks. Both audio defects were reproduced as failing C checks before their fixes. Server checks use local UDP only: stale/cancelled sessions, malformed headers, and 14 s PCM with one deliberately dropped packet and ACK. No live paid STT/LLM/TTS request was made for validation.

Official SDK build targets Pico 2 W / RP2350. Physical playback, perceived loudness and OLED smoothness still need one user observation of the new pair; build/test results are not a new hardware PASS. [Experiment 036](../experiments/036-response-review-and-fullscreen-text.md) gives that milestone.

## Remaining work

1. Uplink is still best-effort UDP, so lost/reordered query samples can harm recognition. Add capture sequence recovery and real loss telemetry before portable/degraded-network claims.
2. Cancellation suppresses delivery but cannot instantly terminate a provider call already running. Timeouts and worker bounds prevent unbounded parallel work; restarting a server drops its sessions.
3. ASCII transliteration is current OLED behavior; native Cyrillic glyphs and keeping a complete long transcript on screen after speech are separate UI work.
4. Same-network/cloud Pi 5 migration is ready as a defined next milestone after the new pair is observed. Hotspot, local AI, service install, discovery and battery work are not implemented here.
5. Standalone local Pico controls/effects/music/phrases remain; QUERY currently requires a reachable server. Central state consolidation/settings persistence remain backlog.

SDK lock/callback handling checked against [official Raspberry Pi networking documentation](https://www.raspberrypi.com/documentation/pico-sdk/networking.html). Functional wire contract: [protocol v2](../protocol.md); portable-service handoff: [Pi 5 next stage](../pi5-next-stage.md).

# Brain protocol v2 — 2026-10-11

Pico and both Python brain servers must be updated together. Version 1 packets are intentionally rejected. UDP port is 5005; existing built-in Wi-Fi and GPIO assignments are unchanged.

Header: little-endian `<IBBHHI`, 14 bytes: magic `0x454C4543`, version `2`, message type, uint16 sequence, uint16 payload length, uint32 request ID. Datagram length must match the header exactly; payload maximum is 512 bytes. Audio is signed PCM16 little-endian, mono, 16 kHz. Pico duplicates QUERY_START/END; server handles these idempotently.

| Type | ID | Payload / behavior |
|---|---:|---|
| HEARTBEAT | 0x01 | ID zero; echoed immediately even during generation |
| QUERY_START | 0x02 | New nonzero ID; language + model bytes; replaces/cancels previous session |
| AUDIO_CHUNK | 0x03 | Up to 256 mono samples; upload remains best-effort, duplicate/old chunks ignored |
| QUERY_END | 0x04 | Starts generation once; redundant END never creates another worker |
| RESP_START | 0x05 | uint32 total sample count; duplicate START must not reset the stream |
| RESP_CHUNK | 0x06 | Audio sequence starts at zero; only next sequence accepted; previous sequence ACKed without replay |
| RESP_END | 0x07 | Sequence equals number of chunks; accepted only after all announced samples arrived |
| CANCEL | 0x08 | Stops this request; old workers cannot send another response |
| RESP_TEXT | 0x09 | ASCII/transliterated text, up to 500 bytes, shown as full-screen marquee |
| ACK | 0x0A | One-byte acknowledged type; echo request ID and sequence |
| PROGRESS | 0x0B | Generation still running for this ID; not a completed answer |
| ERROR | 0x0C | Explicit failure, not a silently truncated success |

Response TEXT/START/chunks/END use stop-and-wait with 200 ms retries and a 12 s packet deadline. The sender uses an absolute 16 kHz pacing clock; timer rounding must not accumulate per chunk. Pico's 32768-sample ring rejects writes when full. Sender retries without advancing sequence; it never overwrites queued audio. Response sequence count is bounded below 65536 chunks (about 17 minutes); server request lifetime is also bounded to 240 s including recording/generation/playback.

Generation sends PROGRESS once per second while heartbeat remains independent. Pico waits at most 180 s after QUERY release and requires response progress within 15 s. A receiving stream stalled for 10 s fails, including when Wi-Fi disconnects; missing END is never interpreted as successful completion. Timeout, error and a new query invalidate the old response ID and stop its audio. The final queued audio drains after a valid END, with no separate one-second preview replay.

At VOLUME 0, response packets are accepted and discarded without waking I2S/DMA; text still scrolls and END completes the transaction. Unmuting resumes later samples instead of replaying the muted portion. Native C checks reproduce and guard the former mute/DMA hang.

Limitations before portable use: request uploads still have no chunk retransmission/reordering buffer; degraded Wi-Fi can affect recognition. Server cancellation suppresses delivery, but cannot forcibly terminate an external STT/LLM/TTS call already in progress. Calls/workers have bounds; up to four generation workers and eight client slots are allowed. Peer address checks and request IDs do not provide authentication/encryption. For now use the existing trusted local network; this document is a functional contract, not a claim of production security.

Definitions: [C header](../firmware/src/net_protocol.h), [Python transport](../tools/brain_transport.py).

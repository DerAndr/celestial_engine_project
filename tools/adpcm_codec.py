#!/usr/bin/env python3
"""
Task 9.3: 4:1 IMA-ADPCM Low-Bandwidth Wireless Audio Codec (64 kbps vs 256 kbps PCM).
1:1 parity with firmware/src/adpcm_codec.c and android/.../ImaAdpcmCodec.kt.
"""

import struct

HEADER_BYTES = 4

IMA_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
)

IMA_INDEX_TABLE = (
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
)


def _clamp_s16(val: int) -> int:
    return max(-32768, min(32767, val))


def _clamp_idx(idx: int) -> int:
    return max(0, min(88, idx))


def _encode_nibble(sample: int, predictor: int, step_index: int):
    step = IMA_STEP_TABLE[step_index]
    diff = sample - predictor
    nibble = 0
    if diff < 0:
        nibble = 8
        diff = -diff
    delta = step >> 3
    if diff >= step:
        nibble |= 4
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        nibble |= 2
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        nibble |= 1
        delta += step
    predictor = _clamp_s16(predictor - delta if (nibble & 8) else predictor + delta)
    step_index = _clamp_idx(step_index + IMA_INDEX_TABLE[nibble])
    return nibble, predictor, step_index


def _decode_nibble(nibble: int, predictor: int, step_index: int):
    step = IMA_STEP_TABLE[step_index]
    delta = step >> 3
    if nibble & 4:
        delta += step
    if nibble & 2:
        delta += step >> 1
    if nibble & 1:
        delta += step >> 2
    predictor = _clamp_s16(predictor - delta if (nibble & 8) else predictor + delta)
    step_index = _clamp_idx(step_index + IMA_INDEX_TABLE[nibble & 0x0F])
    return predictor, predictor, step_index


def encode_block(pcm_bytes: bytes) -> bytes:
    """Encodes little-endian 16-bit mono PCM bytes into a self-contained IMA-ADPCM block."""
    num_samples = len(pcm_bytes) // 2
    if num_samples <= 0:
        return b""
    samples = struct.unpack(f"<{num_samples}h", pcm_bytes[: num_samples * 2])
    predictor = samples[0]
    init_diff = abs(samples[1] - predictor) if num_samples > 1 else 0
    step_index = 0
    while step_index < 88 and IMA_STEP_TABLE[step_index] < init_diff:
        step_index += 1
    out = bytearray(struct.pack("<hBB", predictor, step_index, 0))
    for i in range(0, num_samples, 2):
        lo, predictor, step_index = _encode_nibble(samples[i], predictor, step_index)
        if i + 1 < num_samples:
            hi, predictor, step_index = _encode_nibble(samples[i + 1], predictor, step_index)
        else:
            hi = 0
        out.append((lo & 0x0F) | ((hi & 0x0F) << 4))
    return bytes(out)


def decode_block(adpcm_bytes: bytes, max_samples: int = None) -> bytes:
    """Decodes a self-contained IMA-ADPCM block back into little-endian 16-bit mono PCM."""
    if len(adpcm_bytes) <= HEADER_BYTES:
        return b""
    predictor, step_index, _ = struct.unpack("<hBB", adpcm_bytes[:HEADER_BYTES])
    step_index = _clamp_idx(step_index)
    limit = max_samples if max_samples is not None else (len(adpcm_bytes) - HEADER_BYTES) * 2
    out_samples = []
    for byte_val in adpcm_bytes[HEADER_BYTES:]:
        if len(out_samples) >= limit:
            break
        s0, predictor, step_index = _decode_nibble(byte_val & 0x0F, predictor, step_index)
        out_samples.append(s0)
        if len(out_samples) >= limit:
            break
        s1, predictor, step_index = _decode_nibble((byte_val >> 4) & 0x0F, predictor, step_index)
        out_samples.append(s1)
    return struct.pack(f"<{len(out_samples)}h", *out_samples)

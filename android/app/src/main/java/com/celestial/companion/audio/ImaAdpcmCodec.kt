package com.celestial.companion.audio

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Task 9.3: 4:1 IMA-ADPCM Wireless Audio Codec (`64 kbps` vs `256 kbps` raw 16 kHz PCM).
 *
 * Packet-loss resilient: each block begins with a 4-byte little-endian header:
 * `[int16 initial_predictor][uint8 step_index][uint8 reserved=0]`, followed by packed 4-bit nibbles.
 * 1:1 bit-exact parity with `firmware/src/adpcm_codec.c` and `tools/adpcm_codec.py`.
 */
object ImaAdpcmCodec {

    const val HEADER_BYTES = 4
    const val CODEC_ID_NONE = 0
    const val CODEC_ID_IMA_ADPCM = 1

    private val IMA_STEP_TABLE = intArrayOf(
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
        19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
        337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    )

    private val IMA_INDEX_TABLE = intArrayOf(
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    )

    fun encodeBlock(pcmBytes: ByteArray): ByteArray {
        val numSamples = pcmBytes.size / 2
        if (numSamples <= 0) return ByteArray(0)
        val inBb = ByteBuffer.wrap(pcmBytes).order(ByteOrder.LITTLE_ENDIAN)
        val samples = ShortArray(numSamples) { inBb.short }

        var predictor = samples[0].toInt()
        val initDiff = if (numSamples > 1) kotlin.math.abs(samples[1].toInt() - predictor) else 0
        var stepIndex = 0
        while (stepIndex < 88 && IMA_STEP_TABLE[stepIndex] < initDiff) {
            stepIndex++
        }

        val payloadBytes = (numSamples + 1) / 2
        val out = ByteArray(HEADER_BYTES + payloadBytes)
        out[0] = (predictor and 0xFF).toByte()
        out[1] = ((predictor shr 8) and 0xFF).toByte()
        out[2] = (stepIndex and 0xFF).toByte()
        out[3] = 0

        var outPos = HEADER_BYTES
        var i = 0
        while (i < numSamples) {
            val lo = encodeNibble(samples[i].toInt(), predictor, stepIndex).also {
                predictor = it.second
                stepIndex = it.third
            }.first
            val hi = if (i + 1 < numSamples) {
                encodeNibble(samples[i + 1].toInt(), predictor, stepIndex).also {
                    predictor = it.second
                    stepIndex = it.third
                }.first
            } else 0
            out[outPos++] = ((lo and 0x0F) or ((hi and 0x0F) shl 4)).toByte()
            i += 2
        }
        return out
    }

    fun decodeBlockToPcm16Le(adpcmBytes: ByteArray, maxSamples: Int = (adpcmBytes.size - HEADER_BYTES) * 2): ByteArray =
        decodeBlock(adpcmBytes, maxSamples)

    fun decodeBlock(adpcmBytes: ByteArray, maxSamples: Int = (adpcmBytes.size - HEADER_BYTES) * 2): ByteArray {
        if (adpcmBytes.size <= HEADER_BYTES || maxSamples <= 0) return ByteArray(0)
        var predictor = (((adpcmBytes[1].toInt() and 0xFF) shl 8) or (adpcmBytes[0].toInt() and 0xFF)).toShort().toInt()
        var stepIndex = (adpcmBytes[2].toInt() and 0xFF).coerceIn(0, 88)

        val outSamples = ArrayList<Short>(maxSamples)
        for (pos in HEADER_BYTES until adpcmBytes.size) {
            val b = adpcmBytes[pos].toInt() and 0xFF
            if (outSamples.size < maxSamples) {
                val (s0, p0, i0) = decodeNibble(b and 0x0F, predictor, stepIndex)
                predictor = p0
                stepIndex = i0
                outSamples.add(s0)
            }
            if (outSamples.size < maxSamples) {
                val (s1, p1, i1) = decodeNibble((b ushr 4) and 0x0F, predictor, stepIndex)
                predictor = p1
                stepIndex = i1
                outSamples.add(s1)
            }
        }

        val outBytes = ByteArray(outSamples.size * 2)
        val bb = ByteBuffer.wrap(outBytes).order(ByteOrder.LITTLE_ENDIAN)
        for (s in outSamples) bb.putShort(s)
        return outBytes
    }

    private fun encodeNibble(sample: Int, predictorIn: Int, stepIndexIn: Int): Triple<Int, Int, Int> {
        var step = IMA_STEP_TABLE[stepIndexIn]
        var diff = sample - predictorIn
        var nibble = 0
        if (diff < 0) {
            nibble = 8
            diff = -diff
        }
        var delta = step shr 3
        if (diff >= step) {
            nibble = nibble or 4
            diff -= step
            delta += step
        }
        step = step shr 1
        if (diff >= step) {
            nibble = nibble or 2
            diff -= step
            delta += step
        }
        step = step shr 1
        if (diff >= step) {
            nibble = nibble or 1
            delta += step
        }
        val nextPred = if ((nibble and 8) != 0) {
            (predictorIn - delta).coerceIn(-32768, 32767)
        } else {
            (predictorIn + delta).coerceIn(-32768, 32767)
        }
        val nextIdx = (stepIndexIn + IMA_INDEX_TABLE[nibble]).coerceIn(0, 88)
        return Triple(nibble, nextPred, nextIdx)
    }

    private fun decodeNibble(nibble: Int, predictorIn: Int, stepIndexIn: Int): Triple<Short, Int, Int> {
        val step = IMA_STEP_TABLE[stepIndexIn]
        var delta = step shr 3
        if ((nibble and 4) != 0) delta += step
        if ((nibble and 2) != 0) delta += (step shr 1)
        if ((nibble and 1) != 0) delta += (step shr 2)
        val nextPred = if ((nibble and 8) != 0) {
            (predictorIn - delta).coerceIn(-32768, 32767)
        } else {
            (predictorIn + delta).coerceIn(-32768, 32767)
        }
        val nextIdx = (stepIndexIn + IMA_INDEX_TABLE[nibble and 0x0F]).coerceIn(0, 88)
        return Triple(nextPred.toShort(), nextPred, nextIdx)
    }
}

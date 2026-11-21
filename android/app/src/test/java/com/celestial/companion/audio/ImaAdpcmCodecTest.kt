package com.celestial.companion.audio

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.log10
import kotlin.math.sin

class ImaAdpcmCodecTest {

    @Test
    fun `deterministic 16-sample vector matches C and Python header and encoded bytes bit-for-bit`() {
        val inputSamples = shortArrayOf(
            0, 2500, 4800, 6500, 7200, 6500, 4800, 2500,
            0, -2500, -4800, -6500, -7200, -6500, -4800, -2500
        )
        val inBb = ByteBuffer.allocate(inputSamples.size * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (s in inputSamples) inBb.putShort(s)

        val encoded = ImaAdpcmCodec.encodeBlock(inBb.array())
        assertEquals(12, encoded.size)

        // Exact 4-byte header: predictor=0 (0x00, 0x00), step_index=62 (0x3E), reserved=0
        assertEquals(0, encoded[0].toInt() and 0xFF)
        assertEquals(0, encoded[1].toInt() and 0xFF)
        assertEquals(62, encoded[2].toInt() and 0xFF)
        assertEquals(0, encoded[3].toInt() and 0xFF)

        val expectedBytes = byteArrayOf(
            0x00, 0x00, 0x3E, 0x00,
            0x30, 0x24, 0x90.toByte(), 0xDA.toByte(),
            0xBB.toByte(), 0xAC.toByte(), 0x19, 0x43
        )
        assertArrayEquals(expectedBytes, encoded)

        val decodedBytes = ImaAdpcmCodec.decodeBlockToPcm16Le(encoded, inputSamples.size)
        assertEquals(inputSamples.size * 2, decodedBytes.size)

        val decBb = ByteBuffer.wrap(decodedBytes).order(ByteOrder.LITTLE_ENDIAN)
        val decodedSamples = ShortArray(inputSamples.size) { decBb.short }
        val expectedDecoded = shortArrayOf(
            343, 2528, 5084, 6801, 7113, 6261, 4970, 2389,
            -15, -2200, -4756, -6473, -7409, -6557, -4750, -2638
        )
        assertArrayEquals(expectedDecoded, decodedSamples)

        val snrDb = computeSnrDb(inputSamples, decodedSamples)
        assertTrue("Expected roundtrip SNR > 25 dB, got $snrDb dB", snrDb > 25.0)
    }

    @Test
    fun `one-second 500Hz sine wave compresses 4 to 1 with SNR above 28 dB`() {
        val sampleCount = 16000
        val inputSamples = ShortArray(sampleCount) { i ->
            (sin(2.0 * PI * 500.0 * i / 16000.0) * 16000.0).toInt().toShort()
        }
        val inBb = ByteBuffer.allocate(sampleCount * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (s in inputSamples) inBb.putShort(s)

        val encoded = ImaAdpcmCodec.encodeBlock(inBb.array())
        assertEquals(8004, encoded.size)

        val decodedBytes = ImaAdpcmCodec.decodeBlockToPcm16Le(encoded, sampleCount)
        assertEquals(sampleCount * 2, decodedBytes.size)

        val decBb = ByteBuffer.wrap(decodedBytes).order(ByteOrder.LITTLE_ENDIAN)
        val decodedSamples = ShortArray(sampleCount) { decBb.short }
        val snrDb = computeSnrDb(inputSamples, decodedSamples)
        assertTrue("Expected 1s 500Hz SNR > 28 dB, got $snrDb dB", snrDb > 28.0)
    }

    private fun computeSnrDb(original: ShortArray, decoded: ShortArray): Double {
        var signalPower = 0.0
        var noisePower = 0.0
        for (i in original.indices) {
            val s = original[i].toDouble()
            val e = s - decoded[i].toDouble()
            signalPower += s * s
            noisePower += e * e
        }
        return if (noisePower > 1e-9) 10.0 * log10(signalPower / noisePower) else 99.9
    }
}

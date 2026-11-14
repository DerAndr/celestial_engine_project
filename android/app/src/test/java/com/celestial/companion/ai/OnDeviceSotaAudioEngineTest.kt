package com.celestial.companion.ai

import com.celestial.companion.audio.KokoroVoiceProfile
import com.celestial.companion.domain.model.OraclePersona
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class OnDeviceSotaAudioEngineTest {

    @Test
    fun `forPersona maps all 6 Oracle personas to distinct Kokoro-82M voice profiles`() {
        assertEquals(KokoroVoiceProfile.AM_ADAM, KokoroVoiceProfile.forPersona(OraclePersona.MACHINE_GOD.id))
        assertEquals(KokoroVoiceProfile.BM_GEORGE_ARCHMAGE, KokoroVoiceProfile.forPersona(OraclePersona.ARCHMAGE.id))
        assertEquals(KokoroVoiceProfile.BM_GEORGE_MARVIN, KokoroVoiceProfile.forPersona(OraclePersona.MARVIN.id))
        assertEquals(KokoroVoiceProfile.AF_BELLA, KokoroVoiceProfile.forPersona(OraclePersona.CYBERPUNK.id))
        assertEquals(KokoroVoiceProfile.AM_MICHAEL, KokoroVoiceProfile.forPersona(OraclePersona.PUMPKIN_LORD.id))
        assertEquals(KokoroVoiceProfile.AF_HEART, KokoroVoiceProfile.forPersona(OraclePersona.CUSTOM.id))
    }

    @Test
    fun `pcm16ToFloatArray normalizes signed 16-bit little-endian PCM into -1 to 1 range`() {
        val pcmBytes = ByteBuffer.allocate(6).order(ByteOrder.LITTLE_ENDIAN)
            .putShort(0)
            .putShort(16384)
            .putShort(-32768)
            .array()

        val floats = OnDeviceSotaAudioEngine.pcm16ToFloatArray(pcmBytes)
        assertEquals(3, floats.size)
        assertEquals(0.0f, floats[0], 1e-4f)
        assertEquals(0.5f, floats[1], 1e-4f)
        assertEquals(-1.0f, floats[2], 1e-4f)
    }

    @Test
    fun `parseTokensTable and detokenizePieces reconstruct clean English transcript`() {
        val rawLines = listOf(
            "<s> 1",
            "</s> 2",
            "\u2581Cast 10",
            "\u2581fire 11",
            "ball 12"
        )
        val vocab = OnDeviceSotaAudioEngine.parseTokensTable(rawLines)
        assertEquals(5, vocab.size)

        val pieces = listOf(1, 10, 11, 12, 2).mapNotNull { vocab[it] }
        val text = OnDeviceSotaAudioEngine.detokenizePieces(pieces)
        assertEquals("Cast fireball", text)
        assertTrue(FastSpellMatcher.isSpellIntent(text))
    }
}

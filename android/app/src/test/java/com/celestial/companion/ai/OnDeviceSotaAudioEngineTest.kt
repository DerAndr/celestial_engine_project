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

    @Test
    fun `ByteLevelBpeTokenizer encodes ChatML and decodes byte-level BPE tokens accurately`() {
        val sampleTokenizerJson = """
            {
              "model": {
                "type": "BPE",
                "vocab": {
                  "<|endoftext|>": 0,
                  "<|im_start|>": 1,
                  "<|im_end|>": 2,
                  "s": 10,
                  "y": 11,
                  "t": 12,
                  "e": 13,
                  "m": 14,
                  "Ċ": 15,
                  "Ġ": 16,
                  "H": 17,
                  "i": 18,
                  "ĠH": 19,
                  "ĠHi": 20
                },
                "merges": [
                  "Ġ H",
                  "ĠH i"
                ]
              },
              "added_tokens": [
                {"id": 0, "content": "<|endoftext|>"},
                {"id": 1, "content": "<|im_start|>"},
                {"id": 2, "content": "<|im_end|>"}
              ]
            }
        """.trimIndent()

        val tokenizer = OnDeviceLlmEngine.ByteLevelBpeTokenizer.fromTokenizerJson(sampleTokenizerJson)
        val encoded = tokenizer.encodeText(" Hi")
        assertEquals(listOf(20L), encoded)

        val decoded = tokenizer.decodeTokens(listOf(1, 20, 2))
        assertEquals(" Hi", decoded)
    }

    @Test
    fun `parseSenseVoiceRichTags extracts language badges and emotion event tags for persona humor`() {
        val raw = "<|de|><|HAPPY|><|Laughter|> \u2581Wie \u2581spät \u2581ist \u2581es [applause]"
        val parsed = OnDeviceSotaAudioEngine.parseSenseVoiceRichTags(raw)
        assertEquals("[DE]", parsed.languageTag)
        assertEquals("Wie spät ist es", parsed.text)
        assertTrue(parsed.eventTags.contains("[laughter]"))
        assertTrue(parsed.eventTags.contains("[happy]"))
        assertTrue(parsed.eventTags.contains("[applause]"))
        assertTrue(parsed.formattedForLlm.contains("[laughter]"))
    }

    @Test
    fun `detectAcousticEventsFromPcm16 classifies rhythmic laughter bursts and OfflineEdgeEngine responds with humor`() {
        // Synthesize 400ms (6400 samples) with 4 rhythmic laughter bursts (each 50ms active, 50ms quiet)
        val samples = ShortArray(6400)
        for (frame in 0 until 8) {
            val isBurst = (frame % 2 == 0)
            val amp = if (isBurst) 18000 else 500
            for (i in 0 until 800) {
                // ~800 Hz carrier (ZCR = 0.10, within 0.04..0.24 laughter band)
                val sign = if ((i / 10) % 2 == 0) 1 else -1
                samples[frame * 800 + i] = (amp * sign).toShort()
            }
        }
        val bb = ByteBuffer.allocate(samples.size * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (s in samples) bb.putShort(s)

        val events = OnDeviceSotaAudioEngine.detectAcousticEventsFromPcm16(bb.array())
        assertTrue("Expected [laughter] in detected events: $events", events.contains("[laughter]"))

        val engine = OfflineEdgeEngine()
        val marvinHumor = engine.resolveOfflineProphecy(
            query = "[laughter] hello there",
            personaId = OraclePersona.MARVIN.id,
            language = com.celestial.companion.domain.model.StaffLanguage.ENGLISH
        )
        assertTrue("Expected Marvin humor response, got: $marvinHumor", marvinHumor.lowercase().contains("laugh"))
    }
}

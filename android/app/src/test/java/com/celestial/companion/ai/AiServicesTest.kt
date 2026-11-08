package com.celestial.companion.ai

import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.StaffLanguage
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class AiServicesTest {

    private val transcriptionService = AcousticTranscriptionService()
    private val modelService = GemmaModelService()

    @Test
    fun `AcousticTranscriptionService normalizes quiet PCM and builds valid RIFF WAV container`() {
        // Generate 800 samples (1600 bytes) with peak = 1000
        val pcm = ByteArray(1600)
        for (i in 0 until 800) {
            val sample: Short = if (i % 2 == 0) 1000 else -1000
            pcm[i * 2] = (sample.toInt() and 0xFF).toByte()
            pcm[i * 2 + 1] = ((sample.toInt() shr 8) and 0xFF).toByte()
        }

        val normalized = transcriptionService.normalizePcm16(pcm)
        val metrics = transcriptionService.analyzeWaveform(normalized)
        assertTrue("Expected boosted peak near 22000, got ${metrics.peak}", metrics.peak in 21000..23000)

        val wav = transcriptionService.pcm16ToWav(normalized)
        assertEquals(44 + normalized.size, wav.size)
        assertEquals("RIFF", String(wav, 0, 4, Charsets.US_ASCII))
        assertEquals("WAVE", String(wav, 8, 4, Charsets.US_ASCII))
        assertEquals("fmt ", String(wav, 12, 4, Charsets.US_ASCII))
        assertEquals("data", String(wav, 36, 4, Charsets.US_ASCII))
        val dataLen = ByteBuffer.wrap(wav, 40, 4).order(ByteOrder.LITTLE_ENDIAN).int
        assertEquals(normalized.size, dataLen)
    }

    @Test
    fun `AcousticTranscriptionService constructs stage-1 JSON payload and parses response`() {
        val wav = transcriptionService.pcm16ToWav(ByteArray(320))
        val reqJson = transcriptionService.buildTranscriptionRequestJson(wav)
        assertEquals("google/gemini-2.5-flash", reqJson.getString("model"))
        assertEquals(100, reqJson.getInt("max_tokens"))

        val fakeResponse = """
            {
              "choices": [
                {
                  "message": {
                    "content": " \"Cast Chain Lightning\" "
                  }
                }
              ]
            }
        """.trimIndent()
        assertEquals("Cast Chain Lightning", transcriptionService.parseTranscriptionResponse(fakeResponse))
    }

    @Test
    fun `GemmaModelService detects language, builds persona system prompt, and parses SSE stream`() {
        assertEquals(
            StaffLanguage.RUSSIAN,
            modelService.resolveEffectiveLanguage("Кастани фаербол", StaffLanguage.ENGLISH)
        )
        assertEquals(
            StaffLanguage.GERMAN,
            modelService.resolveEffectiveLanguage("Wie spät ist es?", StaffLanguage.GERMAN)
        )

        val config = GemmaConfig(
            variant = GemmaVariant.GEMMA_4_E2B,
            personaId = OraclePersona.MARVIN.id,
            language = StaffLanguage.ENGLISH,
            enableThinking = true
        )
        val sysPrompt = modelService.buildSystemPrompt(config, "Cast Fireball", isSpellPrompt = true)
        assertTrue(sysPrompt.contains("Marvin"))
        assertTrue(sysPrompt.contains("[SPELL:"))
        assertTrue(sysPrompt.contains("FAST SPELL MODE"))

        // Verify SSE line parser splits <thought> and output tokens properly
        val state = SseTokenStreamState()
        val line1 = """data: {"choices":[{"delta":{"content":"<thought>Analyzing spell intent...</thought>[SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER] Ignis!"}}]}"""
        val chunks = modelService.parseSseLine(line1, state)
        assertNotNull(chunks)
        assertEquals("Analyzing spell intent...", state.fullThoughtTrace.toString())
        assertEquals("[SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER] Ignis!", state.fullAnswer.toString())
        assertTrue(chunks!!.any { it is GemmaStreamChunk.ThoughtToken })
        assertTrue(chunks.any { it is GemmaStreamChunk.OutputToken })

        // Verify [DONE] returns null
        assertNull(modelService.parseSseLine("data: [DONE]", state))
    }
}

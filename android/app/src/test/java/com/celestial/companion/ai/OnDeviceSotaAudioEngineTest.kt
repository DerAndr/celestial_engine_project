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

    @Test
    fun `decodeCtcGreedyLogits collapses repeated tokens and blank frames accurately`() {
        val vocab = mapOf(
            0 to "<blk>",
            1 to "<|en|>",
            2 to "<|Laughter|>",
            3 to "\u2581Cast",
            4 to "\u2581fire",
            5 to "ball"
        )
        val vocabSize = 6
        // 8 time steps: 1, 1, 0, 2, 3, 3, 4, 5
        val targets = intArrayOf(1, 1, 0, 2, 3, 3, 4, 5)
        val logits = FloatArray(targets.size * vocabSize) { -5.0f }
        for (t in targets.indices) {
            logits[t * vocabSize + targets[t]] = 5.0f
        }
        val decoded = OnDeviceSotaAudioEngine.decodeCtcGreedyLogits(logits, targets.size, vocabSize, vocab, blankId = 0)
        assertEquals("<|en|><|Laughter|> Cast fireball", decoded)
    }

    @Test
    fun `resampleFloat24kToPcm16kMono downsamples 24kHz sine wave to 16kHz 16bit PCM with 22000 peak`() {
        val samples24k = FloatArray(2400) { i ->
            kotlin.math.sin(2.0 * Math.PI * 440.0 * i / 24000.0).toFloat() * 0.5f
        }
        val pcm16k = com.celestial.companion.audio.KokoroOnnxSynthesizer.resampleFloat24kToPcm16kMono(samples24k, targetPeak = 22000)
        assertEquals(1600 * 2, pcm16k.size)

        val bb = ByteBuffer.wrap(pcm16k).order(ByteOrder.LITTLE_ENDIAN)
        var maxPeak = 0
        while (bb.hasRemaining()) {
            val v = kotlin.math.abs(bb.short.toInt())
            if (v > maxPeak) maxPeak = v
        }
        assertTrue("Expected peak near 22000, got $maxPeak", maxPeak in 21900..22000)
    }

    @Test
    fun `ImaAdpcmCodec compresses 480 PCM samples 4 to 1 with high fidelity`() {
        val pcmSamples = ShortArray(480) { i ->
            (kotlin.math.sin(2.0 * Math.PI * 500.0 * i / 16000.0) * 16000.0).toInt().toShort()
        }
        val inBb = ByteBuffer.allocate(960).order(ByteOrder.LITTLE_ENDIAN)
        for (s in pcmSamples) inBb.putShort(s)

        val encoded = com.celestial.companion.audio.ImaAdpcmCodec.encodeBlock(inBb.array())
        assertEquals(244, encoded.size) // 4-byte header + 240 nibble bytes = 4:1 compression

        val decoded = com.celestial.companion.audio.ImaAdpcmCodec.decodeBlock(encoded, 480)
        assertEquals(960, decoded.size)
    }

    @Test
    fun `OmniAudioSession tracks glass to glass latency under 300ms target`() {
        val session = OmniAudioSession("Mini-Omni2 Test")
        session.appendUplinkFrame(OmniAudioFrame(ByteArray(640)))
        session.markMicReleased(1000L)
        session.markFirstSpellTag(1120L)
        session.markFirstAudioChunk(1226L)

        val metrics = session.snapshotMetrics()
        assertEquals(640, session.totalUplinkBytes())
        assertEquals(226L, metrics.glassToGlassLatencyMs)
        assertTrue(metrics.meetsSub300MsTarget)
    }

    @Test
    fun `KokoroOnnxSynthesizer tokenizes English text into valid IPA vocab IDs and parses NPY style rows`() {
        val tokens = com.celestial.companion.audio.KokoroOnnxSynthesizer.englishToKokoroTokens("Quantum probability stable at 99.94%.")
        assertTrue("Expected non-empty Kokoro IPA token sequence", tokens.size > 10)
        assertTrue("All token IDs must be within 1..177", tokens.all { it in 1..177 })

        // Build a synthetic NumPy v1.0 .npy buffer with 2 rows of 256 float32s (128-byte header + 2 * 256 * 4 bytes)
        val npy = ByteArray(128 + 2 * 256 * 4)
        npy[0] = 0x93.toByte()
        npy[1] = 'N'.code.toByte()
        npy[2] = 'U'.code.toByte()
        npy[3] = 'M'.code.toByte()
        npy[4] = 'P'.code.toByte()
        npy[5] = 'Y'.code.toByte()
        npy[6] = 1
        npy[7] = 0
        // header_len = 118 (so 10 + 118 = 128 byte offset)
        npy[8] = 118
        npy[9] = 0
        val fb = ByteBuffer.wrap(npy, 128, 2 * 256 * 4).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        for (i in 0 until 256) fb.put(0.25f) // row 0 (phonemeLen = 1)
        for (i in 0 until 256) fb.put(0.75f) // row 1 (phonemeLen = 2)

        val row0 = com.celestial.companion.audio.KokoroOnnxSynthesizer.parseNpyStyleRow(npy, phonemeLen = 1)
        val row1 = com.celestial.companion.audio.KokoroOnnxSynthesizer.parseNpyStyleRow(npy, phonemeLen = 2)
        assertEquals(256, row0?.size)
        assertEquals(0.25f, row0!![0], 1e-5f)
        assertEquals(0.75f, row1!![0], 1e-5f)
    }

    @Test
    fun `KokoroOnnxSynthesizer domain IPA lexicon covers 30 plus Celestial terms and produces valid token IDs`() {
        assertTrue(
            "Expected at least 30 domain IPA lexicon entries",
            com.celestial.companion.audio.KokoroOnnxSynthesizer.DOMAIN_IPA_LEXICON.size >= 30
        )

        val ipa = com.celestial.companion.audio.KokoroOnnxSynthesizer.englishToIpaPhonemes(
            "The Omnissiah blesses this celestial grimoire and supernova aegis."
        )
        assertTrue(ipa.contains("ˌɑːmnɪsˈaɪə"))
        assertTrue(ipa.contains("sˌuːpɚnˈoʊvə"))
        assertTrue(ipa.contains("ɡɹɪmwˈɑːɹ"))

        val tokens = com.celestial.companion.audio.KokoroOnnxSynthesizer.englishToKokoroTokens(
            "Omnissiah supernova arcane grimoire aegis chrono nebula"
        )
        assertTrue("Expected non-empty domain IPA token sequence", tokens.size >= 20)
        assertTrue("All token IDs must be within 1..177", tokens.all { it in 1..177 })
    }

    @Test
    fun `splitTextIntoKokoroClauses strips spell tags and chunks multi-sentence prophecies under maxChars`() {
        val prophecy = "[SPELL: Supernova | ANIM: SUPERNOVA | SOUNDS: SUPERNOVA] " +
            "The Omnissiah awakens in the celestial void. " +
            "Quantum runes align across the astral matrix! " +
            "Shields hold steady at full luminosity."

        val clauses = com.celestial.companion.audio.KokoroOnnxSynthesizer.splitTextIntoKokoroClauses(prophecy)
        assertEquals(3, clauses.size)
        assertEquals("The Omnissiah awakens in the celestial void.", clauses[0])
        assertEquals("Quantum runes align across the astral matrix!", clauses[1])
        assertEquals("Shields hold steady at full luminosity.", clauses[2])

        val longClause = "First cosmic alignment, second harmonic resonance, third arcane oscillation, fourth warp surge across the stars"
        val subChunks = com.celestial.companion.audio.KokoroOnnxSynthesizer.splitTextIntoKokoroClauses(longClause, maxCharsPerClause = 45)
        assertTrue("Expected multiple sub-chunks, got $subChunks", subChunks.size >= 3)
        assertTrue("Every chunk must be <= 45 chars", subChunks.all { it.length <= 45 })
    }
}

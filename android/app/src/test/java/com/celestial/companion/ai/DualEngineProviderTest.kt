package com.celestial.companion.ai

import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.StaffLanguage
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class DualEngineProviderTest {

    private val offlineEngine = OfflineEdgeEngine()

    @Test
    fun `AiEngineMode enum ids and fromId resolution work correctly`() {
        assertEquals(AiEngineMode.AUTO_FALLBACK, AiEngineMode.fromId(0))
        assertEquals(AiEngineMode.OFFLINE_EDGE, AiEngineMode.fromId(1))
        assertEquals(AiEngineMode.CLOUD_ONLY, AiEngineMode.fromId(2))
        assertEquals(AiEngineMode.AUTO_FALLBACK, AiEngineMode.fromId(999)) // Fallback default
    }

    @Test
    fun `OfflineEdgeEngine matches spells and produces valid hardware choreography tags`() {
        val prophecy = offlineEngine.resolveOfflineProphecy(
            query = "cast fireball",
            personaId = OraclePersona.MACHINE_GOD.id,
            language = StaffLanguage.ENGLISH
        )
        assertTrue("Expected [SPELL: tag, got: $prophecy", prophecy.startsWith("[SPELL: Fireball"))
        assertTrue("Expected ANIM: FIRE_SPIRAL, got: $prophecy", prophecy.contains("ANIM: FIRE_SPIRAL"))
        assertTrue("Expected SOUNDS: FIREBALL, got: $prophecy", prophecy.contains("SOUNDS: FIREBALL"))
    }

    @Test
    fun `OfflineEdgeEngine resolves all 6 personas in EN RU DE with word count <= 25`() {
        val personas = listOf(
            OraclePersona.MACHINE_GOD,
            OraclePersona.ARCHMAGE,
            OraclePersona.MARVIN,
            OraclePersona.CYBERPUNK,
            OraclePersona.PUMPKIN_LORD,
            OraclePersona.CUSTOM
        )
        val languages = listOf(
            StaffLanguage.ENGLISH,
            StaffLanguage.RUSSIAN,
            StaffLanguage.GERMAN
        )

        for (persona in personas) {
            for (lang in languages) {
                val response = offlineEngine.resolveOfflineProphecy(
                    query = "What is the cosmic destiny?",
                    personaId = persona.id,
                    language = lang
                )
                assertNotNull("Response should not be null for ${persona.id} in ${lang.name}", response)
                assertTrue("Response should not be blank for ${persona.id} in ${lang.name}", response.isNotBlank())

                val wordCount = response.trim().split(Regex("\\s+")).size
                assertTrue(
                    "Persona ${persona.id} in ${lang.name} must be <= 25 words (was $wordCount): \"$response\"",
                    wordCount <= 25
                )
            }
        }
    }

    @Test
    fun `OfflineEdgeEngine streamInference emits full chunk lifecycle and Complete event`() = runBlocking {
        val chunks = offlineEngine.streamInference(
            prompt = "Status report",
            language = StaffLanguage.ENGLISH,
            personaId = OraclePersona.MACHINE_GOD.id
        ).toList()

        assertTrue("Expected at least 5 stream chunks", chunks.size >= 5)

        val phaseChunks = chunks.filterIsInstance<GemmaStreamChunk.PhaseChanged>()
        assertTrue("Should emit INGESTING phase", phaseChunks.any { it.phase == AiProcessingPhase.INGESTING })
        assertTrue("Should emit THINKING phase", phaseChunks.any { it.phase == AiProcessingPhase.THINKING })
        assertTrue("Should emit STREAMING phase", phaseChunks.any { it.phase == AiProcessingPhase.STREAMING })
        assertTrue("Should emit COMPLETED phase", phaseChunks.any { it.phase == AiProcessingPhase.COMPLETED })

        val outputTokens = chunks.filterIsInstance<GemmaStreamChunk.OutputToken>()
        assertTrue("Should emit output tokens", outputTokens.isNotEmpty())

        val completeChunk = chunks.filterIsInstance<GemmaStreamChunk.Complete>().firstOrNull()
        assertNotNull("Complete chunk must be emitted", completeChunk)
        assertEquals("EDGE-OFFLINE", completeChunk!!.result.modelTag)
        assertTrue("Latency should be non-negative", completeChunk.result.latencyMs >= 0)
        assertTrue("Tokens/sec should be positive", completeChunk.result.tokensPerSec > 0)
    }
}

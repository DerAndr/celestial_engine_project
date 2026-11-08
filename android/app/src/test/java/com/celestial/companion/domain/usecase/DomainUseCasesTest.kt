package com.celestial.companion.domain.usecase

import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.domain.model.ClockStyle
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.domain.model.StaffTelemetry
import com.celestial.companion.protocol.NetMessageType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Locale

class DomainUseCasesTest {

    private val voiceQueryUseCase = ProcessStaffVoiceQueryUseCase()
    private val telemetryUseCase = SyncStaffTelemetryUseCase()
    private val emergencyAbortUseCase = EmergencyAbortUseCase()

    @Test
    fun `ProcessStaffVoiceQueryUseCase validates PCM sample length and duration`() {
        assertFalse(voiceQueryUseCase.isValidVoiceSample(null))
        assertFalse(voiceQueryUseCase.isValidVoiceSample(ByteArray(0)))
        assertFalse(voiceQueryUseCase.isValidVoiceSample(ByteArray(1599)))
        assertTrue(voiceQueryUseCase.isValidVoiceSample(ByteArray(1600)))
        assertEquals(1.5f, voiceQueryUseCase.audioDurationSeconds(ByteArray(48000)), 0.001f)
    }

    @Test
    fun `ProcessStaffVoiceQueryUseCase extracts stage-1 voice token and matches fast spells`() {
        val token = "Acoustic voice received: \"Cast Chain Lightning\"\n\n"
        val extracted = voiceQueryUseCase.extractTranscribedVoiceToken(token)
        assertEquals("Cast Chain Lightning", extracted)
        assertNull(voiceQueryUseCase.extractTranscribedVoiceToken("Regular reasoning token"))

        val spell = voiceQueryUseCase.matchFastSpell(extracted!!)
        assertNotNull(spell)
        assertEquals("Chain Lightning", spell?.name)
        assertEquals(LedAnimation.RADIAL_WAVE, spell?.animation)
    }

    @Test
    fun `ProcessStaffVoiceQueryUseCase intercepts SPELL tag and OLED snippets during streaming`() {
        val spellStream = "[SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER, SUPERNOVA, PULSAR] Ignis"
        val directive = voiceQueryUseCase.evaluateStreamingOutput(
            currentText = spellStream,
            prompt = "Cast fireball",
            spellAlreadyExecuted = false,
            broadcastOnComplete = true,
            lastOledDispatchedLen = 0
        )
        assertTrue(directive is StreamingOutputDirective.TriggerSpell)
        val trigger = directive as StreamingOutputDirective.TriggerSpell
        assertEquals("Fireball", trigger.spellDefinition.name)
        assertEquals(LedAnimation.PLASMA, trigger.ledAnimation)
        assertEquals(listOf(3, 6, 1), trigger.soundEffects)
        assertEquals("[SPELL] Fireball", trigger.oledText)

        // When spellAlreadyExecuted is true, returns None
        val afterSpell = voiceQueryUseCase.evaluateStreamingOutput(
            currentText = spellStream,
            prompt = "Cast fireball",
            spellAlreadyExecuted = true,
            broadcastOnComplete = true,
            lastOledDispatchedLen = 0
        )
        assertEquals(StreamingOutputDirective.None, afterSpell)

        // Conversational sentence ending with '.' dispatches OLED snippet
        val convStream = "The stars align over the horizon."
        val oledDirective = voiceQueryUseCase.evaluateStreamingOutput(
            currentText = convStream,
            spellAlreadyExecuted = false,
            broadcastOnComplete = true,
            lastOledDispatchedLen = 0
        )
        assertTrue(oledDirective is StreamingOutputDirective.DispatchOledSnippet)
        val snippet = oledDirective as StreamingOutputDirective.DispatchOledSnippet
        assertEquals("The stars align over the horizon.", snippet.snippet)
        assertEquals(convStream.length, snippet.newDispatchedLen)
    }

    @Test
    fun `ProcessStaffVoiceQueryUseCase resolves completion plan and TTS locales`() {
        assertEquals(Locale("ru", "RU"), voiceQueryUseCase.resolveTtsLocale("Привет, космический странник!"))
        assertEquals(Locale.GERMAN, voiceQueryUseCase.resolveTtsLocale("Grüße vom Himmelsgewölbe!"))
        assertEquals(Locale.US, voiceQueryUseCase.resolveTtsLocale("Greetings from the Celestial Vault!"))

        val plan = voiceQueryUseCase.resolveCompletion(
            rawOutput = "<think>Choosing Lumos</think>[SPELL: Lumos Maxima | ANIM: HARRY_POTTER | SOUNDS: LUMOS, PORTAL] Lumos maxima lux!",
            rawThought = "Initial trace.",
            spellAlreadyExecuted = false
        )
        assertTrue(plan.prophecy.isSpell)
        assertEquals("Lumos maxima lux!", plan.cleanOutput)
        assertTrue(plan.cleanThought.contains("Choosing Lumos"))
        assertEquals("[SPELL] Lumos Maxima", plan.oledText)
        assertTrue(plan.shouldTriggerVisuals)
        assertEquals(LedAnimation.HARRY_POTTER, plan.animation)
    }

    @Test
    fun `SyncStaffTelemetryUseCase updates heartbeat, RTT, liveness timeout, and signal quality`() {
        val initial = StaffTelemetry(isOnline = false, lastSeenTimestamp = 0L)
        val t1 = 10_000L
        val afterHb1 = telemetryUseCase.onHeartbeatReceived(initial, seqNum = 42, senderIp = "10.111.174.64", nowMs = t1)
        assertTrue(afterHb1.isOnline)
        assertEquals("10.111.174.64", afterHb1.ipAddress)
        assertEquals(42L, afterHb1.tickNumber)
        assertEquals(t1, afterHb1.lastSeenTimestamp)
        assertEquals(100, telemetryUseCase.calculateSignalQualityPercent(afterHb1, nowMs = t1 + 500L))

        val t2 = 11_200L
        val afterHb2 = telemetryUseCase.onHeartbeatReceived(afterHb1, seqNum = 43, senderIp = "10.111.174.64", nowMs = t2)
        assertEquals(43L, afterHb2.tickNumber)
        assertEquals(1200L, afterHb2.roundTripTimeMs)

        // Still online at +3000ms
        val stillAlive = telemetryUseCase.evaluateLiveness(afterHb2, nowMs = t2 + 3000L)
        assertTrue(stillAlive.isOnline)

        // Offline after > 3500ms
        val timedOut = telemetryUseCase.evaluateLiveness(afterHb2, nowMs = t2 + 3600L)
        assertFalse(timedOut.isOnline)
        assertEquals(0, telemetryUseCase.calculateSignalQualityPercent(timedOut, nowMs = t2 + 3600L))
    }

    @Test
    fun `SyncStaffTelemetryUseCase decodes binary APP_TELEMETRY and CATALOG_RESP payloads`() {
        val payload = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
            .put(StaffMode.LED_CORE.id.toByte())
            .put(6.toByte()) // STATE_SPEAKING = 6 in firmware
            .put(75.toByte()) // volumePercent
            .put(LedAnimation.SAILOR_MOON.id.toByte())
            .put(ClockStyle.ORRERY.id.toByte())
            .put(StaffLanguage.GERMAN.id.toByte())
            .put(1.toByte()) // hostType = 1 (PI 5 BRAIN)
            .put(1.toByte()) // preferredHost
            .put(5.toByte()) // aiModelId
            .put(88.toByte()) // micLevelPercent
            .put(2.toByte()) // cosmosZoom
            .put(1.toByte()) // isAudioPlaying = true
            .array()

        val decoded = telemetryUseCase.decodeTelemetryPayload(
            current = StaffTelemetry(),
            payload = payload,
            senderIp = "10.111.174.64",
            nowMs = 50_000L
        )
        assertTrue(decoded.isOnline)
        assertEquals("10.111.174.64", decoded.ipAddress)
        assertEquals(StaffMode.LED_CORE, decoded.mode)
        assertEquals(StaffState.SPEAKING, decoded.state)
        assertEquals(75, decoded.volumePercent)
        assertEquals(LedAnimation.SAILOR_MOON, decoded.activeLedAnim)
        assertEquals(ClockStyle.ORRERY, decoded.clockStyle)
        assertEquals(StaffLanguage.GERMAN, decoded.language)
        assertEquals("PI 5 BRAIN", decoded.activeHost)
        assertEquals(88, decoded.micLevelPercent)
        assertTrue(decoded.isAudioPlaying)

        // Catalog response host decoding
        val catPayload = ByteArray(17)
        catPayload[0] = 2 // PHONE BRAIN
        val nameBytes = "CUSTOM HOST".toByteArray(Charsets.US_ASCII)
        System.arraycopy(nameBytes, 0, catPayload, 1, nameBytes.size)
        val afterCat = telemetryUseCase.decodeCatalogResponseHost(decoded, catPayload)
        assertEquals("CUSTOM HOST", afterCat.activeHost)
    }

    @Test
    fun `EmergencyAbortUseCase cancels coroutines, builds CANCEL packet, and resets telemetry to IDLE`() {
        var coroutinesCancelled = false
        var repositoryAborted = false
        val speakingTelemetry = StaffTelemetry(
            isOnline = true,
            state = StaffState.SPEAKING,
            isAudioPlaying = true
        )

        val outcome = emergencyAbortUseCase.execute(
            currentTelemetry = speakingTelemetry,
            liveStreamingText = "Partial oracle stream",
            onCancelCoroutines = { coroutinesCancelled = true },
            onAbortRepositoryAudio = { repositoryAborted = true }
        )

        assertTrue(coroutinesCancelled)
        assertTrue(repositoryAborted)
        assertFalse(outcome.isGenerating)
        assertEquals(AiProcessingPhase.CANCELLED, outcome.processingPhase)
        assertEquals("Partial oracle stream [STOPPED]", outcome.interruptedText)
        assertEquals(StaffState.IDLE, outcome.updatedTelemetry.state)
        assertFalse(outcome.updatedTelemetry.isAudioPlaying)

        val cancelPacket = emergencyAbortUseCase.buildCancelPacket(seqNum = 7, requestId = 99L)
        assertEquals(NetMessageType.CANCEL, cancelPacket.msgType)
        assertEquals(7, cancelPacket.seqNum)
        assertEquals(99L, cancelPacket.requestId)
        assertEquals(0, cancelPacket.payloadLen)
    }
}

package com.celestial.companion.domain.usecase

import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.domain.model.StaffTelemetry
import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType

/**
 * Snapshot produced when an emergency abort is executed.
 */
data class EmergencyAbortOutcome(
    val isGenerating: Boolean = false,
    val processingPhase: AiProcessingPhase = AiProcessingPhase.CANCELLED,
    val interruptedText: String,
    val updatedTelemetry: StaffTelemetry
)

/**
 * Coordinates universal instant cancellation across:
 * 1. Active LLM streaming coroutines & sound sequence jobs
 * 2. On-device / cloud TTS synthesis & UDP audio chunk streaming
 * 3. `NET_MSG_CANCEL` UDP packet transmission to the physical RP2350 staff
 * 4. Immediate UI & telemetry state reset to `IDLE` / `CANCELLED`
 */
class EmergencyAbortUseCase {

    companion object {
        const val DEFAULT_STOPPED_MESSAGE = "Generation stopped by user. [STOPPED]"
        const val STOPPED_SUFFIX = " [STOPPED]"
        const val BROADCAST_IP = "255.255.255.255"
    }

    /**
     * Constructs a `NET_MSG_CANCEL` (`0x0A`) packet to halt staff DMA audio playback and reset to IDLE.
     */
    fun buildCancelPacket(seqNum: Int = 0, requestId: Long = 0L): CelestialPacket {
        return CelestialPacket(
            msgType = NetMessageType.CANCEL,
            seqNum = seqNum,
            payloadLen = 0,
            requestId = requestId,
            payload = ByteArray(0)
        )
    }

    /**
     * Formats the partial streaming text with the `[STOPPED]` marker when aborted.
     */
    fun formatInterruptedText(liveStreamingText: String): String {
        val base = liveStreamingText.ifBlank { "Generation stopped by user." }
        return if (base.endsWith(STOPPED_SUFFIX)) base else base + STOPPED_SUFFIX
    }

    /**
     * Resets [StaffTelemetry] immediately to [StaffState.IDLE] and clears audio playback flags.
     */
    fun resetTelemetryToIdle(current: StaffTelemetry): StaffTelemetry {
        return current.copy(
            state = StaffState.IDLE,
            isAudioPlaying = false
        )
    }

    /**
     * Coordinates full emergency abort across active coroutines, repository audio/UDP channels,
     * and returns the resulting [EmergencyAbortOutcome].
     */
    fun execute(
        currentTelemetry: StaffTelemetry,
        liveStreamingText: String,
        onCancelCoroutines: () -> Unit = {},
        onAbortRepositoryAudio: () -> Unit = {}
    ): EmergencyAbortOutcome {
        onCancelCoroutines()
        onAbortRepositoryAudio()
        val interrupted = formatInterruptedText(liveStreamingText)
        val idleTelemetry = resetTelemetryToIdle(currentTelemetry)
        return EmergencyAbortOutcome(
            isGenerating = false,
            processingPhase = AiProcessingPhase.CANCELLED,
            interruptedText = interrupted,
            updatedTelemetry = idleTelemetry
        )
    }
}

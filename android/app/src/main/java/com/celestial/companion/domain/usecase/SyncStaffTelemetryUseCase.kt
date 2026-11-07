package com.celestial.companion.domain.usecase

import com.celestial.companion.domain.model.ClockStyle
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.domain.model.StaffTelemetry
import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.TimeZone

/**
 * Domain UseCase managing heartbeat telemetry state, online liveness,
 * tick counter parity, binary telemetry decoding, and signal/latency metrics.
 */
class SyncStaffTelemetryUseCase {

    companion object {
        const val HEARTBEAT_TIMEOUT_MS = 3500L
        const val MIN_TELEMETRY_PAYLOAD_BYTES = 5
        const val MIN_CATALOG_RESP_BYTES = 17
    }

    /**
     * Updates [StaffTelemetry] upon receiving a `NET_MSG_HEARTBEAT` packet from the staff.
     */
    fun onHeartbeatReceived(
        current: StaffTelemetry,
        seqNum: Int,
        senderIp: String,
        nowMs: Long = System.currentTimeMillis()
    ): StaffTelemetry {
        val rtt = if (current.lastSeenTimestamp > 0L && nowMs >= current.lastSeenTimestamp) {
            (nowMs - current.lastSeenTimestamp).coerceAtMost(9999L)
        } else {
            current.roundTripTimeMs
        }
        val resolvedIp = if (senderIp.isNotBlank() && senderIp != "0.0.0.0" && senderIp != "127.0.0.1") {
            senderIp
        } else {
            current.ipAddress
        }
        return current.copy(
            isOnline = true,
            ipAddress = resolvedIp,
            lastSeenTimestamp = nowMs,
            tickNumber = seqNum.toLong(),
            roundTripTimeMs = rtt
        )
    }

    /**
     * Evaluates whether the staff connection has timed out (`> 3500 ms` without heartbeat).
     */
    fun evaluateLiveness(
        current: StaffTelemetry,
        nowMs: Long = System.currentTimeMillis(),
        timeoutMs: Long = HEARTBEAT_TIMEOUT_MS
    ): StaffTelemetry {
        return if (current.isOnline && (nowMs - current.lastSeenTimestamp > timeoutMs)) {
            current.copy(isOnline = false)
        } else {
            current
        }
    }

    /**
     * Computes a normalized signal quality percentage (`0..100`) from online status and heartbeat age.
     */
    fun calculateSignalQualityPercent(
        telemetry: StaffTelemetry,
        nowMs: Long = System.currentTimeMillis()
    ): Int {
        if (!telemetry.isOnline || telemetry.lastSeenTimestamp <= 0L) return 0
        val ageMs = (nowMs - telemetry.lastSeenTimestamp).coerceAtLeast(0L)
        return when {
            ageMs <= 1000L -> 100
            ageMs <= 1500L -> 90
            ageMs <= 2000L -> 75
            ageMs <= 2800L -> 50
            ageMs <= HEARTBEAT_TIMEOUT_MS -> 25
            else -> 0
        }
    }

    /**
     * Builds the 4-byte little-endian local epoch-seconds payload for `NET_MSG_HEARTBEAT`.
     */
    fun buildHeartbeatPayload(
        nowMs: Long = System.currentTimeMillis(),
        tzOffsetMs: Int = TimeZone.getDefault().getOffset(nowMs)
    ): ByteArray {
        val epochSec = ((nowMs + tzOffsetMs) / 1000L).toInt()
        return ByteBuffer.allocate(4)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putInt(epochSec)
            .array()
    }

    /**
     * Builds a complete `NET_MSG_HEARTBEAT` [CelestialPacket] with local epoch-seconds payload.
     */
    fun buildHeartbeatPacket(
        seqNum: Int = 1,
        nowMs: Long = System.currentTimeMillis(),
        tzOffsetMs: Int = TimeZone.getDefault().getOffset(nowMs)
    ): CelestialPacket {
        val payload = buildHeartbeatPayload(nowMs, tzOffsetMs)
        return CelestialPacket(
            msgType = NetMessageType.HEARTBEAT,
            seqNum = seqNum,
            payloadLen = payload.size,
            requestId = 0L,
            payload = payload
        )
    }

    /**
     * Decodes a binary `NET_MSG_APP_TELEMETRY` payload from the RP2350 staff into [StaffTelemetry].
     */
    fun decodeTelemetryPayload(
        current: StaffTelemetry,
        payload: ByteArray,
        senderIp: String = current.ipAddress,
        nowMs: Long = System.currentTimeMillis()
    ): StaffTelemetry {
        if (payload.size < MIN_TELEMETRY_PAYLOAD_BYTES) return current
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val modeId = buffer.get().toInt() and 0xFF
        val stateId = buffer.get().toInt() and 0xFF
        val vol = buffer.get().toInt() and 0xFF
        val animId = buffer.get().toInt() and 0xFF
        val clockStyleId = buffer.get().toInt() and 0xFF
        val langId = if (payload.size >= 6) buffer.get().toInt() and 0xFF else 0
        val hostType = if (payload.size >= 7) buffer.get().toInt() and 0xFF else 0
        val prefHost = if (payload.size >= 8) buffer.get().toInt() and 0xFF else 0
        val aiModelId = if (payload.size >= 9) buffer.get().toInt() and 0xFF else 0
        val micLevel = if (payload.size >= 10) buffer.get().toInt() and 0xFF else 0
        val cosmosZoom = if (payload.size >= 11) buffer.get().toInt() and 0xFF else 0
        val isAudioPlaying = if (payload.size >= 12) (buffer.get().toInt() and 0xFF) != 0 else false

        val resolvedHost = when (hostType) {
            1 -> "PI 5 BRAIN"
            2 -> "PHONE BRAIN"
            else -> current.activeHost
        }
        val resolvedIp = if (senderIp.isNotBlank() && senderIp != "0.0.0.0" && senderIp != "127.0.0.1") {
            senderIp
        } else {
            current.ipAddress
        }

        return current.copy(
            ipAddress = resolvedIp,
            mode = StaffMode.fromId(modeId),
            state = StaffState.fromOrdinalSafe(stateId),
            volumePercent = vol,
            activeLedAnim = LedAnimation.fromId(animId),
            clockStyle = ClockStyle.fromId(clockStyleId),
            language = StaffLanguage.fromId(langId),
            activeHost = resolvedHost,
            preferredHost = prefHost,
            aiModelId = aiModelId,
            micLevelPercent = micLevel,
            cosmosZoom = cosmosZoom,
            isAudioPlaying = isAudioPlaying,
            isOnline = true,
            lastSeenTimestamp = nowMs
        )
    }

    /**
     * Decodes a `NET_MSG_CATALOG_RESP` packet payload to update the active host name.
     */
    fun decodeCatalogResponseHost(current: StaffTelemetry, payload: ByteArray): StaffTelemetry {
        if (payload.size < MIN_CATALOG_RESP_BYTES) return current
        val hostType = payload[0].toInt() and 0xFF
        val hostName = String(payload, 1, 15, Charsets.US_ASCII).trim('\u0000', ' ')
        val resolvedHost = hostName.ifEmpty {
            if (hostType == 1) "PI 5 BRAIN" else "PHONE BRAIN"
        }
        return current.copy(activeHost = resolvedHost)
    }
}

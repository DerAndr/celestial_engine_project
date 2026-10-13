package com.celestial.companion.domain.repository

import com.celestial.companion.domain.model.*
import com.celestial.companion.network.StaffTransport
import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import com.celestial.companion.protocol.ProtocolConstants
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.nio.ByteBuffer
import java.nio.ByteOrder

class StaffRepository(
    private val transport: StaffTransport
) {
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    private val _telemetry = MutableStateFlow(StaffTelemetry())
    val telemetry: StateFlow<StaffTelemetry> = _telemetry.asStateFlow()

    private var activeStaffIp: String = "192.168.178.101" // Default staff fallback IP
    var passkey: String = ""

    fun computeAuthToken(): Int {
        var hash = 0x811c9dc5.toInt()
        for (b in passkey.toByteArray(Charsets.UTF_8)) {
            hash = hash xor (b.toInt() and 0xFF)
            hash *= 0x01000193
        }
        return hash
    }

    init {
        transport.start()
        scope.launch {
            transport.incomingPackets.collect { packet ->
                handleIncomingPacket(packet)
            }
        }

        // Liveness watchdog: mark offline if no heartbeat received for > 3.5 seconds
        scope.launch {
            while (isActive) {
                delay(1000)
                val current = _telemetry.value
                val now = System.currentTimeMillis()
                if (current.isOnline && (now - current.lastSeenTimestamp > 3500)) {
                    _telemetry.value = current.copy(isOnline = false)
                }
            }
        }
    }

    fun setTargetStaffIp(ip: String) {
        activeStaffIp = ip
    }

    fun getTargetStaffIp(): String = activeStaffIp

    private fun handleIncomingPacket(packet: CelestialPacket) {
        val now = System.currentTimeMillis()
        if (packet.senderIp.isNotEmpty() && packet.senderIp != "0.0.0.0") {
            activeStaffIp = packet.senderIp
        }
        when (packet.msgType) {
            NetMessageType.HEARTBEAT -> {
                _telemetry.value = _telemetry.value.copy(
                    isOnline = true,
                    lastSeenTimestamp = now,
                    tickNumber = packet.seqNum.toLong()
                )
            }
            NetMessageType.RESP_TEXT -> {
                val text = String(packet.payload, Charsets.UTF_8)
                _telemetry.value = _telemetry.value.copy(
                    lastAiResponseText = text
                )
            }
            NetMessageType.APP_TELEMETRY -> {
                // If staff sends packed telemetry buffer (mode, volume, anim, state)
                parseTelemetryPayload(packet.payload)
            }
            else -> {
                // Other audio/response packets
            }
        }
    }

    private fun parseTelemetryPayload(payload: ByteArray) {
        if (payload.size < 6) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val modeId = buffer.get().toInt() and 0xFF
        val stateId = buffer.get().toInt() and 0xFF
        val vol = buffer.get().toInt() and 0xFF
        val animId = buffer.get().toInt() and 0xFF
        val clockStyleId = buffer.get().toInt() and 0xFF

        _telemetry.value = _telemetry.value.copy(
            mode = StaffMode.fromId(modeId),
            state = StaffState.fromOrdinalSafe(stateId),
            volumePercent = vol,
            activeLedAnim = LedAnimation.fromId(animId),
            clockStyle = ClockStyle.fromId(clockStyleId),
            isOnline = true,
            lastSeenTimestamp = System.currentTimeMillis()
        )
    }

    suspend fun executeCommand(command: StaffCommand) {
        val token = computeAuthToken()
        when (command) {
            is StaffCommand.SetMode -> {
                transport.sendCommand(activeStaffIp, 'M'.code.toByte(), command.mode.id.toByte(), token)
                _telemetry.value = _telemetry.value.copy(mode = command.mode)
            }
            is StaffCommand.SetVolume -> {
                transport.sendCommand(activeStaffIp, 'V'.code.toByte(), command.volumePercent.toByte(), token)
                _telemetry.value = _telemetry.value.copy(volumePercent = command.volumePercent)
            }
            is StaffCommand.SetLedAnimation -> {
                transport.sendCommand(activeStaffIp, 'L'.code.toByte(), command.animation.id.toByte(), token)
                _telemetry.value = _telemetry.value.copy(activeLedAnim = command.animation)
            }
            is StaffCommand.SetClockStyle -> {
                transport.sendCommand(activeStaffIp, 'K'.code.toByte(), command.style.id.toByte(), token)
                _telemetry.value = _telemetry.value.copy(clockStyle = command.style)
            }
            is StaffCommand.SyncTime -> {
                val payload = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
                    .putInt(token)
                    .putInt(command.epochSeconds.toInt())
                    .putInt(command.millis)
                    .array()
                val packet = CelestialPacket(
                    msgType = NetMessageType.APP_TIME_SYNC,
                    seqNum = 1,
                    payloadLen = payload.size,
                    requestId = 0L,
                    payload = payload
                )
                transport.sendPacket(activeStaffIp, ProtocolConstants.DEFAULT_PORT, packet)
            }
            is StaffCommand.PlaySfx -> {
                transport.sendCommand(activeStaffIp, 'C'.code.toByte(), command.sfxId.toByte(), token)
            }
            is StaffCommand.PlayChiptune -> {
                transport.sendCommand(activeStaffIp, 'T'.code.toByte(), command.trackId.toByte(), token)
            }
            is StaffCommand.PlayVoice -> {
                transport.sendCommand(activeStaffIp, 'P'.code.toByte(), command.phraseId.toByte(), token)
            }
            is StaffCommand.StopAudio -> {
                transport.sendCommand(activeStaffIp, 'S'.code.toByte(), 0.toByte(), token)
            }
        }
    }
}

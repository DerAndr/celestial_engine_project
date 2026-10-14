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

    private val _phoneBrainActive = MutableStateFlow(true)
    val phoneBrainActive: StateFlow<Boolean> = _phoneBrainActive.asStateFlow()

    private val _phoneModels = MutableStateFlow(
        listOf(
            PhoneAiModel(0, 4, "Gemma 2B Mobile", "GEMMA"),
            PhoneAiModel(1, 1, "Qwen 1.5B Mobile", "QWEN"),
            PhoneAiModel(2, 0, "Gemini Flash", "FLASH"),
            PhoneAiModel(3, 2, "Claude Sonnet", "CLAUDE")
        )
    )
    val phoneModels: StateFlow<List<PhoneAiModel>> = _phoneModels.asStateFlow()

    fun setPhoneBrainActive(active: Boolean) {
        _phoneBrainActive.value = active
    }

    fun buildPhoneCatalogPayload(models: List<PhoneAiModel> = _phoneModels.value): ByteArray {
        val hostType: Byte = 2 // HOST_TYPE_PHONE
        val hostName = "PHONE BRAIN"
        val count = models.size.coerceAtMost(8)

        val buffer = ByteBuffer.allocate(17 + count * 26).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put(hostType)
        val nameBytes = hostName.toByteArray(Charsets.US_ASCII)
        val namePadded = ByteArray(15)
        System.arraycopy(nameBytes, 0, namePadded, 0, nameBytes.size.coerceAtMost(14))
        buffer.put(namePadded)
        buffer.put(count.toByte())

        for (m in models.take(8)) {
            buffer.put(m.id.toByte())
            buffer.put(m.themeIdx.toByte())
            val mNameBytes = m.name.toByteArray(Charsets.US_ASCII)
            val mNamePadded = ByteArray(18)
            System.arraycopy(mNameBytes, 0, mNamePadded, 0, mNameBytes.size.coerceAtMost(17))
            buffer.put(mNamePadded)

            val mTagBytes = m.tag.toByteArray(Charsets.US_ASCII)
            val mTagPadded = ByteArray(6)
            System.arraycopy(mTagBytes, 0, mTagPadded, 0, mTagBytes.size.coerceAtMost(5))
            buffer.put(mTagPadded)
        }
        return buffer.array()
    }

    suspend fun sendPhoneCatalog(targetIp: String = activeStaffIp) {
        val payload = buildPhoneCatalogPayload()
        val packet = CelestialPacket(
            msgType = NetMessageType.CATALOG_RESP,
            seqNum = 1,
            payloadLen = payload.size,
            requestId = 0L,
            payload = payload
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, packet)
    }

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
            NetMessageType.CATALOG_REQ -> {
                if (_phoneBrainActive.value) {
                    val target = if (packet.senderIp.isNotEmpty() && packet.senderIp != "0.0.0.0") packet.senderIp else activeStaffIp
                    scope.launch {
                        sendPhoneCatalog(target)
                    }
                }
            }
            NetMessageType.CATALOG_RESP -> {
                if (packet.payload.size >= 17) {
                    val hostType = packet.payload[0].toInt() and 0xFF
                    val hostName = String(packet.payload, 1, 15, Charsets.US_ASCII).trim('\u0000', ' ')
                    _telemetry.value = _telemetry.value.copy(
                        activeHost = hostName.ifEmpty { if (hostType == 1) "PI 5 BRAIN" else "PHONE BRAIN" }
                    )
                }
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
            is StaffCommand.BroadcastCatalog -> {
                sendPhoneCatalog(activeStaffIp)
            }
        }
    }
}

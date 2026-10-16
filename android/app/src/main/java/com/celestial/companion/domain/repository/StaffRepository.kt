package com.celestial.companion.domain.repository

import android.util.Log
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.ai.GemmaResult
import com.celestial.companion.domain.model.*
import com.celestial.companion.network.StaffTransport
import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import com.celestial.companion.protocol.ProtocolConstants
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.asSharedFlow
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

private const val TAG = "StaffRepository"

enum class ConnectionMode(val title: String, val subtitle: String) {
    STANDALONE_PHONE("STANDALONE PHONE", "Local Gemma 4 AI Oracle (No Hardware Required)"),
    PHONE_BRAIN_SERVER("PHONE BRAIN HOST", "Phone acts as Wi-Fi Brain Server for Staff (UDP 5005)"),
    MESH_ACTIVE("FULL MESH", "Staff + Wearable Pi 5 + Companion Phone connected")
}

data class IncomingStaffVoiceQuery(
    val audioBytes: ByteArray,
    val senderIp: String,
    val timestamp: Long = System.currentTimeMillis()
)

class StaffRepository(
    private val transport: StaffTransport,
    val gemmaEngine: GemmaModelEngine? = null
) {
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    private val _telemetry = MutableStateFlow(StaffTelemetry())
    val telemetry: StateFlow<StaffTelemetry> = _telemetry.asStateFlow()

    private val _connectionMode = MutableStateFlow(ConnectionMode.PHONE_BRAIN_SERVER)
    val connectionMode: StateFlow<ConnectionMode> = _connectionMode.asStateFlow()

    fun setConnectionMode(mode: ConnectionMode) {
        _connectionMode.value = mode
        Log.i(TAG, "Connection mode switched to: ${mode.name}")
        when (mode) {
            ConnectionMode.PHONE_BRAIN_SERVER -> {
                _phoneBrainActive.value = true
                scope.launch {
                    executeCommand(StaffCommand.SetHost(2))
                    sendPhoneCatalog(activeStaffIp)
                }
            }
            ConnectionMode.MESH_ACTIVE -> {
                _phoneBrainActive.value = true
                scope.launch {
                    executeCommand(StaffCommand.SetHost(1))
                }
            }
            ConnectionMode.STANDALONE_PHONE -> {
                _phoneBrainActive.value = true
            }
        }
    }

    private var activeStaffIp: String = "192.168.178.101" // Default staff fallback IP
    var passkey: String = "apophis"

    private val incomingAudioStream = ByteArrayOutputStream()

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
            PhoneAiModel(0, 4, "Gemma 4 (E2B)", "G4-MM"),
            PhoneAiModel(1, 1, "Gemma 4 Think", "G4THK"),
            PhoneAiModel(2, 0, "Gemini Nano", "NANO"),
            PhoneAiModel(3, 2, "Pi 5 Core", "PI5")
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

    suspend fun broadcastAiResponse(text: String, targetIp: String = activeStaffIp) {
        val textBytes = text.toByteArray(Charsets.UTF_8)
        val textPacket = CelestialPacket(
            msgType = NetMessageType.RESP_TEXT,
            seqNum = 1,
            payloadLen = textBytes.size,
            requestId = 1L,
            payload = textBytes
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, textPacket)

        val startPacket = CelestialPacket(
            msgType = NetMessageType.RESP_START,
            seqNum = 1,
            payloadLen = 0,
            requestId = 1L,
            payload = ByteArray(0)
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, startPacket)

        delay(150)

        val endPacket = CelestialPacket(
            msgType = NetMessageType.RESP_END,
            seqNum = 2,
            payloadLen = 0,
            requestId = 1L,
            payload = ByteArray(0)
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, endPacket)
    }

    private val _staffVoiceQueries = kotlinx.coroutines.flow.MutableSharedFlow<IncomingStaffVoiceQuery>(extraBufferCapacity = 8)
    val staffVoiceQueries: kotlinx.coroutines.flow.SharedFlow<IncomingStaffVoiceQuery> = _staffVoiceQueries.asSharedFlow()

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
            NetMessageType.QUERY_START -> {
                Log.i(TAG, "Incoming QUERY_START from ${packet.senderIp}")
                incomingAudioStream.reset()
                _telemetry.value = _telemetry.value.copy(
                    state = StaffState.LISTENING,
                    isOnline = true
                )
            }
            NetMessageType.AUDIO_CHUNK -> {
                if (packet.payload.isNotEmpty()) {
                    incomingAudioStream.write(packet.payload)
                }
            }
            NetMessageType.QUERY_END -> {
                Log.i(TAG, "Incoming QUERY_END from ${packet.senderIp}. Audio size: ${incomingAudioStream.size()} bytes.")
                _telemetry.value = _telemetry.value.copy(
                    state = StaffState.PROCESSING
                )
                val audioData = incomingAudioStream.toByteArray()
                incomingAudioStream.reset()

                if (_phoneBrainActive.value) {
                    val emitted = _staffVoiceQueries.tryEmit(IncomingStaffVoiceQuery(audioData, activeStaffIp))
                    if (!emitted || _staffVoiceQueries.subscriptionCount.value == 0) {
                        scope.launch {
                            val fullAnswer = StringBuilder()
                            gemmaEngine?.streamInference("Analyze voice inquiry from staff", audioBytes = audioData)?.collect { chunk ->
                                when (chunk) {
                                    is com.celestial.companion.ai.GemmaStreamChunk.PhaseChanged -> {
                                        if (chunk.phase == com.celestial.companion.ai.AiProcessingPhase.ERROR) {
                                            _telemetry.value = _telemetry.value.copy(
                                                state = StaffState.IDLE
                                            )
                                        }
                                    }
                                    is com.celestial.companion.ai.GemmaStreamChunk.OutputToken -> {
                                        fullAnswer.append(chunk.token)
                                        _telemetry.value = _telemetry.value.copy(
                                            lastAiResponseText = fullAnswer.toString()
                                        )
                                    }
                                    is com.celestial.companion.ai.GemmaStreamChunk.Complete -> {
                                        _telemetry.value = _telemetry.value.copy(
                                            lastAiResponseText = chunk.result.outputText,
                                            state = StaffState.SPEAKING
                                        )
                                        broadcastAiResponse(chunk.result.outputText, activeStaffIp)
                                        delay(1200)
                                        _telemetry.value = _telemetry.value.copy(
                                            state = StaffState.IDLE
                                        )
                                    }
                                    else -> {}
                                }
                            }
                        }
                    }
                }
            }
            NetMessageType.RESP_TEXT -> {
                val text = String(packet.payload, Charsets.UTF_8)
                _telemetry.value = _telemetry.value.copy(
                    lastAiResponseText = text
                )
            }
            NetMessageType.APP_TELEMETRY -> {
                parseTelemetryPayload(packet.payload)
            }
            else -> {
                // Other packets
            }
        }
    }

    private fun parseTelemetryPayload(payload: ByteArray) {
        if (payload.size < 5) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val modeId = buffer.get().toInt() and 0xFF
        val stateId = buffer.get().toInt() and 0xFF
        val vol = buffer.get().toInt() and 0xFF
        val animId = buffer.get().toInt() and 0xFF
        val clockStyleId = buffer.get().toInt() and 0xFF
        val langId = if (payload.size >= 6) buffer.get().toInt() and 0xFF else 0
        val hostType = if (payload.size >= 7) buffer.get().toInt() and 0xFF else 0
        val prefHost = if (payload.size >= 8) buffer.get().toInt() and 0xFF else 0

        _telemetry.value = _telemetry.value.copy(
            mode = StaffMode.fromId(modeId),
            state = StaffState.fromOrdinalSafe(stateId),
            volumePercent = vol,
            activeLedAnim = LedAnimation.fromId(animId),
            clockStyle = ClockStyle.fromId(clockStyleId),
            language = StaffLanguage.fromId(langId),
            activeHost = if (hostType == 1) "PI 5 BRAIN" else if (hostType == 2) "PHONE BRAIN" else _telemetry.value.activeHost,
            preferredHost = prefHost,
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
            is StaffCommand.SetLanguage -> {
                transport.sendCommand(activeStaffIp, 'D'.code.toByte(), command.language.id.toByte(), token)
                _telemetry.value = _telemetry.value.copy(language = command.language)
            }
            is StaffCommand.SetHost -> {
                transport.sendCommand(activeStaffIp, 'H'.code.toByte(), command.hostType.toByte(), token)
                _telemetry.value = _telemetry.value.copy(
                    preferredHost = command.hostType,
                    activeHost = when (command.hostType) {
                        1 -> "PI 5 BRAIN"
                        2 -> "PHONE BRAIN"
                        else -> "AUTO MESH"
                    }
                )
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

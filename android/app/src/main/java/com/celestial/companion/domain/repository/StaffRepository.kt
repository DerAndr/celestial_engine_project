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
    val gemmaEngine: GemmaModelEngine? = null,
    private val context: android.content.Context? = null
) {
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    private val ttsSynthesizer = context?.let { com.celestial.companion.audio.StaffTtsSynthesizer(it) }

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
    private var activeAudioJob: Job? = null

    fun abortCurrentQuery() {
        Log.i(TAG, "abortCurrentQuery: aborting TTS, canceling audio stream, sending CANCEL packet")
        ttsSynthesizer?.stop()
        activeAudioJob?.cancel()
        activeAudioJob = null
        incomingAudioStream.reset()
        scope.launch {
            val cancelPacket = CelestialPacket(
                msgType = NetMessageType.CANCEL,
                seqNum = 0,
                payloadLen = 0,
                requestId = 0L,
                payload = ByteArray(0)
            )
            transport.sendPacket(activeStaffIp, ProtocolConstants.DEFAULT_PORT, cancelPacket)
            transport.sendPacket("255.255.255.255", ProtocolConstants.DEFAULT_PORT, cancelPacket)
        }
        _telemetry.value = _telemetry.value.copy(state = StaffState.IDLE)
    }

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

        // Auto-discovery beacon: broadcast heartbeat every 1.5s when offline to find staff dynamically
        scope.launch {
            while (isActive) {
                if (!_telemetry.value.isOnline) {
                    val curMs = System.currentTimeMillis()
                    val localOffset = java.util.TimeZone.getDefault().getOffset(curMs)
                    val epochSec = ((curMs + localOffset) / 1000).toInt()
                    val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(epochSec).array()
                    val ping = CelestialPacket(
                        msgType = NetMessageType.HEARTBEAT,
                        seqNum = 1,
                        payloadLen = buf.size,
                        requestId = 0L,
                        payload = buf
                    )
                    transport.sendPacket("255.255.255.255", ProtocolConstants.DEFAULT_PORT, ping)
                    if (activeStaffIp.isNotEmpty() && activeStaffIp != "255.255.255.255") {
                        transport.sendPacket(activeStaffIp, ProtocolConstants.DEFAULT_PORT, ping)
                    }
                }
                delay(1500)
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

    suspend fun streamAudioToStaff(pcmBytes: ByteArray, targetIp: String = activeStaffIp, reqId: Long = System.currentTimeMillis()) {
        val sampleCount = pcmBytes.size / 2
        val startPayload = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(sampleCount).array()
        val startPacket = CelestialPacket(
            msgType = NetMessageType.RESP_START,
            seqNum = 1,
            payloadLen = startPayload.size,
            requestId = reqId,
            payload = startPayload
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, startPacket)
        delay(25)

        val chunkSize = 480 // 480 bytes = 240 samples @ 16kHz = 15ms of audio (<= NET_MAX_PAYLOAD_SIZE 512)
        var seq = 0
        var offset = 0
        while (kotlinx.coroutines.currentCoroutineContext().isActive && offset < pcmBytes.size) {
            val length = minOf(chunkSize, pcmBytes.size - offset)
            val chunk = pcmBytes.copyOfRange(offset, offset + length)
            val chunkPacket = CelestialPacket(
                msgType = NetMessageType.RESP_CHUNK,
                seqNum = seq,
                payloadLen = chunk.size,
                requestId = reqId,
                payload = chunk
            )
            transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, chunkPacket)
            offset += length
            seq++
            delay(15) // Matches 15ms playback rate of 240 samples at 16kHz
        }

        if (!kotlinx.coroutines.currentCoroutineContext().isActive) {
            val cancelPacket = CelestialPacket(
                msgType = NetMessageType.CANCEL,
                seqNum = seq,
                payloadLen = 0,
                requestId = reqId,
                payload = ByteArray(0)
            )
            transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, cancelPacket)
            return
        }

        val endPacket = CelestialPacket(
            msgType = NetMessageType.RESP_END,
            seqNum = seq,
            payloadLen = 0,
            requestId = reqId,
            payload = ByteArray(0)
        )
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, endPacket)
        Log.i(TAG, "streamAudioToStaff: finished streaming $sampleCount samples ($seq chunks) to $targetIp")
    }

    suspend fun broadcastOledText(text: String, targetIp: String = activeStaffIp) {
        val textBytes = text.toByteArray(Charsets.UTF_8)
        val reqId = System.currentTimeMillis()
        val textPacket = CelestialPacket(
            msgType = NetMessageType.RESP_TEXT,
            seqNum = 1,
            payloadLen = textBytes.size,
            requestId = reqId,
            payload = textBytes
        )
        // Send direct to active staff IP
        transport.sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, textPacket)
        // Also send to subnet broadcast so if staff IP differed it still arrives
        transport.sendPacket("255.255.255.255", ProtocolConstants.DEFAULT_PORT, textPacket)
        Log.i(TAG, "broadcastOledText: sent ${textBytes.size} bytes of text to $targetIp and broadcast")
    }

    suspend fun broadcastAiResponse(text: String, targetIp: String = activeStaffIp) {
        broadcastOledText(text, targetIp)

        // Synthesize and stream speech audio to staff speaker in background
        activeAudioJob?.cancel()
        activeAudioJob = scope.launch {
                // Check if this is an arcane spell incantation
                val isSpell = text.contains("[SPELL:", ignoreCase = true)

                // Strip technical [SPELL: ...] tag so TTS only speaks the incantation / conversational reply
                val speechText = text.replace(Regex("""\[SPELL:[^\]]*\]\s*"""), "").trim()
                if (speechText.isBlank()) return@launch

                // CRITICAL: Spells MUST ALWAYS use the ENGLISH audio generator (Locale.US)
                val lang = _telemetry.value.language
                val locale = when {
                    isSpell -> java.util.Locale.US
                    speechText.any { it in '\u0400'..'\u04FF' } -> java.util.Locale("ru", "RU")
                    lang != StaffLanguage.AUTO -> lang.locale
                    else -> java.util.Locale.US
                }
                Log.i(TAG, "Synthesizing audio: isSpell=$isSpell, locale=$locale, text='${speechText.take(40)}...'")
                val pcm = ttsSynthesizer?.synthesizeTo16kMonoPcm(speechText, locale)
                if (pcm != null && pcm.isNotEmpty()) {
                    streamAudioToStaff(pcm, targetIp, System.currentTimeMillis())
                } else {
                    Log.w(TAG, "TTS synthesis returned null/empty PCM, speech audio not streamed")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error synthesizing or streaming speech to staff: ${e.message}", e)
            }
        }
    }

    private val _staffVoiceQueries = kotlinx.coroutines.flow.MutableSharedFlow<IncomingStaffVoiceQuery>(extraBufferCapacity = 8)
    val staffVoiceQueries: kotlinx.coroutines.flow.SharedFlow<IncomingStaffVoiceQuery> = _staffVoiceQueries.asSharedFlow()

    private fun handleIncomingPacket(packet: CelestialPacket) {
        val now = System.currentTimeMillis()
        if (packet.senderIp.isEmpty() || packet.senderIp == "0.0.0.0" || packet.senderIp == "127.0.0.1") {
            return
        }
        val isSelf = try {
            java.net.NetworkInterface.getNetworkInterfaces()?.toList()?.any { iface ->
                iface.inetAddresses.toList().any { it.hostAddress == packet.senderIp }
            } == true
        } catch (_: Exception) { false }
        if (isSelf) {
            return
        }
        activeStaffIp = packet.senderIp
        when (packet.msgType) {
            NetMessageType.HEARTBEAT -> {
                _telemetry.value = _telemetry.value.copy(
                    isOnline = true,
                    lastSeenTimestamp = now,
                    tickNumber = packet.seqNum.toLong()
                )
                if (_phoneBrainActive.value) {
                    val target = if (packet.senderIp.isNotEmpty() && packet.senderIp != "0.0.0.0") packet.senderIp else activeStaffIp
                    scope.launch {
                        val curMs = System.currentTimeMillis()
                        val localOffset = java.util.TimeZone.getDefault().getOffset(curMs)
                        val epochSec = ((curMs + localOffset) / 1000).toInt()
                        val buf = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(epochSec).array()
                        val respPacket = CelestialPacket(
                            msgType = NetMessageType.HEARTBEAT,
                            seqNum = packet.seqNum,
                            payloadLen = buf.size,
                            requestId = 0L,
                            payload = buf
                        )
                        transport.sendPacket(target, ProtocolConstants.DEFAULT_PORT, respPacket)
                    }
                }
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
            NetMessageType.CANCEL -> {
                Log.i(TAG, "Incoming CANCEL from staff ${packet.senderIp}")
                ttsSynthesizer?.stop()
                activeAudioJob?.cancel()
                activeAudioJob = null
                incomingAudioStream.reset()
                _telemetry.value = _telemetry.value.copy(state = StaffState.IDLE)
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
        val aiModelId = if (payload.size >= 9) buffer.get().toInt() and 0xFF else 0
        val micLevel = if (payload.size >= 10) buffer.get().toInt() and 0xFF else 0
        val cosmosZoom = if (payload.size >= 11) buffer.get().toInt() and 0xFF else 0
        val isAudioPlaying = if (payload.size >= 12) (buffer.get().toInt() and 0xFF) != 0 else false

        _telemetry.value = _telemetry.value.copy(
            mode = StaffMode.fromId(modeId),
            state = StaffState.fromOrdinalSafe(stateId),
            volumePercent = vol,
            activeLedAnim = LedAnimation.fromId(animId),
            clockStyle = ClockStyle.fromId(clockStyleId),
            language = StaffLanguage.fromId(langId),
            activeHost = if (hostType == 1) "PI 5 BRAIN" else if (hostType == 2) "PHONE BRAIN" else _telemetry.value.activeHost,
            preferredHost = prefHost,
            aiModelId = aiModelId,
            micLevelPercent = micLevel,
            cosmosZoom = cosmosZoom,
            isAudioPlaying = isAudioPlaying,
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
                // Incorporate local timezone offset so Pico's 24-hour clock matches the smartphone wall-clock time
                val now = System.currentTimeMillis()
                val tzOffset = java.util.TimeZone.getDefault().getOffset(now)
                val localEpochSeconds = (now + tzOffset) / 1000
                val millis = ((now + tzOffset) % 1000).toInt()
                val payload = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
                    .putInt(token)
                    .putInt(localEpochSeconds.toInt())
                    .putInt(millis)
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

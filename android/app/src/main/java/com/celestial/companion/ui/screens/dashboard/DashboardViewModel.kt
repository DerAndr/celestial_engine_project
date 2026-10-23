package com.celestial.companion.ui.screens.dashboard

import android.graphics.Bitmap
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.ai.GemmaResult
import com.celestial.companion.ai.GemmaStreamChunk
import com.celestial.companion.ai.GemmaVariant
import com.celestial.companion.domain.model.*
import com.celestial.companion.domain.repository.ConnectionMode
import com.celestial.companion.domain.repository.StaffRepository
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import android.util.Log

data class OracleChatMessage(
    val id: String = java.util.UUID.randomUUID().toString(),
    val isUser: Boolean,
    val text: String,
    val image: Bitmap? = null,
    val thoughtTrace: String = "",
    val modalityUsed: String = "TEXT",
    val latencyMs: Long = 0,
    val tokensPerSec: Float = 0f,
    val timestamp: Long = System.currentTimeMillis(),
    val isStreaming: Boolean = false
)

class DashboardViewModel(
    private val repository: StaffRepository,
    val gemmaEngine: GemmaModelEngine? = repository.gemmaEngine
) : ViewModel() {

    private val TAG = "DashboardViewModel"

    val telemetry: StateFlow<StaffTelemetry> = repository.telemetry
    val phoneBrainActive: StateFlow<Boolean> = repository.phoneBrainActive
    val phoneModels: StateFlow<List<PhoneAiModel>> = repository.phoneModels
    val connectionMode: StateFlow<ConnectionMode> = repository.connectionMode

    private val _currentLanguage = MutableStateFlow(StaffLanguage.ENGLISH)
    val currentLanguage: StateFlow<StaffLanguage> = _currentLanguage.asStateFlow()

    private val _selectedModelVariant = MutableStateFlow(gemmaEngine?.config?.variant ?: GemmaVariant.GEMMA_4_31B)
    val selectedModelVariant: StateFlow<GemmaVariant> = _selectedModelVariant.asStateFlow()

    // Real-Time Processing Monitoring State
    private val _processingPhase = MutableStateFlow(AiProcessingPhase.IDLE)
    val processingPhase: StateFlow<AiProcessingPhase> = _processingPhase.asStateFlow()

    private val _liveElapsedMs = MutableStateFlow(0L)
    val liveElapsedMs: StateFlow<Long> = _liveElapsedMs.asStateFlow()

    private val _liveTokenCount = MutableStateFlow(0)
    val liveTokenCount: StateFlow<Int> = _liveTokenCount.asStateFlow()

    private val _liveTokensPerSec = MutableStateFlow(0f)
    val liveTokensPerSec: StateFlow<Float> = _liveTokensPerSec.asStateFlow()

    private val _liveModality = MutableStateFlow("TEXT")
    val liveModality: StateFlow<String> = _liveModality.asStateFlow()

    private val _activePrompt = MutableStateFlow("")
    val activePrompt: StateFlow<String> = _activePrompt.asStateFlow()

    private val _activeImage = MutableStateFlow<Bitmap?>(null)
    val activeImage: StateFlow<Bitmap?> = _activeImage.asStateFlow()

    private val _selectedImage = MutableStateFlow<Bitmap?>(null)
    val selectedImage: StateFlow<Bitmap?> = _selectedImage.asStateFlow()

    fun setSelectedImage(bitmap: Bitmap?) {
        _selectedImage.value = bitmap
    }

    // Active Spell & Arcane Incantation System
    private val _activeSpell = MutableStateFlow<SpellDefinition?>(null)
    val activeSpell: StateFlow<SpellDefinition?> = _activeSpell.asStateFlow()

    private var soundSequenceJob: Job? = null

    fun clearActiveSpell() {
        _activeSpell.value = null
        soundSequenceJob?.cancel()
        setLedAnimation(LedAnimation.RAINBOW)
    }

    fun castSpell(spell: SpellDefinition) {
        _activeSpell.value = spell
        setLedAnimation(spell.animation)
        viewModelScope.launch {
            repository.broadcastAiResponse("[SPELL] ${spell.name}")
        }
        playSoundSequence(spell.sounds)
        queryGemma(spell.prompt)
    }

    fun playSoundSequence(sounds: List<Int>) {
        soundSequenceJob?.cancel()
        soundSequenceJob = viewModelScope.launch {
            for (sfxId in sounds) {
                if (!isActive) break
                repository.executeCommand(StaffCommand.PlaySfx(sfxId))
                delay(750L)
            }
        }
    }

    private val _chatHistory = MutableStateFlow<List<OracleChatMessage>>(emptyList())
    val chatHistory: StateFlow<List<OracleChatMessage>> = _chatHistory.asStateFlow()

    // Live Gemma 4 Interactive Studio State
    private val _gemmaResult = MutableStateFlow<GemmaResult?>(null)
    val gemmaResult: StateFlow<GemmaResult?> = _gemmaResult.asStateFlow()

    private val _isGenerating = MutableStateFlow(false)
    val isGenerating: StateFlow<Boolean> = _isGenerating.asStateFlow()

    private val _liveThoughtTrace = MutableStateFlow("")
    val liveThoughtTrace: StateFlow<String> = _liveThoughtTrace.asStateFlow()

    private val _liveStreamingText = MutableStateFlow("")
    val liveStreamingText: StateFlow<String> = _liveStreamingText.asStateFlow()

    private var activeGenerationJob: Job? = null
    private var liveTimerJob: Job? = null

    init {
        // Collect incoming voice queries from the physical staff over UDP
        viewModelScope.launch {
            repository.staffVoiceQueries.collect { query ->
                queryGemma(
                    prompt = "",
                    audioBytes = query.audioBytes,
                    broadcastOnComplete = true,
                    targetStaffIp = query.senderIp
                )
            }
        }
    }

    fun setLanguage(lang: StaffLanguage) {
        _currentLanguage.value = lang
        gemmaEngine?.config = gemmaEngine?.config?.copy(language = lang) ?: return
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetLanguage(lang))
        }
    }

    fun setModelVariant(variant: GemmaVariant) {
        _selectedModelVariant.value = variant
        gemmaEngine?.let { engine ->
            val updatedConfig = engine.config.copy(variant = variant)
            engine.saveConfigToPrefs(updatedConfig)
        }
    }

    fun setStaffHost(hostType: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetHost(hostType))
            if (hostType == 2) {
                repository.executeCommand(StaffCommand.BroadcastCatalog())
            }
        }
    }

    fun setConnectionMode(mode: ConnectionMode) {
        repository.setConnectionMode(mode)
    }

    fun cancelGeneration() {
        activeGenerationJob?.cancel()
        liveTimerJob?.cancel()
        soundSequenceJob?.cancel()
        repository.abortCurrentQuery()
        _isGenerating.value = false
        _processingPhase.value = AiProcessingPhase.CANCELLED
        val interruptedText = _liveStreamingText.value.ifBlank { "Generation stopped by user." } + " [STOPPED]"
        _liveStreamingText.value = interruptedText

        // Update active chat message
        _chatHistory.value = _chatHistory.value.map { msg ->
            if (msg.isStreaming) {
                msg.copy(
                    text = interruptedText,
                    thoughtTrace = _liveThoughtTrace.value,
                    isStreaming = false
                )
            } else msg
        }
    }

    fun queryGemma(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null,
        broadcastOnComplete: Boolean = false,
        targetStaffIp: String? = null
    ) {
        val engine = gemmaEngine ?: return
        if (_isGenerating.value) {
            cancelGeneration()
        }

        val hasImage = image != null
        val hasAudio = audioBytes != null && audioBytes.isNotEmpty()
        val modality = when {
            hasImage && hasAudio -> "MULTIMODAL (IMG+VOICE)"
            hasImage -> "VISION + TEXT"
            hasAudio -> "AUDIO + TEXT"
            else -> "TEXT"
        }

        _isGenerating.value = true
        _processingPhase.value = AiProcessingPhase.INGESTING
        _liveElapsedMs.value = 0L
        _liveTokenCount.value = 0
        _liveTokensPerSec.value = 0f
        _liveModality.value = modality
        _liveThoughtTrace.value = ""
        _liveStreamingText.value = ""
        _gemmaResult.value = null
        _activePrompt.value = prompt
        _activeImage.value = image

        // Append to chat history
        val userMsg = OracleChatMessage(
            isUser = true,
            text = prompt.ifBlank { if (hasAudio) "🎤 [VOICE INQUIRY FROM STAFF (${audioBytes?.size ?: 0} B)]" else "..." },
            image = image,
            modalityUsed = modality
        )
        val assistantMsgId = java.util.UUID.randomUUID().toString()
        val placeholderAssistantMsg = OracleChatMessage(
            id = assistantMsgId,
            isUser = false,
            text = "",
            isStreaming = true,
            modalityUsed = modality
        )
        _chatHistory.value = _chatHistory.value + userMsg + placeholderAssistantMsg

        val t0 = System.currentTimeMillis()

        // Real-time ticker job for live latency & throughput monitoring
        liveTimerJob?.cancel()
        liveTimerJob = viewModelScope.launch {
            while (isActive && _isGenerating.value) {
                delay(40)
                val elapsed = System.currentTimeMillis() - t0
                _liveElapsedMs.value = elapsed
                val tokens = _liveTokenCount.value
                if (elapsed > 100 && tokens > 0) {
                    _liveTokensPerSec.value = tokens.toFloat() / (elapsed / 1000f)
                }
            }
        }

        activeGenerationJob?.cancel()
        var spellExecuted = false
        val spellTagRegex = Regex("""\[SPELL:\s*([^|\]]+)\s*\|\s*ANIM:\s*([^|\]]+)\s*\|\s*SOUNDS:\s*([^|\]]+)\]""", RegexOption.IGNORE_CASE)

        activeGenerationJob = viewModelScope.launch {
            try {
                engine.streamInference(prompt, image, audioBytes).collect { chunk ->
                    when (chunk) {
                        is GemmaStreamChunk.PhaseChanged -> {
                            _processingPhase.value = chunk.phase
                            if (chunk.phase == AiProcessingPhase.ERROR) {
                                liveTimerJob?.cancel()
                                _isGenerating.value = false
                            }
                        }
                        is GemmaStreamChunk.ThoughtToken -> {
                            _liveThoughtTrace.value += chunk.token
                            _liveTokenCount.value += 1
                        }
                        is GemmaStreamChunk.OutputToken -> {
                            _liveStreamingText.value += chunk.token
                            _liveTokenCount.value += 1

                            // Check and parse live spell tags from LLM
                            if (!spellExecuted) {
                                val currentText = _liveStreamingText.value
                                val spellMatch = spellTagRegex.find(currentText)
                                if (spellMatch != null) {
                                    spellExecuted = true
                                    val spellName = spellMatch.groupValues[1].trim()
                                    val animStr = spellMatch.groupValues[2].trim()
                                    val soundsStr = spellMatch.groupValues[3].trim()

                                    val ledAnim = parseLedAnimation(animStr)
                                    val soundIds = parseSoundSequence(soundsStr)

                                    val spellDef = SpellDefinition(
                                        name = spellName,
                                        animation = ledAnim,
                                        sounds = soundIds,
                                        prompt = prompt,
                                        description = "Arcane Conjuration"
                                    )
                                    _activeSpell.value = spellDef
                                    setLedAnimation(ledAnim)
                                    viewModelScope.launch {
                                        repository.broadcastAiResponse("[SPELL] $spellName")
                                    }
                                    playSoundSequence(soundIds)
                                    Log.i(TAG, "Cast spell dynamically: '$spellName' with anim=$ledAnim and sounds=$soundIds")
                                }
                            }

                            // Update active chat message live
                            val currentText = _liveStreamingText.value
                            val currentThoughts = _liveThoughtTrace.value
                            _chatHistory.value = _chatHistory.value.map { msg ->
                                if (msg.id == assistantMsgId) {
                                    msg.copy(
                                        text = currentText,
                                        thoughtTrace = currentThoughts,
                                        isStreaming = true
                                    )
                                } else msg
                            }
                        }
                        is GemmaStreamChunk.Complete -> {
                            liveTimerJob?.cancel()
                            _gemmaResult.value = chunk.result
                            _liveStreamingText.value = chunk.result.outputText
                            _processingPhase.value = AiProcessingPhase.COMPLETED
                            _isGenerating.value = false
                            _liveElapsedMs.value = chunk.result.latencyMs
                            _liveTokensPerSec.value = chunk.result.tokensPerSec

                            _chatHistory.value = _chatHistory.value.map { msg ->
                                if (msg.id == assistantMsgId) {
                                    msg.copy(
                                        text = chunk.result.outputText,
                                        thoughtTrace = chunk.result.thoughtTrace,
                                        latencyMs = chunk.result.latencyMs,
                                        tokensPerSec = chunk.result.tokensPerSec,
                                        isStreaming = false
                                    )
                                } else msg
                            }

                            if (broadcastOnComplete) {
                                repository.broadcastAiResponse(
                                    chunk.result.outputText,
                                    targetStaffIp ?: repository.getTargetStaffIp()
                                )
                            }
                        }
                    }
                }
            } catch (e: Throwable) {
                if (e is kotlinx.coroutines.CancellationException) throw e
                Log.e(TAG, "Inference anomaly: ${e.message}", e)
                liveTimerJob?.cancel()
                _isGenerating.value = false
                _processingPhase.value = AiProcessingPhase.ERROR
                val errText = "Inference anomaly: ${e.message ?: e.javaClass.simpleName}"
                _liveStreamingText.value = errText
                _chatHistory.value = _chatHistory.value.map { msg ->
                    if (msg.id == assistantMsgId) {
                        msg.copy(text = errText, isStreaming = false)
                    } else msg
                }
            }
        }
    }

    fun clearChatHistory() {
        _chatHistory.value = emptyList()
        _liveStreamingText.value = ""
        _liveThoughtTrace.value = ""
        _gemmaResult.value = null
        _activePrompt.value = ""
        _activeImage.value = null
        _processingPhase.value = AiProcessingPhase.IDLE
    }

    fun broadcastCurrentAnswerToStaff() {
        val currentText = _gemmaResult.value?.outputText ?: _liveStreamingText.value
        if (currentText.isNotBlank()) {
            viewModelScope.launch {
                repository.broadcastAiResponse(currentText)
            }
        }
    }

    fun togglePhoneBrain(active: Boolean) {
        repository.setPhoneBrainActive(active)
    }

    fun broadcastPhoneCatalog() {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetHost(2))
            repository.executeCommand(StaffCommand.BroadcastCatalog())
        }
    }

    fun setStaffIp(ip: String) {
        repository.setTargetStaffIp(ip)
    }

    fun getStaffIp(): String = repository.getTargetStaffIp()

    fun setPasskey(key: String) {
        repository.passkey = key
    }

    fun getPasskey(): String = repository.passkey

    fun saveConnectionSettings(context: android.content.Context, ip: String, passkey: String) {
        setStaffIp(ip)
        setPasskey(passkey)
        context.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            .edit()
            .putString("staff_ip", ip)
            .putString("auth_passkey", passkey)
            .apply()
    }

    fun selectMode(mode: StaffMode) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetMode(mode))
        }
    }

    fun setVolume(volumePercent: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetVolume(volumePercent))
        }
    }

    fun setClockStyle(style: ClockStyle) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetClockStyle(style))
        }
    }

    fun setLedAnimation(anim: LedAnimation) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetLedAnimation(anim))
        }
    }

    private var toneGenerator: android.media.ToneGenerator? = null

    fun playSfx(sfxId: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlaySfx(sfxId))
        }
        playLocalToneForSfx(sfxId)
    }

    private fun playLocalToneForSfx(sfxId: Int) {
        try {
            if (toneGenerator == null) {
                toneGenerator = android.media.ToneGenerator(android.media.AudioManager.STREAM_MUSIC, 75)
            }
            val toneType = when (sfxId % 10) {
                0 -> android.media.ToneGenerator.TONE_PROP_BEEP2
                1 -> android.media.ToneGenerator.TONE_CDMA_PIP
                2 -> android.media.ToneGenerator.TONE_DTMF_D
                3 -> android.media.ToneGenerator.TONE_PROP_ACK
                4 -> android.media.ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD
                5 -> android.media.ToneGenerator.TONE_CDMA_HIGH_L
                6 -> android.media.ToneGenerator.TONE_CDMA_LOW_L
                7 -> android.media.ToneGenerator.TONE_PROP_PROMPT
                8 -> android.media.ToneGenerator.TONE_CDMA_PRESSHOLDKEY_LITE
                else -> android.media.ToneGenerator.TONE_PROP_BEEP
            }
            toneGenerator?.startTone(toneType, 180)
        } catch (e: Exception) {
            android.util.Log.w("DashboardViewModel", "Local tone playback notice: ${e.message}")
        }
    }

    fun playChiptune(trackId: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlayChiptune(trackId))
        }
        playLocalToneForSfx(trackId + 2)
    }

    fun stopAudio() {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.StopAudio)
        }
        toneGenerator?.stopTone()
    }

    fun syncSystemTime() {
        val now = System.currentTimeMillis()
        val seconds = now / 1000
        val millis = (now % 1000).toInt()
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SyncTime(seconds, millis))
        }
    }

    override fun onCleared() {
        super.onCleared()
        toneGenerator?.release()
        toneGenerator = null
    }

    private fun parseLedAnimation(animStr: String): LedAnimation {
        val upper = animStr.uppercase()
        return when {
            upper.contains("SNAKE") -> LedAnimation.SNAKE
            upper.contains("VORTEX") -> LedAnimation.VORTEX
            upper.contains("PULSAR") || upper.contains("SHOCK") -> LedAnimation.PULSAR
            upper.contains("PLASMA") || upper.contains("FIRE") -> LedAnimation.PLASMA
            upper.contains("RAINBOW") -> LedAnimation.RAINBOW
            upper.contains("STARS") -> LedAnimation.STARS
            upper.contains("GALAXY") -> LedAnimation.GALAXY
            upper.contains("HALLOWEEN") || upper.contains("SPOOK") -> LedAnimation.HALLOWEEN
            upper.contains("ORRERY") -> LedAnimation.CLOCK_ORRERY
            else -> LedAnimation.SNAKE
        }
    }

    private fun parseSoundSequence(soundsStr: String): List<Int> {
        val tokens = soundsStr.split(",", ";", " ", "\n").map { it.trim().uppercase() }.filter { it.isNotEmpty() }
        val sfxList = mutableListOf<Int>()
        for (t in tokens) {
            val sfxId = when {
                t.contains("WARP") || t.contains("HYPER") -> 0
                t.contains("PULSAR") || t.contains("PING") -> 1
                t.contains("ALIEN") || t.contains("SCAN") -> 2
                t.contains("LASER") || t.contains("BLASTER") || t.contains("PEW") -> 3
                t.contains("TRACTOR") || t.contains("HUM") || t.contains("BEAM") -> 4
                t.contains("PORTAL") || t.contains("STARGATE") -> 5
                t.contains("SUPERNOVA") || t.contains("NOVA") || t.contains("BOOM") -> 6
                t.contains("HAPPY") || t.contains("CHIRP") || t.contains("WHISTLE") -> 7
                t.contains("CHATTER") || t.contains("BANTER") -> 8
                t.contains("PANIC") || t.contains("SCREAM") || t.contains("ALARM") -> 9
                else -> null
            }
            if (sfxId != null && !sfxList.contains(sfxId)) {
                sfxList.add(sfxId)
            }
        }
        return if (sfxList.isEmpty()) listOf(2, 9, 6) else sfxList
    }
}

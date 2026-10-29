package com.celestial.companion.ui.screens.dashboard

import android.graphics.Bitmap
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.ai.GemmaResult
import com.celestial.companion.ai.GemmaStreamChunk
import com.celestial.companion.ai.GemmaVariant
import com.celestial.companion.ai.FastSpellMatcher
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

    companion object {
        private const val TAG = "DashboardViewModel"
        const val MAX_CHAT_HISTORY = 40
    }

    val telemetry: StateFlow<StaffTelemetry> = repository.telemetry
    val phoneBrainActive: StateFlow<Boolean> = repository.phoneBrainActive
    val phoneModels: StateFlow<List<PhoneAiModel>> = repository.phoneModels
    val connectionMode: StateFlow<ConnectionMode> = repository.connectionMode

    private val _currentLanguage = MutableStateFlow(gemmaEngine?.config?.language ?: StaffLanguage.ENGLISH)
    val currentLanguage: StateFlow<StaffLanguage> = _currentLanguage.asStateFlow()

    private val _selectedModelVariant = MutableStateFlow(gemmaEngine?.config?.variant ?: GemmaVariant.GEMMA_4_E2B)
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

    // Persistent Grimoire Custom Spells
    private val _customSpells = MutableStateFlow<List<SpellDefinition>>(emptyList())
    val customSpells: StateFlow<List<SpellDefinition>> = _customSpells.asStateFlow()

    // Per-spell voice chant preferences (spellName -> boolean)
    private val _spellVoicePreferences = MutableStateFlow<Map<String, Boolean>>(emptyMap())
    val spellVoicePreferences: StateFlow<Map<String, Boolean>> = _spellVoicePreferences.asStateFlow()

    // Global Dashboard Quick-Cast voice chant toggle
    private val _dashboardVoiceEnabled = MutableStateFlow(true)
    val dashboardVoiceEnabled: StateFlow<Boolean> = _dashboardVoiceEnabled.asStateFlow()

    // Oracle Persona & System Prompt Customization
    private val _currentPersona = MutableStateFlow(
        OraclePersona.fromId(gemmaEngine?.config?.personaId ?: OraclePersona.MACHINE_GOD.id)
    )
    val currentPersona: StateFlow<OraclePersona> = _currentPersona.asStateFlow()

    private val _customSystemPrompt = MutableStateFlow(
        gemmaEngine?.config?.customSystemPrompt ?: ""
    )
    val customSystemPrompt: StateFlow<String> = _customSystemPrompt.asStateFlow()

    fun setPersona(persona: OraclePersona) {
        _currentPersona.value = persona
        gemmaEngine?.let { engine ->
            val updated = engine.config.copy(personaId = persona.id)
            engine.saveConfigToPrefs(updated)
        }
        repository.log("AI-PERSONA", "🎭 Active Persona switched to: ${persona.title} (${persona.icon})", LogLevel.INFO)
    }

    fun setCustomSystemPrompt(prompt: String) {
        _customSystemPrompt.value = prompt
        gemmaEngine?.let { engine ->
            val updated = engine.config.copy(customSystemPrompt = prompt)
            engine.saveConfigToPrefs(updated)
        }
    }

    init {
        loadCustomSpells()
        loadVoicePreferences()
    }

    fun toggleDashboardVoice() {
        _dashboardVoiceEnabled.value = !_dashboardVoiceEnabled.value
    }

    fun isSpellVoiceEnabled(spell: SpellDefinition): Boolean {
        return _spellVoicePreferences.value[spell.name] ?: spell.voiceEnabled
    }

    fun setSpellVoiceEnabled(spellName: String, enabled: Boolean) {
        val updated = _spellVoicePreferences.value + (spellName to enabled)
        _spellVoicePreferences.value = updated
        persistVoicePreferences(updated)
    }

    fun toggleSpellVoice(spell: SpellDefinition) {
        val current = isSpellVoiceEnabled(spell)
        setSpellVoiceEnabled(spell.name, !current)
    }

    private var soundSequenceJob: Job? = null

    fun clearActiveSpell() {
        _activeSpell.value = null
        soundSequenceJob?.cancel()
        setLedAnimation(LedAnimation.RAINBOW)
    }

    /**
     * Instant Zero-Latency Hardware Spell Execution (< 20ms).
     * Fires LED animation, OLED marquee, and sound combo sequence immediately.
     */
    fun executeFastPathSpell(spell: SpellDefinition, targetStaffIp: String? = null) {
        _activeSpell.value = spell
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetMode(StaffMode.LED_CORE))
            repository.executeCommand(StaffCommand.SetLedAnimation(spell.animation))
            val oledMsg = spell.oledText.ifBlank { "[SPELL] ${spell.name}" }
            repository.broadcastOledText(oledMsg, targetStaffIp ?: repository.getTargetStaffIp())
        }
        playSoundSequence(spell.sounds)
        Log.i(TAG, "⚡ FAST-PATH CAST: '${spell.name}' with anim=${spell.animation.name} sounds=${spell.sounds}")
        repository.log("SPELL", "⚡ FAST-PATH CAST: '${spell.name}' (anim=${spell.animation.name}) in <20ms", LogLevel.SUCCESS)
    }

    fun castSpell(spell: SpellDefinition, speakIncantation: Boolean? = null) {
        val shouldSpeak = speakIncantation ?: isSpellVoiceEnabled(spell)
        executeFastPathSpell(spell)
        if (shouldSpeak && spell.chantText.isNotBlank()) {
            viewModelScope.launch {
                val targetIp = repository.getTargetStaffIp()
                repository.broadcastAiResponse(
                    text = spell.chantText,
                    targetIp = targetIp,
                    skipVisualEffects = true,
                    customOledText = spell.oledText.ifBlank { "[SPELL] ${spell.name}" }
                )
            }
        } else if (shouldSpeak && spell.prompt.isNotBlank()) {
            queryGemma(spell.prompt, broadcastOnComplete = true)
        }
    }

    fun saveCustomSpell(spell: SpellDefinition) {
        val updated = _customSpells.value.filter { it.name.trim().lowercase() != spell.name.trim().lowercase() } + spell
        _customSpells.value = updated
        persistCustomSpells(updated)
        repository.log("SPELL", "💾 Saved custom spell: '${spell.name}' to Grimoire", LogLevel.INFO)
    }

    fun deleteCustomSpell(spellName: String) {
        val updated = _customSpells.value.filter { it.name.trim().lowercase() != spellName.trim().lowercase() }
        _customSpells.value = updated
        persistCustomSpells(updated)
        repository.log("SPELL", "🗑️ Deleted custom spell: '$spellName' from Grimoire", LogLevel.INFO)
    }

    private fun persistCustomSpells(spells: List<SpellDefinition>) {
        val ctx = repository.context ?: return
        try {
            val prefs = ctx.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            val arr = org.json.JSONArray()
            spells.forEach { arr.put(it.toJson()) }
            prefs.edit().putString("custom_spells", arr.toString()).apply()
        } catch (e: Exception) {
            Log.w(TAG, "Failed persisting custom spells: ${e.message}")
        }
    }

    fun loadCustomSpells() {
        val ctx = repository.context ?: return
        try {
            val prefs = ctx.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            val raw = prefs.getString("custom_spells", null) ?: return
            val arr = org.json.JSONArray(raw)
            val list = mutableListOf<SpellDefinition>()
            for (i in 0 until arr.length()) {
                list.add(SpellDefinition.fromJson(arr.getJSONObject(i)))
            }
            _customSpells.value = list
            Log.i(TAG, "Loaded ${list.size} custom spells from Grimoire")
        } catch (e: Exception) {
            Log.w(TAG, "Failed loading custom spells: ${e.message}")
        }
    }

    private fun persistVoicePreferences(prefsMap: Map<String, Boolean>) {
        val ctx = repository.context ?: return
        try {
            val prefs = ctx.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            val json = org.json.JSONObject()
            prefsMap.forEach { (k, v) -> json.put(k, v) }
            prefs.edit().putString("spell_voice_prefs", json.toString()).apply()
        } catch (e: Exception) {
            Log.w(TAG, "Failed persisting voice prefs: ${e.message}")
        }
    }

    fun loadVoicePreferences() {
        val ctx = repository.context ?: return
        try {
            val prefs = ctx.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            val raw = prefs.getString("spell_voice_prefs", null) ?: return
            val json = org.json.JSONObject(raw)
            val map = mutableMapOf<String, Boolean>()
            val keys = json.keys()
            while (keys.hasNext()) {
                val k = keys.next()
                map[k] = json.optBoolean(k, true)
            }
            _spellVoicePreferences.value = map
            Log.i(TAG, "Loaded ${map.size} spell voice preferences")
        } catch (e: Exception) {
            Log.w(TAG, "Failed loading voice prefs: ${e.message}")
        }
    }

    fun playSoundSequence(sounds: List<Int>) {
        soundSequenceJob?.cancel()
        soundSequenceJob = viewModelScope.launch {
            for (sfxId in sounds) {
                if (!isActive) break
                repository.executeCommand(StaffCommand.PlaySfx(sfxId))
                delay(400L) // Snappy 400ms combo delay instead of sluggish 750ms
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

    val liveLogs: StateFlow<List<com.celestial.companion.domain.model.LiveLogMessage>> = repository.liveLogs

    fun clearLogs() {
        repository.clearLogs()
    }

    private var activeGenerationJob: Job? = null
    private var liveTimerJob: Job? = null

    init {
        loadCustomSpells()
        val initLang = gemmaEngine?.config?.language ?: StaffLanguage.ENGLISH
        _currentLanguage.value = initLang
        // Collect incoming voice queries from the physical staff over UDP
        viewModelScope.launch {
            repository.staffVoiceQueries.collect { query ->
                if (query.audioBytes.size < 1600) {
                    Log.d(TAG, "DashboardViewModel: ignoring empty/too-short audio query (${query.audioBytes.size} bytes)")
                    return@collect
                }
                repository.log("VOICE", "Voice query received by ViewModel (${query.audioBytes.size} B), initiating Oracle query...", com.celestial.companion.domain.model.LogLevel.INFO)
                queryGemma(
                    prompt = "",
                    audioBytes = query.audioBytes,
                    broadcastOnComplete = true,
                    targetStaffIp = query.senderIp,
                    queryRequestId = query.requestId
                )
            }
        }
    }

    fun setLanguage(lang: StaffLanguage) {
        _currentLanguage.value = lang
        gemmaEngine?.let { engine ->
            val updatedConfig = engine.config.copy(language = lang)
            engine.saveConfigToPrefs(updatedConfig)
        }
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

    fun updateApiSettings(apiKey: String, apiBase: String) {
        gemmaEngine?.let { engine ->
            val updatedConfig = engine.config.copy(apiKey = apiKey.trim(), apiBase = apiBase.trim())
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
        targetStaffIp: String? = null,
        queryRequestId: Long = 0L
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
        val userMsgId = java.util.UUID.randomUUID().toString()
        val userMsg = OracleChatMessage(
            id = userMsgId,
            isUser = true,
            text = prompt.ifBlank { if (hasAudio) "🎤 [Processing staff voice...]" else "..." },
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
        val updatedHistory = _chatHistory.value + userMsg + placeholderAssistantMsg
        _chatHistory.value = if (updatedHistory.size > MAX_CHAT_HISTORY) {
            updatedHistory.takeLast(MAX_CHAT_HISTORY)
        } else {
            updatedHistory
        }

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
        var lastOledDispatchedLen = 0

        // FAST-PATH INSTANT SPELL DISPATCH (< 20ms) for typed text prompts
        if (prompt.isNotBlank()) {
            val fastSpell = FastSpellMatcher.match(prompt, _customSpells.value)
            if (fastSpell != null) {
                spellExecuted = true
                executeFastPathSpell(fastSpell, targetStaffIp)
            }
        }

        activeGenerationJob = viewModelScope.launch {
            try {
                repository.log("AI-ORACLE", "Starting inference (modality=$modality, audio=${audioBytes?.size ?: 0} B)...", com.celestial.companion.domain.model.LogLevel.INFO)
                engine.streamInference(prompt, image, audioBytes).collect { chunk ->
                    when (chunk) {
                        is GemmaStreamChunk.PhaseChanged -> {
                            _processingPhase.value = chunk.phase
                            if (chunk.phase == AiProcessingPhase.ERROR) {
                                liveTimerJob?.cancel()
                                _isGenerating.value = false
                                repository.log("AI-ORACLE", "Inference entered ERROR phase", com.celestial.companion.domain.model.LogLevel.ERROR)
                            }
                        }
                        is GemmaStreamChunk.ThoughtToken -> {
                            _liveThoughtTrace.value += chunk.token
                            _liveTokenCount.value += 1
                            if (chunk.token.startsWith("Acoustic voice received: \"")) {
                                val transcribedText = chunk.token.removePrefix("Acoustic voice received: \"").substringBefore("\"")
                                if (transcribedText.isNotBlank()) {
                                    _chatHistory.value = _chatHistory.value.map { msg ->
                                        if (msg.id == userMsgId) msg.copy(text = "🎤 \"$transcribedText\"") else msg
                                    }
                                    // FAST-PATH INSTANT SPELL DISPATCH (< 20ms) immediately on acoustic voice transcription!
                                    if (!spellExecuted) {
                                        val fastSpell = FastSpellMatcher.match(transcribedText, _customSpells.value)
                                        if (fastSpell != null) {
                                            spellExecuted = true
                                            executeFastPathSpell(fastSpell, targetStaffIp)
                                        }
                                    }
                                }
                            }
                        }
                        is GemmaStreamChunk.OutputToken -> {
                            _liveStreamingText.value += chunk.token
                            _liveTokenCount.value += 1

                            // Check and parse live spell tags from LLM
                            val currentText = _liveStreamingText.value
                            if (!spellExecuted) {
                                val parsed = CelestialProphecyParser.parse(currentText)
                                if (parsed.isSpell) {
                                    spellExecuted = true
                                    val spellName = parsed.spellName.ifBlank { "Arcane Spell" }
                                    val ledAnim = parsed.animation ?: LedAnimation.PLASMA
                                    val soundIds = parsed.soundEffects.ifEmpty { listOf(3, 0, 6) }

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
                                        repository.broadcastOledText(parsed.displayText.ifBlank { "[SPELL] $spellName" }, targetStaffIp ?: repository.getTargetStaffIp())
                                    }
                                    playSoundSequence(soundIds)
                                    Log.i(TAG, "Cast spell dynamically: '$spellName' with anim=$ledAnim and sounds=$soundIds")
                                    repository.log("SPELL", "✨ Cast spell dynamically: '$spellName' (anim=${ledAnim.name})", com.celestial.companion.domain.model.LogLevel.SUCCESS)
                                } else if (broadcastOnComplete && currentText.length >= 12 && (currentText.length - lastOledDispatchedLen >= 20 || currentText.endsWith(".") || currentText.endsWith("!"))) {
                                    // Conversational live stream to staff OLED marquee
                                    lastOledDispatchedLen = currentText.length
                                    val cleanSnippet = parsed.speechText.ifBlank { currentText }.take(48)
                                    viewModelScope.launch {
                                        repository.broadcastOledText(cleanSnippet, targetStaffIp ?: repository.getTargetStaffIp())
                                    }
                                }
                            }

                            // Update active chat message live
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
                            val prophecy = CelestialProphecyParser.parse(chunk.result.outputText, chunk.result.thoughtTrace)
                            val cleanOutput = prophecy.speechText.ifBlank { chunk.result.outputText }
                            val cleanThought = prophecy.thought.ifBlank { chunk.result.thoughtTrace }

                            val updatedResult = chunk.result.copy(
                                outputText = cleanOutput,
                                thoughtTrace = cleanThought
                            )
                            _gemmaResult.value = updatedResult
                            _liveStreamingText.value = cleanOutput
                            _liveThoughtTrace.value = cleanThought
                            _processingPhase.value = AiProcessingPhase.COMPLETED
                            _isGenerating.value = false
                            _liveElapsedMs.value = chunk.result.latencyMs
                            _liveTokensPerSec.value = chunk.result.tokensPerSec
                            repository.log("AI-ORACLE", "Generation complete in ${chunk.result.latencyMs}ms (${chunk.result.tokensPerSec} tok/s, ${cleanOutput.length} chars)", com.celestial.companion.domain.model.LogLevel.SUCCESS)

                            _chatHistory.value = _chatHistory.value.map { msg ->
                                if (msg.id == assistantMsgId) {
                                    msg.copy(
                                        text = cleanOutput,
                                        thoughtTrace = cleanThought,
                                        latencyMs = chunk.result.latencyMs,
                                        tokensPerSec = chunk.result.tokensPerSec,
                                        isStreaming = false
                                    )
                                } else msg
                            }

                            if (broadcastOnComplete) {
                                repository.broadcastAiResponse(
                                    chunk.result.outputText,
                                    targetStaffIp ?: repository.getTargetStaffIp(),
                                    skipVisualEffects = spellExecuted,
                                    customReqId = queryRequestId.takeIf { it != 0L }
                                )
                            }
                        }
                    }
                }
            } catch (e: Throwable) {
                if (e is kotlinx.coroutines.CancellationException) throw e
                Log.e(TAG, "Inference anomaly: ${e.message}", e)
                repository.log("AI-ORACLE", "Inference anomaly: ${e.message}", com.celestial.companion.domain.model.LogLevel.ERROR)
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
            repository.executeCommand(StaffCommand.SetMode(StaffMode.LED_CORE))
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

    fun playAllTracks() {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlayChiptune(0xFE))
        }
    }

    fun playAllCycled() {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlayChiptune(0xFF))
        }
    }

    fun activateThematicPack(pack: com.celestial.companion.domain.model.ThematicPack) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetLedAnimation(pack.animation))
            kotlinx.coroutines.delay(50)
            repository.executeCommand(StaffCommand.PlayChiptune(pack.trackId))
        }
        playLocalToneForSfx(pack.trackId + 2)
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
        return CelestialProphecyParser.parseLedAnimation(animStr)
    }

    private fun parseSoundSequence(soundsStr: String): List<Int> {
        return CelestialProphecyParser.parseSoundSequence(soundsStr)
    }
}

package com.celestial.companion.ui.screens.dashboard

import android.graphics.Bitmap
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.ai.GemmaResult
import com.celestial.companion.ai.GemmaStreamChunk
import com.celestial.companion.domain.model.*
import com.celestial.companion.domain.repository.ConnectionMode
import com.celestial.companion.domain.repository.StaffRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class DashboardViewModel(
    private val repository: StaffRepository,
    val gemmaEngine: GemmaModelEngine? = repository.gemmaEngine
) : ViewModel() {

    val telemetry: StateFlow<StaffTelemetry> = repository.telemetry
    val phoneBrainActive: StateFlow<Boolean> = repository.phoneBrainActive
    val phoneModels: StateFlow<List<PhoneAiModel>> = repository.phoneModels
    val connectionMode: StateFlow<ConnectionMode> = repository.connectionMode

    private val _currentLanguage = MutableStateFlow(StaffLanguage.ENGLISH)
    val currentLanguage: StateFlow<StaffLanguage> = _currentLanguage.asStateFlow()

    fun setLanguage(lang: StaffLanguage) {
        _currentLanguage.value = lang
        gemmaEngine?.config = gemmaEngine?.config?.copy(language = lang) ?: return
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetLanguage(lang))
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

    // Live Gemma 4 Interactive Studio State
    private val _gemmaResult = MutableStateFlow<GemmaResult?>(null)
    val gemmaResult: StateFlow<GemmaResult?> = _gemmaResult.asStateFlow()

    private val _isGenerating = MutableStateFlow(false)
    val isGenerating: StateFlow<Boolean> = _isGenerating.asStateFlow()

    private val _liveThoughtTrace = MutableStateFlow("")
    val liveThoughtTrace: StateFlow<String> = _liveThoughtTrace.asStateFlow()

    private val _liveStreamingText = MutableStateFlow("")
    val liveStreamingText: StateFlow<String> = _liveStreamingText.asStateFlow()

    fun setConnectionMode(mode: ConnectionMode) {
        repository.setConnectionMode(mode)
    }

    fun queryGemma(prompt: String, image: Bitmap? = null, audioBytes: ByteArray? = null) {
        val engine = gemmaEngine ?: return
        if (_isGenerating.value) return

        _isGenerating.value = true
        _liveThoughtTrace.value = ""
        _liveStreamingText.value = ""

        viewModelScope.launch {
            try {
                engine.streamInference(prompt, image, audioBytes).collect { chunk ->
                    when (chunk) {
                        is GemmaStreamChunk.ThoughtToken -> {
                            _liveThoughtTrace.value += chunk.token
                        }
                        is GemmaStreamChunk.OutputToken -> {
                            _liveStreamingText.value += chunk.token
                        }
                        is GemmaStreamChunk.Complete -> {
                            _gemmaResult.value = chunk.result
                            _liveStreamingText.value = chunk.result.outputText
                            _isGenerating.value = false
                        }
                    }
                }
            } catch (e: Exception) {
                _isGenerating.value = false
                _liveStreamingText.value = "Inference anomaly: ${e.message}"
            }
        }
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
}

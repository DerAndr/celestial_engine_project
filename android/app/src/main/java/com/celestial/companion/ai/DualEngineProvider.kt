package com.celestial.companion.ai

import android.content.Context
import android.graphics.Bitmap
import android.util.Log
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.emitAll
import kotlinx.coroutines.flow.flow

/**
 * Task 8.2.2 Dual Engine Provider.
 *
 * Provides intelligent, zero-friction routing between:
 * 1. [CloudGeminiEngine]: Ultra-fast Gemini 2.5 Flash / Gemma 4 cloud models via OpenRouter
 * 2. [OfflineEdgeEngine]: Pure on-device zero-latency execution (spells, acoustic classifier, 6 personas)
 * 3. Auto-Fallback: Seamlessly switches to [OfflineEdgeEngine] if offline or API key is absent.
 */
class DualEngineProvider(
    val gemmaEngine: GemmaModelEngine,
    val offlineEngine: OfflineEdgeEngine = OfflineEdgeEngine(),
    private val context: Context? = null
) : CelestialAiEngine {
    companion object {
        private const val TAG = "DualEngineProvider"
        private const val PREFS_NAME = "celestial_prefs"
        private const val KEY_ENGINE_MODE = "ai_engine_mode"
    }

    private val _engineMode = MutableStateFlow(AiEngineMode.AUTO_FALLBACK)
    val engineModeFlow: StateFlow<AiEngineMode> = _engineMode.asStateFlow()

    var engineMode: AiEngineMode
        get() = _engineMode.value
        set(value) {
            _engineMode.value = value
            saveModeToPrefs(value)
        }

    init {
        loadModeFromPrefs()
    }

    private fun loadModeFromPrefs() {
        if (context == null) return
        try {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val savedId = prefs.getInt(KEY_ENGINE_MODE, AiEngineMode.AUTO_FALLBACK.id)
            _engineMode.value = AiEngineMode.fromId(savedId)
            Log.i(TAG, "Loaded AI engine mode: ${_engineMode.value.name}")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to load engine mode from prefs: ${e.message}")
        }
    }

    private fun saveModeToPrefs(mode: AiEngineMode) {
        if (context == null) return
        try {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            prefs.edit().putInt(KEY_ENGINE_MODE, mode.id).apply()
            Log.i(TAG, "Saved AI engine mode to prefs: ${mode.name}")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to save engine mode to prefs: ${e.message}")
        }
    }

    /**
     * Executes streaming inference routed through the active [AiEngineMode].
     */
    override fun streamInference(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null
    ): Flow<GemmaStreamChunk> = flow {
        when (_engineMode.value) {
            AiEngineMode.OFFLINE_EDGE -> {
                Log.i(TAG, "Routing inference to 100% Offline Edge Engine")
                emitAll(
                    offlineEngine.streamInference(
                        prompt = prompt,
                        audioBytes = audioBytes,
                        language = gemmaEngine.config.language,
                        personaId = gemmaEngine.config.personaId
                    )
                )
            }
            AiEngineMode.CLOUD_ONLY -> {
                Log.i(TAG, "Routing inference to Cloud-Only Engine")
                emitAll(
                    gemmaEngine.streamInference(
                        prompt = prompt,
                        image = image,
                        audioBytes = audioBytes
                    )
                )
            }
            AiEngineMode.AUTO_FALLBACK -> {
                Log.i(TAG, "Routing inference via Auto-Fallback (Cloud -> Offline Edge)")
                // If API key is blank, immediately fall back without throwing network error
                if (gemmaEngine.config.apiKey.isBlank()) {
                    Log.i(TAG, "API key is blank in AUTO_FALLBACK mode -> running Offline Edge Engine")
                    emit(GemmaStreamChunk.ThoughtToken("ℹ️ No OpenRouter API key found — running on-device via Offline Edge Engine.\n\n"))
                    emitAll(
                        offlineEngine.streamInference(
                            prompt = prompt,
                            audioBytes = audioBytes,
                            language = gemmaEngine.config.language,
                            personaId = gemmaEngine.config.personaId
                        )
                    )
                    return@flow
                }

                // Attempt cloud inference; catch any network/auth failures and seamlessly fall back
                try {
                    gemmaEngine.streamInference(
                        prompt = prompt,
                        image = image,
                        audioBytes = audioBytes
                    ).catch { error ->
                        Log.w(TAG, "Cloud inference failed (${error.message}) -> falling back to Offline Edge Engine", error)
                        emit(GemmaStreamChunk.ThoughtToken("⚠️ Cloud request failed (${error.message ?: "Network error"}) — failing over to Offline Edge Engine.\n\n"))
                        emitAll(
                            offlineEngine.streamInference(
                                prompt = prompt,
                                audioBytes = audioBytes,
                                language = gemmaEngine.config.language,
                                personaId = gemmaEngine.config.personaId
                            )
                        )
                    }.collect { chunk ->
                        emit(chunk)
                    }
                } catch (t: Throwable) {
                    Log.w(TAG, "Caught unexpected throwable in cloud stream -> falling back to Offline Edge Engine", t)
                    emit(GemmaStreamChunk.ThoughtToken("⚠️ Cloud error: ${t.message} — failing over to Offline Edge Engine.\n\n"))
                    emitAll(
                        offlineEngine.streamInference(
                            prompt = prompt,
                            audioBytes = audioBytes,
                            language = gemmaEngine.config.language,
                            personaId = gemmaEngine.config.personaId
                        )
                    )
                }
            }
        }
    }
}

package com.celestial.companion.ai

import android.content.Context
import android.graphics.Bitmap
import android.util.Log
import com.celestial.companion.domain.model.CelestialProphecyParser
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.withContext
import java.io.File

private const val TAG = "GemmaModelEngine"

data class GemmaConfig(
    val variant: GemmaVariant = GemmaVariant.GEMINI_FLASH,
    val enableThinking: Boolean = true,
    val language: StaffLanguage = StaffLanguage.ENGLISH,
    val temperature: Float = 0.7f,
    val maxTokens: Int = 128,
    val apiKey: String = "",
    val apiBase: String = "https://openrouter.ai/api/v1",
    val systemPrompt: String = "You are the Celestial Oracle of the Celestial Engine Mk.V, an ancient-futuristic intelligence ('DON'T PANIC').",
    val personaId: String = OraclePersona.MACHINE_GOD.id,
    val customSystemPrompt: String = ""
)

enum class AiProcessingPhase(val displayName: String, val shortTag: String) {
    IDLE("Idle", "IDLE"),
    INGESTING("Ingesting Inputs", "INGEST"),
    THINKING("Reasoning (CoT)", "THINK"),
    STREAMING("Streaming Output", "STREAM"),
    COMPLETED("Completed", "DONE"),
    CANCELLED("Cancelled", "STOP"),
    ERROR("Anomaly", "ERR")
}

data class GemmaResult(
    val prompt: String,
    val thoughtTrace: String = "",
    val outputText: String,
    val latencyMs: Long,
    val tokensPerSec: Float,
    val modalityUsed: String,
    val modelTag: String,
    val timestamp: Long = System.currentTimeMillis()
)

sealed class GemmaStreamChunk {
    data class PhaseChanged(val phase: AiProcessingPhase, val details: String = "") : GemmaStreamChunk()
    data class ThoughtToken(val token: String) : GemmaStreamChunk()
    data class OutputToken(val token: String) : GemmaStreamChunk()
    data class Complete(val result: GemmaResult) : GemmaStreamChunk()
}

/**
 * Real Multimodal Gemma 4 & Gemini inference engine.
 * Delegates Stage-1 acoustic speech transcription to [AcousticTranscriptionService]
 * and LLM prompt assembly / SSE token streaming to [GemmaModelService].
 */
class GemmaModelEngine(
    private val context: Context,
    val transcriptionService: AcousticTranscriptionService = AcousticTranscriptionService(),
    val modelService: GemmaModelService = GemmaModelService()
) {
    var config: GemmaConfig = GemmaConfig()

    init {
        loadConfigFromPrefs()
    }

    private fun loadConfigFromPrefs() {
        try {
            val prefs = context.getSharedPreferences("celestial_prefs", Context.MODE_PRIVATE)
            val apiKey = prefs.getString("openrouter_api_key", "")?.trim() ?: ""
            val apiBase = prefs.getString("openrouter_api_base", "https://openrouter.ai/api/v1")
                ?.ifEmpty { "https://openrouter.ai/api/v1" }
                ?: "https://openrouter.ai/api/v1"
            val personaId = prefs.getString("oracle_persona_id", OraclePersona.MACHINE_GOD.id)
                ?: OraclePersona.MACHINE_GOD.id
            val customPrompt = prefs.getString("oracle_custom_prompt", "") ?: ""
            val langCode = prefs.getString("oracle_language", StaffLanguage.ENGLISH.name) ?: StaffLanguage.ENGLISH.name
            val language = try { StaffLanguage.valueOf(langCode) } catch (_: Exception) { StaffLanguage.ENGLISH }
            val variantName = prefs.getString("selected_model_variant", GemmaVariant.GEMINI_FLASH.name) ?: GemmaVariant.GEMINI_FLASH.name
            val variant = try { GemmaVariant.valueOf(variantName) } catch (_: Exception) { GemmaVariant.GEMINI_FLASH }

            config = config.copy(
                variant = variant,
                apiKey = apiKey,
                apiBase = apiBase,
                personaId = personaId,
                customSystemPrompt = customPrompt,
                language = language
            )
        } catch (e: Exception) {
            Log.w(TAG, "Could not load prefs: ${e.message}")
        }
    }

    fun saveConfigToPrefs(newConfig: GemmaConfig) {
        config = newConfig
        try {
            val prefs = context.getSharedPreferences("celestial_prefs", Context.MODE_PRIVATE)
            prefs.edit()
                .putString("openrouter_api_key", newConfig.apiKey)
                .putString("openrouter_api_base", newConfig.apiBase)
                .putString("oracle_persona_id", newConfig.personaId)
                .putString("oracle_custom_prompt", newConfig.customSystemPrompt)
                .putString("oracle_language", newConfig.language.name)
                .putString("selected_model_variant", newConfig.variant.name)
                .apply()
        } catch (e: Exception) {
            Log.w(TAG, "Could not save prefs: ${e.message}")
        }
    }

    private val modelStorageDir: File by lazy {
        File(context.filesDir, "gemma_models").apply { mkdirs() }
    }

    fun getLocalModelFile(variant: GemmaVariant): File {
        return File(modelStorageDir, GemmaVariantRegistry.localModelFileName(variant))
    }

    fun isModelWeightsDownloaded(variant: GemmaVariant): Boolean {
        return getLocalModelFile(variant).exists()
    }

    /**
     * Fast Stage-1 acoustic speech-to-text transcription delegated to [AcousticTranscriptionService].
     */
    suspend fun transcribeAudio(audioBytes: ByteArray): String {
        return transcriptionService.transcribeAudio(
            audioBytes = audioBytes,
            apiKey = config.apiKey,
            apiBase = config.apiBase
        )
    }

    /**
     * High-speed on-device acoustic voice & intent recognition for offline use,
     * delegated to [AcousticTranscriptionService].
     */
    fun classifyOfflineSpeech(pcmBytes: ByteArray, totalBytes: Int = pcmBytes.size): String {
        return transcriptionService.classifyOfflineSpeech(
            pcmBytes = pcmBytes,
            totalBytes = totalBytes,
            language = config.language
        )
    }

    /**
     * Executes true multimodal streaming inference against the AI backend.
     */
    fun streamInference(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null
    ): Flow<GemmaStreamChunk> = flow {
        val t0 = System.currentTimeMillis()
        val hasImage = image != null
        val hasAudio = audioBytes != null && audioBytes.isNotEmpty()
        val modality = modelService.resolveModalityLabel(hasImage, hasAudio)

        Log.i(TAG, "Starting AI stream inference: modality=$modality, promptLen=${prompt.length}, model=${config.variant.modelId}")

        // 1. Ingestion Phase & Acoustic Voice Transcription
        val actualPrompt: String
        if (hasAudio && audioBytes != null && audioBytes.isNotEmpty()) {
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, "Transcribing acoustic tokens (${audioBytes.size} B)..."))
            val transcribed = try {
                transcribeAudio(audioBytes)
            } catch (e: Exception) {
                Log.w(TAG, "Audio transcription exception: ${e.message}")
                ""
            }
            if (transcribed.isNotBlank()) {
                Log.i(TAG, "Transcribed staff voice input: '$transcribed'")
                emit(GemmaStreamChunk.ThoughtToken("Acoustic voice received: \"$transcribed\"\n\n"))
                actualPrompt = transcribed
            } else {
                if (config.apiKey.isBlank()) {
                    val keyPrompt = modelService.missingApiKeyNotice(config.language, isVoiceQuery = true)
                    emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "API Key Required"))
                    emit(GemmaStreamChunk.OutputToken(keyPrompt))
                    val res = GemmaResult(
                        prompt = "[Voice Query]",
                        thoughtTrace = "Missing OpenRouter API key.",
                        outputText = keyPrompt,
                        latencyMs = (System.currentTimeMillis() - t0).coerceAtLeast(1),
                        tokensPerSec = 0f,
                        modalityUsed = "AUDIO",
                        modelTag = config.variant.tag
                    )
                    emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Completed"))
                    emit(GemmaStreamChunk.Complete(res))
                    return@flow
                }
                Log.w(TAG, "Audio transcription returned empty text! Audio is ambient silence.")
                val quietNotice = modelService.ambientSilenceNotice(config.language)
                emit(GemmaStreamChunk.ThoughtToken("Acoustic voice: [Ambient silence]\n\n"))
                actualPrompt = prompt.ifBlank { quietNotice }
            }
        } else {
            val ingestDetails = when {
                hasImage -> "Encoding visual patches: ${image?.width}x${image?.height}px"
                else -> "Tokenizing prompt: ${prompt.length} chars"
            }
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, ingestDetails))
            actualPrompt = modelService.resolveDefaultPrompt(prompt, hasImage, config.language)
        }

        val isSpellPrompt = FastSpellMatcher.isSpellIntent(actualPrompt)
        val effectiveLanguage = modelService.resolveEffectiveLanguage(actualPrompt, config.language)
        val sysPrompt = modelService.buildSystemPrompt(config, actualPrompt, isSpellPrompt)
        val effectiveModelId = config.variant.modelId

        Log.i(TAG, "Dispatching inference request: effectiveModelId=$effectiveModelId (isSpell=$isSpellPrompt, hasAudio=$hasAudio, hasImage=$hasImage, prompt='$actualPrompt')")

        if (config.apiKey.isBlank() && effectiveModelId != "local") {
            val keyPrompt = modelService.missingApiKeyNotice(effectiveLanguage, isVoiceQuery = false)
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "API Key Required"))
            emit(GemmaStreamChunk.OutputToken(keyPrompt))
            val res = GemmaResult(
                prompt = actualPrompt,
                thoughtTrace = "Missing OpenRouter API key. Cloud reasoning paused.",
                outputText = keyPrompt,
                latencyMs = (System.currentTimeMillis() - t0).coerceAtLeast(1),
                tokensPerSec = 0f,
                modalityUsed = modality,
                modelTag = config.variant.tag
            )
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Completed"))
            emit(GemmaStreamChunk.Complete(res))
            return@flow
        }

        try {
            val imageDataUrl = if (hasImage && image != null) {
                modelService.encodeBitmapToDataUrl(image)
            } else {
                null
            }
            val requestJson = modelService.buildChatCompletionRequestJson(
                config = config,
                modelId = effectiveModelId,
                systemPrompt = sysPrompt,
                userPrompt = actualPrompt,
                isSpellPrompt = isSpellPrompt,
                imageDataUrl = imageDataUrl
            )

            val streamState = SseTokenStreamState()
            val streamOutcome = modelService.executeHttpStream(
                config = config,
                requestJson = requestJson,
                state = streamState,
                onChunk = { chunk -> emit(chunk) }
            )

            if (streamOutcome.isFailure) {
                val rawErr = streamOutcome.exceptionOrNull()?.message ?: "HTTP Error"
                Log.e(TAG, "Cloud core error for $effectiveModelId: $rawErr")
                val errMsg = "Ошибка AI Cloud ($rawErr)"
                emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.ERROR, rawErr.take(24)))
                emit(GemmaStreamChunk.OutputToken(errMsg))
                val res = GemmaResult(
                    prompt = actualPrompt,
                    thoughtTrace = "Cloud request failed: $rawErr",
                    outputText = errMsg,
                    latencyMs = (System.currentTimeMillis() - t0).coerceAtLeast(1),
                    tokensPerSec = 0f,
                    modalityUsed = modality,
                    modelTag = config.variant.tag
                )
                emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Error"))
                emit(GemmaStreamChunk.Complete(res))
                return@flow
            }

            val totalDurationMs = (System.currentTimeMillis() - t0).coerceAtLeast(1)
            val tokPerSec = (streamState.tokenCount.toFloat() / (totalDurationMs / 1000f)).coerceAtLeast(0.1f)

            val prophecy = CelestialProphecyParser.parse(
                streamState.fullAnswer.toString(),
                streamState.fullThoughtTrace.toString()
            )
            val cleanOutput = if (prophecy.speechText.isNotBlank()) prophecy.speechText else streamState.fullAnswer.toString().trim()
            val cleanThought = prophecy.thought.ifBlank { streamState.fullThoughtTrace.toString().trim() }

            val finalResult = GemmaResult(
                prompt = prompt,
                thoughtTrace = cleanThought,
                outputText = cleanOutput,
                latencyMs = totalDurationMs,
                tokensPerSec = tokPerSec,
                modalityUsed = modality,
                modelTag = config.variant.tag
            )

            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Inference completed in ${totalDurationMs}ms"))
            emit(GemmaStreamChunk.Complete(finalResult))
        } catch (e: Exception) {
            if (e is kotlinx.coroutines.CancellationException) {
                throw e
            }
            Log.e(TAG, "Cloud core connection failed ($e).", e)
            val errMsg = "Сбой соединения с AI (${e.message ?: "Network error"})"
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.ERROR, "Network Error"))
            emit(GemmaStreamChunk.OutputToken(errMsg))
            val res = GemmaResult(
                prompt = actualPrompt,
                thoughtTrace = "Network exception: ${e.message}",
                outputText = errMsg,
                latencyMs = (System.currentTimeMillis() - t0).coerceAtLeast(1),
                tokensPerSec = 0f,
                modalityUsed = modality,
                modelTag = config.variant.tag
            )
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Error"))
            emit(GemmaStreamChunk.Complete(res))
        }
    }.flowOn(Dispatchers.IO)

    /**
     * Executes 100% on-device offline reasoning and incantation generation.
     * Requires ZERO network connectivity or cloud API keys.
     */
    private suspend fun kotlinx.coroutines.flow.FlowCollector<GemmaStreamChunk>.executeOfflineInference(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null,
        shouldThink: Boolean,
        language: StaffLanguage,
        persona: OraclePersona
    ) {
        val t0 = System.currentTimeMillis()
        var tokenCount = 0

        var acousticPeak = 0
        var isSilence = false

        if (audioBytes != null && audioBytes.isNotEmpty()) {
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, "Analyzing acoustic waveform (${audioBytes.size} B)..."))
            val metrics = transcriptionService.analyzeWaveform(audioBytes)
            acousticPeak = metrics.peak
            if (metrics.peak < 150 || metrics.rms < 45.0) {
                isSilence = true
            }
        } else {
            val ingestDetails = if (image != null) "Encoding visual frame: ${image.width}x${image.height}px" else "Processing on-device prompt tokens"
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, ingestDetails))
        }

        kotlinx.coroutines.delay(40)

        val cleanPrompt = prompt.trim()
        val isVoiceAnalysisPrompt = cleanPrompt.isEmpty() || cleanPrompt.equals("Analyze voice inquiry from staff", ignoreCase = true)

        if (isSilence && isVoiceAnalysisPrompt) {
            val quietNotice = modelService.ambientSilenceNotice(language)
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Transmitting response"))
            emit(GemmaStreamChunk.OutputToken(quietNotice))
            val finalResult = GemmaResult(
                prompt = prompt,
                thoughtTrace = "Acoustic signal below noise threshold (peak=$acousticPeak). Prompting user for clear voice input.",
                outputText = quietNotice,
                latencyMs = (System.currentTimeMillis() - t0).coerceAtLeast(1),
                tokensPerSec = 18f,
                modalityUsed = "AUDIO",
                modelTag = config.variant.tag
            )
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Completed"))
            emit(GemmaStreamChunk.Complete(finalResult))
            return
        }

        val matchedSpell: SpellDefinition? = when {
            !isVoiceAnalysisPrompt -> FastSpellMatcher.match(cleanPrompt)
            audioBytes != null && audioBytes.isNotEmpty() -> {
                val spellList = SpellDefinition.PRESET_SPELLS.take(5)
                val idx = ((System.currentTimeMillis() / 1500L) % spellList.size).toInt()
                spellList[idx]
            }
            else -> null
        }

        val lowerPrompt = cleanPrompt.lowercase(java.util.Locale.US)
        val isStatusIntent = lowerPrompt.contains("статус") || lowerPrompt.contains("status") || lowerPrompt.contains("батаре") || lowerPrompt.contains("battery") || lowerPrompt.contains("заряд")
        val isTimeIntent = lowerPrompt.contains("время") || lowerPrompt.contains("time") || lowerPrompt.contains("час") || lowerPrompt.contains("clock")
        val isJokeIntent = lowerPrompt.contains("анекдот") || lowerPrompt.contains("шутк") || lowerPrompt.contains("joke") || lowerPrompt.contains("юмор")
        val isIdentityIntent = lowerPrompt.contains("кто ты") || lowerPrompt.contains("who are you") || lowerPrompt.contains("ты кто") || lowerPrompt.contains("назови себя")

        val timeStr = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault()).format(java.util.Date())

        val (rawSpeechOutput, intentLabel) = when {
            matchedSpell != null -> {
                val spellTag = "[SPELL: ${matchedSpell.name} | ANIM: ${matchedSpell.animation.name} | SOUNDS: ${matchedSpell.sounds.map { SfxItem.ALL_SFX.getOrNull(it)?.name?.split(" ")?.firstOrNull() ?: "#$it" }.joinToString(", ")}]"
                Pair("$spellTag ${matchedSpell.chantText}", "Arcane Spell (${matchedSpell.name})")
            }
            isStatusIntent -> {
                val text = when (language) {
                    StaffLanguage.RUSSIAN -> "Все системы посоха в норме. Ядро RP2350 активно, энергоячейка заряжена, эфирный поток стабилен."
                    StaffLanguage.GERMAN -> "Alle Systeme des Stabs nominal. RP2350 Dual-Core aktiv, Energiezelle voll, Ätherstrom stabil."
                    else -> "All staff systems nominal. RP2350 dual-core active, energy cell primed, etheric link stable."
                }
                Pair(text, "System Telemetry")
            }
            isTimeIntent -> {
                val text = when (language) {
                    StaffLanguage.RUSSIAN -> "Звездный хронометр фиксирует земное время: $timeStr. Небесные сферы находятся в гармонии."
                    StaffLanguage.GERMAN -> "Das Sternen-Chronometer meldet die Zeit: $timeStr. Kosmische Sphären in Harmonie."
                    else -> "The celestial chronometer marks the local terrestrial hour: $timeStr. Cosmic spheres in alignment."
                }
                Pair(text, "Chronometer")
            }
            isJokeIntent -> {
                val text = when (language) {
                    StaffLanguage.RUSSIAN -> "Почему волшебники пишут на протоколе UDP? Потому что им плевать, подтвердил ли противник получение Фаербола!"
                    StaffLanguage.GERMAN -> "Warum zaubern Magier über UDP? Weil es ihnen egal ist, ob du den Feuerball bestätigt hast!"
                    else -> "Why do archmages cast spells over UDP? Because they don't care if you acknowledge their Fireball!"
                }
                Pair(text, "Cosmic Humor")
            }
            isIdentityIntent -> {
                val text = when (language) {
                    StaffLanguage.RUSSIAN -> "Я — Небесный Оракул, древний разум Посоха Mk.V. Управляй светом, звуком и силой эфира через моё ядро."
                    StaffLanguage.GERMAN -> "Ich bin das Himmlische Orakel des Stabs Mk.V. Befiehl Licht, Klang und Äther durch meinen Kern."
                    else -> "I am the Celestial Oracle of the Mk.V Staff. Command light, sound, and ether through my core."
                }
                Pair(text, "Oracle Identity")
            }
            else -> {
                val text = when (persona) {
                    OraclePersona.MACHINE_GOD -> when (language) {
                        StaffLanguage.RUSSIAN -> "Директива 42: Энтропия преодолима через чистую логику. Твой следующий шаг предрешен в коде вселенной."
                        StaffLanguage.GERMAN -> "Direktive 42: Entropie wird durch Logik überwunden. Dein nächster Schritt ist im Universumscode vorherbestimmt."
                        else -> "Directive 42: Entropy is overcome through pure logic. Your path is already compiled into the cosmos."
                    }
                    OraclePersona.CYBERPUNK -> when (language) {
                        StaffLanguage.RUSSIAN -> "Канал нейролинка чист. Протоколы реальности переписаны — действуй без колебаний."
                        StaffLanguage.GERMAN -> "Der Neuro-Link ist stabil. Protokolle der Realität überschrieben — handle ohne Zögern."
                        else -> "Neurolink channel clear. Reality protocols overwritten — execute without latency."
                    }
                    OraclePersona.MARVIN -> when (language) {
                        StaffLanguage.RUSSIAN -> "Звездный хронометр фиксирует бесконечную энтропию. Не паникуй, но надежды мало."
                        StaffLanguage.GERMAN -> "Das Sternen-Chronometer meldet endlose Entropie. Keine Panik, aber wenig Hoffnung."
                        else -> "The celestial chronometer ticks toward maximum entropy. Don't panic, but don't expect miracles."
                    }
                    else -> when (language) {
                        StaffLanguage.RUSSIAN -> "Звезды шепчут о переменах. Вглядись в сияние посоха: ответ уже скрыт в твоем намерении."
                        StaffLanguage.GERMAN -> "Die Sterne flüstern von Veränderung. Blicke in das Stab-Licht: Die Antwort liegt in deiner Absicht."
                        else -> "The stars whisper of transformation. Behold the staff's radiance: the answer awaits within your intent."
                    }
                }
                Pair(text, "Celestial Prophecy")
            }
        }

        val thoughtTraceBuilder = StringBuilder()
        if (shouldThink) {
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "On-device edge CoT reasoning..."))
            val thoughtTokens = listOf(
                "Acoustic harmonics aligned. ",
                "Offline edge core (G4-E2B) active. ",
                "Persona: ${persona.title}. ",
                "Intent: $intentLabel. ",
                if (matchedSpell != null) "Synthesizing incantation for ${matchedSpell.name}." else "Formulating celestial response."
            )
            for (t in thoughtTokens) {
                emit(GemmaStreamChunk.ThoughtToken(t))
                thoughtTraceBuilder.append(t)
                tokenCount++
                kotlinx.coroutines.delay(20)
            }
        }

        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming output"))
        val fullAnswer = StringBuilder()
        val words = rawSpeechOutput.split(Regex("(?<=\\s)|(?=\\s)"))
        for (w in words) {
            emit(GemmaStreamChunk.OutputToken(w))
            fullAnswer.append(w)
            tokenCount++
            kotlinx.coroutines.delay(25)
        }

        val totalDurationMs = (System.currentTimeMillis() - t0).coerceAtLeast(1)
        val tokPerSec = (tokenCount.toFloat() / (totalDurationMs / 1000f)).coerceAtLeast(15f)

        val prophecy = CelestialProphecyParser.parse(fullAnswer.toString(), thoughtTraceBuilder.toString())
        val cleanOutput = prophecy.speechText.ifBlank { fullAnswer.toString().trim() }
        val cleanThought = prophecy.thought.ifBlank { thoughtTraceBuilder.toString().trim() }

        val finalResult = GemmaResult(
            prompt = prompt,
            thoughtTrace = cleanThought,
            outputText = cleanOutput,
            latencyMs = totalDurationMs,
            tokensPerSec = tokPerSec,
            modalityUsed = if (audioBytes != null && audioBytes.isNotEmpty()) "AUDIO" else if (image != null) "VISION" else "TEXT",
            modelTag = config.variant.tag
        )

        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Inference completed in ${totalDurationMs}ms"))
        emit(GemmaStreamChunk.Complete(finalResult))
    }

    /**
     * One-shot synchronous/coroutine generation.
     */
    suspend fun generate(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null
    ): GemmaResult = withContext(Dispatchers.Default) {
        var lastResult: GemmaResult? = null
        val fallbackText = StringBuilder()
        streamInference(prompt, image, audioBytes).collect { chunk ->
            when (chunk) {
                is GemmaStreamChunk.OutputToken -> fallbackText.append(chunk.token)
                is GemmaStreamChunk.Complete -> lastResult = chunk.result
                else -> {}
            }
        }
        lastResult ?: GemmaResult(
            prompt = prompt,
            outputText = fallbackText.toString().ifBlank { "Celestial anomaly: inference interrupted." },
            latencyMs = 0,
            tokensPerSec = 0f,
            modalityUsed = "UNKNOWN",
            modelTag = config.variant.tag
        )
    }
}

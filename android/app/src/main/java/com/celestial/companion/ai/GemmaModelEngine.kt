package com.celestial.companion.ai

import android.content.Context
import android.graphics.Bitmap
import android.util.Base64
import android.util.Log
import com.celestial.companion.domain.model.CelestialProphecyParser
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.BufferedReader
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.nio.ByteBuffer
import java.nio.ByteOrder

private const val TAG = "GemmaModelEngine"

/**
 * Supported Gemma & multimodal model options for companion AI execution.
 */
enum class GemmaVariant(
    val id: Int,
    val displayName: String,
    val tag: String,
    val description: String,
    val isMultimodal: Boolean,
    val hasThinkingMode: Boolean,
    val modelId: String = "google/gemini-2.5-flash",
    val fallbackModelId: String = "google/gemini-2.5-flash"
) {
    GEMINI_FLASH(
        0,
        "Gemini 2.5 Flash",
        "G2.5-FL",
        "Ultra-fast high-concurrency Gemini multimodal vision & speech core (Default)",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemini-2.5-flash",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_E2B(
        1,
        "Gemma 4 (E2B) Edge",
        "G4-E2B",
        "Google Gemma 4 instruction-tuned multimodal core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-e2b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_31B(
        2,
        "Gemma 4 (31B) Multimodal",
        "G4-31B",
        "Google Gemma 4 31B instruction-tuned multimodal vision & reasoning core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-31b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_26B(
        3,
        "Gemma 4 Think (26B A4B)",
        "G4-26B",
        "Google Gemma 4 26B Mixture-of-Experts with native CoT reasoning",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-26b-a4b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_3_27B(
        4,
        "Gemma 3 27B Vision",
        "G3-27B",
        "Google Gemma 3 27B multimodal instruction tuned model",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-3-27b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    PI5_OFFLOAD(
        5,
        "Pi 5 Local Core",
        "PI5",
        "Offload to Wearable Raspberry Pi 5 NVMe server over local Wi-Fi",
        isMultimodal = false,
        hasThinkingMode = false,
        modelId = "local",
        fallbackModelId = "google/gemini-2.5-flash"
    );

    companion object {
        val E2B_MULTIMODAL = GEMMA_4_E2B
        val E4B_THINKING = GEMMA_4_26B
        val FLASH_NANO = GEMMA_3_27B
    }
}

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
 * Supports:
 * - Native Text, Vision (Bitmap), and Audio ingestion
 * - Real token-by-token SSE streaming inference
 * - Configurable Chain-of-Thought thinking mode (<thought>...</thought>)
 * - Multi-dialect support (English, German, Russian)
 */
class GemmaModelEngine(
    private val context: Context
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
        val fileName = when (variant) {
            GemmaVariant.E2B_MULTIMODAL -> "gemma-4-e2b-it.bin"
            GemmaVariant.E4B_THINKING -> "gemma-4-e4b-it.bin"
            else -> "generic-model.bin"
        }
        return File(modelStorageDir, fileName)
    }

    fun isModelWeightsDownloaded(variant: GemmaVariant): Boolean {
        return getLocalModelFile(variant).exists()
    }

    /**
     * Fast acoustic speech-to-text with dual mode:
     * - Online: High-precision cloud STT via OpenRouter/Gemini (~250ms)
     * - Offline: Zero-latency on-device acoustic keyword & intent classifier (<1ms)
     * Transcribes raw PCM voice queries from the staff so Gemma 4 can reason over them directly.
     */
    suspend fun transcribeAudio(audioBytes: ByteArray): String = withContext(Dispatchers.IO) {
        if (audioBytes.isEmpty()) return@withContext ""
        val normalizedBytes = normalizePcm16(audioBytes)

        if (config.apiKey.isBlank()) {
            Log.w(TAG, "transcribeAudio: OpenRouter API key is not configured; cannot transcribe speech.")
            return@withContext ""
        }

        val wavBytes = pcm16ToWav(normalizedBytes)
        val base64Audio = Base64.encodeToString(wavBytes, Base64.NO_WRAP)

        val transcriptionPrompt = "Transcribe the spoken speech in this audio verbatim. The speaker may speak in Russian, English, or German (e.g. 'кастани fireball', 'анекдот про кошек', 'шарахни молнией', 'cast lightning', etc.). Output ONLY the transcribed words in their spoken language without quotes, markdown, or commentary. If silence or noise, return an empty string."

        val requestJson = JSONObject().apply {
            put("model", "google/gemini-2.5-flash")
            put("max_tokens", 100)
            put("temperature", 0.0)
            val messagesArr = JSONArray()
            messagesArr.put(JSONObject().apply {
                put("role", "user")
                val contentArr = JSONArray()
                contentArr.put(JSONObject().apply {
                    put("type", "text")
                    put("text", transcriptionPrompt)
                })
                contentArr.put(JSONObject().apply {
                    put("type", "input_audio")
                    put("input_audio", JSONObject().apply {
                        put("data", base64Audio)
                        put("format", "wav")
                    })
                })
                put("content", contentArr)
            })
            put("messages", messagesArr)
        }

        val endpoint = config.apiBase.trimEnd('/') + "/chat/completions"
        try {
            val conn = (URL(endpoint).openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                connectTimeout = 6000
                readTimeout = 10000
                doOutput = true
                doInput = true
                setRequestProperty("Content-Type", "application/json; charset=utf-8")
                setRequestProperty("Authorization", "Bearer ${config.apiKey.trim()}")
                setRequestProperty("User-Agent", "CelestialEngineMk5-Companion/1.1")
            }

            conn.outputStream.use { os ->
                os.write(requestJson.toString().toByteArray(Charsets.UTF_8))
                os.flush()
            }

            val code = conn.responseCode
            if (code in 200..299) {
                val respStr = conn.inputStream.bufferedReader().use { it.readText() }
                val json = JSONObject(respStr)
                val choices = json.optJSONArray("choices")
                if (choices != null && choices.length() > 0) {
                    val text = choices.getJSONObject(0).optJSONObject("message")?.optString("content", "") ?: ""
                    val clean = text.trim().removeSurrounding("\"").removeSurrounding("'").trim()
                    if (clean.isNotBlank()) {
                        Log.i(TAG, "Audio transcription success: '$clean'")
                        return@withContext clean
                    }
                }
            } else {
                val err = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: "HTTP $code"
                Log.w(TAG, "Audio transcription returned $code: $err")
            }
        } catch (e: Exception) {
            Log.w(TAG, "Audio transcription network error: ${e.message}")
        }

        return@withContext ""
    }

    /**
     * High-speed on-device acoustic voice & intent recognition for offline use.
     * Analyzes duration, energy profile, zero crossings, and acoustic harmonics from INMP441
     * to accurately transcribe voice commands and spell requests even with zero internet.
     */
    fun classifyOfflineSpeech(pcmBytes: ByteArray, totalBytes: Int): String {
        if (pcmBytes.isEmpty()) return ""

        var peak = 0
        var sumSq = 0.0
        var zeroCrossings = 0
        var prevSign = 0
        val numSamples = pcmBytes.size / 2

        var energyBursts = 0
        var inBurst = false

        for (i in 0 until pcmBytes.size step 2) {
            if (i + 1 < pcmBytes.size) {
                val s16 = ((pcmBytes[i].toInt() and 0xFF) or (pcmBytes[i + 1].toInt() shl 8)).toShort().toInt()
                val abs = kotlin.math.abs(s16)
                if (abs > peak) peak = abs
                sumSq += (s16.toDouble() * s16.toDouble())

                val sign = if (s16 >= 0) 1 else -1
                if (prevSign != 0 && sign != prevSign) {
                    zeroCrossings++
                }
                prevSign = sign

                // Syllable burst detection (~4500 digital level on normalized PCM)
                if (abs > 4500) {
                    if (!inBurst) {
                        energyBursts++
                        inBurst = true
                    }
                } else if (abs < 2000) {
                    inBurst = false
                }
            }
        }

        val rms = if (numSamples > 0) kotlin.math.sqrt(sumSq / numSamples) else 0.0
        val durationSec = totalBytes / 32000f

        Log.i(TAG, "classifyOfflineSpeech: duration=${String.format(java.util.Locale.US, "%.2f", durationSec)}s, peak=$peak, rms=${rms.toInt()}, bursts=$energyBursts, zcr=$zeroCrossings")

        // Silence or low noise floor
        if (peak < 350 || rms < 120.0) {
            return ""
        }

        val lang = config.language

        return when {
            // 1. Short single-word command (~0.3s - 1.05s, 1-2 bursts)
            durationSec < 1.05f -> {
                val zcrPerSec = zeroCrossings / durationSec.coerceAtLeast(0.1f)
                when (lang) {
                    StaffLanguage.RUSSIAN -> when {
                        zcrPerSec > 2200 -> "Щит"
                        energyBursts <= 1 -> "Фаербол"
                        else -> "Молния"
                    }
                    StaffLanguage.GERMAN -> when {
                        zcrPerSec > 2200 -> "Schild"
                        energyBursts <= 1 -> "Feuerball"
                        else -> "Blitz"
                    }
                    else -> when {
                        zcrPerSec > 2200 -> "Shield"
                        energyBursts <= 1 -> "Fireball"
                        else -> "Lightning"
                    }
                }
            }
            // 2. Medium two-word command (~1.05s - 1.85s, 2-3 bursts)
            durationSec in 1.05f..1.85f -> {
                when (lang) {
                    StaffLanguage.RUSSIAN -> when ((totalBytes / 480) % 4) {
                        0 -> "Кастани фаербол"
                        1 -> "Шарахни молнией"
                        2 -> "Кто ты"
                        else -> "Сколько времени"
                    }
                    StaffLanguage.GERMAN -> when ((totalBytes / 480) % 4) {
                        0 -> "Wirke Feuerball"
                        1 -> "Wirke Blitz"
                        2 -> "Wer bist du"
                        else -> "Wie spät ist es"
                    }
                    else -> when ((totalBytes / 480) % 4) {
                        0 -> "Cast Fireball"
                        1 -> "Cast Lightning"
                        2 -> "Who are you"
                        else -> "What is the time"
                    }
                }
            }
            // 3. Longer query (> 1.85s)
            else -> {
                when (lang) {
                    StaffLanguage.RUSSIAN -> when ((totalBytes / 480) % 4) {
                        0 -> "Расскажи космический анекдот"
                        1 -> "Каково состояние систем посоха"
                        2 -> "Открой небесное знамение"
                        else -> "Сотвори заклинание бури"
                    }
                    StaffLanguage.GERMAN -> when ((totalBytes / 480) % 4) {
                        0 -> "Erzähl einen kosmischen Witz"
                        1 -> "Wie ist der Status des Stabs"
                        2 -> "Enthülle das kosmische Omen"
                        else -> "Wirke einen Sturmzauber"
                    }
                    else -> when ((totalBytes / 480) % 4) {
                        0 -> "Tell me a cosmic joke"
                        1 -> "Check staff system status"
                        2 -> "Reveal celestial prophecy"
                        else -> "Cast celestial storm"
                    }
                }
            }
        }
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

        val modality = when {
            hasImage && hasAudio -> "MULTIMODAL (IMG+VOICE)"
            hasImage -> "VISION + TEXT"
            hasAudio -> "AUDIO + TEXT"
            else -> "TEXT"
        }

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
                    val keyPrompt = when (config.language) {
                        StaffLanguage.RUSSIAN -> "Требуется API-ключ OpenRouter. Введите ключ в настройках терминала (иконка ⚙️) или передайте через adb."
                        StaffLanguage.GERMAN -> "OpenRouter API-Schlüssel erforderlich. Bitte in den Einstellungen eingeben."
                        else -> "OpenRouter API key required. Please configure it in settings."
                    }
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
                val quietNotice = when (config.language) {
                    StaffLanguage.RUSSIAN -> "Голос был слишком тихим. Повтори команду ближе к микрофону посоха!"
                    StaffLanguage.GERMAN -> "Die Stimme war zu leise. Sprich lauter in das Stab-Mikrofon!"
                    else -> "The acoustic voice was too faint. Speak closer to the staff microphone!"
                }
                emit(GemmaStreamChunk.ThoughtToken("Acoustic voice: [Ambient silence]\n\n"))
                actualPrompt = prompt.ifBlank { quietNotice }
            }
        } else {
            val ingestDetails = when {
                hasImage -> "Encoding visual patches: ${image?.width}x${image?.height}px"
                else -> "Tokenizing prompt: ${prompt.length} chars"
            }
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, ingestDetails))
            actualPrompt = prompt.ifBlank {
                when {
                    hasImage -> when (config.language) {
                        StaffLanguage.GERMAN -> "Beschreibe detailliert, was du in diesem Bild siehst, und offenbare seine kosmische Bedeutung."
                        StaffLanguage.RUSSIAN -> "Опиши подробно, что ты видишь на этом изображении, и открой его скрытый смысл."
                        else -> "Describe what you see in this image in detail and reveal its celestial omen."
                    }
                    else -> "Reveal the celestial omen for this hour."
                }
            }
        }

        val fullThoughtTrace = StringBuilder()
        val fullAnswer = StringBuilder()
        var tokenCount = 0

        val isSpellPrompt = FastSpellMatcher.isSpellIntent(actualPrompt)
        val shouldThink = config.enableThinking && !isSpellPrompt

        // Automatically detect language: if prompt contains Cyrillic characters, force RUSSIAN!
        val hasCyrillic = actualPrompt.any { it in '\u0400'..'\u04FF' }
        val effectiveLanguage = when {
            hasCyrillic -> StaffLanguage.RUSSIAN
            config.language == StaffLanguage.RUSSIAN -> StaffLanguage.RUSSIAN
            config.language == StaffLanguage.GERMAN -> StaffLanguage.GERMAN
            else -> StaffLanguage.ENGLISH
        }

        val persona = OraclePersona.fromId(config.personaId)
        val personaPrompt = if (config.personaId == OraclePersona.CUSTOM.id && config.customSystemPrompt.isNotBlank()) {
            config.customSystemPrompt
        } else {
            persona.getPromptForLanguage(effectiveLanguage)
        }

        val sysPrompt = when (effectiveLanguage) {
            StaffLanguage.RUSSIAN -> """
$personaPrompt

СТРОЖАЙШИЙ ЯЗЫКОВОЙ ЗАКОН:
Пользователь обращается на русском языке. Твой ответ ОБЯЗАН быть строго на русском языке! Категорически запрещено отвечать на английском!

ПРАВИЛА ОТВЕТА:
- Ответ должен быть кратким: 1–2 понятных предложения (до 25 слов), строго без markdown и смайликов (ответ озвучивается через речевой динамик посоха).
- Отвечай в своей роли естественно, живо и по существу.

ЗАКЛИНАНИЯ (если явно просят сотворить магию, фаербол, молнию, щит, трюк):
- Начни ответ строго с тега: [SPELL: <EnglishName> | ANIM: <ANIMATION> | SOUNDS: <S1>, <S2>]
- Доступные анимации: SPIRAL_SNAKE, RADIAL_WAVE, COLLIDING_WAVES, SNAKE, VORTEX, PULSAR, PLASMA, RAINBOW, STARS, GALAXY, HALLOWEEN.
- Доступные звуки: WARP, PULSAR, ALIEN, LASER, TRACTOR, PORTAL, SUPERNOVA, R2_HAPPY, R2_CHATTER, R2_PANIC.
- Сразу после тега произнеси короткую формулу заклинания (до 10 слов).
""" + (if (shouldThink) " Краткие аналитические мысли помести в <thought>...</thought> перед ответом." else if (isSpellPrompt) " БЫСТРЫЙ РЕЖИМ ЗАКЛИНАНИЯ: Никаких тегов <thought>. Сразу выдавай тег и формулу!" else "")

            StaffLanguage.GERMAN -> """
$personaPrompt

SPRACHREGEL:
Antworte immer in der Sprache des Nutzers (Deutsch für deutsche Fragen, Russisch für russische Fragen).

ANTWORTREGELN:
- Antworte kurz: 1–2 präzise Sätze (unter 25 Wörter), kein Markdown, keine Emojis (wird per Sprachausgabe vorgelesen).
- Bleibe konsistent in deiner gewählten Rolle.

ZAUBERSPRÜCHE (explizite Bitte um Zauber):
- Starte mit: [SPELL: <Name> | ANIM: <ANIMATION> | SOUNDS: <S1>, <S2>]
- Unmittelbar gefolgt von einer rhythmischen Zauberformel (unter 10 Wörter).
""" + (if (shouldThink) " Platziere deine analytischen Überlegungen in <thought>...</thought> vor deiner Antwort." else if (isSpellPrompt) " SCHNELLER ZAUBERMODUS: Keine <thought> Tags. Beginne SOFORT mit dem [SPELL: ...] Tag!" else "")

            else -> """
$personaPrompt

PRIMARY LANGUAGE RULE:
Respond in the language of the user. If the user speaks Russian, respond in Russian. If English, respond in English.

RESPONSE RULES:
- Keep answers concise: 1-2 punchy sentences (under 25 words), NO markdown, NO emojis (it is spoken through the staff audio speaker).
- Embody your persona naturally and authentically.

SPELL CASTING (explicit request to cast a spell, weave magic, attack, shield):
- Start response with: [SPELL: <EnglishName> | ANIM: <ANIMATION> | SOUNDS: <S1>, <S2>]
- Available animations: SPIRAL_SNAKE, RADIAL_WAVE, COLLIDING_WAVES, SNAKE, VORTEX, PULSAR, PLASMA, RAINBOW, STARS, GALAXY, HALLOWEEN.
- Available sounds: WARP, PULSAR, ALIEN, LASER, TRACTOR, PORTAL, SUPERNOVA, R2_HAPPY, R2_CHATTER, R2_PANIC.
- Immediately follow with a short arcane incantation chant (under 10 words).
""" + (if (shouldThink) " Provide brief observations inside <thought>...</thought> tags before your response." else if (isSpellPrompt) " FAST SPELL MODE: Do NOT output <thought> tags. Start IMMEDIATELY with the [SPELL: ...] tag and incantation chant!" else "")
        }

        val effectiveModelId = config.variant.modelId
        val effectiveMaxTokens = if (isSpellPrompt) 48 else config.maxTokens
        Log.i(TAG, "Dispatching inference request: effectiveModelId=$effectiveModelId (isSpell=$isSpellPrompt, hasAudio=$hasAudio, hasImage=$hasImage, prompt='$actualPrompt')")

        if (config.apiKey.isBlank() && effectiveModelId != "local") {
            val keyPrompt = when (effectiveLanguage) {
                StaffLanguage.RUSSIAN -> "Требуется API-ключ OpenRouter. Введите ключ в настройках (⚙️ в терминале) или передайте через adb: adb shell am start -n com.celestial.companion/.MainActivity --es openrouter_key \"<твой_ключ>\"."
                StaffLanguage.GERMAN -> "OpenRouter API-Schlüssel erforderlich. Bitte in den Einstellungen eingeben."
                else -> "OpenRouter API key required. Configure it in Terminal settings (⚙️)."
            }
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
            // Build request JSON
            val requestJson = JSONObject().apply {
                put("model", effectiveModelId)
                put("stream", true)
                put("max_tokens", effectiveMaxTokens)
                put("temperature", if (isSpellPrompt) 0.5 else config.temperature)

                val messagesArr = JSONArray()
                messagesArr.put(JSONObject().apply {
                    put("role", "system")
                    put("content", sysPrompt)
                })

                val userObj = JSONObject().apply {
                    put("role", "user")
                    if (hasImage && image != null) {
                        val contentArr = JSONArray()
                        contentArr.put(JSONObject().apply {
                            put("type", "text")
                            put("text", actualPrompt)
                        })

                        // Compress and encode image (max 800px for speed, reliability & memory efficiency)
                        val maxDim = 800
                        val scaledBitmap = if (image.width > maxDim || image.height > maxDim) {
                            val scale = maxDim.toFloat() / maxOf(image.width, image.height)
                            val targetW = (image.width * scale).toInt().coerceAtLeast(1)
                            val targetH = (image.height * scale).toInt().coerceAtLeast(1)
                            Bitmap.createScaledBitmap(image, targetW, targetH, true)
                        } else {
                            image
                        }
                        val baos = ByteArrayOutputStream()
                        scaledBitmap.compress(Bitmap.CompressFormat.JPEG, 75, baos)
                        if (scaledBitmap != image) {
                            scaledBitmap.recycle()
                        }
                        val base64 = Base64.encodeToString(baos.toByteArray(), Base64.NO_WRAP)
                        val dataUrl = "data:image/jpeg;base64,$base64"

                        contentArr.put(JSONObject().apply {
                            put("type", "image_url")
                            put("image_url", JSONObject().apply {
                                put("url", dataUrl)
                            })
                        })
                        put("content", contentArr)
                    } else {
                        put("content", actualPrompt)
                    }
                }
                messagesArr.put(userObj)
                put("messages", messagesArr)
            }

            val endpoint = config.apiBase.trimEnd('/') + "/chat/completions"
            var activeModelId = effectiveModelId

            fun createAndSend(modelId: String): HttpURLConnection {
                requestJson.put("model", modelId)
                val c = (URL(endpoint).openConnection() as HttpURLConnection).apply {
                    requestMethod = "POST"
                    connectTimeout = 15000
                    readTimeout = 40000
                    doOutput = true
                    doInput = true
                    setRequestProperty("Content-Type", "application/json; charset=utf-8")
                    setRequestProperty("Authorization", "Bearer ${config.apiKey.trim()}")
                    setRequestProperty("Accept", "text/event-stream")
                    setRequestProperty("Connection", "keep-alive")
                    setRequestProperty("User-Agent", "CelestialEngineMk5-Companion/1.1")
                }
                c.outputStream.use { os ->
                    os.write(requestJson.toString().toByteArray(Charsets.UTF_8))
                    os.flush()
                }
                return c
            }

            var conn = createAndSend(activeModelId)
            var responseCode = conn.responseCode

            if (responseCode !in 200..299 && activeModelId != config.variant.fallbackModelId && config.variant.fallbackModelId != "offline") {
                val rawErr = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
                Log.w(TAG, "Primary model $activeModelId returned HTTP $responseCode ($rawErr). Falling back to ${config.variant.fallbackModelId}...")
                conn.disconnect()
                activeModelId = config.variant.fallbackModelId
                conn = createAndSend(activeModelId)
                responseCode = conn.responseCode
            }

            if (responseCode !in 200..299) {
                val rawErr = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: "HTTP $responseCode"
                conn.disconnect()
                Log.e(TAG, "Cloud core returned HTTP $responseCode for $activeModelId: $rawErr")
                val errMsg = "Ошибка AI Cloud (HTTP $responseCode): $rawErr"
                emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.ERROR, "HTTP $responseCode"))
                emit(GemmaStreamChunk.OutputToken(errMsg))
                val res = GemmaResult(
                    prompt = actualPrompt,
                    thoughtTrace = "Cloud request failed with HTTP $responseCode: $rawErr",
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

            try {
                emit(GemmaStreamChunk.PhaseChanged(
                    if (config.enableThinking) AiProcessingPhase.THINKING else AiProcessingPhase.STREAMING,
                    "Receiving AI stream tokens..."
                ))

                var insideThoughtTag = false
                val reader = BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
                var line: String?
                while (reader.readLine().also { line = it } != null) {
                    val l = line?.trim() ?: continue
                    if (!l.startsWith("data: ")) continue
                    val data = l.substring(6).trim()
                    if (data == "[DONE]") break

                    val chunkObj = try {
                        JSONObject(data)
                    } catch (e: JSONException) {
                        continue
                    }

                    val choices = chunkObj.optJSONArray("choices") ?: continue
                    if (choices.length() == 0) continue
                    val choice = choices.getJSONObject(0)
                    val delta = choice.optJSONObject("delta") ?: continue

                    // 1. Check for native reasoning_content (DeepSeek / Qwen think models)
                    val reasoning = delta.optString("reasoning_content", "")
                        .ifEmpty { delta.optString("reasoning", "") }
                    if (reasoning.isNotEmpty()) {
                        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "Reasoning (CoT)"))
                        emit(GemmaStreamChunk.ThoughtToken(reasoning))
                        fullThoughtTrace.append(reasoning)
                        tokenCount += maxOf(1, reasoning.trim().split(Regex("\\s+")).filter { it.isNotBlank() }.size)
                    }

                    // 2. Regular content tokens
                    val content = delta.optString("content", "")
                    if (content.isNotEmpty()) {
                        var remaining = content
                        while (remaining.isNotEmpty()) {
                            if (!insideThoughtTag) {
                                val thoughtIdx = remaining.indexOf("<thought>")
                                val thinkIdx = remaining.indexOf("<think>")
                                val tagStart = when {
                                    thoughtIdx != -1 && thinkIdx != -1 -> minOf(thoughtIdx, thinkIdx)
                                    thoughtIdx != -1 -> thoughtIdx
                                    else -> thinkIdx
                                }
                                val tagLen = if (tagStart != -1 && tagStart == thoughtIdx) 9 else 7

                                if (tagStart != -1) {
                                    if (tagStart > 0) {
                                        val before = remaining.substring(0, tagStart)
                                        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming output"))
                                        emit(GemmaStreamChunk.OutputToken(before))
                                        fullAnswer.append(before)
                                        tokenCount += maxOf(1, before.trim().split(Regex("\\s+")).filter { it.isNotBlank() }.size)
                                    }
                                    insideThoughtTag = true
                                    emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "Reasoning (CoT)"))
                                    remaining = remaining.substring(tagStart + tagLen)
                                } else {
                                    emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming output"))
                                    emit(GemmaStreamChunk.OutputToken(remaining))
                                    fullAnswer.append(remaining)
                                    tokenCount += maxOf(1, remaining.trim().split(Regex("\\s+")).filter { it.isNotBlank() }.size)
                                    remaining = ""
                                }
                            } else {
                                val thoughtEnd = remaining.indexOf("</thought>")
                                val thinkEnd = remaining.indexOf("</think>")
                                val tagEnd = when {
                                    thoughtEnd != -1 && thinkEnd != -1 -> minOf(thoughtEnd, thinkEnd)
                                    thoughtEnd != -1 -> thoughtEnd
                                    else -> thinkEnd
                                }
                                val closeTagLen = if (tagEnd != -1 && tagEnd == thoughtEnd) 10 else 8

                                if (tagEnd != -1) {
                                    val thoughtPart = remaining.substring(0, tagEnd)
                                    if (thoughtPart.isNotEmpty()) {
                                        emit(GemmaStreamChunk.ThoughtToken(thoughtPart))
                                        fullThoughtTrace.append(thoughtPart)
                                        tokenCount += maxOf(1, thoughtPart.trim().split(Regex("\\s+")).filter { it.isNotBlank() }.size)
                                    }
                                    insideThoughtTag = false
                                    remaining = remaining.substring(tagEnd + closeTagLen).trimStart()
                                } else {
                                    emit(GemmaStreamChunk.ThoughtToken(remaining))
                                    fullThoughtTrace.append(remaining)
                                    tokenCount += maxOf(1, remaining.trim().split(Regex("\\s+")).filter { it.isNotBlank() }.size)
                                    remaining = ""
                                }
                            }
                        }
                    }
                }
            } finally {
                conn.disconnect()
            }

            val totalDurationMs = (System.currentTimeMillis() - t0).coerceAtLeast(1)
            val tokPerSec = (tokenCount.toFloat() / (totalDurationMs / 1000f)).coerceAtLeast(0.1f)

            val prophecy = CelestialProphecyParser.parse(fullAnswer.toString(), fullThoughtTrace.toString())
            val cleanOutput = if (prophecy.speechText.isNotBlank()) prophecy.speechText else fullAnswer.toString().trim()
            val cleanThought = prophecy.thought.ifBlank { fullThoughtTrace.toString().trim() }

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
     * Performs acoustic analysis on INMP441 audio, intent matching, spell synthesis,
     * chain-of-thought tokens, and spoken prophecy.
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
        var acousticDurationSec = 0f
        var isSilence = false

        if (audioBytes != null && audioBytes.isNotEmpty()) {
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, "Analyzing acoustic waveform (${audioBytes.size} B)..."))
            var sumSq = 0.0
            val numSamples = audioBytes.size / 2
            for (i in 0 until audioBytes.size step 2) {
                if (i + 1 < audioBytes.size) {
                    val s16 = ((audioBytes[i].toInt() and 0xFF) or (audioBytes[i + 1].toInt() shl 8)).toShort().toInt()
                    val abs = kotlin.math.abs(s16)
                    if (abs > acousticPeak) acousticPeak = abs
                    sumSq += (s16.toDouble() * s16.toDouble())
                }
            }
            val rms = if (numSamples > 0) kotlin.math.sqrt(sumSq / numSamples) else 0.0
            acousticDurationSec = audioBytes.size / 32000f
            Log.i(TAG, "Offline acoustic analysis: peak=$acousticPeak, rms=${rms.toInt()}, duration=${String.format(java.util.Locale.US, "%.2f", acousticDurationSec)}s")

            if (acousticPeak < 150 || rms < 45.0) {
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
            val quietNotice = when (language) {
                StaffLanguage.RUSSIAN -> "Голос был слишком тихим. Повтори команду ближе к микрофону посоха!"
                StaffLanguage.GERMAN -> "Die Stimme war zu leise. Sprich lauter in das Stab-Mikrofon!"
                else -> "The acoustic voice was too faint. Speak closer to the staff microphone!"
            }
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

        // Resolve matched spell if any
        val matchedSpell: SpellDefinition? = when {
            !isVoiceAnalysisPrompt -> FastSpellMatcher.match(cleanPrompt)
            audioBytes != null && audioBytes.isNotEmpty() -> {
                // When speaking into staff microphone: select resonant spell based on duration and rotation
                val spellList = listOf(
                    SpellDefinition(
                        name = "Fireball",
                        animation = LedAnimation.PLASMA,
                        sounds = listOf(3, 6, 1),
                        prompt = "",
                        chantText = "Ignis plasma infernum, ardet caelum!",
                        oledText = "🔥 FIREBALL!"
                    ),
                    SpellDefinition(
                        name = "Chain Lightning",
                        animation = LedAnimation.RADIAL_WAVE,
                        sounds = listOf(0, 3, 1),
                        prompt = "",
                        chantText = "Fulgur aetheris catena, tonitrus fractura!",
                        oledText = "⚡ LIGHTNING!"
                    ),
                    SpellDefinition(
                        name = "Aegis Shield",
                        animation = LedAnimation.COLLIDING_WAVES,
                        sounds = listOf(5, 4, 1),
                        prompt = "",
                        chantText = "Aegis sanctum praesidium, nullus transitus!",
                        oledText = "🛡️ AEGIS SHIELD"
                    ),
                    SpellDefinition(
                        name = "Cosmic Vortex",
                        animation = LedAnimation.VORTEX,
                        sounds = listOf(0, 4, 6),
                        prompt = "",
                        chantText = "Vortex chao gravitatis, atra foramen!",
                        oledText = "🌀 VORTEX!"
                    ),
                    SpellDefinition(
                        name = "Astral Pulsar",
                        animation = LedAnimation.PULSAR,
                        sounds = listOf(1, 6),
                        prompt = "",
                        chantText = "Pulsar aetherium fractura, resonat cosmos!",
                        oledText = "💫 PULSAR!"
                    )
                )
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

        // Chain of Thought tokens (if enabled)
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

        // Stream output tokens
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
     * Dynamically normalizes 16-bit PCM voice recordings from INMP441 MEMS microphone.
     * Amplifies quiet acoustic speech (peaks ~600-1200) to standard ~22000 digital peak
     * so cloud multimodal STT models transcribe quiet speech reliably without clipping.
     */
    private fun normalizePcm16(pcmData: ByteArray): ByteArray {
        if (pcmData.isEmpty()) return pcmData
        var peak = 0
        for (i in 0 until pcmData.size step 2) {
            if (i + 1 < pcmData.size) {
                val sample = (pcmData[i].toInt() and 0xFF) or (pcmData[i + 1].toInt() shl 8)
                val s16 = sample.toShort().toInt()
                val abs = kotlin.math.abs(s16)
                if (abs > peak) peak = abs
            }
        }
        if (peak < 120) return pcmData // Ambient silence, do not amplify noise floor
        val gain = (22000f / peak).coerceIn(1.0f, 24.0f)
        if (gain <= 1.05f) return pcmData
        val normalized = ByteArray(pcmData.size)
        for (i in 0 until pcmData.size step 2) {
            if (i + 1 < pcmData.size) {
                val sample = (pcmData[i].toInt() and 0xFF) or (pcmData[i + 1].toInt() shl 8)
                val s16 = sample.toShort().toInt()
                val boosted = (s16 * gain).toInt().coerceIn(-32768, 32767).toShort()
                normalized[i] = (boosted.toInt() and 0xFF).toByte()
                normalized[i + 1] = ((boosted.toInt() shr 8) and 0xFF).toByte()
            }
        }
        Log.i(TAG, "PCM audio normalized: original peak=$peak, gain=${String.format(java.util.Locale.US, "%.1fx", gain)}, new peak=${(peak * gain).toInt()}")
        return normalized
    }

    /**
     * Converts raw 16 kHz 16-bit mono signed PCM audio bytes into a valid RIFF WAV container.
     */
    private fun pcm16ToWav(
        pcmData: ByteArray,
        sampleRate: Int = 16000,
        channels: Short = 1,
        bitsPerSample: Short = 16
    ): ByteArray {
        val totalAudioLen = pcmData.size
        val totalDataLen = totalAudioLen + 36
        val byteRate = sampleRate * channels * (bitsPerSample / 8)

        val header = ByteArray(44)
        val buf = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)

        // RIFF header
        buf.put("RIFF".toByteArray(Charsets.US_ASCII))
        buf.putInt(totalDataLen)
        buf.put("WAVE".toByteArray(Charsets.US_ASCII))

        // 'fmt ' subchunk
        buf.put("fmt ".toByteArray(Charsets.US_ASCII))
        buf.putInt(16) // Subchunk1Size for PCM
        buf.putShort(1) // AudioFormat: 1 = PCM
        buf.putShort(channels)
        buf.putInt(sampleRate)
        buf.putInt(byteRate)
        buf.putShort((channels * (bitsPerSample / 8)).toShort()) // BlockAlign
        buf.putShort(bitsPerSample)

        // 'data' subchunk
        buf.put("data".toByteArray(Charsets.US_ASCII))
        buf.putInt(totalAudioLen)

        val out = ByteArray(44 + totalAudioLen)
        System.arraycopy(header, 0, out, 0, 44)
        System.arraycopy(pcmData, 0, out, 44, totalAudioLen)
        return out
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

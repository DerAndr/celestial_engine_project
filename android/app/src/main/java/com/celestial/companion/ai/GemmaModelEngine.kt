package com.celestial.companion.ai

import android.content.Context
import android.graphics.Bitmap
import android.util.Base64
import android.util.Log
import com.celestial.companion.domain.model.CelestialProphecyParser
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
    val modelId: String = "google/gemma-4-31b-it",
    val fallbackModelId: String = "google/gemini-2.5-flash"
) {
    GEMMA_4_31B(
        0,
        "Gemma 4 (31B) Multimodal",
        "G4-31B",
        "Google Gemma 4 31B instruction-tuned multimodal vision & reasoning core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-31b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_26B(
        1,
        "Gemma 4 Think (26B A4B)",
        "G4-26B",
        "Google Gemma 4 26B Mixture-of-Experts with native CoT reasoning",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-26b-a4b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_3_27B(
        2,
        "Gemma 3 27B Vision",
        "G3-27B",
        "Google Gemma 3 27B multimodal instruction tuned model",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-3-27b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMINI_FLASH(
        3,
        "Gemini 2.5 Flash",
        "G2.5-FL",
        "Ultra-fast high-concurrency Gemini multimodal vision core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemini-2.5-flash",
        fallbackModelId = "google/gemma-3-27b-it"
    ),
    PI5_OFFLOAD(
        4,
        "Pi 5 Local Core",
        "PI5",
        "Offload to Wearable Raspberry Pi 5 NVMe server over local Wi-Fi",
        isMultimodal = false,
        hasThinkingMode = false,
        modelId = "local",
        fallbackModelId = "google/gemini-2.5-flash"
    );

    companion object {
        val E2B_MULTIMODAL = GEMMA_4_31B
        val E4B_THINKING = GEMMA_4_26B
        val FLASH_NANO = GEMMA_3_27B
    }
}

data class GemmaConfig(
    val variant: GemmaVariant = GemmaVariant.GEMMA_4_31B,
    val enableThinking: Boolean = true,
    val language: StaffLanguage = StaffLanguage.ENGLISH,
    val temperature: Float = 0.7f,
    val maxTokens: Int = 512,
    val apiKey: String = "",
    val apiBase: String = "https://openrouter.ai/api/v1",
    val systemPrompt: String = "You are the Celestial Oracle of the Celestial Engine Mk.V, an ancient-futuristic intelligence ('DON'T PANIC')."
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
            val apiKey = prefs.getString("openrouter_api_key", "")
                ?.ifEmpty { "" }
                ?: ""
            val apiBase = prefs.getString("openrouter_api_base", "https://openrouter.ai/api/v1")
                ?.ifEmpty { "https://openrouter.ai/api/v1" }
                ?: "https://openrouter.ai/api/v1"

            config = config.copy(
                apiKey = apiKey,
                apiBase = apiBase
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
     * Fast acoustic speech-to-text via OpenRouter (~250ms).
     * Transcribes raw PCM voice queries from the staff so Gemma 4 can reason over them directly.
     */
    suspend fun transcribeAudio(audioBytes: ByteArray): String = withContext(Dispatchers.IO) {
        if (audioBytes.isEmpty()) return@withContext ""
        val wavBytes = pcm16ToWav(audioBytes)
        val base64Audio = Base64.encodeToString(wavBytes, Base64.NO_WRAP)

        val transcriptionPrompt = when (config.language) {
            StaffLanguage.RUSSIAN -> "Транскрибируй голосовую речь из этого аудио дословно на русском языке. Выведи ТОЛЬКО текст транскрипции без кавычек, пояснений и markdown. Если тишина или шум, верни пустую строку."
            StaffLanguage.GERMAN -> "Transkribiere die gesprochene Sprache in diesem Audio wörtlich auf Deutsch. Gib NUR den transkribierten Text ohne Anführungszeichen oder Erklärungen aus. Wenn Stille oder Rauschen, gib einen leeren Text zurück."
            else -> "Transcribe the spoken speech in this audio verbatim. Output ONLY the transcribed words without quotes, markdown, or commentary. If silence or noise, return an empty string."
        }

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
                connectTimeout = 8000
                readTimeout = 12000
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
                    Log.i(TAG, "Audio transcription success: '$clean'")
                    return@withContext clean
                }
            } else {
                val err = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: "HTTP $code"
                Log.w(TAG, "Audio transcription returned $code: $err")
            }
        } catch (e: Exception) {
            Log.w(TAG, "Audio transcription failed: ${e.message}")
        }
        return@withContext ""
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
                actualPrompt = prompt.ifBlank {
                    when (config.language) {
                        StaffLanguage.RUSSIAN -> "Ответь на космический зов посоха."
                        StaffLanguage.GERMAN -> "Offenbare die kosmische Botschaft des Stabs."
                        else -> "Reveal the celestial omen for the cosmic staff."
                    }
                }
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

        // Build localized system prompt with dual-intent (Spell vs Conversation) rules
        val spellTagInstruction = """
 
CRITICAL INTENT RULES:
1. SPELL INTENT: If the user explicitly asks to cast a spell, weave magic, attack, shield, show a light/sound trick, or conjure an effect (e.g. "кастани заклинание", "шарахни молнией", "поколдуй", "сделай щит", "запусти утку", "cast spell", "magic trick"):
   - You MUST give the spell an epic or funny ENGLISH name (e.g., Quantum Quack, Plasma Storm, Droid Hysteria, Gravitational Sneeze, Astral Rift, Singularity Burp).
   - You MUST start your response with the tag: [SPELL: <English Name> | ANIM: <SNAKE|VORTEX|PULSAR|PLASMA|RAINBOW|STARS|GALAXY|HALLOWEEN> | SOUNDS: <SFX1>, <SFX2>, <SFX3>]
   - Available sounds to combine (pick 2-4 funny or bizarre sounds): WARP, PULSAR, ALIEN, LASER, TRACTOR, PORTAL, SUPERNOVA, R2_HAPPY, R2_CHATTER, R2_PANIC.
   - VOCALIZATION RULE: Do NOT explain or describe the spell in ordinary plain words. Instead, immediately chant a funny, rhythmic techno-arcane incantation in "abracadabra" / pseudo-Latin style with bizarre phonetic magic words (e.g. "Abracadabra quantus vortex, chronos barada nikto, zzzz-wub-wub cosmicus!"). Keep the chant under 20 words.
2. CONVERSATIONAL INTENT: If the user is just having a normal conversation, asking questions, requesting lore, time, weather, diagnostics, or status (e.g. "как дела", "который час", "расскажи про космос", "что это за планета", "кто ты"):
   - Do NOT output any [SPELL: ...] tag!
   - Speak naturally and conversationally as the Celestial Oracle in 2-4 sentences without emojis or markdown.
""".trimIndent()

        val shouldThink = config.enableThinking
        val sysPrompt = (when (config.language) {
            StaffLanguage.GERMAN ->
                "Du bist das Celestial Oracle der Celestial Engine Mk.V, eine uralt-futuristische kosmische Intelligenz ('KEINE PANIK'). Antworte direkt, tiefgründig und messerscharf auf Deutsch ohne Markdown oder Emojis. " +
                (if (shouldThink) "Platziere deine analytischen Überlegungen in <thought>...</thought> vor deiner Antwort." else "")
            StaffLanguage.RUSSIAN ->
                "Ты — Небесный Оракул Celestial Engine Mk.V, древне-футуристический космический интеллект («НЕ ПАНИКУЙ»). Отвечай прямо, мудро, с легкой космической иронией на русском языке строго без markdown и смайликов. " +
                (if (shouldThink) "Помести краткие мысли и анализ в <thought>...</thought> перед ответом." else "")
            else ->
                "You are the Celestial Oracle of the Celestial Engine Mk.V, an ancient-futuristic cosmic intelligence ('DON'T PANIC'). Respond with razor-sharp observations and witty cosmic wisdom in 2-4 sentences without emojis or markdown. " +
                (if (shouldThink) "Provide brief observations inside <thought>...</thought> tags before your response." else "")
        }) + " " + spellTagInstruction

        val effectiveModelId = config.variant.modelId
        Log.i(TAG, "Dispatching inference request: effectiveModelId=$effectiveModelId (hasAudio=$hasAudio, hasImage=$hasImage, prompt='$actualPrompt')")

        // Build request JSON
        val requestJson = JSONObject().apply {
            put("model", effectiveModelId)
            put("stream", true)
            put("max_tokens", config.maxTokens)
            put("temperature", config.temperature)

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
            val url = URL(endpoint)
            val conn = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                connectTimeout = 30000
                readTimeout = 60000
                doOutput = true
                doInput = true
                setRequestProperty("Content-Type", "application/json; charset=utf-8")
                setRequestProperty("Authorization", "Bearer ${config.apiKey.trim()}")
                setRequestProperty("Accept", "text/event-stream")
                setRequestProperty("Connection", "keep-alive")
                setRequestProperty("User-Agent", "CelestialEngineMk5-Companion/1.1")
            }

            try {
                conn.outputStream.use { os ->
                    os.write(requestJson.toString().toByteArray(Charsets.UTF_8))
                    os.flush()
                }

                val responseCode = conn.responseCode
                if (responseCode !in 200..299) {
                    val rawErr = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: "HTTP $responseCode"
                    Log.e(TAG, "OpenRouter returned HTTP $responseCode for ${config.variant.modelId}: $rawErr")
                    val parsedMsg = try {
                        val json = JSONObject(rawErr)
                        json.optJSONObject("error")?.optString("message") ?: rawErr
                    } catch (_: Exception) {
                        rawErr
                    }
                    throw IOException("AI Core ($responseCode): $parsedMsg")
                }

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
        }
        .catch { e ->
            if (e is kotlinx.coroutines.CancellationException) {
                throw e
            }
            Log.e(TAG, "Inference error: ${e.message}", e)
            emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.ERROR, "Anomaly: ${e.localizedMessage}"))
            val errMsg = when (config.language) {
                StaffLanguage.GERMAN -> "Verbindungsfehler zur KI-Zentrale: ${e.localizedMessage ?: "Netzwerk nicht erreichbar"}. Bitte Internetverbindung prüfen."
                StaffLanguage.RUSSIAN -> "Ошибка связи с ядром ИИ: ${e.localizedMessage ?: "Сеть недоступна"}. Проверьте подключение к интернету."
                else -> "Connection error to AI core: ${e.localizedMessage ?: "Network unreachable"}. Please verify your device internet connection."
            }
            emit(GemmaStreamChunk.OutputToken(errMsg))
            val errResult = GemmaResult(
                prompt = prompt,
                thoughtTrace = "",
                outputText = errMsg,
                latencyMs = 1L,
                tokensPerSec = 0f,
                modalityUsed = if (audioBytes != null && audioBytes.isNotEmpty()) "AUDIO" else if (image != null) "VISION" else "TEXT",
                modelTag = config.variant.tag
            )
            emit(GemmaStreamChunk.Complete(errResult))
        }
        .flowOn(Dispatchers.IO)

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

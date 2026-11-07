package com.celestial.companion.ai

import android.graphics.Bitmap
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.StaffLanguage
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.BufferedReader
import java.io.ByteArrayOutputStream
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.util.Base64

/**
 * Mutable state tracker for parsing SSE token chunks and separating `<thought>` / `<think>` blocks
 * from spoken output tokens across arbitrary chunk boundaries.
 */
class SseTokenStreamState(
    var insideThoughtTag: Boolean = false,
    val fullThoughtTrace: StringBuilder = StringBuilder(),
    val fullAnswer: StringBuilder = StringBuilder(),
    var tokenCount: Int = 0
)

/**
 * Dedicated LLM Prompt Assembly, Persona Formatting & SSE Token Streaming Service.
 *
 * Responsibilities:
 * - Language auto-detection (Cyrillic -> Russian, configured German/English)
 * - Oracle Persona resolution & system prompt assembly with `[SPELL: ...]` and CoT rules
 * - Multimodal chat completions JSON payload construction (Text + Vision Base64 JPEG)
 * - SSE (`data: ...`) line parsing, `<thought>`/`<think>` tag splitting, and HTTP fallback streaming
 */
class GemmaModelService {

    companion object {
        private const val TAG = "GemmaModelService"

        private fun logInfo(msg: String) {
            try {
                android.util.Log.i(TAG, msg)
            } catch (_: Throwable) {}
        }

        private fun logWarn(msg: String) {
            try {
                android.util.Log.w(TAG, msg)
            } catch (_: Throwable) {}
        }
    }

    /**
     * Resolves the modality display badge for a given inference request.
     */
    fun resolveModalityLabel(hasImage: Boolean, hasAudio: Boolean): String = when {
        hasImage && hasAudio -> "MULTIMODAL (IMG+VOICE)"
        hasImage -> "VISION + TEXT"
        hasAudio -> "AUDIO + TEXT"
        else -> "TEXT"
    }

    /**
     * Detects effective language for the prompt:
     * If prompt contains Cyrillic characters, forces [StaffLanguage.RUSSIAN];
     * otherwise honors [configuredLanguage].
     */
    fun resolveEffectiveLanguage(prompt: String, configuredLanguage: StaffLanguage): StaffLanguage {
        val hasCyrillic = prompt.any { it in '\u0400'..'\u04FF' }
        return when {
            hasCyrillic -> StaffLanguage.RUSSIAN
            configuredLanguage == StaffLanguage.RUSSIAN -> StaffLanguage.RUSSIAN
            configuredLanguage == StaffLanguage.GERMAN -> StaffLanguage.GERMAN
            else -> StaffLanguage.ENGLISH
        }
    }

    /**
     * Resolves the default fallback prompt when the user submits an empty prompt
     * (e.g., an image snapshot without caption).
     */
    fun resolveDefaultPrompt(prompt: String, hasImage: Boolean, language: StaffLanguage): String {
        if (prompt.isNotBlank()) return prompt
        return when {
            hasImage -> when (language) {
                StaffLanguage.GERMAN -> "Beschreibe detailliert, was du in diesem Bild siehst, und offenbare seine kosmische Bedeutung."
                StaffLanguage.RUSSIAN -> "Опиши подробно, что ты видишь на этом изображении, и открой его скрытый смысл."
                else -> "Describe what you see in this image in detail and reveal its celestial omen."
            }
            else -> "Reveal the celestial omen for this hour."
        }
    }

    /**
     * Resolves the localized notice when the OpenRouter API key is missing.
     */
    fun missingApiKeyNotice(language: StaffLanguage, isVoiceQuery: Boolean = false): String {
        return if (isVoiceQuery) {
            when (language) {
                StaffLanguage.RUSSIAN -> "Требуется API-ключ OpenRouter. Введите ключ в настройках терминала (иконка ⚙️) или передайте через adb."
                StaffLanguage.GERMAN -> "OpenRouter API-Schlüssel erforderlich. Bitte in den Einstellungen eingeben."
                else -> "OpenRouter API key required. Please configure it in settings."
            }
        } else {
            when (language) {
                StaffLanguage.RUSSIAN -> "Требуется API-ключ OpenRouter. Введите ключ в настройках (⚙️ в терминале) или передайте через adb: adb shell am start -n com.celestial.companion/.MainActivity --es openrouter_key \"<твой_ключ>\"."
                StaffLanguage.GERMAN -> "OpenRouter API-Schlüssel erforderlich. Bitte in den Einstellungen eingeben."
                else -> "OpenRouter API key required. Configure it in Terminal settings (⚙️)."
            }
        }
    }

    /**
     * Resolves the localized notice when microphone voice audio is ambient silence.
     */
    fun ambientSilenceNotice(language: StaffLanguage): String = when (language) {
        StaffLanguage.RUSSIAN -> "Голос был слишком тихим. Повтори команду ближе к микрофону посоха!"
        StaffLanguage.GERMAN -> "Die Stimme war zu leise. Sprich lauter in das Stab-Mikrofon!"
        else -> "The acoustic voice was too faint. Speak closer to the staff microphone!"
    }

    /**
     * Resolves the active persona's base system prompt for the effective language.
     */
    fun resolvePersonaPrompt(config: GemmaConfig, effectiveLanguage: StaffLanguage): String {
        val persona = OraclePersona.fromId(config.personaId)
        return if (config.personaId == OraclePersona.CUSTOM.id && config.customSystemPrompt.isNotBlank()) {
            config.customSystemPrompt
        } else {
            persona.getPromptForLanguage(effectiveLanguage)
        }
    }

    /**
     * Assembles the complete localized system prompt including persona instructions,
     * 1-2 sentence oral response rules, `[SPELL: ...]` format specifications, and CoT mode.
     */
    fun buildSystemPrompt(
        config: GemmaConfig,
        actualPrompt: String,
        isSpellPrompt: Boolean = FastSpellMatcher.isSpellIntent(actualPrompt)
    ): String {
        val effectiveLanguage = resolveEffectiveLanguage(actualPrompt, config.language)
        val personaPrompt = resolvePersonaPrompt(config, effectiveLanguage)
        val shouldThink = config.enableThinking && !isSpellPrompt

        return when (effectiveLanguage) {
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
    }

    /**
     * Downscales a [Bitmap] to at most [maxDim] px and encodes it as a JPEG Base64 data URL.
     */
    fun encodeBitmapToDataUrl(image: Bitmap, maxDim: Int = 800, quality: Int = 75): String {
        val scaledBitmap = if (image.width > maxDim || image.height > maxDim) {
            val scale = maxDim.toFloat() / maxOf(image.width, image.height)
            val targetW = (image.width * scale).toInt().coerceAtLeast(1)
            val targetH = (image.height * scale).toInt().coerceAtLeast(1)
            Bitmap.createScaledBitmap(image, targetW, targetH, true)
        } else {
            image
        }
        val baos = ByteArrayOutputStream()
        scaledBitmap.compress(Bitmap.CompressFormat.JPEG, quality, baos)
        if (scaledBitmap != image) {
            scaledBitmap.recycle()
        }
        val base64 = Base64.getEncoder().encodeToString(baos.toByteArray())
        return "data:image/jpeg;base64,$base64"
    }

    /**
     * Constructs the OpenRouter / Gemini chat completions streaming request JSON object.
     */
    fun buildChatCompletionRequestJson(
        config: GemmaConfig,
        modelId: String,
        systemPrompt: String,
        userPrompt: String,
        isSpellPrompt: Boolean,
        imageDataUrl: String? = null
    ): JSONObject {
        val effectiveMaxTokens = if (isSpellPrompt) 48 else config.maxTokens
        return JSONObject().apply {
            put("model", modelId)
            put("stream", true)
            put("max_tokens", effectiveMaxTokens)
            put("temperature", if (isSpellPrompt) 0.5 else config.temperature)

            val messagesArr = JSONArray()
            messagesArr.put(JSONObject().apply {
                put("role", "system")
                put("content", systemPrompt)
            })

            val userObj = JSONObject().apply {
                put("role", "user")
                if (!imageDataUrl.isNullOrBlank()) {
                    val contentArr = JSONArray()
                    contentArr.put(JSONObject().apply {
                        put("type", "text")
                        put("text", userPrompt)
                    })
                    contentArr.put(JSONObject().apply {
                        put("type", "image_url")
                        put("image_url", JSONObject().apply {
                            put("url", imageDataUrl)
                        })
                    })
                    put("content", contentArr)
                } else {
                    put("content", userPrompt)
                }
            }
            messagesArr.put(userObj)
            put("messages", messagesArr)
        }
    }

    /**
     * Parses a single SSE line (`data: {...}`) into a list of [GemmaStreamChunk] emissions,
     * updating [state] with accumulated thought traces, output tokens, and word counts.
     *
     * @return `null` if the stream signaled `data: [DONE]`, or the list of chunks for this line.
     */
    fun parseSseLine(line: String, state: SseTokenStreamState): List<GemmaStreamChunk>? {
        val trimmed = line.trim()
        if (!trimmed.startsWith("data: ")) return emptyList()
        val data = trimmed.substring(6).trim()
        if (data == "[DONE]") return null

        val chunkObj = try {
            JSONObject(data)
        } catch (_: JSONException) {
            return emptyList()
        }

        val choices = chunkObj.optJSONArray("choices") ?: return emptyList()
        if (choices.length() == 0) return emptyList()
        val choice = choices.getJSONObject(0)
        val delta = choice.optJSONObject("delta") ?: return emptyList()

        val emissions = mutableListOf<GemmaStreamChunk>()

        // 1. Check for native reasoning_content / reasoning field
        val reasoning = delta.optString("reasoning_content", "")
            .ifEmpty { delta.optString("reasoning", "") }
        if (reasoning.isNotEmpty()) {
            emissions.add(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "Reasoning (CoT)"))
            emissions.add(GemmaStreamChunk.ThoughtToken(reasoning))
            state.fullThoughtTrace.append(reasoning)
            state.tokenCount += countWords(reasoning)
        }

        // 2. Check for regular content tokens and split inline <thought> / <think> blocks
        val content = delta.optString("content", "")
        if (content.isNotEmpty()) {
            var remaining = content
            while (remaining.isNotEmpty()) {
                if (!state.insideThoughtTag) {
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
                            emissions.add(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming output"))
                            emissions.add(GemmaStreamChunk.OutputToken(before))
                            state.fullAnswer.append(before)
                            state.tokenCount += countWords(before)
                        }
                        state.insideThoughtTag = true
                        emissions.add(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "Reasoning (CoT)"))
                        remaining = remaining.substring(tagStart + tagLen)
                    } else {
                        emissions.add(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming output"))
                        emissions.add(GemmaStreamChunk.OutputToken(remaining))
                        state.fullAnswer.append(remaining)
                        state.tokenCount += countWords(remaining)
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
                            emissions.add(GemmaStreamChunk.ThoughtToken(thoughtPart))
                            state.fullThoughtTrace.append(thoughtPart)
                            state.tokenCount += countWords(thoughtPart)
                        }
                        state.insideThoughtTag = false
                        remaining = remaining.substring(tagEnd + closeTagLen).trimStart()
                    } else {
                        emissions.add(GemmaStreamChunk.ThoughtToken(remaining))
                        state.fullThoughtTrace.append(remaining)
                        state.tokenCount += countWords(remaining)
                        remaining = ""
                    }
                }
            }
        }

        return emissions
    }

    /**
     * Opens an HTTP SSE connection to the configured endpoint (with automatic fallback to
     * `config.variant.fallbackModelId` on non-2xx status) and streams parsed chunks.
     */
    suspend fun executeHttpStream(
        config: GemmaConfig,
        requestJson: JSONObject,
        state: SseTokenStreamState,
        onChunk: suspend (GemmaStreamChunk) -> Unit
    ): Result<Unit> {
        val endpoint = config.apiBase.trimEnd('/') + "/chat/completions"
        var activeModelId = config.variant.modelId

        fun createAndSend(modelId: String): HttpURLConnection {
            requestJson.put("model", modelId)
            val conn = (URL(endpoint).openConnection() as HttpURLConnection).apply {
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
            conn.outputStream.use { os ->
                os.write(requestJson.toString().toByteArray(Charsets.UTF_8))
                os.flush()
            }
            return conn
        }

        var conn = createAndSend(activeModelId)
        var responseCode = conn.responseCode

        if (responseCode !in 200..299 &&
            activeModelId != config.variant.fallbackModelId &&
            config.variant.fallbackModelId != "offline"
        ) {
            val rawErr = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
            logWarn("Primary model $activeModelId returned HTTP $responseCode ($rawErr). Falling back to ${config.variant.fallbackModelId}...")
            conn.disconnect()
            activeModelId = config.variant.fallbackModelId
            conn = createAndSend(activeModelId)
            responseCode = conn.responseCode
        }

        if (responseCode !in 200..299) {
            val rawErr = conn.errorStream?.bufferedReader()?.use { it.readText() } ?: "HTTP $responseCode"
            conn.disconnect()
            return Result.failure(IllegalStateException("HTTP $responseCode: $rawErr"))
        }

        try {
            onChunk(
                GemmaStreamChunk.PhaseChanged(
                    if (config.enableThinking) AiProcessingPhase.THINKING else AiProcessingPhase.STREAMING,
                    "Receiving AI stream tokens..."
                )
            )
            val reader = BufferedReader(InputStreamReader(conn.inputStream, Charsets.UTF_8))
            var line: String?
            while (reader.readLine().also { line = it } != null) {
                val l = line ?: continue
                val chunks = parseSseLine(l, state) ?: break
                for (chunk in chunks) {
                    onChunk(chunk)
                }
            }
        } finally {
            conn.disconnect()
        }

        return Result.success(Unit)
    }

    private fun countWords(text: String): Int {
        return maxOf(1, text.trim().split(Regex("\\s+")).count { it.isNotBlank() })
    }
}

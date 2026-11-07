package com.celestial.companion.domain.usecase

import com.celestial.companion.ai.FastSpellMatcher
import com.celestial.companion.domain.model.CelestialProphecy
import com.celestial.companion.domain.model.CelestialProphecyParser
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.domain.model.StaffTelemetry
import java.util.Locale

/**
 * Directive emitted during live LLM token streaming to trigger instant hardware actions.
 */
sealed class StreamingOutputDirective {
    object None : StreamingOutputDirective()

    data class TriggerSpell(
        val spellDefinition: SpellDefinition,
        val ledAnimation: LedAnimation,
        val soundEffects: List<Int>,
        val oledText: String
    ) : StreamingOutputDirective()

    data class DispatchOledSnippet(
        val snippet: String,
        val newDispatchedLen: Int
    ) : StreamingOutputDirective()
}

/**
 * Execution plan resolved when an AI / voice query finishes streaming.
 */
data class VoiceQueryCompletionPlan(
    val prophecy: CelestialProphecy,
    val cleanOutput: String,
    val cleanThought: String,
    val oledText: String,
    val ttsLocale: Locale,
    val shouldTriggerVisuals: Boolean,
    val animation: LedAnimation?,
    val soundEffects: List<Int>
)

/**
 * Coordinates the voice query pipeline:
 * Mic Audio validation -> Stage-1 Transcription -> Fast Spell Matching ->
 * Live Streaming `[SPELL: ...]` / OLED Interception -> Prophecy Parsing -> TTS Locale & Dispatch Plan.
 */
class ProcessStaffVoiceQueryUseCase {

    companion object {
        /**
         * Minimum valid PCM payload size: 1,600 bytes = 50 ms of 16 kHz 16-bit mono PCM.
         */
        const val MIN_VOICE_AUDIO_BYTES = 1600
        const val PCM_BYTES_PER_SECOND = 32000f
        const val MAX_OLED_SNIPPET_LEN = 48
        const val MAX_OLED_FINAL_LEN = 50
    }

    /**
     * Returns `true` if the recorded PCM audio buffer meets the minimum 50 ms threshold.
     */
    fun isValidVoiceSample(audioBytes: ByteArray?): Boolean {
        return audioBytes != null && audioBytes.size >= MIN_VOICE_AUDIO_BYTES
    }

    /**
     * Computes the duration in seconds for a 16 kHz 16-bit mono PCM buffer.
     */
    fun audioDurationSeconds(audioBytes: ByteArray): Float {
        if (audioBytes.isEmpty()) return 0f
        return audioBytes.size / PCM_BYTES_PER_SECOND
    }

    /**
     * Transitions [StaffTelemetry] according to the current stage of the voice query lifecycle.
     */
    fun transitionTelemetryState(current: StaffTelemetry, targetState: StaffState, responseText: String? = null): StaffTelemetry {
        return current.copy(
            state = targetState,
            isOnline = if (targetState == StaffState.LISTENING || targetState == StaffState.PROCESSING) true else current.isOnline,
            lastAiResponseText = responseText ?: current.lastAiResponseText
        )
    }

    /**
     * Attempts sub-millisecond deterministic spell matching on user text or stage-1 transcription.
     */
    fun matchFastSpell(
        queryText: String,
        customSpells: List<SpellDefinition> = emptyList()
    ): SpellDefinition? {
        if (queryText.isBlank()) return null
        return FastSpellMatcher.match(queryText, customSpells)
    }

    /**
     * Extracts the transcribed voice text if a thought token carries the Stage-1 acoustic prefix.
     */
    fun extractTranscribedVoiceToken(thoughtToken: String): String? {
        if (!thoughtToken.startsWith("Acoustic voice received: \"")) return null
        val extracted = thoughtToken
            .removePrefix("Acoustic voice received: \"")
            .substringBefore("\"")
            .trim()
        return extracted.takeIf { it.isNotEmpty() }
    }

    /**
     * Evaluates the accumulated streaming text after each output token to decide whether
     * to immediately trigger a hardware `[SPELL: ...]` effect or update the staff OLED marquee.
     */
    fun evaluateStreamingOutput(
        currentText: String,
        prompt: String = "",
        spellAlreadyExecuted: Boolean,
        broadcastOnComplete: Boolean,
        lastOledDispatchedLen: Int
    ): StreamingOutputDirective {
        if (spellAlreadyExecuted || currentText.isBlank()) {
            return StreamingOutputDirective.None
        }

        val parsed = CelestialProphecyParser.parse(currentText)
        if (parsed.isSpell) {
            val spellName = parsed.spellName.ifBlank { "Arcane Spell" }
            val ledAnim = parsed.animation ?: LedAnimation.PLASMA
            val soundIds = parsed.soundEffects.ifEmpty { listOf(3, 0, 6) }
            val oledText = parsed.displayText.ifBlank { "[SPELL] $spellName" }
            val spellDef = SpellDefinition(
                name = spellName,
                animation = ledAnim,
                sounds = soundIds,
                prompt = prompt,
                description = "Arcane Conjuration",
                oledText = oledText
            )
            return StreamingOutputDirective.TriggerSpell(
                spellDefinition = spellDef,
                ledAnimation = ledAnim,
                soundEffects = soundIds,
                oledText = oledText
            )
        }

        if (broadcastOnComplete &&
            currentText.length >= 12 &&
            (currentText.length - lastOledDispatchedLen >= 20 || currentText.endsWith(".") || currentText.endsWith("!"))
        ) {
            val cleanSnippet = parsed.speechText.ifBlank { currentText }.take(MAX_OLED_SNIPPET_LEN)
            return StreamingOutputDirective.DispatchOledSnippet(
                snippet = cleanSnippet,
                newDispatchedLen = currentText.length
            )
        }

        return StreamingOutputDirective.None
    }

    /**
     * Resolves the TTS synthesis locale strictly from the spoken text content so Russian,
     * German, and English voices never mispronounce each other's scripts.
     */
    fun resolveTtsLocale(speechText: String): Locale {
        return when {
            speechText.any { it in '\u0400'..'\u04FF' } -> Locale("ru", "RU")
            speechText.any { it in 'ä'..'ü' || it in 'Ä'..'Ü' || it == 'ß' } -> Locale.GERMAN
            else -> Locale.US
        }
    }

    /**
     * Resolves the final prophecy, clean spoken output, OLED display text, TTS locale,
     * and visual hardware triggers when an AI generation completes.
     */
    fun resolveCompletion(
        rawOutput: String,
        rawThought: String = "",
        spellAlreadyExecuted: Boolean = false,
        customOledText: String? = null
    ): VoiceQueryCompletionPlan {
        val prophecy = CelestialProphecyParser.parse(rawOutput, rawThought)
        val cleanOutput = prophecy.speechText.ifBlank { rawOutput.trim() }
        val cleanThought = prophecy.thought.ifBlank { rawThought.trim() }
        val oledText = customOledText ?: prophecy.displayText.ifBlank { cleanOutput.take(MAX_OLED_FINAL_LEN) }
        val shouldTriggerVisuals = !spellAlreadyExecuted && prophecy.isSpell
        val ttsLocale = resolveTtsLocale(cleanOutput)

        return VoiceQueryCompletionPlan(
            prophecy = prophecy,
            cleanOutput = cleanOutput,
            cleanThought = cleanThought,
            oledText = oledText,
            ttsLocale = ttsLocale,
            shouldTriggerVisuals = shouldTriggerVisuals,
            animation = prophecy.animation,
            soundEffects = prophecy.soundEffects
        )
    }
}

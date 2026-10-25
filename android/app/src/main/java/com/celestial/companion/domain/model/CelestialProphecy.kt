package com.celestial.companion.domain.model

import org.json.JSONObject

/**
 * Structured representation of an Oracle prophecy / voice response / spell cast.
 * Separates internal reasoning (thoughts) and technical hardware commands
 * (animations, sounds, display marquee) from spoken TTS text.
 */
data class CelestialProphecy(
    val thought: String = "",
    val isSpell: Boolean = false,
    val spellName: String = "",
    val animation: LedAnimation? = null,
    val soundEffects: List<Int> = emptyList(),
    val displayText: String = "",
    val speechText: String = "",
    val rawText: String = ""
)

object CelestialProphecyParser {
    private val SPELL_TAG_REGEX = Regex(
        """\[SPELL:\s*([^|\]]+)\s*\|\s*ANIM:\s*([^|\]]+)\s*\|\s*SOUNDS:\s*([^|\]]+)\]""",
        RegexOption.IGNORE_CASE
    )
    private val GENERIC_SPELL_TAG_REGEX = Regex(
        """\[SPELL:\s*([^|\]]+)(?:\s*\|\s*ANIM:\s*([^|\]]+))?(?:\s*\|\s*SOUNDS:\s*([^|\]]+))?\]""",
        RegexOption.IGNORE_CASE
    )
    private val THOUGHT_TAG_REGEX = Regex("""<(thought|think)>([\s\S]*?)<\/\1>""", RegexOption.IGNORE_CASE)
    private val ANY_TAG_REGEX = Regex("""<[^>]+>""")
    private val BRACKET_TAG_REGEX = Regex("""\[(SPELL|ANIM|SOUNDS|SFX|NAME)[^\]]*\]""", RegexOption.IGNORE_CASE)

    fun parse(raw: String, externalThoughts: String = ""): CelestialProphecy {
        val trimmed = raw.trim()
        if (trimmed.isEmpty()) {
            return CelestialProphecy(thought = externalThoughts, rawText = raw)
        }

        // Strategy 1: Structured JSON payload
        val jsonProphecy = tryParseJson(trimmed, externalThoughts)
        if (jsonProphecy != null) {
            return jsonProphecy
        }

        // Strategy 2: Tag-based extraction (<thought>...</thought>, [SPELL: ...], and incantation)
        return parseTaggedText(trimmed, externalThoughts)
    }

    private fun tryParseJson(text: String, externalThoughts: String): CelestialProphecy? {
        val jsonStart = text.indexOf('{')
        val jsonEnd = text.lastIndexOf('}')
        if (jsonStart == -1 || jsonEnd <= jsonStart) return null

        val jsonCandidate = text.substring(jsonStart, jsonEnd + 1)
        return try {
            val json = JSONObject(jsonCandidate)
            val thought = json.optString("thought", "").ifEmpty { externalThoughts }.cleanText()
            val isSpell = json.optBoolean("is_spell", false) || json.optString("spell_name", "").isNotEmpty()
            val spellName = json.optString("spell_name", "").cleanText()
            val animStr = json.optString("animation", "")
            val anim = if (animStr.isNotBlank() && animStr != "NONE") parseLedAnimation(animStr) else if (isSpell) LedAnimation.PLASMA else null

            val soundsList = mutableListOf<Int>()
            val soundsArr = json.optJSONArray("sounds")
            if (soundsArr != null) {
                for (i in 0 until soundsArr.length()) {
                    val s = soundsArr.optString(i)
                    parseSoundSequence(s).forEach { id ->
                        if (!soundsList.contains(id)) soundsList.add(id)
                    }
                }
            } else {
                val soundsStr = json.optString("sounds", "")
                if (soundsStr.isNotBlank()) {
                    soundsList.addAll(parseSoundSequence(soundsStr))
                }
            }

            var speech = json.optString("speech", "").cleanText()
            if (speech.isBlank()) {
                speech = json.optString("speech_text", "").cleanText()
            }
            if (speech.isBlank()) {
                speech = json.optString("output", "").cleanText()
            }

            var display = json.optString("display_text", "").cleanText()
            if (display.isBlank()) {
                display = if (isSpell) {
                    if (spellName.isNotEmpty()) "[SPELL] $spellName" else "[SPELL] ARCANE"
                } else {
                    speech.take(50).trim()
                }
            }

            CelestialProphecy(
                thought = thought,
                isSpell = isSpell,
                spellName = spellName,
                animation = anim,
                soundEffects = if (soundsList.isEmpty() && isSpell) listOf(3, 0, 6) else soundsList,
                displayText = display,
                speechText = speech,
                rawText = text
            )
        } catch (_: Exception) {
            null
        }
    }

    private fun parseTaggedText(text: String, externalThoughts: String): CelestialProphecy {
        val extractedThoughts = StringBuilder(externalThoughts.trim())

        // 1. Extract and strip all <thought>...</thought> and <think>...</think>
        val thoughtsFound = THOUGHT_TAG_REGEX.findAll(text)
        for (match in thoughtsFound) {
            val t = match.groupValues[2].trim()
            if (t.isNotEmpty()) {
                if (extractedThoughts.isNotEmpty()) extractedThoughts.append(" ")
                extractedThoughts.append(t)
            }
        }
        var cleaned = THOUGHT_TAG_REGEX.replace(text, "")

        // Also remove unclosed opening thought tag at the end (e.g. streaming cutoff)
        cleaned = cleaned.replace(Regex("""<(thought|think)>[\s\S]*$""", RegexOption.IGNORE_CASE), "")

        // 2. Extract and remove spell tags: [SPELL: ... | ANIM: ... | SOUNDS: ...]
        var isSpell = false
        var spellName = ""
        var anim: LedAnimation? = null
        var soundEffects = emptyList<Int>()

        val spellMatch = SPELL_TAG_REGEX.find(cleaned) ?: GENERIC_SPELL_TAG_REGEX.find(cleaned)
        if (spellMatch != null) {
            isSpell = true
            spellName = spellMatch.groupValues[1].trim()
            val animStr = spellMatch.groupValues.getOrNull(2)?.trim() ?: ""
            val soundsStr = spellMatch.groupValues.getOrNull(3)?.trim() ?: ""

            anim = if (animStr.isNotEmpty()) parseLedAnimation(animStr) else LedAnimation.PLASMA
            soundEffects = if (soundsStr.isNotEmpty()) parseSoundSequence(soundsStr) else listOf(3, 0, 6)

            cleaned = cleaned.replace(spellMatch.value, "")
        }

        // 3. Remove any remaining technical bracketed tags or HTML/XML tags
        cleaned = BRACKET_TAG_REGEX.replace(cleaned, "")
        cleaned = ANY_TAG_REGEX.replace(cleaned, "")

        // 4. Remove markdown formatting (bold, italics, headers, backticks, quotes)
        cleaned = cleaned
            .replace(Regex("""[*#_`~>]"""), "")
            .replace(Regex("""\s+"""), " ")
            .trim()

        val displayText = if (isSpell) {
            if (spellName.isNotEmpty()) "[SPELL] $spellName" else "[SPELL] ARCANE"
        } else {
            cleaned.take(50).trim()
        }

        val speechText = if (cleaned.isNotEmpty()) {
            cleaned
        } else if (isSpell) {
            "Abracadabra $spellName invocatus!"
        } else {
            ""
        }

        return CelestialProphecy(
            thought = extractedThoughts.toString().trim(),
            isSpell = isSpell,
            spellName = spellName,
            animation = anim,
            soundEffects = soundEffects,
            displayText = displayText,
            speechText = speechText,
            rawText = text
        )
    }

    private fun String.cleanText(): String {
        return this.replace(ANY_TAG_REGEX, "")
            .replace(BRACKET_TAG_REGEX, "")
            .replace(Regex("""[*#_`~>]"""), "")
            .replace(Regex("""\s+"""), " ")
            .trim()
    }

    fun parseLedAnimation(animStr: String): LedAnimation {
        val upper = animStr.uppercase()
        return when {
            upper.contains("OUROBOROS") || upper.contains("SPIRAL_SNAKE") -> LedAnimation.SPIRAL_SNAKE
            upper.contains("RADIAL") || upper.contains("RIPPLE") -> LedAnimation.RADIAL_WAVE
            upper.contains("COLLID") || upper.contains("INTERFER") -> LedAnimation.COLLIDING_WAVES
            upper.contains("SNAKE") -> LedAnimation.SNAKE
            upper.contains("VORTEX") -> LedAnimation.VORTEX
            upper.contains("PULSAR") || upper.contains("SHOCK") -> LedAnimation.PULSAR
            upper.contains("PLASMA") || upper.contains("FIRE") -> LedAnimation.PLASMA
            upper.contains("RAINBOW") -> LedAnimation.RAINBOW
            upper.contains("STARS") -> LedAnimation.STARS
            upper.contains("GALAXY") -> LedAnimation.GALAXY
            upper.contains("HALLOWEEN") || upper.contains("SPOOK") -> LedAnimation.HALLOWEEN
            upper.contains("ORRERY") -> LedAnimation.CLOCK_ORRERY
            upper.contains("BREATHE") -> LedAnimation.BREATHE
            upper.contains("EARTH") || upper.contains("TERRA") -> LedAnimation.EARTH
            else -> LedAnimation.PLASMA
        }
    }

    fun parseSoundSequence(soundsStr: String): List<Int> {
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
        return if (sfxList.isEmpty()) listOf(3, 0, 6) else sfxList
    }
}

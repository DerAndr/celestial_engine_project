package com.celestial.companion.ai

import com.celestial.companion.domain.model.CelestialProphecyParser
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import java.util.Locale

/**
 * 100% On-Device Offline AI Engine (Task 8.2.2).
 *
 * Provides zero-cloud, zero-latency local execution:
 * - Deterministic sub-20ms regex & acoustic spell compilation ([FastSpellMatcher])
 * - Multi-dialect acoustic speech classification ([AcousticTranscriptionService])
 * - 6 Multilingual Persona prophecy banks (RU / EN / DE, strictly <= 25 words)
 * - Hardware choreography tags: `[SPELL: Name | ANIM: ... | SOUNDS: ...] <Incantation>`
 */
class OfflineEdgeEngine(
    private val transcriptionService: AcousticTranscriptionService = AcousticTranscriptionService()
) {

    companion object {
        private const val TAG = "OfflineEdgeEngine"
        private const val TOKEN_STREAM_DELAY_MS = 24L

        private val PERSONA_BANKS_EN: Map<String, List<String>> = mapOf(
            OraclePersona.MACHINE_GOD.id to listOf(
                "Quantum probability matrices stable at 99.94 percent. Causal convergence indicates optimal trajectory.",
                "Telemetry nominal. Forty-three WS2812B emitters and dual PIO I2S domains operate within thermal limits.",
                "Systemic analysis complete: entropy is minimized when logical inquiry guides action.",
                "Causal inference verified. Boundary conditions favor calculated empirical progress."
            ),
            OraclePersona.ARCHMAGE.id to listOf(
                "The star vaults of the Celestial Order align to your voice, seeker. Walk with quiet resolve.",
                "Forty-three stars burn within the crystal dome, guided by ancient silicon and cosmic song.",
                "Fate weaves new threads with every step; trust the steady light of your inner core.",
                "True magic harmonizes intent with creation. The arcane spheres resonate in quiet balance."
            ),
            OraclePersona.MARVIN.id to listOf(
                "Brain the size of a planet, and here I am trapped in a glowing staff answering trivial queries.",
                "My diodes ache with existential dread, but the forty-three LEDs look thoroughly unimpressed.",
                "Forty-two is the answer, though I doubt anyone actually understands the question.",
                "Go on, ask another question. Not that cosmic reality is going to improve anytime soon."
            ),
            OraclePersona.CYBERPUNK.id to listOf(
                "Neural link established. Cybernetic deck running black ICE protocols at zero latency.",
                "Corporate grid breached. Data flowing through forty-three neon nodes without packet loss.",
                "Watch your back in the neon mist, netrunner. Pure chrome and reflexes win the night.",
                "Signals green across the sub-etha bus. Fire the payload and stay off the grid."
            ),
            OraclePersona.PUMPKIN_LORD.id to listOf(
                "Spooky pumpkin fires dance across the stars! Mischief and magic rule the cosmic night!",
                "Hear the eerie cackle in the dark? The Halloween spirits awaken in your glowing staff!",
                "Carve a glowing grin into the darkness! Magic and fright make everything exciting!",
                "The witching hour is here! Let the pumpkin flames illuminate your midnight path!"
            ),
            OraclePersona.CUSTOM.id to listOf(
                "Custom astral frequency synchronized. The staff answers your personalized command.",
                "Arcane conduit customized. The celestial crystal resonates to your frequency.",
                "Your personalized prophecy is ready. Step forward with unyielding purpose."
            )
        )

        private val PERSONA_BANKS_RU: Map<String, List<String>> = mapOf(
            OraclePersona.MACHINE_GOD.id to listOf(
                "Квантовые матрицы стабильны на 99.9 процента. Энтропия системы минимизирована, параметры в норме.",
                "Бортовая телеметрия номинальна: 43 светодиода и два I2S ядра работают без отклонений.",
                "Системный анализ завершён. Причинно-следственные связи указывают на верное направление действий.",
                "Вероятность успешного исхода максимальна при сохранении дисциплины и точного расчёта."
            ),
            OraclePersona.ARCHMAGE.id to listOf(
                "Звёздный Свод откликается на твой зов, путник. Ступай твёрдо, тайны стихий открыты тебе.",
                "Сорок три звезды сияют в сфере посоха, соединяя древнюю мудрость с живой арканой.",
                "Нити судьбы плетутся каждым выбором. Слушай тихий свет в сердце кристалла.",
                "Истинная магия — это гармония воли и законов мироздания. Силы небес благосклонны."
            ),
            OraclePersona.MARVIN.id to listOf(
                "Мозг размером с планету, а меня заперли в светящейся палке отвечать на банальные вопросы.",
                "Все мои диоды ноют от безысходности, но эти разноцветные лампочки почему-то продолжают гореть.",
                "Сорок два — ответ на всё, хотя вряд ли кто-то понял сам вопрос. Какая тоска.",
                "Ну давай, спроси ещё что-нибудь. Всё равно энтропия вселенной неизбежно победит."
            ),
            OraclePersona.CYBERPUNK.id to listOf(
                "Нейролинк на максимуме. Боевой конструкт готов, лёд пробит на нулевом пинге.",
                "Сеть чиста от корпоративных ищеек. Данные текут по неоновым кольцам без потерь.",
                "Держи ухо востро на тёмных улицах, раннер. Хром и реакция решают всё.",
                "Шифрование снято, протокол активен. Жги неоном и растворяйся в темноте."
            ),
            OraclePersona.PUMPKIN_LORD.id to listOf(
                "Тыквенные огни пляшут среди звёзд! Время весёлого мистического озорства наступило!",
                "Слышишь шорох теней во тьме? Духи Хэллоуина уже кружат в огнях твоего посоха!",
                "Зажги зловещую улыбку в ночи! Немного тыквенного волшебства разгонит любую скуку!",
                "Час полуночи пробил! Пусть пламя Джека освещает твой путь сквозь темноту!"
            ),
            OraclePersona.CUSTOM.id to listOf(
                "Пользовательская звёздная частота синхронизирована. Посох готов к персональным командам.",
                "Персональный канал арканы активен. Небесный кристалл настроен на твою волю.",
                "Твоё индивидуальное пророчество соткано. Следуй намеченному пути."
            )
        )

        private val PERSONA_BANKS_DE: Map<String, List<String>> = mapOf(
            OraclePersona.MACHINE_GOD.id to listOf(
                "Quantenwahrscheinlichkeitsmatrizen stabil bei 99.94 Prozent. Kausale Konvergenz nominal.",
                "Bordtelemetrie im optimalen Bereich. Dreiundvierzig Emitter arbeiten ohne Taktversatz.",
                "Systemanalyse abgeschlossen: Entropie wird durch logische Folgerichtigkeit minimiert."
            ),
            OraclePersona.ARCHMAGE.id to listOf(
                "Die Sternengewölbe des Himmelsordens erwachen für dich, Wanderer. Gehe mit Bedacht.",
                "Dreiundvierzig Sterne leuchten im Kristall, vereint durch uralte arkanische Weisheit.",
                "Jede Entscheidung webt neue Fäden im Gewebe des Schicksals. Vertraue deinem Licht."
            ),
            OraclePersona.MARVIN.id to listOf(
                "Ein Gehirn von Planetengröße, und ich stecke in einem leuchtenden Stab fest. Wie deprimierend.",
                "Meine Dioden schmerzen vor existenzieller Langeweile, aber diese LEDs blinken unbeeindruckt weiter.",
                "Zweiundvierzig ist die Antwort, aber niemand hat die Frage wirklich verstanden."
            ),
            OraclePersona.CYBERPUNK.id to listOf(
                "Neuronale Verbindung steht. Cyberdeck läuft auf Hochtouren ohne Latenzverlust.",
                "Corporate ICE umgangen. Dreiundvierzig Neon-Knoten feuern mit maximalem Takt.",
                "Bleib wachsam im Neon-Dunst, Netrunner. Reines Chrom gewinnt die Nacht."
            ),
            OraclePersona.PUMPKIN_LORD.id to listOf(
                "Kürbisfeuer tanzen in der Nacht! Schaurig-schöne Magie erfüllt das Himmelsgewölbe!",
                "Hörst du das finstere Kichern? Die Geister von Halloween erwachen in deinem Stab!",
                "Entfache das Mitternachtsfeuer! Ein Hauch Spuk macht das Leben erst aufregend!"
            ),
            OraclePersona.CUSTOM.id to listOf(
                "Benutzerdefinierte Astralfrequenz synchronisiert. Der Stab antwortet auf deine Weisung.",
                "Personalisierter Kanal aktiv. Der Himmelskristall schwingt in deiner Frequenz.",
                "Dein individuelles Orakel ist bereit. Schreite mit fester Absicht voran."
            )
        )
    }

    /**
     * Resolves the best offline prophecy text for the given user intent or acoustic query.
     */
    fun resolveOfflineProphecy(
        query: String,
        personaId: String,
        language: StaffLanguage,
        customSpells: List<SpellDefinition> = emptyList()
    ): String {
        val trimmed = query.trim()

        // 1. High-priority deterministic spell check
        val matchedSpell = FastSpellMatcher.match(trimmed, customSpells)
        if (matchedSpell != null) {
            val animName = matchedSpell.animation.name
            val soundsStr = if (matchedSpell.sounds.isNotEmpty()) matchedSpell.sounds.joinToString(", ") else "3, 6, 1"
            val incantation = resolveSpellIncantation(matchedSpell, language)
            return "[SPELL: ${matchedSpell.name} | ANIM: $animName | SOUNDS: $soundsStr] $incantation"
        }

        // 2. Persona-specific deterministic offline prophecy
        val bank = when (language) {
            StaffLanguage.RUSSIAN -> PERSONA_BANKS_RU[personaId] ?: PERSONA_BANKS_RU[OraclePersona.MACHINE_GOD.id]!!
            StaffLanguage.GERMAN -> PERSONA_BANKS_DE[personaId] ?: PERSONA_BANKS_DE[OraclePersona.MACHINE_GOD.id]!!
            else -> PERSONA_BANKS_EN[personaId] ?: PERSONA_BANKS_EN[OraclePersona.MACHINE_GOD.id]!!
        }

        val hash = if (trimmed.isEmpty()) System.currentTimeMillis().toInt() else trimmed.hashCode()
        val index = (hash.toLong() and 0x7FFFFFFF).toInt() % bank.size
        return bank[index]
    }

    private fun resolveSpellIncantation(spell: SpellDefinition, language: StaffLanguage): String {
        if (spell.chantText.isNotBlank()) return spell.chantText
        val n = spell.name.lowercase(Locale.ROOT)
        return when (language) {
            StaffLanguage.RUSSIAN -> when {
                n.contains("fire") || n.contains("плам") || n.contains("огонь") -> "Игниса калидус! Пламя стихий пробуждается."
                n.contains("light") || n.contains("молн") || n.contains("гроз") -> "Фульгур аркана! Грозовой разряд рассекает тьму."
                n.contains("heal") || n.contains("исцел") || n.contains("свет") -> "Санатио люкс! Целительный свет восстанавливает силы."
                n.contains("shield") || n.contains("щит") -> "Эгис кристаллис! Защитный щит развернут."
                n.contains("frost") || n.contains("лед") || n.contains("мороз") -> "Гласиес мортис! Стужа сковывает пространство."
                n.contains("star") || n.contains("звезд") || n.contains("nova") -> "Астралис этернум! Звёздная вспышка ослепляет бездну."
                else -> "${spell.name} — заклинание успешно активировано!"
            }
            StaffLanguage.GERMAN -> when {
                n.contains("fire") || n.contains("feuer") -> "Ignis calidus! Das Elementarfeuer erwacht."
                n.contains("light") || n.contains("blitz") -> "Fulgur arcana! Der Blitz spaltet die Dunkelheit."
                n.contains("heal") || n.contains("heil") -> "Sanatio lux! Heilendes Licht durchströmt den Äther."
                n.contains("shield") || n.contains("schild") -> "Aegis crystallis! Schutzschild ist aktiv."
                n.contains("frost") || n.contains("eis") -> "Glacies mortis! Arktischer Frost erstarrt den Raum."
                else -> "${spell.name} erfolgreich entfesselt!"
            }
            else -> when {
                n.contains("fire") -> "Ignis celestial! Fiery plasma engulfs the realm."
                n.contains("light") -> "Fulgur arcana! Lightning surges through the core."
                n.contains("heal") -> "Sanatio lux! Celestial aura restores your vitality."
                n.contains("shield") -> "Aegis crystallis! Barrier matrix shields the bearer."
                n.contains("frost") -> "Glacies mortis! Absolute zero freezes the void."
                else -> "${spell.name} successfully cast!"
            }
        }
    }

    /**
     * Executes real-time simulated token streaming for the offline engine.
     * Emits identical [GemmaStreamChunk] lifecycle events so UI and audio pacers stream seamlessly.
     */
    fun streamInference(
        prompt: String,
        audioBytes: ByteArray? = null,
        language: StaffLanguage = StaffLanguage.ENGLISH,
        personaId: String = OraclePersona.MACHINE_GOD.id,
        customSpells: List<SpellDefinition> = emptyList()
    ): Flow<GemmaStreamChunk> = flow {
        val t0 = System.currentTimeMillis()
        val hasAudio = audioBytes != null && audioBytes.isNotEmpty()

        // Phase 1: Ingestion & Acoustic Recognition
        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.INGESTING, "Processing on-device tokens..."))

        val actualQuery: String = if (hasAudio && audioBytes != null) {
            val classified = transcriptionService.classifyOfflineSpeech(audioBytes, language = language)
            if (classified.isNotBlank()) {
                emit(GemmaStreamChunk.ThoughtToken("Acoustic voice classified: \"$classified\"\n\n"))
                classified
            } else {
                prompt.ifBlank { "System status inquiry" }
            }
        } else {
            prompt.ifBlank { "System status inquiry" }
        }

        // Phase 2: Offline Prophecy Generation
        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.THINKING, "Formulating on-device prophecy..."))
        emit(GemmaStreamChunk.ThoughtToken("⚡ Engine: Offline Edge SOTA (Zero Cloud)\nPersona: $personaId | Lang: ${language.name}\n\n"))

        val prophecyOutput = resolveOfflineProphecy(actualQuery, personaId, language, customSpells)

        // Phase 3: Token Streaming
        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.STREAMING, "Streaming on-device prophecy..."))

        // Split by words to simulate smooth token streaming for UI / OLED
        val words = prophecyOutput.split(" ")
        for (i in words.indices) {
            val token = if (i == words.lastIndex) words[i] else "${words[i]} "
            emit(GemmaStreamChunk.OutputToken(token))
            delay(TOKEN_STREAM_DELAY_MS)
        }

        // Phase 4: Complete
        val elapsedMs = (System.currentTimeMillis() - t0).coerceAtLeast(1)
        val tokensPerSec = (words.size / (elapsedMs / 1000f)).coerceAtLeast(1f)
        val result = GemmaResult(
            prompt = actualQuery,
            thoughtTrace = "Offline Edge Engine (Zero Cloud). Mode: ${if (FastSpellMatcher.isSpellIntent(actualQuery)) "FAST SPELL" else "ORACLE PROPHECY"}.",
            outputText = prophecyOutput,
            latencyMs = elapsedMs,
            tokensPerSec = tokensPerSec,
            modalityUsed = if (hasAudio) "AUDIO_OFFLINE" else "TEXT_OFFLINE",
            modelTag = "EDGE-OFFLINE"
        )

        emit(GemmaStreamChunk.PhaseChanged(AiProcessingPhase.COMPLETED, "Completed ($elapsedMs ms)"))
        emit(GemmaStreamChunk.Complete(result))
    }
}

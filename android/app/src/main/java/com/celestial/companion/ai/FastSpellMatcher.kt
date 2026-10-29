package com.celestial.companion.ai

import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.SpellDefinition

/**
 * High-speed Zero-Latency Spell Matcher.
 * Matches spoken voice transcriptions and typed prompts directly to spell presets
 * in <1ms without waiting for Cloud LLM Chain-of-Thought reasoning.
 */
object FastSpellMatcher {

    data class FastSpell(
        val keywords: List<String>,
        val definition: SpellDefinition
    )

    private val FAST_SPELLS = listOf(
        FastSpell(
            keywords = listOf(
                "fireball", "фаербол", "фаер бол", "огненный шар", "огонь", "пламя", "метеор", "подпали", "fire", "flame", "meteor"
            ),
            definition = SpellDefinition(
                name = "Fireball",
                animation = LedAnimation.PLASMA,
                sounds = listOf(3, 6, 1), // Laser -> Supernova -> Pulsar
                prompt = "Cast the Fireball spell! [SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER, SUPERNOVA, PULSAR] Ignis plasma infernum!",
                description = "Plasma • Laser • Supernova • Pulsar",
                chantText = "Ignis plasma infernum, ardet caelum!",
                oledText = "🔥 FIREBALL!"
            )
        ),
        FastSpell(
            keywords = listOf(
                "молния", "молнию", "цепная молния", "гроза", "разряд", "гром", "шарахни", "молнией", "lightning", "thunder", "thunderbolt", "zap"
            ),
            definition = SpellDefinition(
                name = "Chain Lightning",
                animation = LedAnimation.RADIAL_WAVE,
                sounds = listOf(0, 3, 1), // Warp -> Laser -> Pulsar
                prompt = "Cast the Chain Lightning spell! [SPELL: Chain Lightning | ANIM: RADIAL_WAVE | SOUNDS: WARP, LASER, PULSAR] Fulgur aetheris catena!",
                description = "Radial Wave • Warp • Laser • Pulsar",
                chantText = "Fulgur aetheris catena, tonitrus fractura!",
                oledText = "⚡ LIGHTNING!"
            )
        ),
        FastSpell(
            keywords = listOf(
                "щит", "эгида", "защита", "барьер", "щит эгиды", "защитный", "защити", "shield", "aegis", "barrier", "protect", "defense"
            ),
            definition = SpellDefinition(
                name = "Aegis Shield",
                animation = LedAnimation.COLLIDING_WAVES,
                sounds = listOf(5, 4, 1), // Portal -> Tractor -> Pulsar
                prompt = "Cast the Aegis Shield spell! [SPELL: Aegis Shield | ANIM: COLLIDING_WAVES | SOUNDS: PORTAL, TRACTOR, PULSAR] Aegis sanctum praesidium!",
                description = "Colliding Waves • Portal • Tractor • Pulsar",
                chantText = "Aegis sanctum praesidium, nullus transitus!",
                oledText = "🛡️ AEGIS SHIELD"
            )
        ),
        FastSpell(
            keywords = listOf(
                "водоворот", "черная дыра", "черную дыру", "сингулярность", "гравитация", "вихрь", "закрути", "vortex", "black hole", "singularity", "gravity", "swirl"
            ),
            definition = SpellDefinition(
                name = "Cosmic Vortex",
                animation = LedAnimation.VORTEX,
                sounds = listOf(0, 4, 6), // Warp -> Tractor -> Supernova
                prompt = "Cast the Cosmic Vortex spell! [SPELL: Cosmic Vortex | ANIM: VORTEX | SOUNDS: WARP, TRACTOR, SUPERNOVA] Vortex chao gravitatis!",
                description = "Vortex • Warp • Tractor • Supernova",
                chantText = "Vortex chao gravitatis, atra foramen!",
                oledText = "🌀 VORTEX!"
            )
        ),
        FastSpell(
            keywords = listOf(
                "змейка", "змейку", "уроборос", "змея", "змею", "snake", "ouroboros", "serpent"
            ),
            definition = SpellDefinition(
                name = "Ouroboros Serpent",
                animation = LedAnimation.SPIRAL_SNAKE,
                sounds = listOf(2, 1, 0), // Alien -> Pulsar -> Warp
                prompt = "Cast the Ouroboros Serpent spell! [SPELL: Ouroboros Serpent | ANIM: SPIRAL_SNAKE | SOUNDS: ALIEN, PULSAR, WARP] Ouroboros serpens aeterna!",
                description = "Spiral Snake • Alien • Pulsar • Warp",
                chantText = "Ouroboros serpens aeterna, volvitur orbis!",
                oledText = "🐍 OUROBOROS"
            )
        ),
        FastSpell(
            keywords = listOf(
                "пульсар", "шок", "взрыв", "ударная волна", "бабах", "взорви", "pulsar", "shockwave", "blast", "shock", "explosion"
            ),
            definition = SpellDefinition(
                name = "Astral Pulsar",
                animation = LedAnimation.PULSAR,
                sounds = listOf(1, 6), // Pulsar -> Supernova
                prompt = "Cast the Astral Pulsar spell! [SPELL: Astral Pulsar | ANIM: PULSAR | SOUNDS: PULSAR, SUPERNOVA] Pulsar aetherium fractura!",
                description = "Pulsar • Pulsar • Supernova",
                chantText = "Pulsar aetherium fractura, resonat cosmos!",
                oledText = "💫 PULSAR!"
            )
        ),
        FastSpell(
            keywords = listOf(
                "лед", "льдом", "вьюга", "стужа", "метель", "мороз", "заморозка", "заморозь", "иней", "снег", "frost", "blizzard", "ice", "freeze", "chilling", "snow"
            ),
            definition = SpellDefinition(
                name = "Frost Nova",
                animation = LedAnimation.STARS,
                sounds = listOf(5, 2, 1), // Portal -> Alien -> Pulsar
                prompt = "Cast the Frost Nova spell! [SPELL: Frost Nova | ANIM: STARS | SOUNDS: PORTAL, ALIEN, PULSAR] Glacies nebula congelatio!",
                description = "Stars • Portal • Alien • Pulsar",
                chantText = "Glacies nebula congelatio, frigus absolutum!",
                oledText = "❄️ FROST NOVA"
            )
        ),
        FastSpell(
            keywords = listOf(
                "галактика", "галактику", "космос", "звезды", "звездный", "млечный путь", "galaxy", "cosmos", "stars", "cosmic"
            ),
            definition = SpellDefinition(
                name = "Galaxy Drift",
                animation = LedAnimation.GALAXY,
                sounds = listOf(5, 0, 7), // Portal -> Warp -> Happy
                prompt = "Cast the Galaxy Drift spell! [SPELL: Galaxy Drift | ANIM: GALAXY | SOUNDS: PORTAL, WARP, R2_HAPPY] Astrum galaxia universum!",
                description = "Galaxy • Portal • Warp • Chirp",
                chantText = "Astrum galaxia universum, stellae micantes!",
                oledText = "🌌 GALAXY"
            )
        ),
        FastSpell(
            keywords = listOf(
                "радуга", "утка", "утку", "кряк", "квантовая утка", "крякни", "rainbow", "quack", "duck"
            ),
            definition = SpellDefinition(
                name = "Quantum Quack",
                animation = LedAnimation.RAINBOW,
                sounds = listOf(8, 9, 7), // Chatter -> Panic -> Happy
                prompt = "Cast the Quantum Quack spell! [SPELL: Quantum Quack | ANIM: RAINBOW | SOUNDS: R2_CHATTER, R2_PANIC, R2_HAPPY] Quantus anas quackus maximus!",
                description = "Rainbow • Chatter • Panic • Chirp",
                chantText = "Quantus anas quackus maximus, zzzz-wub-wub!",
                oledText = "🦆 QUACK!"
            )
        ),
        FastSpell(
            keywords = listOf(
                "хэллоуин", "тыква", "тыкву", "светильник", "кошмар", "ужас", "halloween", "pumpkin", "spooky", "scare"
            ),
            definition = SpellDefinition(
                name = "Jack-O'-Lantern",
                animation = LedAnimation.HALLOWEEN,
                sounds = listOf(2, 6), // Alien -> Supernova
                prompt = "Cast the Jack-O'-Lantern spell! [SPELL: Jack-O'-Lantern | ANIM: HALLOWEEN | SOUNDS: ALIEN, SUPERNOVA] Cucurbita umbra nocturna!",
                description = "Halloween • Alien • Supernova",
                chantText = "Cucurbita umbra nocturna, tenebrae surgunt!",
                oledText = "🎃 HALLOWEEN"
            )
        ),
        FastSpell(
            keywords = listOf(
                "дроид", "дроида", "паника", "r2d2", "пищалка", "астромех", "droid", "hysteria", "r2"
            ),
            definition = SpellDefinition(
                name = "Droid Hysteria",
                animation = LedAnimation.VORTEX,
                sounds = listOf(7, 8, 9), // Happy -> Chatter -> Panic
                prompt = "Cast the Droid Hysteria spell! [SPELL: Droid Hysteria | ANIM: VORTEX | SOUNDS: R2_HAPPY, R2_CHATTER, R2_PANIC] Bip-boop astromech hysterium!",
                description = "Vortex • Chirp • Chatter • Scream",
                chantText = "Bip-boop bip-bop astromech hysterium, waaaah!",
                oledText = "🤖 DROID"
            )
        )
    )

    private val SPELL_INTENT_WORDS = setOf(
        "заклинание", "заклинанием", "кастани", "скастуй", "кастуй", "поколдуй", "колдуй",
        "шарахни", "жахни", "швырни", "запусти", "активируй", "примени",
        "cast", "spell", "magic", "incantation", "conjure", "invoke"
    )

    private fun textMatchesKeyword(lowerText: String, words: Set<String>, kw: String): Boolean {
        val kwLower = kw.lowercase().trim()
        if (kwLower.isEmpty()) return false
        if (kwLower.contains(' ')) {
            // Multi-word phrase like "огненный шар", "black hole", "цепная молния"
            return lowerText.contains(kwLower)
        }
        if (kwLower.length <= 4) {
            // Short keywords (e.g. "ice", "fire", "лед", "гром", "щит", "утка", "снег")
            // MUST match whole word! Never match inside "voice", "след", "огромный"
            return words.contains(kwLower)
        }
        // Longer keywords (>4 chars): match whole word or prefix of word (for declensions/suffixes)
        return words.contains(kwLower) || words.any { it.startsWith(kwLower) }
    }

    /**
     * Checks if the text has spell-casting intent.
     */
    fun isSpellIntent(text: String): Boolean {
        val lower = text.lowercase().trim()
        if (lower.isEmpty()) return false
        val words = lower.split(Regex("[\\s,.:;!?\"'()\\[\\]{}]+")).filter { it.isNotBlank() }.toSet()
        if (SPELL_INTENT_WORDS.any { textMatchesKeyword(lower, words, it) }) return true
        return FAST_SPELLS.any { fs -> fs.keywords.any { textMatchesKeyword(lower, words, it) } }
    }

    /**
     * Matches user input (speech or text) to a spell definition in < 1ms.
     * Checks custom spells first, then the built-in fast spells catalog.
     */
    fun match(text: String, customSpells: List<SpellDefinition> = emptyList()): SpellDefinition? {
        val lower = text.lowercase().trim()
        if (lower.isEmpty()) return null
        val words = lower.split(Regex("[\\s,.:;!?\"'()\\[\\]{}]+")).filter { it.isNotBlank() }.toSet()

        // 1. Check user custom spells first
        for (custom in customSpells) {
            val nameLower = custom.name.lowercase().trim()
            if (nameLower.isNotEmpty() && textMatchesKeyword(lower, words, nameLower)) {
                return custom
            }
        }

        // 2. Check built-in fast spells
        for (fs in FAST_SPELLS) {
            for (kw in fs.keywords) {
                if (textMatchesKeyword(lower, words, kw)) {
                    return fs.definition
                }
            }
        }

        // 3. If user said generic "кастани заклинание" / "cast a spell" without specific spell
        if (isSpellIntent(lower)) {
            // Pick Fireball or Plasma Storm as default spell
            return FAST_SPELLS.first().definition
        }

        return null
    }
}

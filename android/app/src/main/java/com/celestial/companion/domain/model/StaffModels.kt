package com.celestial.companion.domain.model

enum class StaffState(val displayName: String) {
    IDLE("IDLE"),
    LISTENING("LISTENING"),
    PROCESSING("PROCESSING"),
    SPEAKING("SPEAKING");

    companion object {
        fun fromOrdinalSafe(idx: Int): StaffState {
            return when (idx) {
                2 -> IDLE          // STATE_IDLE = 2 in firmware
                3 -> LISTENING     // STATE_LISTENING = 3 in firmware
                4, 5 -> PROCESSING // STATE_UPLOADING = 4, STATE_PROCESSING = 5 in firmware
                6 -> SPEAKING      // STATE_SPEAKING = 6 in firmware
                else -> IDLE
            }
        }
    }
}

enum class StaffMode(val id: Int, val displayName: String, val subtitle: String) {
    GUIDE(0, "GUIDE", "Celestial Status & Telemetry"),
    ARCHITECTURE(1, "ARCHITECTURE", "Staff & AI Host Wireless Link"),
    COSMOS(2, "COSMOS", "Galactic Map, Solar Orrery & Terra Zoom"),
    CHRONOS(3, "CHRONOS", "Astronomical Chronometer"),
    AI_MODEL(4, "AI CORE", "Cloud Neural Engine Selector"),
    LANGUAGE(5, "LINGUA", "Language Dialect"),
    VOLUME(6, "VOLUME", "Master Audio Volume"),
    LED_CORE(7, "RUNE LEDS", "3-Ring WS2812B Animations"),
    MIC_TEST(8, "MIC VU", "INMP441 Microphone Meter"),
    CHIPTUNE(9, "JUKEBOX", "8-Bit Retro Chiptunes"),
    COSMIC(10, "COSMIC FX", "Sci-Fi & Astromech Sound Suite"),
    VOICE(11, "ORACLE", "Arcane Spoken Liturgy"),
    SYSTEM(12, "SYSTEM", "RP2350 M33 Hardware Telemetry");

    companion object {
        fun fromId(id: Int): StaffMode {
            return entries.find { it.id == id } ?: GUIDE
        }
    }
}

enum class ClockStyle(val id: Int, val title: String, val desc: String) {
    DIAL(0, "DIAL CHRONOS", "12H Astronomical Dial with Hands & Comet"),
    HOURGLASS(1, "AETHER HOURGLASS", "Concentric Ring Liquid Accumulator"),
    PRISMATIC(2, "CHROMA PRISMATIC", "Full Spectral Color-Encoded Clock"),
    ORRERY(3, "CELESTIAL ORRERY", "Kinematic Concentric Triple Planetary Gears");

    companion object {
        fun fromId(id: Int): ClockStyle {
            return entries.find { it.id == id } ?: HOURGLASS
        }
    }
}

enum class LedAnimation(val id: Int, val title: String, val subtitle: String) {
    BREATHE(0, "Cosmic Breathe", "Slow cosmic breathing across all 3 rings"),
    VORTEX(1, "Galaxy Vortex", "Spinning galaxy comet vortex & singularity"),
    SNAKE(2, "Cosmic Snake", "Cosmic dragon snake winding across rings"),
    RAINBOW(3, "Rainbow Wave", "Smooth chromatic rotating rainbow wave"),
    PULSAR(4, "Pulsar Shockwave", "Cosmic heartbeat / pulsar shockwave"),
    PLASMA(5, "Solar Plasma", "Living solar plasma & turbulent solar flare"),
    STARS(6, "Starry Night", "Deep navy & diamond starlight sparkle"),
    VOICE(7, "Voice Halo", "Live audio & voice reactive VU halo"),
    SOLAR(8, "Solar Orrery", "Solar Orrery concentric planetary rings"),
    DATA_RECV(9, "Data Reception", "Concentric data reception radar ripples"),
    EXPAND_SPEECH(10, "Sonic Expansion", "Expanding sonic ripples from core outward"),
    GALAXY(11, "Milky Way Galaxy", "Milky Way galactic spiral & Sol beacon"),
    EARTH(12, "Terra Planet", "Terra globe: azure oceans, emerald land, aurora"),
    HALLOWEEN(13, "Halloween Spook", "Jack-o'-Lantern amber & spooky ectoplasm"),
    CLOCK_DIAL(14, "Celestial Clock", "3-ring astronomical dial with comet hand"),
    CLOCK_HOURGLASS(15, "Aether Hourglass", "Concentric ring liquid accumulator"),
    CLOCK_PRISMATIC(16, "Chroma Prismatic", "Full spectral color-encoded clock"),
    CLOCK_ORRERY(17, "Celestial Orrery", "Kinematic triple planetary orbital clock"),
    SPIRAL_SNAKE(18, "Ouroboros Snake", "Concentric snake winding through rings 24->12->7"),
    RADIAL_WAVE(19, "Radial Shockwave", "Concentric waves expanding from core to outer rim"),
    COLLIDING_WAVES(20, "Colliding Waves", "Counter-rotating waves colliding at 0° and 180° with white flash");

    val displayName: String get() = title

    companion object {
        fun fromId(id: Int): LedAnimation {
            return entries.find { it.id == id } ?: RAINBOW
        }
    }
}

data class SfxItem(
    val id: Int,
    val name: String,
    val isDroid: Boolean = false
) {
    companion object {
        val ALL_SFX = listOf(
            SfxItem(0, "Warp Jump (Hyperdrive)"),
            SfxItem(1, "Pulsar Beacon"),
            SfxItem(2, "Alien Scanner Arpeggio"),
            SfxItem(3, "Photon Laser Blaster"),
            SfxItem(4, "Gravitational Tractor Beam"),
            SfxItem(5, "Stargate Portal Shimmer"),
            SfxItem(6, "Supernova Bass Rumble"),
            SfxItem(7, "R2-D2 Happy Whistle & Chirp", isDroid = true),
            SfxItem(8, "R2-D2 Sassy Astromech Banter", isDroid = true),
            SfxItem(9, "R2-D2 Panicked Alarm Scream", isDroid = true)
        )
    }
}

enum class StaffLanguage(
    val id: Int,
    val code: String,
    val displayName: String,
    val shortTag: String,
    val locale: java.util.Locale
) {
    ENGLISH(0, "en-US", "English", "EN", java.util.Locale.US),
    AUTO(1, "auto", "Auto Detect", "AUTO", java.util.Locale.US),
    GERMAN(2, "de-DE", "Deutsch", "DE", java.util.Locale.GERMANY),
    RUSSIAN(3, "ru-RU", "Русский", "RU", java.util.Locale("ru", "RU"));

    companion object {
        fun fromId(id: Int): StaffLanguage = entries.find { it.id == id } ?: ENGLISH
    }
}

data class SpellDefinition(
    val name: String,
    val animation: LedAnimation,
    val sounds: List<Int>, // SFX IDs 0..9 (Warp, Pulsar, Alien, Laser, Tractor, Portal, Supernova, DroidHappy, DroidChatter, DroidPanic)
    val prompt: String,
    val description: String = "",
    val chantText: String = "",
    val oledText: String = "",
    val isCustom: Boolean = false
) {
    fun toJson(): org.json.JSONObject {
        val json = org.json.JSONObject()
        json.put("name", name)
        json.put("animation", animation.name)
        val sArr = org.json.JSONArray()
        sounds.forEach { sArr.put(it) }
        json.put("sounds", sArr)
        json.put("prompt", prompt)
        json.put("description", description)
        json.put("chantText", chantText)
        json.put("oledText", oledText)
        json.put("isCustom", isCustom)
        return json
    }

    companion object {
        fun fromJson(json: org.json.JSONObject): SpellDefinition {
            val animStr = json.optString("animation", "PLASMA")
            val anim = try { LedAnimation.valueOf(animStr) } catch (_: Exception) { LedAnimation.PLASMA }
            val soundsList = mutableListOf<Int>()
            val sArr = json.optJSONArray("sounds")
            if (sArr != null) {
                for (i in 0 until sArr.length()) {
                    soundsList.add(sArr.optInt(i))
                }
            }
            return SpellDefinition(
                name = json.optString("name", "Custom Spell"),
                animation = anim,
                sounds = if (soundsList.isEmpty()) listOf(3, 6, 1) else soundsList,
                prompt = json.optString("prompt", ""),
                description = json.optString("description", ""),
                chantText = json.optString("chantText", ""),
                oledText = json.optString("oledText", ""),
                isCustom = json.optBoolean("isCustom", true)
            )
        }

        val PRESET_SPELLS = listOf(
            SpellDefinition(
                name = "Fireball",
                animation = LedAnimation.PLASMA,
                sounds = listOf(3, 6, 1), // Laser -> Supernova -> Pulsar
                prompt = "Cast the Fireball arcane spell! [SPELL: Fireball | ANIM: PLASMA | SOUNDS: LASER, SUPERNOVA, PULSAR] Ignis plasma infernum!",
                description = "Plasma • Laser • Supernova • Pulsar",
                chantText = "Ignis plasma infernum, ardet caelum!",
                oledText = "🔥 FIREBALL!"
            ),
            SpellDefinition(
                name = "Chain Lightning",
                animation = LedAnimation.RADIAL_WAVE,
                sounds = listOf(0, 3, 1), // Warp -> Laser -> Pulsar
                prompt = "Cast the Chain Lightning arcane spell! [SPELL: Chain Lightning | ANIM: RADIAL_WAVE | SOUNDS: WARP, LASER, PULSAR] Fulgur aetheris catena!",
                description = "Radial Wave • Warp • Laser • Pulsar",
                chantText = "Fulgur aetheris catena, tonitrus fractura!",
                oledText = "⚡ LIGHTNING!"
            ),
            SpellDefinition(
                name = "Aegis Shield",
                animation = LedAnimation.COLLIDING_WAVES,
                sounds = listOf(5, 4, 1), // Portal -> Tractor -> Pulsar
                prompt = "Cast the Aegis Shield arcane spell! [SPELL: Aegis Shield | ANIM: COLLIDING_WAVES | SOUNDS: PORTAL, TRACTOR, PULSAR] Aegis sanctum praesidium!",
                description = "Colliding Waves • Portal • Tractor • Pulsar",
                chantText = "Aegis sanctum praesidium, nullus transitus!",
                oledText = "🛡️ AEGIS SHIELD"
            ),
            SpellDefinition(
                name = "Cosmic Vortex",
                animation = LedAnimation.VORTEX,
                sounds = listOf(0, 4, 6), // Warp -> Tractor -> Supernova
                prompt = "Cast the Cosmic Vortex arcane spell! [SPELL: Cosmic Vortex | ANIM: VORTEX | SOUNDS: WARP, TRACTOR, SUPERNOVA] Vortex chao gravitatis!",
                description = "Vortex • Warp • Tractor • Supernova",
                chantText = "Vortex chao gravitatis, atra foramen!",
                oledText = "🌀 VORTEX!"
            ),
            SpellDefinition(
                name = "Ouroboros Serpent",
                animation = LedAnimation.SPIRAL_SNAKE,
                sounds = listOf(2, 1, 0), // Alien -> Pulsar -> Warp
                prompt = "Cast the Ouroboros Serpent arcane spell! [SPELL: Ouroboros Serpent | ANIM: SPIRAL_SNAKE | SOUNDS: ALIEN, PULSAR, WARP] Ouroboros serpens aeterna!",
                description = "2D Ouroboros • Alien • Pulsar • Warp",
                chantText = "Ouroboros serpens aeterna, volvitur orbis!",
                oledText = "🐍 OUROBOROS"
            ),
            SpellDefinition(
                name = "Quantum Quack",
                animation = LedAnimation.RAINBOW,
                sounds = listOf(8, 9, 7), // Chatter -> Panic -> Happy
                prompt = "Cast the Quantum Quack arcane spell! [SPELL: Quantum Quack | ANIM: RAINBOW | SOUNDS: R2_CHATTER, R2_PANIC, R2_HAPPY] Quantus anas quackus maximus!",
                description = "Rainbow • Chatter • Panic • Chirp",
                chantText = "Quantus anas quackus maximus, zzzz-wub-wub!",
                oledText = "🦆 QUACK!"
            ),
            SpellDefinition(
                name = "Frost Nova",
                animation = LedAnimation.STARS,
                sounds = listOf(5, 2, 1), // Portal -> Alien -> Pulsar
                prompt = "Cast the Frost Nova arcane spell! [SPELL: Frost Nova | ANIM: STARS | SOUNDS: PORTAL, ALIEN, PULSAR] Glacies nebula congelatio!",
                description = "Stars • Portal • Alien • Pulsar",
                chantText = "Glacies nebula congelatio, frigus absolutum!",
                oledText = "❄️ FROST NOVA"
            ),
            SpellDefinition(
                name = "Astral Rift",
                animation = LedAnimation.GALAXY,
                sounds = listOf(5, 0, 7), // Portal -> Warp -> Happy
                prompt = "Cast the Astral Rift arcane spell! [SPELL: Astral Rift | ANIM: GALAXY | SOUNDS: PORTAL, WARP, R2_HAPPY] Astrum galaxia universum!",
                description = "Galaxy • Portal • Warp • Chirp",
                chantText = "Astrum galaxia universum, stellae micantes!",
                oledText = "🌌 GALAXY"
            ),
            SpellDefinition(
                name = "Jack-O'-Lantern",
                animation = LedAnimation.HALLOWEEN,
                sounds = listOf(2, 6), // Alien -> Supernova
                prompt = "Cast the Jack-O'-Lantern arcane spell! [SPELL: Jack-O'-Lantern | ANIM: HALLOWEEN | SOUNDS: ALIEN, SUPERNOVA] Cucurbita umbra nocturna!",
                description = "Halloween • Alien • Supernova",
                chantText = "Cucurbita umbra nocturna, tenebrae surgunt!",
                oledText = "🎃 HALLOWEEN"
            ),
            SpellDefinition(
                name = "Droid Hysteria",
                animation = LedAnimation.VORTEX,
                sounds = listOf(7, 8, 9), // Happy -> Chatter -> Panic
                prompt = "Cast the Droid Hysteria arcane spell! [SPELL: Droid Hysteria | ANIM: VORTEX | SOUNDS: R2_HAPPY, R2_CHATTER, R2_PANIC] Bip-boop astromech hysterium!",
                description = "Vortex • Chirp • Chatter • Scream",
                chantText = "Bip-boop bip-bop astromech hysterium, waaaah!",
                oledText = "🤖 DROID"
            )
        )
    }
}

enum class LogLevel(val label: String) {
    DEBUG("DEBUG"),
    INFO("INFO"),
    WARN("WARN"),
    ERROR("ERROR"),
    SUCCESS("SUCCESS")
}

data class LiveLogMessage(
    val id: Long = System.nanoTime(),
    val timestamp: String,
    val tag: String,
    val message: String,
    val level: LogLevel = LogLevel.INFO
)


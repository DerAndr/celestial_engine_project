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
    CLOCK_ORRERY(17, "Celestial Orrery", "Kinematic triple planetary orbital clock");

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
    val description: String = ""
) {
    companion object {
        val PRESET_SPELLS = listOf(
            SpellDefinition(
                name = "Quantum Quack",
                animation = LedAnimation.SNAKE,
                sounds = listOf(2, 9, 6), // Alien -> Droid Panic -> Supernova
                prompt = "Cast the Quantum Quack arcane spell! Begin with [SPELL: Quantum Quack | ANIM: SNAKE | SOUNDS: ALIEN, PANIC, SUPERNOVA] and chant a funny abracadabra pseudo-Latin formula!",
                description = "Snake • Alien • Panic • Nova"
            ),
            SpellDefinition(
                name = "Plasma Storm",
                animation = LedAnimation.PLASMA,
                sounds = listOf(3, 0, 6), // Laser -> Warp -> Supernova
                prompt = "Cast the Plasma Storm arcane spell! Begin with [SPELL: Plasma Storm | ANIM: PLASMA | SOUNDS: LASER, WARP, SUPERNOVA] and chant an epic abracadabra fire incantation!",
                description = "Plasma • Laser • Warp • Nova"
            ),
            SpellDefinition(
                name = "Droid Hysteria",
                animation = LedAnimation.VORTEX,
                sounds = listOf(7, 8, 9), // Happy -> Chatter -> Panic
                prompt = "Cast the Droid Hysteria arcane spell! Begin with [SPELL: Droid Hysteria | ANIM: VORTEX | SOUNDS: R2_HAPPY, R2_CHATTER, R2_PANIC] and chant a hilarious astromech gibberish spell!",
                description = "Vortex • Chirp • Chatter • Scream"
            ),
            SpellDefinition(
                name = "Gravitational Sneeze",
                animation = LedAnimation.PULSAR,
                sounds = listOf(4, 2, 6), // Tractor -> Alien -> Supernova
                prompt = "Cast the Gravitational Sneeze arcane spell! Begin with [SPELL: Gravitational Sneeze | ANIM: PULSAR | SOUNDS: TRACTOR, ALIEN, SUPERNOVA] and chant a chaotic pseudo-Latin incantation!",
                description = "Pulsar • Tractor • Alien • Nova"
            ),
            SpellDefinition(
                name = "Astral Rift",
                animation = LedAnimation.GALAXY,
                sounds = listOf(5, 0, 7), // Portal -> Warp -> Happy
                prompt = "Cast the Astral Rift arcane spell! Begin with [SPELL: Astral Rift | ANIM: GALAXY | SOUNDS: PORTAL, WARP, R2_HAPPY] and chant a mystical cosmic portal incantation!",
                description = "Galaxy • Portal • Warp • Chirp"
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


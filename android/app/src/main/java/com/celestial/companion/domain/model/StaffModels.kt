package com.celestial.companion.domain.model

enum class StaffState(val displayName: String) {
    IDLE("IDLE"),
    LISTENING("LISTENING"),
    PROCESSING("PROCESSING"),
    SPEAKING("SPEAKING");

    companion object {
        fun fromOrdinalSafe(idx: Int): StaffState {
            return entries.getOrElse(idx) { IDLE }
        }
    }
}

enum class StaffMode(val id: Int, val displayName: String, val subtitle: String) {
    GUIDE(0, "GUIDE", "Celestial Status & Telemetry"),
    GALAXY(1, "GALAXY", "Milky Way Galactic Map"),
    SOLAR(2, "SOL SYSTEM", "Solar Planetary Orbits"),
    TERRA(3, "TERRA", "Earth Close-Up Zoom"),
    CHRONOS(4, "CHRONOS", "Astronomical Chronometer"),
    AI_MODEL(5, "AI CORE", "Cloud Neural Engine Selector"),
    LANGUAGE(6, "LINGUA", "Language Dialect"),
    VOLUME(7, "VOLUME", "Master Audio Volume"),
    LED_CORE(8, "RUNE LEDS", "3-Ring WS2812B Animations"),
    MIC_TEST(9, "MIC VU", "INMP441 Microphone Meter"),
    CHIPTUNE(10, "JUKEBOX", "8-Bit Retro Chiptunes"),
    COSMIC(11, "COSMIC FX", "Sci-Fi & Astromech Sound Suite"),
    VOICE(12, "ORACLE", "Arcane Spoken Liturgy"),
    SYSTEM(13, "SYSTEM", "RP2350 M33 Hardware Telemetry");

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

enum class LedAnimation(val id: Int, val title: String) {
    RAINBOW(0, "Rainbow Runes"),
    AURORA(1, "Aurora Borealis"),
    BREATHE(2, "Cosmic Pulse"),
    SPARKLE(3, "Starlight Sparkle"),
    CHASE(4, "Orbital Chase"),
    FIRE(5, "Solar Flare / Fire"),
    WAVE(6, "Aetheric Wave"),
    COLOR_WIPE(7, "Color Wipe"),
    THEATER(8, "Theater Marquee"),
    GALAXY(9, "Milky Way Spiral"),
    SOLAR(10, "Planetary Orrery"),
    EARTH(11, "Terra Atmosphere"),
    CLOCK_DIAL(12, "Chronos Dial"),
    CLOCK_HOURGLASS(13, "Aether Hourglass"),
    CLOCK_PRISMATIC(14, "Chroma Prismatic"),
    CLOCK_ORRERY(15, "Celestial Orrery"),
    SPEECH_EXPAND(16, "Speech Waves"),
    QUANTUM(17, "Quantum Shift");

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

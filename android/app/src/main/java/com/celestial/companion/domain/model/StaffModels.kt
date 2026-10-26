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
    COLLIDING_WAVES(20, "Colliding Waves", "Counter-rotating waves colliding at 0° and 180° with white flash"),
    ARCHIMEDES_SPIRAL(21, "Archimedes Spiral", "Hypnotic rotating dual spiral arms with radial phase lag"),
    SACRED_POLYGON(22, "Sacred Runes", "Clockwork geometric runes: Triquetra, Hexagram & Dodecagram"),
    DOOM(23, "Doom Hellfire", "Hellfire Crimson & BFG9000 plasma shockwaves"),
    TETRIS(24, "Tetris Arcade", "Concentric falling tetrominoes & 4-line clear"),
    ZELDA(25, "Zelda Triforce", "Golden Sacred Triforce & Courage Core"),
    STARWARS(26, "Star Wars Duel", "Lightsaber Duel Sith vs Jedi Plasma Clash"),
    GHOSTBUSTERS(27, "Ghostbusters", "Slimer Neon Ectoplasm & Crossing Proton Streams"),
    MORROWIND(28, "Morrowind Ash", "Red Mountain Ash Storm & Vivec Turquoise Aurora"),
    SPOOKY(29, "Spooky Skeletons", "Bone Dance Strobe & Jack-o'-Lantern Glow"),
    DAFT_PUNK(30, "Daft Punk Visor", "Cyberpunk Chrome Gold & Silver Visor"),
    EBAY(31, "eBay Auction", "4-Color Auction Strobe & Gavel Shockwave"),
    HARRY_POTTER(32, "Harry Potter", "Hogwarts 4 House Runes & Lumos Core Flash"),
    POKEMON(33, "Pokémon Pokéball", "Red & White Pokéball with Pulsing Center Button"),
    SAILOR_MOON(34, "Sailor Moon", "Moon Prism Power Pastel Transformation Spiral");

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
            SfxItem(9, "R2-D2 Panicked Alarm Scream", isDroid = true),
            SfxItem(10, "eBay Cash Register (Cha-Ching!)"),
            SfxItem(11, "Pokémon Center Healing Jingle"),
            SfxItem(12, "Lumos Wand Flare"),
            SfxItem(13, "Expecto Patronum Mist")
        )
    }
}

data class ThematicPack(
    val id: Int,
    val title: String,
    val subtitle: String,
    val trackId: Int,
    val animation: LedAnimation,
    val iconEmoji: String,
    val description: String,
    val quickActionName: String? = null,
    val quickActionSfxId: Int? = null,
    val secondaryActionName: String? = null,
    val secondaryActionSfxId: Int? = null
) {
    companion object {
        val ALL_PACKS = listOf(
            ThematicPack(
                id = 0,
                title = "DOOM (E1M1)",
                subtitle = "Hellfire Crimson & BFG Plasma",
                trackId = 2,
                animation = LedAnimation.DOOM,
                iconEmoji = "🔥",
                description = "At Doom's Gate heavy metal chiptune riff + BFG9000 shockwaves"
            ),
            ThematicPack(
                id = 1,
                title = "TETRIS ARCADE",
                subtitle = "Korobeiniki 4-Line Clear",
                trackId = 1,
                animation = LedAnimation.TETRIS,
                iconEmoji = "🧱",
                description = "Full 64-note Korobeiniki + falling tetrominoes and line clear"
            ),
            ThematicPack(
                id = 2,
                title = "LEGEND OF ZELDA",
                subtitle = "Triforce of Courage",
                trackId = 3,
                animation = LedAnimation.ZELDA,
                iconEmoji = "🗡️",
                description = "Overworld March fanfare + sacred spinning golden Triforce"
            ),
            ThematicPack(
                id = 3,
                title = "STAR WARS",
                subtitle = "Imperial March Sith vs Jedi",
                trackId = 4,
                animation = LedAnimation.STARWARS,
                iconEmoji = "⚔️",
                description = "Darth Vader's theme + dual red & blue lightsaber clash"
            ),
            ThematicPack(
                id = 4,
                title = "GHOSTBUSTERS",
                subtitle = "Ectoplasm & Proton Streams",
                trackId = 5,
                animation = LedAnimation.GHOSTBUSTERS,
                iconEmoji = "👻",
                description = "Slap-bass groove + 'Who you gonna call?!' + crossing streams"
            ),
            ThematicPack(
                id = 5,
                title = "MORROWIND",
                subtitle = "Nerevar Rising Ash Storm",
                trackId = 6,
                animation = LedAnimation.MORROWIND,
                iconEmoji = "🌋",
                description = "Elder Scrolls majestic horns + Red Mountain ash aurora"
            ),
            ThematicPack(
                id = 6,
                title = "SPOOKY SKELETONS",
                subtitle = "Bone Dance Strobe",
                trackId = 7,
                animation = LedAnimation.SPOOKY,
                iconEmoji = "💀",
                description = "Electro-swing xylophone triplets + Jack-o'-Lantern candlelight"
            ),
            ThematicPack(
                id = 7,
                title = "DAFT PUNK",
                subtitle = "Harder, Better, Faster, Stronger",
                trackId = 8,
                animation = LedAnimation.DAFT_PUNK,
                iconEmoji = "🤖",
                description = "French touch electro vocoder synth + Cyberpunk chrome visor waves"
            ),
            ThematicPack(
                id = 8,
                title = "EBAY CHAOS AUCTION",
                subtitle = "4-Color Strobe & Cha-Ching!",
                trackId = 9,
                animation = LedAnimation.EBAY,
                iconEmoji = "🔨",
                description = "Accelerating bidding war + 4-color strobe + cash register bell",
                quickActionName = "Cha-Ching! 💰",
                quickActionSfxId = 10
            ),
            ThematicPack(
                id = 9,
                title = "HARRY POTTER",
                subtitle = "Hedwig's Theme & Hogwarts Runes",
                trackId = 10,
                animation = LedAnimation.HARRY_POTTER,
                iconEmoji = "⚡",
                description = "16-bit Hedwig's Theme waltz + Hogwarts 4 House Runes",
                quickActionName = "Lumos! 🪄",
                quickActionSfxId = 12,
                secondaryActionName = "Patronus! 🦅",
                secondaryActionSfxId = 13
            ),
            ThematicPack(
                id = 10,
                title = "POKÉMON PACK",
                subtitle = "Wild Battle & Pokéball Ring",
                trackId = 11,
                animation = LedAnimation.POKEMON,
                iconEmoji = "🔴",
                description = "Wild Pokémon Battle Theme + concentric red & white Pokéball",
                quickActionName = "PokéCenter Heal 🏥",
                quickActionSfxId = 11
            ),
            ThematicPack(
                id = 11,
                title = "SAILOR MOON",
                subtitle = "Moonlight Densetsu (ムーンライト伝説)",
                trackId = 12,
                animation = LedAnimation.SAILOR_MOON,
                iconEmoji = "🌙",
                description = "Romantic 8-bit anime theme + Moon Prism Power pastel transformation"
            ),
            ThematicPack(
                id = 12,
                title = "SUPER MARIO BROS",
                subtitle = "Overworld Theme",
                trackId = 0,
                animation = LedAnimation.RAINBOW,
                iconEmoji = "🍄",
                description = "Classic Nintendo 8-bit Overworld Theme"
            )
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
    val isCustom: Boolean = false,
    val voiceEnabled: Boolean = true
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
        json.put("voiceEnabled", voiceEnabled)
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
                isCustom = json.optBoolean("isCustom", true),
                voiceEnabled = json.optBoolean("voiceEnabled", true)
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
            ),
            SpellDefinition(
                name = "Lumos Maxima",
                animation = LedAnimation.HARRY_POTTER,
                sounds = listOf(12, 5, 1),
                prompt = "Cast Lumos Maxima! [SPELL: Lumos Maxima | ANIM: HARRY_POTTER | SOUNDS: LUMOS, PORTAL, PULSAR] Lumos maxima lux aetheris!",
                description = "Hogwarts Runes • Lumos • Portal • Shimmer",
                chantText = "Lumos maxima lux aetheris, illustra noctem!",
                oledText = "🪄 LUMOS!"
            ),
            SpellDefinition(
                name = "Expecto Patronum",
                animation = LedAnimation.HARRY_POTTER,
                sounds = listOf(13, 0, 7),
                prompt = "Cast Expecto Patronum! [SPELL: Expecto Patronum | ANIM: HARRY_POTTER | SOUNDS: PATRONUS, WARP, HAPPY] Expecto patronum anima argentea!",
                description = "Hogwarts Runes • Patronus Mist • Warp",
                chantText = "Expecto patronum anima argentea, averte dementores!",
                oledText = "🦅 PATRONUS!"
            ),
            SpellDefinition(
                name = "eBay Chaos Bid",
                animation = LedAnimation.EBAY,
                sounds = listOf(10, 3, 6),
                prompt = "Cast eBay Chaos Bid! [SPELL: eBay Chaos Bid | ANIM: EBAY | SOUNDS: CHA_CHING, LASER, SUPERNOVA] Cha-ching gavel strike!",
                description = "4-Color Strobe • Cha-Ching • Laser • Gavel",
                chantText = "Going once, going twice, sold for ten thousand credits!",
                oledText = "🔨 SOLD!"
            ),
            SpellDefinition(
                name = "Moon Prism Power",
                animation = LedAnimation.SAILOR_MOON,
                sounds = listOf(5, 1, 7),
                prompt = "Cast Moon Prism Power! [SPELL: Moon Prism Power | ANIM: SAILOR_MOON | SOUNDS: PORTAL, PULSAR, HAPPY] Moon Prism Power Make Up!",
                description = "Pastel Spiral • Portal • Shimmer • Chime",
                chantText = "Moon Prism Power, Make Up! Tsuki ni kawatte oshioki yo!",
                oledText = "🌙 MOON PRISM"
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


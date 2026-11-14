package com.celestial.companion.audio

import com.celestial.companion.domain.model.OraclePersona

/**
 * Canonical Kokoro-82M INT8 ONNX voice profiles mapped to the 6 authoritative Oracle Personas
 * (1:1 parity with Raspberry Pi 5 `PERSONA_KOKORO_VOICE` in `tools/local_ai_engine.py`).
 */
enum class KokoroVoiceProfile(
    val voiceId: String,
    val displayLabel: String,
    val personaId: String,
    val pitch: Float,
    val speechRate: Float
) {
    AM_ADAM(
        voiceId = "am_adam",
        displayLabel = "am_adam (US Male — Machine God)",
        personaId = OraclePersona.MACHINE_GOD.id,
        pitch = 0.88f,
        speechRate = 1.10f
    ),
    BM_GEORGE_ARCHMAGE(
        voiceId = "bm_george",
        displayLabel = "bm_george (UK Male — Wise Archmage)",
        personaId = OraclePersona.ARCHMAGE.id,
        pitch = 0.84f,
        speechRate = 0.96f
    ),
    BM_GEORGE_MARVIN(
        voiceId = "bm_george",
        displayLabel = "bm_george (UK Male — Marvin Android)",
        personaId = OraclePersona.MARVIN.id,
        pitch = 0.85f,
        speechRate = 0.92f
    ),
    AF_BELLA(
        voiceId = "af_bella",
        displayLabel = "af_bella (US Female — Cyberpunk AI)",
        personaId = OraclePersona.CYBERPUNK.id,
        pitch = 1.08f,
        speechRate = 1.20f
    ),
    AM_MICHAEL(
        voiceId = "am_michael",
        displayLabel = "am_michael (Deep US Male — Jack Pumpkin)",
        personaId = OraclePersona.PUMPKIN_LORD.id,
        pitch = 1.12f,
        speechRate = 1.15f
    ),
    AF_HEART(
        voiceId = "af_heart",
        displayLabel = "af_heart (US Female — Studio Default)",
        personaId = OraclePersona.CUSTOM.id,
        pitch = 1.00f,
        speechRate = 1.12f
    );

    companion object {
        fun forPersona(personaId: String): KokoroVoiceProfile {
            return entries.firstOrNull { it.personaId == personaId } ?: AM_ADAM
        }
    }
}

package com.celestial.companion.ai

/**
 * Operating mode for the Celestial Engine AI subsystem (Task 8.2.2 Dual Engine Provider).
 */
enum class AiEngineMode(
    val id: Int,
    val displayName: String,
    val shortTag: String,
    val description: String
) {
    AUTO_FALLBACK(
        0,
        "Auto Fallback",
        "AUTO",
        "Cloud Gemini 2.5 Flash with seamless on-device offline fallback"
    ),
    OFFLINE_EDGE(
        1,
        "100% Offline Edge",
        "OFFLINE",
        "Pure on-device execution: zero cloud, zero API keys, sub-20ms spell forge"
    ),
    CLOUD_ONLY(
        2,
        "Cloud Only",
        "CLOUD",
        "Strictly route queries to OpenRouter cloud models (Gemini 2.5 Flash / Gemma 4)"
    );

    companion object {
        fun fromId(id: Int): AiEngineMode = entries.find { it.id == id } ?: AUTO_FALLBACK
    }
}


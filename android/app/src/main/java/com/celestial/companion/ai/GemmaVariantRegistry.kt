package com.celestial.companion.ai

/**
 * Supported Gemma & multimodal model options for companion AI execution.
 */
enum class GemmaVariant(
    val id: Int,
    val displayName: String,
    val tag: String,
    val description: String,
    val isMultimodal: Boolean,
    val hasThinkingMode: Boolean,
    val modelId: String = "google/gemini-2.5-flash",
    val fallbackModelId: String = "google/gemini-2.5-flash"
) {
    GEMINI_FLASH(
        0,
        "Gemini 2.5 Flash",
        "G2.5-FL",
        "Ultra-fast high-concurrency Gemini multimodal vision & speech core (Default)",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemini-2.5-flash",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_E2B(
        1,
        "Gemma 4 (E2B) Edge",
        "G4-E2B",
        "Google Gemma 4 instruction-tuned multimodal core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-e2b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_31B(
        2,
        "Gemma 4 (31B) Multimodal",
        "G4-31B",
        "Google Gemma 4 31B instruction-tuned multimodal vision & reasoning core",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-31b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_4_26B(
        3,
        "Gemma 4 Think (26B A4B)",
        "G4-26B",
        "Google Gemma 4 26B Mixture-of-Experts with native CoT reasoning",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-4-26b-a4b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    GEMMA_3_27B(
        4,
        "Gemma 3 27B Vision",
        "G3-27B",
        "Google Gemma 3 27B multimodal instruction tuned model",
        isMultimodal = true,
        hasThinkingMode = true,
        modelId = "google/gemma-3-27b-it",
        fallbackModelId = "google/gemini-2.5-flash"
    ),
    PI5_OFFLOAD(
        5,
        "Pi 5 Local Core",
        "PI5",
        "Offload to Wearable Raspberry Pi 5 NVMe server over local Wi-Fi",
        isMultimodal = false,
        hasThinkingMode = false,
        modelId = "local",
        fallbackModelId = "google/gemini-2.5-flash"
    );

    /**
     * Alias for [tag] to match unified shortTag conventions across UI/Domain models.
     */
    val shortTag: String
        get() = tag

    companion object {
        val DEFAULT: GemmaVariant = GEMMA_4_E2B
        val E2B_MULTIMODAL: GemmaVariant = GEMMA_4_E2B
        val E4B_THINKING: GemmaVariant = GEMMA_4_26B
        val FLASH_NANO: GemmaVariant = GEMMA_3_27B

        fun fromId(id: Int): GemmaVariant? = entries.find { it.id == id }

        fun fromModelId(modelId: String): GemmaVariant? {
            val normalized = modelId.trim()
            if (normalized.isEmpty()) return null
            return entries.find { it.modelId.equals(normalized, ignoreCase = true) }
        }

        fun fromTag(tag: String): GemmaVariant? {
            val normalized = tag.trim()
            if (normalized.isEmpty()) return null
            return entries.find { it.tag.equals(normalized, ignoreCase = true) }
        }

        fun fromNameOrDefault(name: String?, default: GemmaVariant = DEFAULT): GemmaVariant {
            if (name.isNullOrBlank()) return default
            return entries.find { it.name.equals(name.trim(), ignoreCase = true) } ?: default
        }

        fun localModelFileName(variant: GemmaVariant): String {
            return when (variant) {
                E2B_MULTIMODAL -> "gemma-4-e2b-it.bin"
                E4B_THINKING -> "gemma-4-e4b-it.bin"
                else -> "generic-model.bin"
            }
        }
    }
}

/**
 * Pure metadata registry and lookup helpers for [GemmaVariant].
 */
object GemmaVariantRegistry {
    val DEFAULT_VARIANT: GemmaVariant = GemmaVariant.DEFAULT

    fun allVariants(): List<GemmaVariant> = GemmaVariant.entries

    fun multimodalVariants(): List<GemmaVariant> = GemmaVariant.entries.filter { it.isMultimodal }

    fun findById(id: Int): GemmaVariant? = GemmaVariant.fromId(id)

    fun findByModelId(modelId: String): GemmaVariant? = GemmaVariant.fromModelId(modelId)

    fun findByTag(tag: String): GemmaVariant? = GemmaVariant.fromTag(tag)

    fun fromNameOrDefault(name: String?, default: GemmaVariant = DEFAULT_VARIANT): GemmaVariant =
        GemmaVariant.fromNameOrDefault(name, default)

    fun localModelFileName(variant: GemmaVariant): String =
        GemmaVariant.localModelFileName(variant)
}

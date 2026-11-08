package com.celestial.companion.ai

import android.graphics.Bitmap
import kotlinx.coroutines.flow.Flow

/**
 * Common contract for streaming AI inference engines across Cloud, Edge, and Dual routing.
 */
interface CelestialAiEngine {
    fun streamInference(
        prompt: String,
        image: Bitmap? = null,
        audioBytes: ByteArray? = null
    ): Flow<GemmaStreamChunk>
}


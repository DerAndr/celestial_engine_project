package com.celestial.companion.ai

/**
 * Task 9.4: End-to-End Speech-to-Speech (Omni / Mini-Omni2) Latency Harness.
 *
 * Models streaming 20 ms (`640-byte`) 16 kHz mono PCM uplink frames and tracks
 * glass-to-glass latency (`TTFA` = Time-To-First-Audio) alongside concurrent `[SPELL: ...]`
 * control token dispatch targeting `< 300 ms` on 16 GB RAM Android devices.
 */
data class OmniAudioFrame(
    val pcm16Bytes: ByteArray,
    val sampleRateHz: Int = 16000,
    val timestampMs: Long = System.currentTimeMillis()
) {
    val frameDurationMs: Int
        get() = ((pcm16Bytes.size / 2) * 1000) / sampleRateHz.coerceAtLeast(1)
}

data class OmniLatencyMetrics(
    val tMicReleaseMs: Long,
    val tFirstAudioChunkMs: Long,
    val tFirstSpellTagMs: Long? = null,
    val engineName: String = "Sequential SOTA (Moonshine + SmolLM2 + Kokoro)"
) {
    val glassToGlassLatencyMs: Long
        get() = (tFirstAudioChunkMs - tMicReleaseMs).coerceAtLeast(0L)

    val meetsSub300MsTarget: Boolean
        get() = glassToGlassLatencyMs in 1L..299L
}

class OmniAudioSession(
    val engineName: String = "Mini-Omni2 / Sequential SOTA Harness"
) {
    private val uplinkFrames = ArrayList<OmniAudioFrame>(64)
    private var tMicReleaseMs: Long = 0L
    private var tFirstAudioChunkMs: Long = 0L
    private var tFirstSpellTagMs: Long? = null

    fun appendUplinkFrame(frame: OmniAudioFrame) {
        uplinkFrames.add(frame)
    }

    fun markMicReleased(nowMs: Long = System.currentTimeMillis()) {
        tMicReleaseMs = nowMs
    }

    fun markFirstSpellTag(nowMs: Long = System.currentTimeMillis()) {
        if (tFirstSpellTagMs == null) tFirstSpellTagMs = nowMs
    }

    fun markFirstAudioChunk(nowMs: Long = System.currentTimeMillis()) {
        if (tFirstAudioChunkMs == 0L) tFirstAudioChunkMs = nowMs
    }

    fun totalUplinkBytes(): Int = uplinkFrames.sumOf { it.pcm16Bytes.size }

    fun snapshotMetrics(): OmniLatencyMetrics {
        return OmniLatencyMetrics(
            tMicReleaseMs = tMicReleaseMs,
            tFirstAudioChunkMs = if (tFirstAudioChunkMs >= tMicReleaseMs) tFirstAudioChunkMs else tMicReleaseMs,
            tFirstSpellTagMs = tFirstSpellTagMs,
            engineName = engineName
        )
    }
}

package com.celestial.companion.domain.model

data class StaffTelemetry(
    val isOnline: Boolean = false,
    val ipAddress: String = "0.0.0.0",
    val tickNumber: Long = 0,
    val state: StaffState = StaffState.IDLE,
    val mode: StaffMode = StaffMode.GUIDE,
    val volumePercent: Int = 30,
    val clockStyle: ClockStyle = ClockStyle.HOURGLASS,
    val activeLedAnim: LedAnimation = LedAnimation.RAINBOW,
    val lastSeenTimestamp: Long = 0,
    val lastRecognizedText: String = "",
    val lastAiResponseText: String = "",
    val roundTripTimeMs: Long = 0,
    val activeHost: String = "STANDALONE",
    val language: StaffLanguage = StaffLanguage.ENGLISH,
    val preferredHost: Int = 0
)

data class PhoneAiModel(
    val id: Int,
    val themeIdx: Int,
    val name: String,
    val tag: String
)

sealed class StaffCommand {
    data class SetMode(val mode: StaffMode) : StaffCommand()
    data class SetVolume(val volumePercent: Int) : StaffCommand()
    data class SetLedAnimation(val animation: LedAnimation) : StaffCommand()
    data class SetClockStyle(val style: ClockStyle) : StaffCommand()
    data class SetLanguage(val language: StaffLanguage) : StaffCommand()
    data class SetHost(val hostType: Int) : StaffCommand()
    data class SyncTime(val epochSeconds: Long, val millis: Int) : StaffCommand()
    data class PlaySfx(val sfxId: Int) : StaffCommand()
    data class PlayChiptune(val trackId: Int) : StaffCommand()
    data class PlayVoice(val phraseId: Int) : StaffCommand()
    data class BroadcastCatalog(val models: List<PhoneAiModel> = emptyList()) : StaffCommand()
    object StopAudio : StaffCommand()
}

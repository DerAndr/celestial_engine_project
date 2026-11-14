package com.celestial.companion.ui.screens.dashboard

import android.graphics.Bitmap
import com.celestial.companion.ai.AiEngineMode
import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.ai.GemmaVariant
import com.celestial.companion.domain.model.ClockStyle
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.OraclePersona
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffTelemetry
import com.celestial.companion.domain.model.ThematicPack
import com.celestial.companion.domain.repository.ConnectionMode
import java.util.UUID

data class OracleChatMessage(
    val id: String = UUID.randomUUID().toString(),
    val isUser: Boolean,
    val text: String,
    val image: Bitmap? = null,
    val thoughtTrace: String = "",
    val modalityUsed: String = "TEXT",
    val latencyMs: Long = 0,
    val tokensPerSec: Float = 0f,
    val timestamp: Long = System.currentTimeMillis(),
    val isStreaming: Boolean = false
)

/**
 * Unidirectional MVI UI State snapshot for the Companion Dashboard & Oracle Console.
 */
data class StaffUiState(
    val telemetry: StaffTelemetry = StaffTelemetry(),
    val connectionMode: ConnectionMode = ConnectionMode.PHONE_BRAIN_SERVER,
    val engineMode: AiEngineMode = AiEngineMode.AUTO_FALLBACK,
    val currentPersona: OraclePersona = OraclePersona.MACHINE_GOD,
    val currentLanguage: StaffLanguage = StaffLanguage.ENGLISH,
    val selectedModelVariant: GemmaVariant = GemmaVariant.GEMINI_FLASH,
    val processingPhase: AiProcessingPhase = AiProcessingPhase.IDLE,
    val isGenerating: Boolean = false,
    val activeSpell: SpellDefinition? = null
)

/**
 * Unidirectional MVI User Intents dispatched from Compose screens to DashboardViewModel.
 */
sealed interface StaffIntent {
    data class SelectMode(val mode: StaffMode) : StaffIntent
    data class SetVolume(val volumePercent: Int) : StaffIntent
    data class SetLedAnimation(val animation: LedAnimation) : StaffIntent
    data class SetClockStyle(val style: ClockStyle) : StaffIntent
    data class SetLanguage(val language: StaffLanguage) : StaffIntent
    data class SetPersona(val persona: OraclePersona) : StaffIntent
    data class SetEngineMode(val mode: AiEngineMode) : StaffIntent
    data class SetStaffHost(val hostType: Int) : StaffIntent
    data class CastSpell(val spell: SpellDefinition, val speakIncantation: Boolean? = null) : StaffIntent
    data class ActivateThematicPack(val pack: ThematicPack) : StaffIntent
    data class SubmitOracleQuery(val prompt: String, val image: Bitmap? = null) : StaffIntent
    data object CancelGeneration : StaffIntent
    data object StopAudio : StaffIntent
    data object SyncSystemTime : StaffIntent
}

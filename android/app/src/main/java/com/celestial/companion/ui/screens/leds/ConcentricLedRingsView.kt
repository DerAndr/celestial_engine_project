package com.celestial.companion.ui.screens.leds

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.StaffState
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import java.util.Calendar
import kotlin.math.*
import kotlin.random.Random

/**
 * AI Neural Model LED Themes mirroring firmware/src/leds.c:
 * - r1, g1, b1: Primary inner ring glow
 * - r2, g2, b2: Secondary middle ring accent
 * - cr, cg, cb: Center focal core highlight
 */
data class LlmLedTheme(
    val r1: Int, val g1: Int, val b1: Int,
    val r2: Int, val g2: Int, val b2: Int,
    val cr: Int, val cg: Int, val cb: Int
)

val LLM_THEMES = listOf(
    // 0: DEEP THOUGHT (DeepSeek V3) - Cosmic Deep Ocean Cyan & Diamond Blue
    LlmLedTheme(0, 170, 255, 15, 70, 240, 180, 245, 255),
    // 1: EDDIE (Ministral 8B) - Sunny Solar Amber & Radiant Gold
    LlmLedTheme(255, 170, 15, 255, 100, 0, 255, 235, 90),
    // 2: GROK (Grok 4.5) - Cyberpunk Neon Magenta & Acid Violet / Mint Green
    LlmLedTheme(255, 15, 160, 150, 0, 255, 0, 255, 160),
    // 3: MARVIN (Llama 70B) - Melancholic Glacial Steel-Blue & Frosted Silver
    LlmLedTheme(90, 140, 215, 50, 75, 160, 190, 210, 245),
    // 4: LOCAL (Pi 5 Edge) - Cyber Emerald Matrix & Raspberry Ruby Core
    LlmLedTheme(10, 255, 90, 0, 200, 70, 255, 15, 80)
)

/**
 * Real-time Concentric WS2812B LED Rings visualizer for Celestial Engine Mk.V.
 * Accurately mirrors the 43 physical LEDs of the staff headpiece:
 * - Center Stone: 1 LED (Index 0)
 * - Inner Ring:   6 LEDs (Indices 1..6)
 * - Middle Ring:  12 LEDs (Indices 7..18)
 * - Outer Ring:   24 LEDs (Indices 19..42)
 *
 * Runs all 18 firmware animation modes with exact mathematical parity to Pico 2 W RP2350,
 * reactive to active state (LISTENING, PROCESSING, SPEAKING), voice volume, and neural model.
 */
@Composable
fun ConcentricLedRingsView(
    animation: LedAnimation,
    modifier: Modifier = Modifier,
    size: Dp = 240.dp,
    micLevelPct: Int = 0,
    aiModelId: Int = 0,
    state: StaffState = StaffState.IDLE,
    brightnessPct: Int = 100
) {
    // 33 Hz animation tick loop matching firmware 30ms interval
    var animTick by remember { mutableLongStateOf(0L) }

    LaunchedEffect(animation, state) {
        while (isActive) {
            delay(30)
            animTick++
        }
    }

    // Determine the actual rendered animation based on staff state
    val effectiveAnimation = when (state) {
        StaffState.LISTENING -> LedAnimation.VORTEX
        StaffState.PROCESSING -> LedAnimation.DATA_RECV
        StaffState.SPEAKING -> LedAnimation.EXPAND_SPEECH
        StaffState.IDLE -> animation
    }

    // Compute pixel RGB colors for all 43 LEDs
    val colors = remember(effectiveAnimation, animTick, micLevelPct, aiModelId, brightnessPct) {
        computeLedColors(effectiveAnimation, animTick, micLevelPct, aiModelId, brightnessPct)
    }

    Box(
        modifier = modifier.size(size)
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val cx = size.toPx() / 2f
            val cy = size.toPx() / 2f
            val maxR = (size.toPx() / 2f) * 0.88f

            val rInner = maxR * 0.33f
            val rMiddle = maxR * 0.64f
            val rOuter = maxR * 0.95f

            val ledDotRadius = (size.toPx() / 240f) * 6.5f

            // 1. Draw subtle background mounting guide rings
            val guideColor = Color(0x2200E5FF)
            drawCircle(color = guideColor, radius = rInner, center = Offset(cx, cy), style = Stroke(width = 1f))
            drawCircle(color = guideColor, radius = rMiddle, center = Offset(cx, cy), style = Stroke(width = 1f))
            drawCircle(color = guideColor, radius = rOuter, center = Offset(cx, cy), style = Stroke(width = 1f))

            // 2. Draw all 43 LEDs with lens body and glowing optical halo
            for (idx in 0 until 43) {
                val (x, y) = when {
                    idx == 0 -> Offset(cx, cy)
                    idx in 1..6 -> {
                        val angle = (idx - 1) * (2f * PI.toFloat() / 6f) - (PI.toFloat() / 2f)
                        Offset(cx + rInner * cos(angle), cy + rInner * sin(angle))
                    }
                    idx in 7..18 -> {
                        val angle = (idx - 7) * (2f * PI.toFloat() / 12f) - (PI.toFloat() / 2f)
                        Offset(cx + rMiddle * cos(angle), cy + rMiddle * sin(angle))
                    }
                    else -> { // 19..42
                        val angle = (idx - 19) * (2f * PI.toFloat() / 24f) - (PI.toFloat() / 2f)
                        Offset(cx + rOuter * cos(angle), cy + rOuter * sin(angle))
                    }
                }

                val ledColor = colors.getOrElse(idx) { Color.Black }
                val haloRadius = if (idx == 0) ledDotRadius * 2.8f else ledDotRadius * 2.2f
                val dotR = if (idx == 0) ledDotRadius * 1.35f else ledDotRadius

                // Soft outer optical halo
                if (ledColor != Color.Black) {
                    drawCircle(
                        brush = Brush.radialGradient(
                            colors = listOf(ledColor.copy(alpha = 0.55f), Color.Transparent),
                            center = Offset(x, y),
                            radius = haloRadius
                        ),
                        radius = haloRadius,
                        center = Offset(x, y)
                    )
                }

                // LED lens body
                drawCircle(
                    color = ledColor,
                    radius = dotR,
                    center = Offset(x, y)
                )

                // Bright inner specular core highlight
                val coreAlpha = if (ledColor == Color.Black) 0.05f else 0.65f
                drawCircle(
                    color = Color.White.copy(alpha = coreAlpha),
                    radius = dotR * 0.35f,
                    center = Offset(x - dotR * 0.2f, y - dotR * 0.2f)
                )
            }
        }
    }
}

/**
 * Computes exact RGB colors for all 43 concentric LEDs matching firmware/src/leds.c.
 */
private fun computeLedColors(
    animation: LedAnimation,
    tick: Long,
    micLevel: Int,
    aiModelId: Int,
    brightnessPct: Int
): Array<Color> {
    val arr = Array(43) { Color.Black }
    val brightnessFactor = (brightnessPct.coerceIn(10, 100) / 100f)
    val theme = LLM_THEMES.getOrElse(aiModelId) { LLM_THEMES[0] }

    fun rgb(r: Int, g: Int, b: Int, alpha: Float = 1f): Color {
        val rClamped = (r.coerceIn(0, 255) * brightnessFactor).toInt().coerceIn(0, 255)
        val gClamped = (g.coerceIn(0, 255) * brightnessFactor).toInt().coerceIn(0, 255)
        val bClamped = (b.coerceIn(0, 255) * brightnessFactor).toInt().coerceIn(0, 255)
        return Color(rClamped, gClamped, bClamped).copy(alpha = alpha.coerceIn(0f, 1f))
    }

    when (animation) {
        LedAnimation.BREATHE -> { // 0. Cosmic Breathe
            val angle = (tick % 120) * 2f * PI.toFloat() / 120f
            val breath = 0.2f + 0.8f * ((sin(angle) + 1f) * 0.5f)
            val baseHue = (tick * 0.8f) % 360f
            val color = Color.hsv(baseHue, 0.85f, (breath * brightnessFactor).coerceIn(0f, 1f))
            for (i in 0 until 43) arr[i] = color
        }

        LedAnimation.VORTEX -> { // 1. Galaxy Vortex (also used for STATE_LISTENING)
            // Voice-reactive suction spiral into center RED recording core
            val voiceGain = (micLevel.toFloat() / 100f).coerceIn(0f, 1f)

            // Center LED 0: Pulsating RED recording indicator with vocal peak flare
            val cWave = (sin(tick * 0.20f) + 1f) * 0.5f
            val cr = (160 + 95f * (cWave * 0.4f + voiceGain * 0.6f)).toInt()
            val cg = (theme.cg * 0.30f * voiceGain).toInt()
            val cb = (theme.cb * 0.30f * voiceGain).toInt()
            arr[0] = rgb(cr, cg, cb)

            // Dynamic rotation speed accelerating with voice
            val rotSpeed = 0.08f + 0.14f * voiceGain
            val rot = tick * rotSpeed

            // Ring 0 (1..6): Radius 1.0, Model primary
            for (i in 1..6) {
                val theta = (i - 1) * (2f * PI.toFloat() / 6f)
                val diff = theta - rot
                val armProx = cos(2f * diff)
                val norm = (armProx + 1f) * 0.5f
                val pwr = norm * norm * norm
                val r = (theme.r1 * 0.08f + pwr * (theme.r1 * (0.75f + 0.25f * voiceGain))).toInt()
                val g = (theme.g1 * 0.08f + pwr * (theme.g1 * (0.75f + 0.25f * voiceGain))).toInt()
                val b = (theme.b1 * 0.08f + pwr * (theme.b1 * (0.75f + 0.25f * voiceGain))).toInt()
                arr[i] = rgb(r, g, b)
            }

            // Ring 1 (7..18): Radius 2.0, twist = 0.80 rad, Model accent
            for (i in 7..18) {
                val theta = (i - 7) * (2f * PI.toFloat() / 12f)
                val diff = theta - 0.80f - rot
                val armProx = cos(2f * diff)
                val norm = (armProx + 1f) * 0.5f
                val pwr = norm * norm * norm
                val r = (theme.r2 * 0.08f + pwr * (theme.r2 * (0.75f + 0.25f * voiceGain))).toInt()
                val g = (theme.g2 * 0.08f + pwr * (theme.g2 * (0.75f + 0.25f * voiceGain))).toInt()
                val b = (theme.b2 * 0.08f + pwr * (theme.b2 * (0.75f + 0.25f * voiceGain))).toInt()
                arr[i] = rgb(r, g, b)
            }

            // Ring 2 (19..42): Radius 3.2, twist = 1.65 rad, Harmonic blend
            val brR = (theme.r1 + theme.r2) * 0.5f
            val brG = (theme.g1 + theme.g2) * 0.5f
            val brB = (theme.b1 + theme.b2) * 0.5f
            for (i in 19..42) {
                val theta = (i - 19) * (2f * PI.toFloat() / 24f)
                val diff = theta - 1.65f - rot
                val armProx = cos(2f * diff)
                val norm = (armProx + 1f) * 0.5f
                val pwr = norm * norm * norm
                val r = (brR * 0.05f + pwr * brR).toInt()
                val g = (brG * 0.05f + pwr * brG).toInt()
                val b = (brB * 0.05f + pwr * brB).toInt()
                arr[i] = rgb(r, g, b)
            }
        }

        LedAnimation.SNAKE -> { // 2. Cosmic Snake
            val head = (tick % 43).toInt()
            for (i in 0 until 43) {
                val dist = (head - i + 43) % 43
                if (dist < 8) {
                    val fade = 1f - (dist / 8f)
                    val h = (140f + dist * 15f) % 360f // Emerald to cyan
                    arr[i] = Color.hsv(h, 0.9f, (fade * brightnessFactor).coerceIn(0f, 1f))
                } else {
                    arr[i] = rgb(3, 7, 20)
                }
            }
        }

        LedAnimation.RAINBOW -> { // 3. Rainbow Wave
            for (i in 0 until 43) {
                val (rFactor, relAngle) = when {
                    i == 0 -> Pair(0f, 0f)
                    i in 1..6 -> Pair(40f, (i - 1) * (360f / 6f))
                    i in 7..18 -> Pair(80f, (i - 7) * (360f / 12f))
                    else -> Pair(120f, (i - 19) * (360f / 24f))
                }
                val hue = (relAngle + rFactor + tick * 4f) % 360f
                arr[i] = Color.hsv(hue, 0.95f, brightnessFactor.coerceIn(0f, 1f))
            }
        }

        LedAnimation.PULSAR -> { // 4. Pulsar Shockwave
            val cycle = (tick % 50).toInt()
            for (i in 0 until 43) arr[i] = rgb(2, 4, 10)
            when (cycle) {
                in 0..8 -> {
                    val p = 1f - (cycle / 8f)
                    arr[0] = rgb((255 * p).toInt(), (255 * p).toInt(), (255 * p).toInt())
                }
                in 8..18 -> {
                    val p = 1f - ((cycle - 8) / 10f)
                    for (i in 1..6) arr[i] = rgb(0, (229 * p).toInt(), (255 * p).toInt())
                }
                in 18..32 -> {
                    val p = 1f - ((cycle - 18) / 14f)
                    for (i in 7..18) arr[i] = rgb((138 * p).toInt(), (43 * p).toInt(), (226 * p).toInt())
                }
                in 30..48 -> {
                    val p = 1f - ((cycle - 30) / 18f)
                    for (i in 19..42) arr[i] = rgb((255 * p).toInt(), 0, (127 * p).toInt())
                }
            }
        }

        LedAnimation.PLASMA -> { // 5. Solar Plasma
            val t = tick * 0.06f
            for (i in 0 until 43) {
                val angle = (i * 360f / 43f) * (PI.toFloat() / 180f)
                val v1 = sin(angle * 3f + t)
                val v2 = cos(angle * 2f - t * 0.8f)
                val plasma = ((v1 + v2 + 2f) / 4f).coerceIn(0f, 1f)
                val hue = 15f + plasma * 45f // Red (15) to Yellow (60)
                arr[i] = Color.hsv(hue, 1f, ((0.4f + 0.6f * plasma) * brightnessFactor).coerceIn(0f, 1f))
            }
        }

        LedAnimation.STARS -> { // 6. Starry Night
            val rnd = Random(tick / 4)
            for (i in 0 until 43) {
                val isTwinkle = rnd.nextInt(6) == 0
                if (isTwinkle) {
                    val hue = rnd.nextFloat() * 60f + 180f // Cyan to Sapphire
                    arr[i] = Color.hsv(hue, 0.4f, ((0.7f + 0.3f * rnd.nextFloat()) * brightnessFactor).coerceIn(0f, 1f))
                } else {
                    arr[i] = rgb(6, 11, 28)
                }
            }
        }

        LedAnimation.VOICE -> { // 7. Voice Halo
            val level = if (micLevel > 0) micLevel else ((sin(tick * 0.15f) + 1f) * 45f).toInt()
            arr[0] = rgb(255, 215, 0)
            val innerActive = level > 15
            val midCount = ((level / 100f) * 12).toInt().coerceIn(0, 12)
            val outCount = ((level / 100f) * 24).toInt().coerceIn(0, 24)

            for (i in 1..6) arr[i] = if (innerActive) rgb(0, 229, 255) else rgb(3, 16, 30)
            for (i in 7..18) {
                arr[i] = if ((i - 7) < midCount) rgb(0, 230, 118) else rgb(2, 20, 10)
            }
            for (i in 19..42) {
                arr[i] = if ((i - 19) < outCount) rgb(255, 82, 82) else rgb(20, 4, 4)
            }
        }

        LedAnimation.SOLAR -> { // 8. Solar Orrery
            for (i in 0 until 43) arr[i] = rgb(4, 6, 18)
            arr[0] = rgb(255, 215, 0) // Sol (Sun)
            // Mercury/Venus on inner ring
            val innerPos = ((tick / 3) % 6).toInt() + 1
            arr[innerPos] = rgb(255, 152, 0)
            // Earth on middle ring
            val midPos = ((tick / 7) % 12).toInt() + 7
            arr[midPos] = rgb(0, 229, 255)
            // Mars & Jupiter on outer ring
            val outPos1 = ((tick / 11) % 24).toInt() + 19
            val outPos2 = (((tick / 17) + 8) % 24).toInt() + 19
            arr[outPos1] = rgb(255, 61, 0)
            arr[outPos2] = rgb(224, 64, 251)
        }

        LedAnimation.DATA_RECV -> { // 9. Data Reception (also used for STATE_PROCESSING)
            if (aiModelId == 4) { // Special Quantum Matrix animation for Pi 5 Edge
                val coreBreathe = 0.5f + 0.5f * sin(tick * 0.45f)
                arr[0] = rgb((255 * coreBreathe).toInt(), (15 * coreBreathe).toInt(), (80 * coreBreathe).toInt())

                val r0Head = 1 + ((tick * 2) % 6).toInt()
                for (i in 1..6) {
                    arr[i] = if (i == r0Head) rgb(10, 255, 90) else rgb(0, 30, 15)
                }

                val r1Head = 7 + ((12 - (tick % 12)) % 12).toInt()
                for (i in 7..18) {
                    arr[i] = if (i == r1Head) rgb(0, 230, 180) else rgb(0, 25, 20)
                }

                val r2Head1 = 19 + ((tick * 3) % 24).toInt()
                val r2Head2 = 19 + ((tick * 3 + 12) % 24).toInt()
                for (i in 19..42) {
                    arr[i] = if (i == r2Head1 || i == r2Head2) rgb(10, 255, 90) else rgb(2, 12, 6)
                }
            } else {
                // Concentric data reception radar ripples cascading inward
                val phase = (tick % 18).toInt()
                for (i in 0 until 43) arr[i] = rgb(2, 8, 16)

                // Inward cascade: Outer -> Middle -> Inner -> Center
                when (phase) {
                    in 0..5 -> {
                        val d = phase / 5f
                        val factor = 1f - d * 0.7f
                        for (i in 19..42) arr[i] = rgb((theme.r2 * factor).toInt(), (theme.g2 * factor).toInt(), (theme.b2 * factor).toInt())
                    }
                    in 4..9 -> {
                        val d = (phase - 4) / 5f
                        val factor = 1f - d * 0.7f
                        for (i in 7..18) arr[i] = rgb((theme.r1 * factor).toInt(), (theme.g1 * factor).toInt(), (theme.b1 * factor).toInt())
                    }
                    in 8..13 -> {
                        val d = (phase - 8) / 5f
                        val factor = 1f - d * 0.7f
                        for (i in 1..6) arr[i] = rgb((theme.cr * factor).toInt(), (theme.cg * factor).toInt(), (theme.cb * factor).toInt())
                    }
                    in 12..17 -> {
                        val d = (phase - 12) / 5f
                        val factor = 1f - d * 0.5f
                        arr[0] = rgb((theme.cr * factor).toInt(), (theme.cg * factor).toInt(), (theme.cb * factor).toInt())
                    }
                }
            }
        }

        LedAnimation.EXPAND_SPEECH -> { // 10. Sonic Expansion (also used for STATE_SPEAKING)
            val wavePhase = (tick % 16).toInt()

            // Center Core (0): Fires brightly at wave start
            val cWave = if (wavePhase <= 3) (1.0f - wavePhase * 0.25f) else 0.20f
            arr[0] = rgb((theme.cr * cWave).toInt(), (theme.cg * cWave).toInt(), (theme.cb * cWave).toInt())

            // Ring 0 (1..6): Peak at phase 2..6
            var r0Wave = 0.05f
            if (wavePhase in 2..6) {
                val d = (wavePhase - 2) / 4.0f
                r0Wave = 1.0f - d * 0.8f
            }
            for (i in 1..6) {
                arr[i] = rgb((theme.r1 * r0Wave + 6).toInt(), (theme.g1 * r0Wave + 6).toInt(), (theme.b1 * r0Wave + 6).toInt())
            }

            // Ring 1 (7..18): Peak at phase 5..9
            var r1Wave = 0.05f
            if (wavePhase in 5..9) {
                val d = (wavePhase - 5) / 4.0f
                r1Wave = 1.0f - d * 0.8f
            }
            for (i in 7..18) {
                arr[i] = rgb((theme.r2 * r1Wave + 4).toInt(), (theme.g2 * r1Wave + 4).toInt(), (theme.b2 * r1Wave + 4).toInt())
            }

            // Ring 2 (19..42): Peak at phase 8..13
            var r2Wave = 0.05f
            if (wavePhase in 8..13) {
                val d = (wavePhase - 8) / 5.0f
                r2Wave = 1.0f - d * 0.8f
            }
            for (i in 19..42) {
                arr[i] = rgb((theme.r1 * r2Wave + 2).toInt(), (theme.g1 * r2Wave + 2).toInt(), (theme.b1 * r2Wave + 2).toInt())
            }
        }

        LedAnimation.GALAXY -> { // 11. Milky Way Galaxy
            val t = tick * 0.05f
            arr[0] = rgb(255, 215, 0) // Sol Core
            for (i in 19..42) {
                val rel = i - 19
                val arm1 = ((rel + (t * 8f)) % 24).toInt()
                val arm2 = ((rel + 12 + (t * 8f)) % 24).toInt()
                if (arm1 in 0..3 || arm2 in 0..3) {
                    arr[i] = rgb(224, 64, 251)
                } else {
                    arr[i] = rgb(8, 3, 20)
                }
            }
            for (i in 7..18) {
                arr[i] = Color.hsv((260f + (i * 10f) + tick * 2f) % 360f, 0.8f, brightnessFactor.coerceIn(0f, 1f))
            }
        }

        LedAnimation.EARTH -> { // 12. Terra Planet
            arr[0] = rgb(0, 229, 255) // Core
            for (i in 1..6) arr[i] = rgb(0, 230, 118) // Continents
            for (i in 7..18) { // Oceans & Atmosphere
                val isLand = (i % 3 == 0)
                arr[i] = if (isLand) rgb(0, 200, 83) else rgb(2, 136, 209)
            }
            for (i in 19..42) { // Outer Aurora Ribbon
                val auroraHue = (120f + sin(tick * 0.08f + i * 0.3f) * 60f)
                arr[i] = Color.hsv(auroraHue, 0.9f, (0.8f * brightnessFactor).coerceIn(0f, 1f))
            }
        }

        LedAnimation.HALLOWEEN -> { // 13. Halloween Spook
            for (i in 0 until 43) {
                val isPumpkin = (i + (tick / 6)) % 2 == 0L
                arr[i] = if (isPumpkin) rgb(255, 102, 0) else rgb(142, 36, 170)
            }
        }

        LedAnimation.CLOCK_DIAL -> { // 14. Celestial Clock
            val cal = Calendar.getInstance()
            val sec = cal.get(Calendar.SECOND)
            val min = cal.get(Calendar.MINUTE)
            val hour = cal.get(Calendar.HOUR) % 12

            for (i in 0 until 43) arr[i] = rgb(3, 6, 18)
            // Center Stone: 1 Hz pulsing second
            val secPulse = (sin((System.currentTimeMillis() % 1000) * 2f * PI.toFloat() / 1000f) + 1f) * 0.5f
            arr[0] = rgb((0 * secPulse).toInt(), (229 * (0.3f + 0.7f * secPulse)).toInt(), (255 * (0.3f + 0.7f * secPulse)).toInt())

            // Middle ring: 12 Hours
            val hourIdx = 7 + hour
            if (hourIdx in 7..18) arr[hourIdx] = rgb(255, 215, 0)

            // Outer ring: Minutes (0..23 LEDs)
            val minIdx = 19 + ((min * 24) / 60)
            if (minIdx in 19..42) arr[minIdx] = rgb(0, 230, 118)

            // Seconds comet hand on outer ring
            val secIdx = 19 + ((sec * 24) / 60)
            if (secIdx in 19..42) arr[secIdx] = rgb(255, 82, 82)
        }

        LedAnimation.CLOCK_HOURGLASS -> { // 15. Aether Hourglass
            val cal = Calendar.getInstance()
            val sec = cal.get(Calendar.SECOND)
            val min = cal.get(Calendar.MINUTE)
            val hour = cal.get(Calendar.HOUR) % 12

            val fillHour = (hour / 12f * 6).toInt()
            val fillMin = (min / 60f * 12).toInt()
            val fillSec = (sec / 60f * 24).toInt()

            arr[0] = rgb(255, 215, 0)
            for (i in 1..6) arr[i] = if ((i - 1) < fillHour) rgb(255, 215, 0) else rgb(20, 13, 2)
            for (i in 7..18) arr[i] = if ((i - 7) < fillMin) rgb(0, 229, 255) else rgb(2, 18, 26)
            for (i in 19..42) arr[i] = if ((i - 19) < fillSec) rgb(0, 230, 118) else rgb(3, 20, 8)
        }

        LedAnimation.CLOCK_PRISMATIC -> { // 16. Chroma Prismatic
            val cal = Calendar.getInstance()
            val hour = cal.get(Calendar.HOUR) % 12
            val min = cal.get(Calendar.MINUTE)

            // 12 Spectral Hues on middle ring
            for (i in 7..18) {
                val hIdx = i - 7
                val hue = hIdx * (360f / 12f)
                val isCurrentHour = hIdx == hour
                arr[i] = Color.hsv(hue, 1f, if (isCurrentHour) 1f else 0.25f)
            }
            // Minutes fill on outer ring
            val minLeds = (min * 24 / 60)
            for (i in 19..42) {
                val rel = i - 19
                arr[i] = if (rel < minLeds) Color.hsv((min * 6f) % 360f, 0.9f, 0.9f) else rgb(4, 6, 18)
            }
            arr[0] = Color.hsv((tick * 2f) % 360f, 1f, 1f)
        }

        LedAnimation.CLOCK_ORRERY -> { // 17. Celestial Orrery
            for (i in 0 until 43) arr[i] = rgb(3, 7, 24)
            arr[0] = rgb(255, 215, 0) // Solar core
            val o1 = 1 + ((tick / 4) % 6).toInt()
            val o2 = 7 + ((tick / 8) % 12).toInt()
            val o3 = 19 + ((tick / 16) % 24).toInt()
            arr[o1] = rgb(0, 229, 255)
            arr[o2] = rgb(0, 230, 118)
            arr[o3] = rgb(255, 0, 127)
        }

        LedAnimation.SPIRAL_SNAKE -> { // 18. Ouroboros Snake
            val ouroborosPath = intArrayOf(
                19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
                7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                1, 2, 3, 4, 5, 6,
                0,
                6, 5, 4, 3, 2, 1,
                18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7
            )
            for (i in 0 until 43) arr[i] = rgb(2, 4, 14)
            val pathLen = ouroborosPath.size
            val snakeLen = 12
            val headPos = (tick % pathLen).toInt()

            for (seg in snakeLen - 1 downTo 0) {
                val pIdx = (headPos - seg + pathLen) % pathLen
                val ledIdx = ouroborosPath[pIdx]
                if (seg == 0) {
                    arr[ledIdx] = rgb(255, 245, 200)
                    if (ledIdx == 0) {
                        for (c in 1..6) arr[c] = rgb(240, 160, 40)
                    }
                } else {
                    val fade = 1.0f - (seg.toFloat() / snakeLen.toFloat())
                    val hue = ((tick * 5 + seg * 20) % 360).toFloat()
                    arr[ledIdx] = Color.hsv(hue, 0.9f, (fade * brightnessFactor).coerceIn(0f, 1f))
                }
            }
        }

        LedAnimation.RADIAL_WAVE -> { // 19. Radial Shockwave
            val t = tick * 0.15f
            val ringR = floatArrayOf(0.0f, 1.0f, 2.0f, 3.2f)
            for (i in 0 until 43) arr[i] = rgb(3, 6, 18)

            for (layer in 0..3) {
                val r = ringR[layer]
                var waveVal = 0.0f
                for (p in 0..1) {
                    val waveFront = (t + p * 2.2f) % 4.4f
                    val dist = abs(r - waveFront)
                    if (dist < 0.75f) {
                        val amp = 0.5f + 0.5f * cos(dist * PI.toFloat() / 0.75f)
                        val damping = 1.0f / (1.0f + 0.35f * r)
                        waveVal += amp * damping
                    }
                }
                if (waveVal > 1.0f) waveVal = 1.0f

                if (waveVal > 0.02f) {
                    val wr = (waveVal * (if (layer == 0) 255f else 40f)).toInt()
                    val wg = (waveVal * (if (layer <= 1) 240f else 160f)).toInt()
                    val wb = (waveVal * 255f).toInt()
                    val c = rgb(wr, wg, wb)
                    when (layer) {
                        0 -> arr[0] = c
                        1 -> for (i in 1..6) arr[i] = c
                        2 -> for (i in 7..18) arr[i] = c
                        3 -> for (i in 19..42) arr[i] = c
                    }
                }
            }
        }

        LedAnimation.COLLIDING_WAVES -> { // 20. Colliding Waves
            val thetaCw = (tick * 5f) % 360f
            val thetaCcw = (360f - thetaCw) % 360f

            val distNode0 = min(thetaCw, 360f - thetaCw)
            val distNode180 = abs(thetaCw - 180f)
            val minCollisionDist = min(distNode0, distNode180)

            var flashFactor = 0f
            if (minCollisionDist < 18f) {
                val f = 1f - (minCollisionDist / 18f)
                flashFactor = f * f
            }

            for (i in 0 until 43) arr[i] = rgb(2, 4, 12)
            if (flashFactor > 0.05f) {
                arr[0] = rgb((255f * flashFactor).toInt(), (255f * flashFactor).toInt(), (255f * flashFactor).toInt())
            }

            // Ring 0 (1..6)
            for (i in 1..6) {
                val angle = (i - 1) * 60f
                var dCw = abs(angle - thetaCw); if (dCw > 180f) dCw = 360f - dCw
                var dCcw = abs(angle - thetaCcw); if (dCcw > 180f) dCcw = 360f - dCcw
                val ampCw = if (dCw < 45f) (1f - dCw / 45f) else 0f
                val ampCcw = if (dCcw < 45f) (1f - dCcw / 45f) else 0f
                var r = (ampCcw * 255f).toInt()
                var g = (ampCw * 200f).toInt()
                var b = (ampCw * 255f + ampCcw * 120f).toInt()
                if (flashFactor > 0f && (i == 1 || i == 4)) {
                    val fl = (255f * flashFactor).toInt()
                    r = min(255, r + fl); g = min(255, g + fl); b = min(255, b + fl)
                }
                arr[i] = rgb(r, g, b)
            }

            // Ring 1 (7..18)
            for (i in 7..18) {
                val angle = (i - 7) * 30f
                var dCw = abs(angle - thetaCw); if (dCw > 180f) dCw = 360f - dCw
                var dCcw = abs(angle - thetaCcw); if (dCcw > 180f) dCcw = 360f - dCcw
                val ampCw = if (dCw < 40f) (1f - dCw / 40f) else 0f
                val ampCcw = if (dCcw < 40f) (1f - dCcw / 40f) else 0f
                var r = (ampCcw * 255f).toInt()
                var g = (ampCw * 220f).toInt()
                var b = (ampCw * 255f + ampCcw * 80f).toInt()
                if (flashFactor > 0f && (i == 7 || i == 13)) {
                    val fl = (255f * flashFactor).toInt()
                    r = min(255, r + fl); g = min(255, g + fl); b = min(255, b + fl)
                }
                arr[i] = rgb(r, g, b)
            }

            // Ring 2 (19..42)
            for (i in 19..42) {
                val angle = (i - 19) * 15f
                var dCw = abs(angle - thetaCw); if (dCw > 180f) dCw = 360f - dCw
                var dCcw = abs(angle - thetaCcw); if (dCcw > 180f) dCcw = 360f - dCcw
                val ampCw = if (dCw < 30f) (1f - dCw / 30f) else 0f
                val ampCcw = if (dCcw < 30f) (1f - dCcw / 30f) else 0f
                var r = (ampCcw * 255f).toInt()
                var g = (ampCw * 210f).toInt()
                var b = (ampCw * 255f + ampCcw * 100f).toInt()
                if (flashFactor > 0f) {
                    var nodeDist = min(abs(angle), abs(angle - 180f))
                    if (nodeDist > 180f) nodeDist = 360f - nodeDist
                    if (nodeDist < 20f) {
                        val fl = (flashFactor * (1f - nodeDist / 20f) * 255f).toInt()
                        r = min(255, r + fl); g = min(255, g + fl); b = min(255, b + fl)
                    }
                }
                arr[i] = rgb(r, g, b)
            }
        }
    }

    return arr
}

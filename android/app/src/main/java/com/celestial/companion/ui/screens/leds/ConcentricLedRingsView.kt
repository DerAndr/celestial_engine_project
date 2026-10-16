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
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import java.util.Calendar
import kotlin.math.*
import kotlin.random.Random

/**
 * Real-time Concentric WS2812B LED Rings visualizer for Celestial Engine Mk.V.
 * Accurately mirrors the 43 physical LEDs of the staff headpiece:
 * - Center Stone: 1 LED (Index 0)
 * - Inner Ring:   6 LEDs (Indices 1..6)
 * - Middle Ring:  12 LEDs (Indices 7..18)
 * - Outer Ring:   24 LEDs (Indices 19..42)
 *
 * Runs all 18 firmware animation modes in real-time at 33 Hz, synchronizing
 * seamlessly with the active animation selected on the Pico 2 W.
 */
@Composable
fun ConcentricLedRingsView(
    animation: LedAnimation,
    modifier: Modifier = Modifier,
    size: Dp = 240.dp,
    micLevelPct: Int = 0,
    brightnessPct: Int = 100
) {
    // 33 Hz animation tick loop matching firmware 30ms interval
    var animTick by remember { mutableLongStateOf(0L) }

    LaunchedEffect(animation) {
        while (isActive) {
            delay(30)
            animTick++
        }
    }

    // Compute pixel RGB colors for all 43 LEDs
    val colors = remember(animation, animTick, micLevelPct, brightnessPct) {
        computeLedColors(animation, animTick, micLevelPct, brightnessPct)
    }

    Box(
        modifier = modifier.size(size)
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val cx = size.toPx() / 2f
            val cy = size.toPx() / 2f
            val maxR = (size.toPx() / 2f) * 0.88f

            val rCenter = 0f
            val rInner = maxR * 0.33f
            val rMiddle = maxR * 0.64f
            val rOuter = maxR * 0.95f

            val ledDotRadius = (size.toPx() / 240f) * 6.5f

            // 1. Draw subtle background mounting guide rings
            val guideColor = Color(0x2200E5FF)
            drawCircle(color = guideColor, radius = rInner, center = Offset(cx, cy), style = Stroke(width = 1f))
            drawCircle(color = guideColor, radius = rMiddle, center = Offset(cx, cy), style = Stroke(width = 1f))
            drawCircle(color = guideColor, radius = rOuter, center = Offset(cx, cy), style = Stroke(width = 1f))

            // 2. Draw all 43 LEDs with lens body and glowing halo
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
 * Computes exact RGB colors for all 43 concentric LEDs matching the 18 firmware algorithms.
 */
private fun computeLedColors(
    animation: LedAnimation,
    tick: Long,
    micLevel: Int,
    brightnessPct: Int
): Array<Color> {
    val arr = Array(43) { Color.Black }
    val brightnessFactor = (brightnessPct.coerceIn(10, 100) / 100f)

    when (animation) {
        LedAnimation.BREATHE -> { // 0. Cosmic Breathe
            val angle = (tick % 120) * 2f * PI.toFloat() / 120f
            val breath = 0.2f + 0.8f * ((sin(angle) + 1f) * 0.5f)
            val baseHue = (tick * 0.8f) % 360f
            val color = Color.hsv(baseHue, 0.85f, breath * brightnessFactor)
            for (i in 0 until 43) arr[i] = color
        }

        LedAnimation.VORTEX -> { // 1. Galaxy Vortex
            val t = tick * 0.08f
            // Center Singularity: pulsing cyan/gold
            val centerPulse = (sin(t * 2f) + 1f) * 0.5f
            arr[0] = Color(0xFF00E5FF).copy(alpha = (0.5f + 0.5f * centerPulse) * brightnessFactor)

            // Inner ring (1..6)
            for (i in 1..6) {
                val rel = i - 1
                val angle = rel * (360f / 6f) - (t * 50f)
                val h = (180f + sin(Math.toRadians(angle.toDouble())).toFloat() * 60f) % 360f
                arr[i] = Color.hsv(h, 0.9f, brightnessFactor)
            }
            // Middle ring (7..18)
            for (i in 7..18) {
                val rel = i - 7
                val angle = rel * (360f / 12f) + (t * 35f)
                val h = (220f + cos(Math.toRadians(angle.toDouble())).toFloat() * 70f) % 360f
                arr[i] = Color.hsv(h, 0.95f, brightnessFactor)
            }
            // Outer ring (19..42)
            for (i in 19..42) {
                val rel = i - 19
                val angle = rel * (360f / 24f) - (t * 25f)
                val h = (260f + sin(Math.toRadians(angle.toDouble())).toFloat() * 80f) % 360f
                arr[i] = Color.hsv(h, 0.85f, brightnessFactor)
            }
        }

        LedAnimation.SNAKE -> { // 2. Cosmic Snake
            val head = (tick % 43).toInt()
            for (i in 0 until 43) {
                val dist = (head - i + 43) % 43
                if (dist < 8) {
                    val fade = 1f - (dist / 8f)
                    val h = (140f + dist * 15f) % 360f // Emerald to cyan
                    arr[i] = Color.hsv(h, 0.9f, fade * brightnessFactor)
                } else {
                    arr[i] = Color(0xFF030714)
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
                arr[i] = Color.hsv(hue, 0.95f, brightnessFactor)
            }
        }

        LedAnimation.PULSAR -> { // 4. Pulsar Shockwave
            val cycle = (tick % 50).toInt()
            for (i in 0 until 43) arr[i] = Color(0xFF02040A)
            when (cycle) {
                in 0..8 -> {
                    val p = 1f - (cycle / 8f)
                    arr[0] = Color(0xFFFFFFFF).copy(alpha = p * brightnessFactor)
                }
                in 8..18 -> {
                    val p = 1f - ((cycle - 8) / 10f)
                    for (i in 1..6) arr[i] = Color(0xFF00E5FF).copy(alpha = p * brightnessFactor)
                }
                in 18..32 -> {
                    val p = 1f - ((cycle - 18) / 14f)
                    for (i in 7..18) arr[i] = Color(0xFF8A2BE2).copy(alpha = p * brightnessFactor)
                }
                in 30..48 -> {
                    val p = 1f - ((cycle - 30) / 18f)
                    for (i in 19..42) arr[i] = Color(0xFFFF007F).copy(alpha = p * brightnessFactor)
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
                arr[i] = Color.hsv(hue, 1f, (0.4f + 0.6f * plasma) * brightnessFactor)
            }
        }

        LedAnimation.STARS -> { // 6. Starry Night
            val rnd = Random(tick / 4)
            for (i in 0 until 43) {
                val isTwinkle = rnd.nextInt(6) == 0
                if (isTwinkle) {
                    val hue = rnd.nextFloat() * 60f + 180f // Cyan to Sapphire
                    arr[i] = Color.hsv(hue, 0.4f, (0.7f + 0.3f * rnd.nextFloat()) * brightnessFactor)
                } else {
                    arr[i] = Color(0xFF060B1C)
                }
            }
        }

        LedAnimation.VOICE -> { // 7. Voice Halo
            val level = if (micLevel > 0) micLevel else ((sin(tick * 0.15f) + 1f) * 45f).toInt()
            arr[0] = Color(0xFFFFD700)
            val innerActive = level > 15
            val midCount = ((level / 100f) * 12).toInt().coerceIn(0, 12)
            val outCount = ((level / 100f) * 24).toInt().coerceIn(0, 24)

            for (i in 1..6) arr[i] = if (innerActive) Color(0xFF00E5FF) else Color(0xFF03101E)
            for (i in 7..18) {
                arr[i] = if ((i - 7) < midCount) Color(0xFF00E676) else Color(0xFF02140A)
            }
            for (i in 19..42) {
                arr[i] = if ((i - 19) < outCount) Color(0xFFFF5252) else Color(0xFF140404)
            }
        }

        LedAnimation.SOLAR -> { // 8. Solar Orrery
            for (i in 0 until 43) arr[i] = Color(0xFF040612)
            arr[0] = Color(0xFFFFD700) // Sol (Sun)
            // Mercury/Venus on inner ring
            val innerPos = ((tick / 3) % 6).toInt() + 1
            arr[innerPos] = Color(0xFFFF9800)
            // Earth on middle ring
            val midPos = ((tick / 7) % 12).toInt() + 7
            arr[midPos] = Color(0xFF00E5FF)
            // Mars/Jupiter on outer ring
            val outPos1 = ((tick / 11) % 24).toInt() + 19
            val outPos2 = (((tick / 17) + 8) % 24).toInt() + 19
            arr[outPos1] = Color(0xFFFF3D00)
            arr[outPos2] = Color(0xFFE040FB)
        }

        LedAnimation.DATA_RECV -> { // 9. Data Reception
            val sweep = ((tick * 2) % 24).toInt()
            for (i in 0 until 43) arr[i] = Color(0xFF020C06)
            arr[0] = Color(0xFF00E676)
            for (i in 19..42) {
                val rel = i - 19
                if (rel == sweep) arr[i] = Color(0xFF00FF88)
                else if ((sweep - rel + 24) % 24 < 5) {
                    val fade = 1f - ((sweep - rel + 24) % 24) / 5f
                    arr[i] = Color(0xFF00E676).copy(alpha = fade)
                }
            }
        }

        LedAnimation.EXPAND_SPEECH -> { // 10. Sonic Expansion
            val phase = (tick % 36).toInt()
            for (i in 0 until 43) arr[i] = Color(0xFF03050F)
            when (phase) {
                in 0..6 -> arr[0] = Color(0xFF00E5FF)
                in 6..14 -> for (i in 1..6) arr[i] = Color(0xFF00E5FF)
                in 14..24 -> for (i in 7..18) arr[i] = Color(0xFF8A2BE2)
                in 24..35 -> for (i in 19..42) arr[i] = Color(0xFFFFD700)
            }
        }

        LedAnimation.GALAXY -> { // 11. Milky Way Galaxy
            val t = tick * 0.05f
            arr[0] = Color(0xFFFFD700) // Sol Core
            for (i in 19..42) {
                val rel = i - 19
                val arm1 = ((rel + (t * 8f)) % 24).toInt()
                val arm2 = ((rel + 12 + (t * 8f)) % 24).toInt()
                if (arm1 in 0..3 || arm2 in 0..3) {
                    arr[i] = Color(0xFFE040FB)
                } else {
                    arr[i] = Color(0xFF080314)
                }
            }
            for (i in 7..18) {
                arr[i] = Color.hsv((260f + (i * 10f) + tick * 2f) % 360f, 0.8f, brightnessFactor)
            }
        }

        LedAnimation.EARTH -> { // 12. Terra Planet
            arr[0] = Color(0xFF00E5FF) // Core
            for (i in 1..6) arr[i] = Color(0xFF00E676) // Continents
            for (i in 7..18) { // Oceans & Atmosphere
                val isLand = (i % 3 == 0)
                arr[i] = if (isLand) Color(0xFF00C853) else Color(0xFF0288D1)
            }
            for (i in 19..42) { // Outer Aurora Ribbon
                val auroraHue = (120f + sin(tick * 0.08f + i * 0.3f) * 60f).toFloat()
                arr[i] = Color.hsv(auroraHue, 0.9f, 0.8f * brightnessFactor)
            }
        }

        LedAnimation.HALLOWEEN -> { // 13. Halloween Spook
            for (i in 0 until 43) {
                val isPumpkin = (i + (tick / 6)) % 2 == 0L
                arr[i] = if (isPumpkin) Color(0xFFFF6600) else Color(0xFF8E24AA)
            }
        }

        LedAnimation.CLOCK_DIAL -> { // 14. Celestial Clock
            val cal = Calendar.getInstance()
            val sec = cal.get(Calendar.SECOND)
            val min = cal.get(Calendar.MINUTE)
            val hour = cal.get(Calendar.HOUR) % 12

            for (i in 0 until 43) arr[i] = Color(0xFF030612)
            // Center Stone: 1 Hz pulsing second
            val secPulse = (sin((System.currentTimeMillis() % 1000) * 2f * PI.toFloat() / 1000f) + 1f) * 0.5f
            arr[0] = Color(0xFF00E5FF).copy(alpha = (0.3f + 0.7f * secPulse) * brightnessFactor)

            // Middle ring: 12 Hours
            val hourIdx = 7 + hour
            if (hourIdx in 7..18) arr[hourIdx] = Color(0xFFFFD700)

            // Outer ring: Minutes (0..23 LEDs)
            val minIdx = 19 + ((min * 24) / 60)
            if (minIdx in 19..42) arr[minIdx] = Color(0xFF00E676)

            // Seconds comet hand on outer ring
            val secIdx = 19 + ((sec * 24) / 60)
            if (secIdx in 19..42) arr[secIdx] = Color(0xFFFF5252)
        }

        LedAnimation.CLOCK_HOURGLASS -> { // 15. Aether Hourglass
            val cal = Calendar.getInstance()
            val sec = cal.get(Calendar.SECOND)
            val min = cal.get(Calendar.MINUTE)
            val hour = cal.get(Calendar.HOUR) % 12

            val fillHour = (hour / 12f * 6).toInt()
            val fillMin = (min / 60f * 12).toInt()
            val fillSec = (sec / 60f * 24).toInt()

            arr[0] = Color(0xFFFFD700)
            for (i in 1..6) arr[i] = if ((i - 1) < fillHour) Color(0xFFFFD700) else Color(0xFF140D02)
            for (i in 7..18) arr[i] = if ((i - 7) < fillMin) Color(0xFF00E5FF) else Color(0xFF02121A)
            for (i in 19..42) arr[i] = if ((i - 19) < fillSec) Color(0xFF00E676) else Color(0xFF031408)
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
                arr[i] = if (rel < minLeds) Color.hsv((min * 6f) % 360f, 0.9f, 0.9f) else Color(0xFF040612)
            }
            arr[0] = Color.hsv((tick * 2f) % 360f, 1f, 1f)
        }

        LedAnimation.CLOCK_ORRERY -> { // 17. Celestial Orrery
            for (i in 0 until 43) arr[i] = Color(0xFF030718)
            arr[0] = Color(0xFFFFD700) // Solar core
            val o1 = 1 + ((tick / 4) % 6).toInt()
            val o2 = 7 + ((tick / 8) % 12).toInt()
            val o3 = 19 + ((tick / 16) % 24).toInt()
            arr[o1] = Color(0xFF00E5FF)
            arr[o2] = Color(0xFF00E676)
            arr[o3] = Color(0xFFFF007F)
        }
    }

    return arr
}

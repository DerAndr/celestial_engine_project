package com.celestial.companion.ui.components

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.domain.model.ClockStyle
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.ui.theme.AetherCyan
import com.celestial.companion.ui.theme.EmeraldGreen
import com.celestial.companion.ui.theme.RunicGold
import com.celestial.companion.ui.theme.SpaceBlack
import java.text.SimpleDateFormat
import java.util.*
import kotlin.math.*

/**
 * Authentic 128x64 SH1106 I2C OLED HUD display mirror for Celestial Engine Mk.V.
 * Accurately reproduces the physical staff display mounted on the staff shaft:
 * - 2:1 Aspect ratio matching 128x64 OLED resolution
 * - Luminous Icy Cyan/White phosphor pixels
 * - Dynamic state-reactive screens (LISTENING voice VU, PROCESSING radar, SPEAKING equalizer)
 * - Mode-specific HUD screens (COSMOS 1x/1k/1m zoom, CHRONOS dial, AI CORE, GUIDE)
 */
@Composable
fun StaffOledScreenView(
    state: StaffState,
    mode: StaffMode,
    volumePercent: Int,
    clockStyle: ClockStyle,
    activeHost: String,
    aiModelId: Int,
    micLevelPercent: Int,
    cosmosZoom: Int,
    isOnline: Boolean,
    modifier: Modifier = Modifier,
    width: Dp = 260.dp
) {
    val height = width / 2f // 128x64 aspect ratio (2:1)
    val phosphorColor = Color(0xFF00E5FF)
    val phosphorDim = Color(0x3300E5FF)
    val oledBackground = Color(0xFF040810)

    // Periodic tick for scanlines, blinking indicators, and radar sweeps
    val infiniteTransition = rememberInfiniteTransition(label = "OledAnim")
    val sweepAngle by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(1800, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "Sweep"
    )
    val blinkPhase by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(600, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "Blink"
    )

    // Bezel Frame
    Box(
        modifier = modifier
            .width(width)
            .height(height)
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0xFF0D121F))
            .border(2.dp, Color(0xFF1E293B), RoundedCornerShape(8.dp))
            .padding(4.dp)
    ) {
        // Inner OLED Glass Surface
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(RoundedCornerShape(4.dp))
                .background(oledBackground)
                .border(1.dp, Color(0x3300E5FF), RoundedCornerShape(4.dp))
                .padding(horizontal = 8.dp, vertical = 6.dp)
        ) {
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                // 1. OLED Status Header (Mode Name, Host, Volume)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(6.dp)
                                .clip(RoundedCornerShape(1.dp))
                                .background(if (isOnline) phosphorColor else Color(0xFFFF5252))
                        )
                        Text(
                            text = mode.displayName,
                            color = phosphorColor,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // Active Host Badge
                    Text(
                        text = if (activeHost.contains("PI")) "PI 5" else if (activeHost.contains("PHONE")) "PHONE" else "AUTO",
                        color = RunicGold,
                        fontSize = 9.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    // Volume Badge
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.VolumeUp,
                            contentDescription = null,
                            tint = phosphorColor,
                            modifier = Modifier.size(10.dp)
                        )
                        Text(
                            text = "$volumePercent%",
                            color = phosphorColor,
                            fontSize = 9.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                // Subtle OLED Horizontal Divider
                Canvas(modifier = Modifier.fillMaxWidth().height(1.dp)) {
                    drawLine(
                        color = phosphorDim,
                        start = Offset(0f, 0f),
                        end = Offset(size.width, 0f),
                        strokeWidth = 1f
                    )
                }

                // 2. Dynamic Main Screen Content
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    contentAlignment = Alignment.Center
                ) {
                    when (state) {
                        StaffState.LISTENING -> {
                            // Voice Recording VU HUD
                            Column(
                                horizontalAlignment = Alignment.CenterHorizontally,
                                verticalArrangement = Arrangement.Center
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    val recAlpha = if (blinkPhase > 0.5f) 1f else 0.2f
                                    Box(
                                        modifier = Modifier
                                            .size(8.dp)
                                            .clip(RoundedCornerShape(2.dp))
                                            .background(Color.Red.copy(alpha = recAlpha))
                                    )
                                    Text(
                                        text = "RECORDING VOICE",
                                        color = Color.White,
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.Bold,
                                        fontFamily = FontFamily.Monospace
                                    )
                                }

                                Spacer(modifier = Modifier.height(4.dp))

                                // Live Waveform / VU Meter
                                Canvas(
                                    modifier = Modifier
                                        .fillMaxWidth(0.9f)
                                        .height(18.dp)
                                ) {
                                    val barCount = 18
                                    val barWidth = size.width / (barCount * 1.5f)
                                    val activeLevel = micLevelPercent.coerceIn(5, 100) / 100f
                                    for (i in 0 until barCount) {
                                        val x = i * (barWidth * 1.5f)
                                        val sinH = sin((i * 0.4f) + (sweepAngle * 0.05f))
                                        val hFactor = ((sinH + 1f) * 0.5f * activeLevel).coerceIn(0.1f, 1f)
                                        val barH = size.height * hFactor
                                        drawRect(
                                            color = if (i > barCount * 0.75f) Color(0xFFFF5252) else phosphorColor,
                                            topLeft = Offset(x, size.height - barH),
                                            size = Size(barWidth, barH)
                                        )
                                    }
                                }
                            }
                        }

                        StaffState.PROCESSING -> {
                            // Sub-etha Neural Radar Sweep
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(10.dp)
                            ) {
                                Canvas(modifier = Modifier.size(24.dp)) {
                                    val c = Offset(size.width / 2f, size.height / 2f)
                                    val r = size.width / 2f
                                    drawCircle(color = phosphorDim, radius = r, style = Stroke(1f))
                                    drawCircle(color = phosphorDim, radius = r * 0.5f, style = Stroke(1f))
                                    val rad = Math.toRadians(sweepAngle.toDouble())
                                    val px = c.x + r * cos(rad).toFloat()
                                    val py = c.y + r * sin(rad).toFloat()
                                    drawLine(color = phosphorColor, start = c, end = Offset(px, py), strokeWidth = 1.5f)
                                }

                                Column {
                                    Text(
                                        text = "THINKING...",
                                        color = phosphorColor,
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.Bold,
                                        fontFamily = FontFamily.Monospace
                                    )
                                    val modelLabel = when (aiModelId) {
                                        0 -> "DEEP THOUGHT (DEEPSEEK)"
                                        1 -> "EDDIE (MINISTRAL)"
                                        2 -> "GROK (CYBERPUNK)"
                                        3 -> "MARVIN (LLAMA)"
                                        4 -> "LOCAL PI 5 EDGE"
                                        else -> "NEURAL ORACLE"
                                    }
                                    Text(
                                        text = modelLabel,
                                        color = RunicGold,
                                        fontSize = 8.sp,
                                        fontFamily = FontFamily.Monospace
                                    )
                                }
                            }
                        }

                        StaffState.SPEAKING -> {
                            // Equalizer Audio Spectrum
                            Column(
                                horizontalAlignment = Alignment.CenterHorizontally,
                                verticalArrangement = Arrangement.Center
                            ) {
                                Text(
                                    text = "ORACLE SPEAKING",
                                    color = EmeraldGreen,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )

                                Spacer(modifier = Modifier.height(4.dp))

                                Canvas(
                                    modifier = Modifier
                                        .fillMaxWidth(0.85f)
                                        .height(16.dp)
                                ) {
                                    val bars = 16
                                    val bw = size.width / (bars * 1.4f)
                                    for (i in 0 until bars) {
                                        val x = i * (bw * 1.4f)
                                        val phase = sin(sweepAngle * 0.1f + i * 0.6f)
                                        val bh = (size.height * (0.3f + 0.7f * ((phase + 1f) * 0.5f))).coerceIn(2f, size.height)
                                        drawRect(
                                            color = EmeraldGreen,
                                            topLeft = Offset(x, size.height - bh),
                                            size = Size(bw, bh)
                                        )
                                    }
                                }
                            }
                        }

                        StaffState.IDLE -> {
                            when (mode) {
                                StaffMode.COSMOS -> {
                                    val zoomName = when (cosmosZoom) {
                                        0 -> "MILKY WAY GALAXY"
                                        1 -> "SOLAR ORRERY"
                                        else -> "TERRA PLANET"
                                    }
                                    val zoomTag = when (cosmosZoom) {
                                        0 -> "ZOOM: 1x (GALAXY)"
                                        1 -> "ZOOM: 1k (SOLAR)"
                                        else -> "ZOOM: 1m (TERRA)"
                                    }
                                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                        Text(
                                            text = zoomName,
                                            color = phosphorColor,
                                            fontSize = 11.sp,
                                            fontWeight = FontWeight.Bold,
                                            fontFamily = FontFamily.Monospace
                                        )
                                        Text(
                                            text = zoomTag,
                                            color = RunicGold,
                                            fontSize = 9.sp,
                                            fontFamily = FontFamily.Monospace
                                        )
                                    }
                                }

                                StaffMode.CHRONOS -> {
                                    val timeFmt = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
                                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                        Text(
                                            text = timeFmt,
                                            color = phosphorColor,
                                            fontSize = 14.sp,
                                            fontWeight = FontWeight.Bold,
                                            fontFamily = FontFamily.Monospace
                                        )
                                        Text(
                                            text = clockStyle.title,
                                            color = RunicGold,
                                            fontSize = 8.sp,
                                            fontFamily = FontFamily.Monospace
                                        )
                                    }
                                }

                                StaffMode.AI_MODEL -> {
                                    val modelName = when (aiModelId) {
                                        0 -> "DEEP THOUGHT (V3)"
                                        1 -> "EDDIE (MINISTRAL)"
                                        2 -> "GROK (GROK 4.5)"
                                        3 -> "MARVIN (LLAMA 70B)"
                                        4 -> "PI 5 LOCAL EDGE"
                                        else -> "NEURAL ENGINE"
                                    }
                                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                        Text(
                                            text = "ACTIVE AI CORE",
                                            color = phosphorColor,
                                            fontSize = 9.sp,
                                            fontFamily = FontFamily.Monospace
                                        )
                                        Text(
                                            text = modelName,
                                            color = RunicGold,
                                            fontSize = 11.sp,
                                            fontWeight = FontWeight.Bold,
                                            fontFamily = FontFamily.Monospace
                                        )
                                    }
                                }

                                else -> {
                                    // Default Status HUD
                                    Column(
                                        modifier = Modifier.fillMaxWidth(),
                                        verticalArrangement = Arrangement.spacedBy(2.dp)
                                    ) {
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween
                                        ) {
                                            Text(
                                                text = "RP2350 M33 DUAL",
                                                color = phosphorColor,
                                                fontSize = 9.sp,
                                                fontFamily = FontFamily.Monospace
                                            )
                                            Text(
                                                text = "33 Hz TICK",
                                                color = phosphorColor,
                                                fontSize = 9.sp,
                                                fontFamily = FontFamily.Monospace
                                            )
                                        }
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween
                                        ) {
                                            Text(
                                                text = "43 LED WS2812B",
                                                color = RunicGold,
                                                fontSize = 9.sp,
                                                fontFamily = FontFamily.Monospace
                                            )
                                            Text(
                                                text = "I2S MAX98357A",
                                                color = EmeraldGreen,
                                                fontSize = 9.sp,
                                                fontFamily = FontFamily.Monospace
                                            )
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 3. Bottom Marquee Status Ticker
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "UDP 5005 LINK",
                        color = phosphorDim,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = "SH1106 128x64",
                        color = phosphorDim,
                        fontSize = 8.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }
        }
    }
}

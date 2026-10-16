package com.celestial.companion.ui.screens.cosmos

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ZoomIn
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.ui.theme.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlin.math.*

enum class CosmosZoomLevel(val title: String, val scaleLabel: String) {
    GALAXY("MILKY WAY GALAXY", "1x (100,000 LY)"),
    SOLAR("SOLAR SYSTEM ORRERY", "1,000x (40 AU)"),
    TERRA("TERRA PLANET GLOBE", "1,000,000x (12,742 KM)")
}

/**
 * Unified Celestial Cosmos visualizer: combines Galaxy Star Map, Solar Orrery,
 * and Terra Planetary Zoom into a single interactive viewport.
 * Clicking anywhere on the canvas seamlessly zooms in and out between scales.
 */
@Composable
fun CosmosZoomView(
    modifier: Modifier = Modifier,
    initialZoom: CosmosZoomLevel = CosmosZoomLevel.GALAXY,
    onZoomChange: ((CosmosZoomLevel) -> Unit)? = null
) {
    var zoomLevel by remember { mutableStateOf(initialZoom) }
    var tick by remember { mutableLongStateOf(0L) }

    LaunchedEffect(Unit) {
        while (isActive) {
            delay(33) // ~30 Hz animation tick
            tick++
        }
    }

    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(200.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(DeepVoid)
            .border(1.dp, CardBorder, RoundedCornerShape(16.dp))
            .clickable {
                zoomLevel = when (zoomLevel) {
                    CosmosZoomLevel.GALAXY -> CosmosZoomLevel.SOLAR
                    CosmosZoomLevel.SOLAR -> CosmosZoomLevel.TERRA
                    CosmosZoomLevel.TERRA -> CosmosZoomLevel.GALAXY
                }
                onZoomChange?.invoke(zoomLevel)
            }
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val cx = size.width / 2f
            val cy = size.height / 2f

            when (zoomLevel) {
                CosmosZoomLevel.GALAXY -> {
                    // 1. Sagittarius A* Supermassive Black Hole Core
                    drawCircle(color = Color(0xFFFFD700), radius = 5f, center = Offset(cx, cy))
                    drawCircle(color = Color(0x5500E5FF), radius = 12f, center = Offset(cx, cy), style = Stroke(1.5f))

                    // 2. Dynamic Rotating Spiral Arms
                    val rot = tick * 0.015f
                    for (arm in 0 until 2) {
                        val armOffset = arm * PI.toFloat()
                        var theta = 0.6f
                        while (theta < 4.6f) {
                            val r = 12f + 16f * theta
                            val a = theta + rot + armOffset
                            val px = cx + cos(a) * r * 1.5f
                            val py = cy + sin(a) * r * 0.7f

                            val hue = (260f + theta * 25f) % 360f
                            val alpha = (1f - (theta / 5f)).coerceIn(0.2f, 0.9f)
                            drawCircle(
                                color = Color.hsv(hue, 0.8f, 0.9f).copy(alpha = alpha),
                                radius = 2f,
                                center = Offset(px, py)
                            )
                            theta += 0.18f
                        }
                    }

                        // 3. Sol Beacon (Orion Spur)
                    val solA = 1.35f + rot
                    val solX = cx + cos(solA) * 65f * 1.5f
                    val solY = cy + sin(solA) * 65f * 0.7f

                    // Pulsing reticle on Sol
                    val pulse = ((tick / 8) % 2 == 0L)
                    if (pulse) {
                        drawCircle(color = Color(0xFFFFD700), radius = 4f, center = Offset(solX, solY))
                        drawLine(Color(0xFF00E5FF), Offset(solX - 6f, solY), Offset(solX + 6f, solY), 1.5f)
                        drawLine(Color(0xFF00E5FF), Offset(solX, solY - 6f), Offset(solX, solY + 6f), 1.5f)
                    }
                }

                CosmosZoomLevel.SOLAR -> {
                    // Sun at center
                    val sunPulse = (sin(tick * 0.1f) + 1f) * 0.5f
                    drawCircle(color = Color(0xFFFFD700), radius = 7f + 2f * sunPulse, center = Offset(cx, cy))

                    // 4 Concentric Orbits: Mercury, Venus, Earth, Mars
                    val orbits = listOf(
                        Triple(28f, 16f, tick * 0.08f),   // Mercury
                        Triple(48f, 26f, tick * 0.035f),  // Venus
                        Triple(72f, 38f, tick * 0.02f),   // Earth (Terra)
                        Triple(96f, 50f, tick * 0.011f)   // Mars
                    )

                    orbits.forEachIndexed { i, (rx, ry, ang) ->
                        // Orbit line
                        drawOval(
                            color = if (i == 2) Color(0x5500E5FF) else Color(0x22FFFFFF),
                            topLeft = Offset(cx - rx, cy - ry),
                            size = androidx.compose.ui.geometry.Size(rx * 2f, ry * 2f),
                            style = Stroke(if (i == 2) 1.5f else 1f)
                        )

                        // Planet
                        val px = cx + cos(ang) * rx
                        val py = cy + sin(ang) * ry
                        val planetColor = when (i) {
                            0 -> Color(0xFFB0BEC5)
                            1 -> Color(0xFFFFCC80)
                            2 -> Color(0xFF00E5FF) // Earth
                            else -> Color(0xFFFF5252)
                        }
                        val planetR = if (i == 2) 4.5f else 3f
                        drawCircle(color = planetColor, radius = planetR, center = Offset(px, py))

                        // Earth's moon and target crosshair
                        if (i == 2) {
                            val moonAng = tick * 0.12f
                            val mx = px + cos(moonAng) * 9f
                            val my = py + sin(moonAng) * 5f
                            drawCircle(color = Color.White, radius = 1.5f, center = Offset(mx, my))

                            // Presence indicator arrow
                            val arrowPulse = ((tick / 6) % 2 == 0L)
                            if (arrowPulse) {
                                drawCircle(color = Color(0xFFFFD700), radius = 6.5f, center = Offset(px, py), style = Stroke(1.2f))
                            }
                        }
                    }
                }

                CosmosZoomLevel.TERRA -> {
                    // Earth globe
                    val globeR = 55f
                    drawCircle(color = Color(0xFF021B35), radius = globeR, center = Offset(cx, cy))
                    drawCircle(color = Color(0xFF00E5FF), radius = globeR, center = Offset(cx, cy), style = Stroke(2f))

                    // Latitudes
                    drawOval(
                        color = Color(0x4400E5FF),
                        topLeft = Offset(cx - globeR, cy - 18f),
                        size = androidx.compose.ui.geometry.Size(globeR * 2f, 36f),
                        style = Stroke(1f)
                    )

                    // Rotating Continents
                    val rot = tick * 0.02f
                    for (p in 0 until 8) {
                        val lon = rot + p * (PI.toFloat() / 4f)
                        val lat = if (p % 3 == 0) 0.35f else if (p % 2 == 0) -0.25f else 0.5f
                        if (cos(lon) > 0.05f) {
                            val lx = cx + sin(lon) * (globeR - 6f) * cos(lat)
                            val ly = cy - sin(lat) * (globeR - 6f)
                            drawCircle(color = Color(0xFF00E676), radius = 5f, center = Offset(lx, ly))
                        }
                    }

                    // Moon in distant orbit
                    val mAng = tick * 0.03f
                    val mx = cx + cos(mAng) * 85f
                    val my = cy + sin(mAng) * 35f
                    drawCircle(color = Color(0xFFECEFF1), radius = 4f, center = Offset(mx, my))

                    // Reticle on surface location
                    val reticlePulse = ((tick / 6) % 2 == 0L)
                    if (reticlePulse) {
                        val tx = cx + 8f
                        val ty = cy - 12f
                        drawLine(Color(0xFFFFD700), Offset(tx - 8f, ty), Offset(tx + 8f, ty), 1.5f)
                        drawLine(Color(0xFFFFD700), Offset(tx, ty - 8f), Offset(tx, ty + 8f), 1.5f)
                    }
                }
            }
        }

        // Header Zoom Badge overlay
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = zoomLevel.title,
                    color = AetherCyan,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace
                )
                Text(
                    text = zoomLevel.scaleLabel,
                    color = TextSecondary,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace
                )
            }

            // Click to Zoom Icon Pill
            Row(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(SpaceBlack.copy(alpha = 0.85f))
                    .border(1.dp, AetherCyan, RoundedCornerShape(8.dp))
                    .padding(horizontal = 8.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.ZoomIn,
                    contentDescription = "Zoom",
                    tint = AetherCyan,
                    modifier = Modifier.size(14.dp)
                )
                Text(
                    text = "CLICK TO ZOOM",
                    color = AetherCyan,
                    fontSize = 9.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace
                )
            }
        }
    }
}

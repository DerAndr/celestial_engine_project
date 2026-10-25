package com.celestial.companion.ui.screens.leds

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Star
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.spells.SpellCrafterScreen
import com.celestial.companion.ui.theme.*
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset

@Composable
fun LedScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val telemetry by viewModel.telemetry.collectAsState()
    var selectedTab by remember { mutableStateOf(0) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
    ) {
        // Top Tab Selector (Spell Crafter vs 43-LED Modes)
        TabRow(
            selectedTabIndex = selectedTab,
            containerColor = DeepVoid,
            contentColor = AetherCyan,
            indicator = { tabPositions ->
                TabRowDefaults.SecondaryIndicator(
                    modifier = Modifier.tabIndicatorOffset(tabPositions[selectedTab]),
                    color = if (selectedTab == 0) RunicGold else AetherCyan
                )
            }
        ) {
            Tab(
                selected = selectedTab == 0,
                onClick = { selectedTab = 0 },
                text = {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Icon(
                            Icons.Default.AutoAwesome,
                            contentDescription = null,
                            tint = if (selectedTab == 0) RunicGold else TextMuted,
                            modifier = Modifier.size(16.dp)
                        )
                        Text(
                            "КРАФТЕР ЗАКЛИНАНИЙ",
                            fontWeight = FontWeight.Bold,
                            color = if (selectedTab == 0) RunicGold else TextMuted,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            )
            Tab(
                selected = selectedTab == 1,
                onClick = { selectedTab = 1 },
                text = {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Icon(
                            Icons.Default.Star,
                            contentDescription = null,
                            tint = if (selectedTab == 1) AetherCyan else TextMuted,
                            modifier = Modifier.size(16.dp)
                        )
                        Text(
                            "43-LED РЕЖИМЫ",
                            fontWeight = FontWeight.Bold,
                            color = if (selectedTab == 1) AetherCyan else TextMuted,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            )
        }

        if (selectedTab == 0) {
            SpellCrafterScreen(viewModel = viewModel, modifier = Modifier.weight(1f))
        } else {
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp)
            ) {
        // 1. Concentric Rings Real-Time Visualizer Hero Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                // Header with sync indicator
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(8.dp)
                                .clip(CircleShape)
                                .background(if (telemetry.isOnline) EmeraldGreen else RunicGold)
                        )
                        Text(
                            text = if (telemetry.isOnline) "PICO 2 W SYNCED (GP15 • 43 LEDS)" else "STANDALONE SIMULATOR (43 LEDS)",
                            color = if (telemetry.isOnline) EmeraldGreen else RunicGold,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Text(
                        text = "MODE #${telemetry.activeLedAnim.id}",
                        color = NebulaPurple,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                }

                // Interactive 43-LED Concentric Rings Canvas
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    contentAlignment = Alignment.Center
                ) {
                    ConcentricLedRingsView(
                        animation = telemetry.activeLedAnim,
                        size = 210.dp,
                        micLevelPct = telemetry.micLevelPercent,
                        aiModelId = telemetry.aiModelId,
                        state = telemetry.state
                    )
                }

                // Active Mode Title & Subtitle Banner
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(2.dp)
                ) {
                    Text(
                        text = telemetry.activeLedAnim.title.uppercase(),
                        color = AetherCyan,
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = telemetry.activeLedAnim.subtitle,
                        color = TextSecondary,
                        fontSize = 11.sp
                    )
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Topology Chips Strip (Center 1, Inner 6, Middle 12, Outer 24)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TopologyChip(label = "CENTER", count = "1 LED")
                    TopologyChip(label = "INNER", count = "6 LEDS")
                    TopologyChip(label = "MIDDLE", count = "12 LEDS")
                    TopologyChip(label = "OUTER", count = "24 LEDS")
                }
            }
        }

        // 2. Preset Selection Header
        Text(
            text = "ANIMATION PRESETS (18 MODES)",
            color = AetherCyan,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace
        )

        // 3. Scrollable List of 18 Presets
        LazyColumn(
            verticalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxSize()
        ) {
            items(LedAnimation.entries.toTypedArray()) { anim ->
                val isSelected = anim == telemetry.activeLedAnim
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .clickable { viewModel.setLedAnimation(anim) },
                    shape = RoundedCornerShape(12.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = if (isSelected) DeepVoid else CardSurface
                    ),
                    border = BorderStroke(
                        if (isSelected) 1.5.dp else 1.dp,
                        if (isSelected) NebulaPurple else CardBorder
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(10.dp)
                        ) {
                            // ID Badge
                            Box(
                                modifier = Modifier
                                    .size(28.dp)
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(if (isSelected) NebulaPurple else DeepVoid)
                                    .border(1.dp, if (isSelected) NebulaPurple else CardBorder, RoundedCornerShape(6.dp)),
                                contentAlignment = Alignment.Center
                            ) {
                                Text(
                                    text = "${anim.id}",
                                    color = if (isSelected) SpaceBlack else TextSecondary,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                            }

                            Column {
                                Text(
                                    text = anim.title,
                                    color = if (isSelected) TextPrimary else TextSecondary,
                                    fontSize = 13.sp,
                                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                    fontFamily = FontFamily.Monospace
                                )
                                Text(
                                    text = anim.subtitle,
                                    color = TextMuted,
                                    fontSize = 10.sp
                                )
                            }
                        }

                        if (isSelected) {
                            Icon(
                                Icons.Default.Check,
                                contentDescription = "Active",
                                tint = NebulaPurple,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    }
                }
            }
            }
        }
    }
}

@Composable
private fun TopologyChip(label: String, count: String) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(1.dp)
    ) {
        Text(text = label, color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
        Text(text = count, color = AetherCyan, fontSize = 11.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
    }
}

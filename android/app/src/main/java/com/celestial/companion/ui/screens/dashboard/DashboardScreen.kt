package com.celestial.companion.ui.screens.dashboard

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.horizontalScroll
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.ui.screens.cosmos.CosmosZoomView
import com.celestial.companion.ui.screens.leds.ConcentricLedRingsView
import com.celestial.companion.ui.components.StaffOledScreenView
import com.celestial.companion.ui.components.LiveLogConsole
import com.celestial.companion.ui.theme.*

@Composable
fun DashboardScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val telemetry by viewModel.telemetry.collectAsState()
    val liveLogs by viewModel.liveLogs.collectAsState()
    val currentLanguage by viewModel.currentLanguage.collectAsState()
    var showIpDialog by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 1. Header & Live Telemetry HUD
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "CELESTIAL ENGINE",
                            color = AetherCyan,
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = "RP2350 Mk.V Staff Companion",
                            color = TextSecondary,
                            fontSize = 12.sp
                        )
                    }

                    // Connection Indicator
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        modifier = Modifier
                            .clip(RoundedCornerShape(20.dp))
                            .background(if (telemetry.isOnline) Color(0x2200E676) else Color(0x22FF5252))
                            .padding(horizontal = 10.dp, vertical = 4.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(8.dp)
                                .clip(CircleShape)
                                .background(if (telemetry.isOnline) EmeraldGreen else Color.Red)
                        )
                        Text(
                            text = if (telemetry.isOnline) "ONLINE" else "OFFLINE",
                            color = if (telemetry.isOnline) EmeraldGreen else Color.Red,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Quick Telemetry Row
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TelemetryItem(label = "TARGET IP", value = viewModel.getStaffIp(), onClick = { showIpDialog = true })
                    TelemetryItem(
                        label = "AUTH KEY",
                        value = if (viewModel.getPasskey().isEmpty()) "CLICK TO SET" else "PROTECTED",
                        onClick = { showIpDialog = true }
                    )
                    TelemetryItem(label = "STATE", value = telemetry.state.displayName)
                    TelemetryItem(label = "VOL", value = "${telemetry.volumePercent}%")
                    TelemetryItem(label = "TICK", value = "#${telemetry.tickNumber}")
                }
            }
        }

        // Emergency Stop / Abort Button (Displayed whenever staff is listening, processing, or speaking)
        val isGenerating by viewModel.isGenerating.collectAsState()
        if (telemetry.state != StaffState.IDLE || isGenerating) {
            Button(
                onClick = { viewModel.cancelGeneration() },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(52.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFD32F2F),
                    contentColor = Color.White
                ),
                shape = RoundedCornerShape(14.dp),
                elevation = ButtonDefaults.buttonElevation(defaultElevation = 6.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Stop,
                        contentDescription = "Stop",
                        modifier = Modifier.size(24.dp)
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "СТОП / ПРЕРВАТЬ (${telemetry.state.displayName.uppercase()})",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }
        }

        // Quick-Cast Grimoire Bar (One-tap spells directly from dashboard)
        val customSpells by viewModel.customSpells.collectAsState()
        val allSpells = customSpells + SpellDefinition.PRESET_SPELLS
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Icon(Icons.Default.Bolt, contentDescription = null, tint = RunicGold, modifier = Modifier.size(16.dp))
                        Text(
                            text = "ГРИМУАР (БЫСТРЫЙ КАСТ)",
                            color = RunicGold,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                    Text(
                        text = "Конструктор во вкладке Spells →",
                        color = AetherCyan,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    allSpells.forEach { spell ->
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(8.dp))
                                .background(DeepVoid)
                                .border(1.dp, if (spell.isCustom) NebulaPurple else CardBorder, RoundedCornerShape(8.dp))
                                .clickable { viewModel.castSpell(spell, speakIncantation = true) }
                                .padding(horizontal = 10.dp, vertical = 6.dp)
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Text(
                                    text = when {
                                        spell.name.contains("Fire", ignoreCase = true) -> "🔥"
                                        spell.name.contains("Lightning", ignoreCase = true) -> "⚡"
                                        spell.name.contains("Shield", ignoreCase = true) -> "🛡️"
                                        spell.name.contains("Vortex", ignoreCase = true) -> "🌀"
                                        spell.name.contains("Quack", ignoreCase = true) -> "🦆"
                                        spell.name.contains("Frost", ignoreCase = true) -> "❄️"
                                        spell.name.contains("Snake", ignoreCase = true) || spell.name.contains("Ouroboros", ignoreCase = true) -> "🐍"
                                        else -> "✨"
                                    },
                                    fontSize = 12.sp
                                )
                                Text(
                                    text = spell.name,
                                    color = if (spell.isCustom) NebulaPurple else TextPrimary,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }
                    }
                }
            }
        }

        // Live Staff Telemetry & Traffic Console
        LiveLogConsole(
            logs = liveLogs,
            onClearLogs = { viewModel.clearLogs() },
            initialExpanded = true
        )

        // 1.5 Connection Architecture Card (Simplified Pure Pictogram Scheme)
        val connectionMode by viewModel.connectionMode.collectAsState()
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Interactive 3-Node Architecture Pictogram Scheme (No text clutter)
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(DeepVoid)
                        .border(1.dp, CardBorder, RoundedCornerShape(12.dp))
                        .padding(vertical = 12.dp, horizontal = 14.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Node 1: Phone
                    val phoneActive = connectionMode != com.celestial.companion.domain.repository.ConnectionMode.STANDALONE_PHONE
                    Box(
                        modifier = Modifier
                            .size(36.dp)
                            .clip(CircleShape)
                            .background(if (phoneActive) AetherCyan.copy(alpha = 0.2f) else SpaceBlack)
                            .border(1.5.dp, if (phoneActive) AetherCyan else CardBorder, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.Smartphone,
                            contentDescription = "Phone",
                            tint = if (phoneActive) AetherCyan else TextMuted,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    // Wireless Link 1 (Phone <-> Staff)
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        repeat(3) {
                            Box(
                                modifier = Modifier
                                    .size(4.dp)
                                    .clip(CircleShape)
                                    .background(if (phoneActive) AetherCyan else CardBorder)
                            )
                        }
                        Icon(
                            imageVector = Icons.Default.Sync,
                            contentDescription = "Sync",
                            tint = if (phoneActive) AetherCyan else CardBorder,
                            modifier = Modifier.size(14.dp)
                        )
                        repeat(3) {
                            Box(
                                modifier = Modifier
                                    .size(4.dp)
                                    .clip(CircleShape)
                                    .background(if (phoneActive) AetherCyan else CardBorder)
                            )
                        }
                    }

                    // Node 2: Staff
                    Box(
                        modifier = Modifier
                            .size(36.dp)
                            .clip(CircleShape)
                            .background(RunicGold.copy(alpha = 0.2f))
                            .border(1.5.dp, if (telemetry.isOnline) RunicGold else CardBorder, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.AutoAwesome,
                            contentDescription = "Staff",
                            tint = if (telemetry.isOnline) RunicGold else TextMuted,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    // Wireless Link 2 (Staff <-> Pi 5)
                    val meshActive = connectionMode == com.celestial.companion.domain.repository.ConnectionMode.MESH_ACTIVE
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        repeat(3) {
                            Box(
                                modifier = Modifier
                                    .size(4.dp)
                                    .clip(CircleShape)
                                    .background(if (meshActive) EmeraldGreen else CardBorder)
                            )
                        }
                        Icon(
                            imageVector = Icons.Default.Wifi,
                            contentDescription = "Link",
                            tint = if (meshActive) EmeraldGreen else CardBorder,
                            modifier = Modifier.size(14.dp)
                        )
                        repeat(3) {
                            Box(
                                modifier = Modifier
                                    .size(4.dp)
                                    .clip(CircleShape)
                                    .background(if (meshActive) EmeraldGreen else CardBorder)
                            )
                        }
                    }

                    // Node 3: Pi 5
                    Box(
                        modifier = Modifier
                            .size(36.dp)
                            .clip(CircleShape)
                            .background(if (meshActive) EmeraldGreen.copy(alpha = 0.2f) else SpaceBlack)
                            .border(1.5.dp, if (meshActive) EmeraldGreen else CardBorder, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.Memory,
                            contentDescription = "Pi 5",
                            tint = if (meshActive) EmeraldGreen else TextMuted,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }

                // 3 Pictogram Selector Pills (Clean icons, no text clutter)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    com.celestial.companion.domain.repository.ConnectionMode.entries.forEach { mode ->
                        val isSelected = mode == connectionMode
                        val activeColor = when (mode) {
                            com.celestial.companion.domain.repository.ConnectionMode.STANDALONE_PHONE -> EmeraldGreen
                            com.celestial.companion.domain.repository.ConnectionMode.PHONE_BRAIN_SERVER -> AetherCyan
                            com.celestial.companion.domain.repository.ConnectionMode.MESH_ACTIVE -> RunicGold
                        }
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .clip(RoundedCornerShape(10.dp))
                                .background(if (isSelected) DeepVoid else SpaceBlack)
                                .border(
                                    1.5.dp,
                                    if (isSelected) activeColor else CardBorder,
                                    RoundedCornerShape(10.dp)
                                )
                                .clickable { viewModel.setConnectionMode(mode) }
                                .padding(vertical = 10.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                when (mode) {
                                    com.celestial.companion.domain.repository.ConnectionMode.STANDALONE_PHONE -> {
                                        Icon(
                                            imageVector = Icons.Default.Smartphone,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                    }
                                    com.celestial.companion.domain.repository.ConnectionMode.PHONE_BRAIN_SERVER -> {
                                        Icon(
                                            imageVector = Icons.Default.Smartphone,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                        Icon(
                                            imageVector = Icons.Default.Sync,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextMuted,
                                            modifier = Modifier.size(12.dp)
                                        )
                                        Icon(
                                            imageVector = Icons.Default.AutoAwesome,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                    }
                                    com.celestial.companion.domain.repository.ConnectionMode.MESH_ACTIVE -> {
                                        Icon(
                                            imageVector = Icons.Default.Smartphone,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                        Icon(
                                            imageVector = Icons.Default.Memory,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                        Icon(
                                            imageVector = Icons.Default.AutoAwesome,
                                            contentDescription = null,
                                            tint = if (isSelected) activeColor else TextSecondary,
                                            modifier = Modifier.size(16.dp)
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Staff Host & Dialect Selector Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Host Target
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "STAFF AI HOST TARGET",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = telemetry.activeHost,
                        color = if (telemetry.preferredHost == 2) EmeraldGreen else AetherCyan,
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    // 1. Lock Phone
                    Button(
                        onClick = { viewModel.setStaffHost(2) },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 2) EmeraldGreen else DeepVoid
                        ),
                        border = androidx.compose.foundation.BorderStroke(
                            1.dp,
                            if (telemetry.preferredHost == 2) EmeraldGreen else CardBorder
                        ),
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(vertical = 8.dp)
                    ) {
                        Text(
                            text = "LOCK PHONE",
                            color = if (telemetry.preferredHost == 2) SpaceBlack else TextPrimary,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // 2. Lock Pi 5
                    Button(
                        onClick = { viewModel.setStaffHost(1) },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 1) AetherCyan else DeepVoid
                        ),
                        border = androidx.compose.foundation.BorderStroke(
                            1.dp,
                            if (telemetry.preferredHost == 1) AetherCyan else CardBorder
                        ),
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(vertical = 8.dp)
                    ) {
                        Text(
                            text = "LOCK PI 5",
                            color = if (telemetry.preferredHost == 1) SpaceBlack else TextPrimary,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // 3. Auto Mesh
                    Button(
                        onClick = { viewModel.setStaffHost(0) },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 0) RunicGold else DeepVoid
                        ),
                        border = androidx.compose.foundation.BorderStroke(
                            1.dp,
                            if (telemetry.preferredHost == 0) RunicGold else CardBorder
                        ),
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(vertical = 8.dp)
                    ) {
                        Text(
                            text = "AUTO MESH",
                            color = if (telemetry.preferredHost == 0) SpaceBlack else TextPrimary,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Dialect pills
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "VOICE TTS / STT DIALECT",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = currentLanguage.displayName,
                        color = RunicGold,
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    listOf(StaffLanguage.ENGLISH, StaffLanguage.GERMAN, StaffLanguage.RUSSIAN).forEach { lang ->
                        val isSel = lang == currentLanguage
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .clip(RoundedCornerShape(8.dp))
                                .background(if (isSel) DeepVoid else SpaceBlack)
                                .border(
                                    1.dp,
                                    if (isSel) RunicGold else CardBorder,
                                    RoundedCornerShape(8.dp)
                                )
                                .clickable { viewModel.setLanguage(lang) }
                                .padding(vertical = 8.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = "${lang.shortTag} (${lang.displayName})",
                                color = if (isSel) RunicGold else TextSecondary,
                                fontSize = 11.sp,
                                fontWeight = if (isSel) FontWeight.Bold else FontWeight.Normal,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }
            }
        }

        // Live Staff Digital Twin: Headpiece WS2812B Rings + Shaft SH1106 OLED Display
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                // Header row with sync indicator & staff state
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
                            text = if (telemetry.isOnline) "STAFF DIGITAL TWIN (LIVE)" else "STAFF DIGITAL TWIN (SIM)",
                            color = if (telemetry.isOnline) EmeraldGreen else RunicGold,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Text(
                        text = "STATE: ${telemetry.state.displayName}",
                        color = when (telemetry.state) {
                            StaffState.LISTENING -> Color(0xFFFF5252)
                            StaffState.PROCESSING -> AetherCyan
                            StaffState.SPEAKING -> EmeraldGreen
                            StaffState.IDLE -> TextSecondary
                        },
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                }

                // Digital Twin Visualizer: Ring on the left, OLED Screen Mirror on the right
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    // Left: 43-LED Concentric Rings Visualizer
                    Box(
                        modifier = Modifier
                            .size(110.dp)
                            .clip(CircleShape)
                            .background(DeepVoid)
                            .border(1.dp, CardBorder, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        ConcentricLedRingsView(
                            animation = telemetry.activeLedAnim,
                            modifier = Modifier.size(106.dp),
                            size = 106.dp,
                            micLevelPct = telemetry.micLevelPercent,
                            aiModelId = telemetry.aiModelId,
                            state = telemetry.state
                        )
                    }

                    // Right: SH1106 128x64 OLED HUD Screen Mirror
                    Box(
                        modifier = Modifier.weight(1f),
                        contentAlignment = Alignment.Center
                    ) {
                        StaffOledScreenView(
                            state = telemetry.state,
                            mode = telemetry.mode,
                            volumePercent = telemetry.volumePercent,
                            clockStyle = telemetry.clockStyle,
                            activeHost = telemetry.activeHost,
                            aiModelId = telemetry.aiModelId,
                            micLevelPercent = telemetry.micLevelPercent,
                            cosmosZoom = telemetry.cosmosZoom,
                            isOnline = telemetry.isOnline,
                            width = 190.dp
                        )
                    }
                }

                // Subtitle / Specs footer
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "43 LEDs • ${telemetry.activeLedAnim.displayName}",
                        color = AetherCyan,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = "SH1106 OLED • I2C1 400kHz",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }
        }

        // 1.95 Complete Live Hardware & Network Telemetry Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
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
                                .background(if (telemetry.isOnline) EmeraldGreen else Color.Red)
                        )
                        Text(
                            text = "LIVE HARDWARE TELEMETRY",
                            color = TextSecondary,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Text(
                        text = if (telemetry.isOnline) "RP2350 LINK ACTIVE" else "NO LINK / SIM",
                        color = if (telemetry.isOnline) EmeraldGreen else TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Row 1: Network & Host routing
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TelemetryItem(
                        label = "TARGET ENDPOINT",
                        value = "${viewModel.getStaffIp()}:5005",
                        onClick = { showIpDialog = true }
                    )
                    TelemetryItem(
                        label = "AI HOST ROUTE",
                        value = telemetry.activeHost
                    )
                    TelemetryItem(
                        label = "LOCK MODE",
                        value = when (telemetry.preferredHost) {
                            1 -> "LOCKED PI 5"
                            2 -> "LOCKED PHONE"
                            else -> "AUTO MESH"
                        }
                    )
                }

                // Row 2: Audio & State
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TelemetryItem(
                        label = "STAFF STATE",
                        value = telemetry.state.displayName
                    )
                    TelemetryItem(
                        label = "DAC I2S AUDIO",
                        value = if (telemetry.isAudioPlaying) "PLAYING" else "STANDBY"
                    )
                    TelemetryItem(
                        label = "AUDIO VOL",
                        value = "${telemetry.volumePercent}%"
                    )
                }

                // Row 3: Clock, Dialect, Packets
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    TelemetryItem(
                        label = "CLOCK STYLE",
                        value = telemetry.clockStyle.title
                    )
                    TelemetryItem(
                        label = "TTS DIALECT",
                        value = telemetry.language.displayName
                    )
                    TelemetryItem(
                        label = "UDP PACKET TICK",
                        value = "#${telemetry.tickNumber}"
                    )
                }

                // Row 4: Live Microphone Meter (INMP441 I2S VU)
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(DeepVoid)
                        .padding(horizontal = 10.dp, vertical = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "INMP441 MIC VU LEVEL",
                            color = TextMuted,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "${telemetry.micLevelPercent}%",
                            color = if (telemetry.micLevelPercent > 50) EmeraldGreen else AetherCyan,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    // Meter Bar
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(6.dp)
                            .clip(RoundedCornerShape(3.dp))
                            .background(CardBorder)
                    ) {
                        val fraction = (telemetry.micLevelPercent / 100f).coerceIn(0f, 1f)
                        Box(
                            modifier = Modifier
                                .fillMaxWidth(fraction)
                                .fillMaxHeight()
                                .clip(RoundedCornerShape(3.dp))
                                .background(
                                    when {
                                        telemetry.micLevelPercent > 75 -> Color(0xFFFF5252)
                                        telemetry.micLevelPercent > 35 -> RunicGold
                                        else -> EmeraldGreen
                                    }
                                )
                        )
                    }
                }
            }
        }

        // 2. Active Mode Display & Selector
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "ACTIVE STAFF MODE",
                        color = TextSecondary,
                        fontSize = 12.sp,
                        fontFamily = FontFamily.Monospace
                    )
                    Text(
                        text = "[${telemetry.mode.displayName}]",
                        color = RunicGold,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                }

                // Horizontal Carousel of Modes
                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    contentPadding = PaddingValues(vertical = 4.dp)
                ) {
                    items(StaffMode.entries.toTypedArray()) { mode ->
                        val isSelected = mode == telemetry.mode
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(10.dp))
                                .background(if (isSelected) AetherCyan else DeepVoid)
                                .border(
                                    1.dp,
                                    if (isSelected) CyanGlow else CardBorder,
                                    RoundedCornerShape(10.dp)
                                )
                                .clickable { viewModel.selectMode(mode) }
                                .padding(horizontal = 14.dp, vertical = 8.dp)
                        ) {
                            Text(
                                text = mode.displayName,
                                color = if (isSelected) SpaceBlack else TextPrimary,
                                fontSize = 12.sp,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }
            }
        }

        // 2.5 Unified Interactive Cosmos View (Click to Zoom: Galaxy -> Solar -> Terra)
        CosmosZoomView()

        // 3. Dynamic AI Host & Mobile Catalog Card
        val phoneBrainActive by viewModel.phoneBrainActive.collectAsState()
        val phoneModels by viewModel.phoneModels.collectAsState()

        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "AI BRAIN HOST",
                            color = TextSecondary,
                            fontSize = 12.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = telemetry.activeHost,
                            color = if (telemetry.activeHost.contains("PI")) RunicGold else AetherCyan,
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Text(
                            text = if (phoneBrainActive) "Phone Brain ON" else "Phone Brain OFF",
                            color = if (phoneBrainActive) EmeraldGreen else TextMuted,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Switch(
                            checked = phoneBrainActive,
                            onCheckedChange = { viewModel.togglePhoneBrain(it) },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = AetherCyan,
                                checkedTrackColor = Color(0x3300E5FF)
                            )
                        )
                    }
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                Text(
                    text = "Phone Mobile Models (Auto-Advertised to Staff):",
                    color = TextPrimary,
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )

                // List of phone models
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    phoneModels.forEach { m ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .background(DeepVoid)
                                .padding(horizontal = 12.dp, vertical = 6.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = m.name,
                                color = TextPrimary,
                                fontSize = 12.sp,
                                fontFamily = FontFamily.Monospace
                            )
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(Color(0x3300E5FF))
                                    .padding(horizontal = 8.dp, vertical = 2.dp)
                            ) {
                                Text(
                                    text = m.tag,
                                    color = AetherCyan,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }
                    }
                }

                Button(
                    onClick = { viewModel.broadcastPhoneCatalog() },
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(10.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = DeepVoid),
                    border = androidx.compose.foundation.BorderStroke(1.dp, AetherCyan)
                ) {
                    Icon(Icons.Default.CloudSync, contentDescription = null, tint = AetherCyan)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "PUSH PHONE MODELS TO STAFF",
                        color = AetherCyan,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }
        }

        // 4. Audio & Volume Control
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("MASTER VOLUME", color = TextSecondary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                    Text("${telemetry.volumePercent}%", color = AetherCyan, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }

                Slider(
                    value = telemetry.volumePercent.toFloat(),
                    onValueChange = { viewModel.setVolume(it.toInt()) },
                    valueRange = 0f..100f,
                    colors = SliderDefaults.colors(
                        thumbColor = AetherCyan,
                        activeTrackColor = AetherCyan,
                        inactiveTrackColor = DeepVoid
                    )
                )
            }
        }

        // 4. Quick Actions Grid
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Button(
                onClick = { viewModel.syncSystemTime() },
                modifier = Modifier.weight(1f),
                colors = ButtonDefaults.buttonColors(containerColor = DeepVoid),
                border = androidx.compose.foundation.BorderStroke(1.dp, AetherCyan),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(Icons.Default.Sync, contentDescription = null, tint = AetherCyan)
                Spacer(modifier = Modifier.width(6.dp))
                Text("Sync Time", color = AetherCyan, fontSize = 12.sp)
            }

            Button(
                onClick = { viewModel.stopAudio() },
                modifier = Modifier.weight(1f),
                colors = ButtonDefaults.buttonColors(containerColor = DeepVoid),
                border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFFFF5252)),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(Icons.Default.Stop, contentDescription = null, tint = Color(0xFFFF5252))
                Spacer(modifier = Modifier.width(6.dp))
                Text("Stop Audio", color = Color(0xFFFF5252), fontSize = 12.sp)
            }
        }
    }

    if (showIpDialog) {
        val context = LocalContext.current
        var ipInput by remember { mutableStateOf(viewModel.getStaffIp()) }
        var passkeyInput by remember { mutableStateOf(viewModel.getPasskey()) }
        AlertDialog(
            onDismissRequest = { showIpDialog = false },
            title = { Text("Staff Connection & Security", color = TextPrimary) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    OutlinedTextField(
                        value = ipInput,
                        onValueChange = { ipInput = it },
                        label = { Text("IP Address (or auto-discover)") },
                        singleLine = true
                    )
                    OutlinedTextField(
                        value = passkeyInput,
                        onValueChange = { passkeyInput = it },
                        label = { Text("Security Passkey") },
                        placeholder = { Text("Enter secret key") },
                        singleLine = true
                    )
                    Text(
                        text = "Leave blank if staff has no passkey, or enter your secret passkey. Never leaks to cloud/git.",
                        color = TextMuted,
                        fontSize = 11.sp
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.saveConnectionSettings(context, ipInput, passkeyInput)
                    showIpDialog = false
                }) {
                    Text("Save", color = AetherCyan)
                }
            },
            dismissButton = {
                TextButton(onClick = { showIpDialog = false }) {
                    Text("Cancel", color = TextSecondary)
                }
            },
            containerColor = CardSurface
        )
    }
}

@Composable
private fun TelemetryItem(label: String, value: String, onClick: (() -> Unit)? = null) {
    Column(
        modifier = if (onClick != null) Modifier.clickable { onClick() } else Modifier
    ) {
        Text(text = label, color = TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
        Text(text = value, color = TextPrimary, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, fontFamily = FontFamily.Monospace)
    }
}

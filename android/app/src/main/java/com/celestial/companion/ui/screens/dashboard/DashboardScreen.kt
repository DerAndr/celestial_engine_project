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
import com.celestial.companion.domain.model.StaffMode
import com.celestial.companion.domain.model.StaffState
import com.celestial.companion.ui.theme.*

@Composable
fun DashboardScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val telemetry by viewModel.telemetry.collectAsState()
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

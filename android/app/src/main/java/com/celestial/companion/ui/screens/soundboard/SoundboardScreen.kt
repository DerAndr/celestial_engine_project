package com.celestial.companion.ui.screens.soundboard

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.ThematicPack
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.theme.*

@Composable
fun SoundboardScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    var selectedTab by remember { mutableIntStateOf(0) }
    val tabs = listOf("🎮 THEMES (16)", "🌌 SFX (14)", "🪄 COMBOS")

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Top Action Header with Mute
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = "ARCANE SOUNDBOARD",
                    color = AetherCyan,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace
                )
                Text(
                    text = "Chiptunes, Themes & Sound Synthesizer",
                    color = TextMuted,
                    fontSize = 10.sp
                )
            }

            Button(
                onClick = { viewModel.stopAudio() },
                colors = ButtonDefaults.buttonColors(containerColor = Color(0x33FF5252)),
                border = BorderStroke(1.dp, Color(0xFFFF5252)),
                shape = RoundedCornerShape(8.dp),
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
            ) {
                Icon(Icons.Default.Stop, contentDescription = null, tint = Color(0xFFFF5252), modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(4.dp))
                Text("Mute", color = Color(0xFFFF5252), fontSize = 12.sp)
            }
        }

        // Section Tabs
        TabRow(
            selectedTabIndex = selectedTab,
            containerColor = CardSurface,
            contentColor = AetherCyan,
            indicator = { tabPositions ->
                TabRowDefaults.Indicator(
                    modifier = Modifier.tabIndicatorOffset(tabPositions[selectedTab]),
                    color = AetherCyan,
                    height = 2.dp
                )
            },
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(10.dp))
                .border(1.dp, CardBorder, RoundedCornerShape(10.dp))
        ) {
            tabs.forEachIndexed { index, title ->
                Tab(
                    selected = selectedTab == index,
                    onClick = { selectedTab = index },
                    text = {
                        Text(
                            text = title,
                            fontSize = 11.sp,
                            fontWeight = if (selectedTab == index) FontWeight.Bold else FontWeight.Normal,
                            fontFamily = FontFamily.Monospace,
                            color = if (selectedTab == index) AetherCyan else TextMuted
                        )
                    }
                )
            }
        }

        when (selectedTab) {
            0 -> ThematicPacksView(viewModel)
            1 -> SfxGridView(viewModel)
            2 -> SoundCombosView(viewModel)
        }
    }
}

@Composable
private fun ThematicPacksView(viewModel: DashboardViewModel) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        item {
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp)),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(10.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Button(
                        onClick = { viewModel.playAllTracks() },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0x334CAF50)),
                        border = BorderStroke(1.dp, Color(0xFF4CAF50)),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.weight(1f),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp)
                    ) {
                        Icon(Icons.Default.PlayArrow, contentDescription = null, tint = Color(0xFF4CAF50), modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("Play All", color = Color(0xFF4CAF50), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = { viewModel.playAllCycled() },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0x3300E5FF)),
                        border = BorderStroke(1.dp, Color(0xFF00E5FF)),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.weight(1f),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp)
                    ) {
                        Icon(Icons.Default.Bolt, contentDescription = null, tint = Color(0xFF00E5FF), modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("Cycle All", color = Color(0xFF00E5FF), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                    }

                    Button(
                        onClick = { viewModel.stopAudio() },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0x33FF5252)),
                        border = BorderStroke(1.dp, Color(0xFFFF5252)),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.weight(1f),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp)
                    ) {
                        Icon(Icons.Default.Stop, contentDescription = null, tint = Color(0xFFFF5252), modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("Stop", color = Color(0xFFFF5252), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }

        items(ThematicPack.ALL_PACKS) { pack ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp)),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    // Title Row
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(10.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(36.dp)
                                    .clip(CircleShape)
                                    .background(DeepVoid)
                                    .border(1.dp, RunicGold, CircleShape),
                                contentAlignment = Alignment.Center
                            ) {
                                Text(text = pack.iconEmoji, fontSize = 18.sp)
                            }

                            Column {
                                Text(
                                    text = pack.title,
                                    color = TextPrimary,
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                                Text(
                                    text = pack.subtitle,
                                    color = RunicGold,
                                    fontSize = 10.sp,
                                    fontWeight = FontWeight.Medium
                                )
                            }
                        }
                    }

                    // Description
                    Text(
                        text = pack.description,
                        color = TextMuted,
                        fontSize = 10.sp,
                        lineHeight = 14.sp
                    )

                    // Action Buttons Row
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Button(
                            onClick = { viewModel.activateThematicPack(pack) },
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.buttonColors(containerColor = Color(0x3300E5FF)),
                            border = BorderStroke(1.dp, AetherCyan),
                            shape = RoundedCornerShape(8.dp),
                            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.PlayArrow, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(4.dp))
                            Text("Play Theme & Runes", color = AetherCyan, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                        }

                        if (pack.quickActionName != null && pack.quickActionSfxId != null) {
                            OutlinedButton(
                                onClick = { viewModel.playSfx(pack.quickActionSfxId) },
                                colors = ButtonDefaults.outlinedButtonColors(contentColor = RunicGold),
                                border = BorderStroke(1.dp, RunicGold),
                                shape = RoundedCornerShape(8.dp),
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp)
                            ) {
                                Text(pack.quickActionName, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                            }
                        }

                        if (pack.secondaryActionName != null && pack.secondaryActionSfxId != null) {
                            OutlinedButton(
                                onClick = { viewModel.playSfx(pack.secondaryActionSfxId) },
                                colors = ButtonDefaults.outlinedButtonColors(contentColor = NebulaPurple),
                                border = BorderStroke(1.dp, NebulaPurple),
                                shape = RoundedCornerShape(8.dp),
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 6.dp)
                            ) {
                                Text(pack.secondaryActionName, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SfxGridView(viewModel: DashboardViewModel) {
    LazyVerticalGrid(
        columns = GridCells.Fixed(2),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
        modifier = Modifier.fillMaxSize()
    ) {
        items(SfxItem.ALL_SFX) { sfx ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .clickable { viewModel.playSfx(sfx.id) },
                shape = RoundedCornerShape(12.dp),
                colors = CardDefaults.cardColors(
                    containerColor = if (sfx.isDroid) Color(0xFF172033) else CardSurface
                ),
                border = BorderStroke(
                    1.dp,
                    if (sfx.isDroid) AetherCyan else CardBorder
                )
            ) {
                Column(
                    modifier = Modifier.padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Icon(
                            if (sfx.isDroid) Icons.Default.Star else Icons.Default.PlayArrow,
                            contentDescription = null,
                            tint = if (sfx.isDroid) RunicGold else AetherCyan,
                            modifier = Modifier.size(16.dp)
                        )
                        Text(
                            text = if (sfx.isDroid) "DROID" else "SFX #${sfx.id}",
                            color = if (sfx.isDroid) RunicGold else TextMuted,
                            fontSize = 9.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    Text(
                        text = sfx.name,
                        color = TextPrimary,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 2
                    )
                }
            }
        }
    }
}

@Composable
private fun SoundCombosView(viewModel: DashboardViewModel) {
    val soundCombos = listOf(
        Triple("🦆 Quantum Quack", listOf(2, 9, 6), "Alien • Panic • Nova"),
        Triple("🤖 Droid Hysteria", listOf(7, 8, 9), "Chirp • Banter • Scream"),
        Triple("⚡ Plasma Storm", listOf(3, 0, 6), "Laser • Warp • Nova"),
        Triple("🌀 Gravity Sneeze", listOf(4, 2, 6), "Tractor • Alien • Nova"),
        Triple("🌌 Astral Rift", listOf(5, 0, 7), "Portal • Warp • Chirp"),
        Triple("💰 Cha-Ching Auction", listOf(10, 3, 6), "Cha-Ching • Laser • Gavel"),
        Triple("🪄 Lumos Flare", listOf(12, 5, 1), "Lumos • Portal • Shimmer"),
        Triple("🦅 Expecto Patronum", listOf(13, 0, 7), "Patronus • Warp • Chirp"),
        Triple("🏥 PokéCenter Healing", listOf(11, 7), "Pokécenter Jingle • Droid Chirp")
    )

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        items(soundCombos) { (name, sounds, desc) ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .clickable { viewModel.playSoundSequence(sounds) },
                shape = RoundedCornerShape(12.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, RunicGold)
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(14.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = RunicGold, modifier = Modifier.size(16.dp))
                            Text(text = name, color = RunicGold, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                        }
                        Text(text = desc, color = TextMuted, fontSize = 10.sp)
                    }

                    Icon(Icons.Default.PlayArrow, contentDescription = null, tint = RunicGold, modifier = Modifier.size(20.dp))
                }
            }
        }
    }
}

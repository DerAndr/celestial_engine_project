package com.celestial.companion.ui.screens.soundboard

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.theme.*

@Composable
fun SoundboardScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Stop audio bar
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "INTERACTIVE SOUNDBOARD",
                color = AetherCyan,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace
            )

            Button(
                onClick = { viewModel.stopAudio() },
                colors = ButtonDefaults.buttonColors(containerColor = Color(0x33FF5252)),
                border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFFFF5252)),
                shape = RoundedCornerShape(8.dp),
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
            ) {
                Icon(Icons.Default.Stop, contentDescription = null, tint = Color(0xFFFF5252), modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(4.dp))
                Text("Mute", color = Color(0xFFFF5252), fontSize = 12.sp)
            }
        }

        // Sound Combos / Смешные наборы звуков
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "SPELL SOUND COMBOS",
                    color = RunicGold,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace
                )
                Text(
                    text = "400ms combo",
                    color = TextMuted,
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
                val soundCombos = listOf(
                    Triple("🦆 Quantum Quack", listOf(2, 9, 6), "Alien • Panic • Nova"),
                    Triple("🤖 Droid Hysteria", listOf(7, 8, 9), "Chirp • Banter • Scream"),
                    Triple("⚡ Plasma Storm", listOf(3, 0, 6), "Laser • Warp • Nova"),
                    Triple("🌀 Gravity Sneeze", listOf(4, 2, 6), "Tractor • Alien • Nova"),
                    Triple("🌌 Astral Rift", listOf(5, 0, 7), "Portal • Warp • Chirp")
                )

                soundCombos.forEach { (name, sounds, desc) ->
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(DeepVoid)
                            .border(1.dp, RunicGold, RoundedCornerShape(8.dp))
                            .clickable { viewModel.playSoundSequence(sounds) }
                            .padding(horizontal = 10.dp, vertical = 6.dp)
                    ) {
                        Column {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = RunicGold, modifier = Modifier.size(12.dp))
                                Text(text = name, color = RunicGold, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                            }
                            Text(text = desc, color = TextMuted, fontSize = 9.sp)
                        }
                    }
                }
            }
        }

        // SFX Grid
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
                    border = androidx.compose.foundation.BorderStroke(
                        1.dp,
                        if (sfx.isDroid) AetherCyan else CardBorder
                    )
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Icon(
                                if (sfx.isDroid) Icons.Default.Star else Icons.Default.PlayArrow,
                                contentDescription = null,
                                tint = if (sfx.isDroid) RunicGold else AetherCyan,
                                modifier = Modifier.size(18.dp)
                            )
                            Text(
                                text = if (sfx.isDroid) "DROID" else "SCI-FI",
                                color = if (sfx.isDroid) RunicGold else TextMuted,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        Text(
                            text = sfx.name,
                            color = TextPrimary,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            maxLines = 2
                        )
                    }
                }
            }
        }
    }
}

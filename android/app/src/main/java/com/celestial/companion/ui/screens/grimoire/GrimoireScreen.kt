package com.celestial.companion.ui.screens.grimoire

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
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
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.leds.ConcentricLedRingsView
import com.celestial.companion.ui.theme.*

private val RANDOM_SPELL_NAMES = listOf(
    "Astral Fireball", "Quantum Singularity", "Plasma Storm", "Aegis Shield",
    "Chrono Flash", "Frost Nova", "Thunder Strike", "Ouroboros Serpent",
    "Supernova Spark", "Cosmic Vortex", "Gravitational Hammer", "Phantom Scanner",
    "Celestial Flare", "Hyperdrive Burst", "Aether Ward", "Solar Flare"
)

private val RANDOM_INCANTATIONS = listOf(
    "Ignis plasma infernum, ardet caelum!",
    "Fulgur aetheris catena, tonitrus fractura!",
    "Aegis sanctum praesidium, nullus transitus!",
    "Vortex chao gravitatis, atra foramen!",
    "Ouroboros serpens aeterna, volvitur orbis!",
    "Quantus anas quackus maximus, zzzz-wub-wub!",
    "Glacies nebula congelatio, frigus absolutum!",
    "Astrum galaxia universum, stellae micantes!",
    "Cucurbita umbra nocturna, tenebrae surgunt!",
    "Bip-boop astromech hysterium, waaaah!",
    "Chronos barada nikto, tempestas invocatus!"
)

enum class SpellElement(val title: String, val color: Color, val icon: String) {
    FIRE("Fire", Color(0xFFFF5722), "🔥"),
    LIGHTNING("Lightning", Color(0xFFFFEB3B), "⚡"),
    FROST("Frost", Color(0xFF00E5FF), "❄️"),
    COSMIC("Cosmic", Color(0xFFE040FB), "🌌"),
    ARCANE("Arcane", Color(0xFFB388FF), "🔮"),
    VOID("Void", Color(0xFF7C4DFF), "🌀"),
    HOLY("Holy", Color(0xFFFFD700), "✨")
}

@Composable
fun GrimoireScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    var selectedTab by remember { mutableStateOf(0) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
    ) {
        // Top Navigation Tabs: GRIMOIRE vs SPELL FORGE
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
                            "GRIMOIRE LIBRARY",
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
                            Icons.Default.Build,
                            contentDescription = null,
                            tint = if (selectedTab == 1) AetherCyan else TextMuted,
                            modifier = Modifier.size(16.dp)
                        )
                        Text(
                            "SPELL FORGE",
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
            GrimoireLibraryTab(
                viewModel = viewModel,
                onNavigateToForge = { selectedTab = 1 }
            )
        } else {
            SpellForgeTab(
                viewModel = viewModel,
                onSpellSaved = { selectedTab = 0 }
            )
        }
    }
}

@Composable
private fun GrimoireLibraryTab(
    viewModel: DashboardViewModel,
    onNavigateToForge: () -> Unit
) {
    val customSpells by viewModel.customSpells.collectAsState()
    val activeSpell by viewModel.activeSpell.collectAsState()
    val allSpells = customSpells + SpellDefinition.PRESET_SPELLS
    var searchQuery by remember { mutableStateOf("") }
    var selectedFilter by remember { mutableStateOf("ALL") }

    val filteredSpells = remember(allSpells, searchQuery, selectedFilter) {
        allSpells.filter { spell ->
            val matchesSearch = searchQuery.isBlank() ||
                    spell.name.contains(searchQuery, ignoreCase = true) ||
                    spell.chantText.contains(searchQuery, ignoreCase = true) ||
                    spell.description.contains(searchQuery, ignoreCase = true)

            val matchesFilter = when (selectedFilter) {
                "ALL" -> true
                "CUSTOM" -> spell.isCustom
                "ANCIENT" -> !spell.isCustom
                "FIRE" -> spell.name.contains("fire", true) || spell.oledText.contains("fire", true) || spell.animation == LedAnimation.PLASMA
                "LIGHTNING" -> spell.name.contains("lightning", true) || spell.animation == LedAnimation.RADIAL_WAVE
                "COSMIC" -> spell.animation in listOf(LedAnimation.VORTEX, LedAnimation.GALAXY, LedAnimation.PULSAR)
                "FROST" -> spell.name.contains("frost", true) || spell.animation == LedAnimation.STARS
                else -> true
            }
            matchesSearch && matchesFilter
        }
    }

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Header Card with Live Status & Quick Actions
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
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
                                text = "ARCANE GRIMOIRE",
                                color = RunicGold,
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                            Text(
                                text = "${allSpells.size} Spells Inscribed (${customSpells.size} Custom)",
                                color = TextSecondary,
                                fontSize = 11.sp
                            )
                        }

                        Button(
                            onClick = onNavigateToForge,
                            shape = RoundedCornerShape(10.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = AetherCyan, contentColor = SpaceBlack),
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.Add, contentDescription = null, modifier = Modifier.size(16.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("FORGE SPELL", fontSize = 11.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                        }
                    }

                    if (activeSpell != null) {
                        HorizontalDivider(color = CardBorder, thickness = 1.dp)
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .background(DeepVoid)
                                .padding(horizontal = 10.dp, vertical = 6.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                Box(
                                    modifier = Modifier
                                        .size(8.dp)
                                        .clip(CircleShape)
                                        .background(EmeraldGreen)
                                )
                                Text(
                                    text = "ACTIVE: ${activeSpell?.name?.uppercase()}",
                                    color = EmeraldGreen,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                            TextButton(
                                onClick = { viewModel.clearActiveSpell() },
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp)
                            ) {
                                Text("CLEAR", color = TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                            }
                        }
                    }
                }
            }
        }

        // Search & Filter Row
        item {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = searchQuery,
                    onValueChange = { searchQuery = it },
                    placeholder = { Text("Search spells, chants, elements...", color = TextMuted, fontSize = 12.sp) },
                    leadingIcon = { Icon(Icons.Default.Search, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(18.dp)) },
                    trailingIcon = {
                        if (searchQuery.isNotEmpty()) {
                            IconButton(onClick = { searchQuery = "" }) {
                                Icon(Icons.Default.Clear, contentDescription = null, tint = TextMuted, modifier = Modifier.size(16.dp))
                            }
                        }
                    },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(12.dp),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedContainerColor = DeepVoid,
                        unfocusedContainerColor = CardSurface,
                        focusedBorderColor = AetherCyan,
                        unfocusedBorderColor = CardBorder,
                        focusedTextColor = TextPrimary,
                        unfocusedTextColor = TextPrimary
                    )
                )

                // Filter Chips Strip
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    val filterTags = listOf("ALL", "CUSTOM", "ANCIENT", "FIRE", "LIGHTNING", "COSMIC", "FROST")
                    filterTags.forEach { tag ->
                        val isSelected = selectedFilter == tag
                        FilterChip(
                            selected = isSelected,
                            onClick = { selectedFilter = tag },
                            label = { Text(tag, fontSize = 10.sp, fontFamily = FontFamily.Monospace) },
                            colors = FilterChipDefaults.filterChipColors(
                                selectedContainerColor = Color(0x3300E5FF),
                                selectedLabelColor = AetherCyan,
                                containerColor = CardSurface,
                                labelColor = TextSecondary
                            ),
                            border = FilterChipDefaults.filterChipBorder(
                                enabled = true,
                                selected = isSelected,
                                borderColor = if (isSelected) AetherCyan else CardBorder
                            )
                        )
                    }
                }
            }
        }

        // Spells List
        items(filteredSpells) { spell ->
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, if (spell.isCustom) Color(0x5500E5FF) else CardBorder)
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
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text(
                                text = spell.name,
                                color = TextPrimary,
                                fontSize = 15.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                            if (spell.isCustom) {
                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(4.dp))
                                        .background(Color(0x3300E5FF))
                                        .padding(horizontal = 6.dp, vertical = 2.dp)
                                ) {
                                    Text("CUSTOM", color = AetherCyan, fontSize = 9.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                                }
                            }
                        }

                        if (spell.isCustom) {
                            IconButton(
                                onClick = { viewModel.deleteCustomSpell(spell.name) },
                                modifier = Modifier.size(24.dp)
                            ) {
                                Icon(Icons.Default.Delete, contentDescription = "Delete", tint = Color(0xFFFF5252), modifier = Modifier.size(16.dp))
                            }
                        }
                    }

                    // Metadata Badges (Animation & SFX)
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(DeepVoid)
                                .border(1.dp, NebulaPurple, RoundedCornerShape(6.dp))
                                .padding(horizontal = 8.dp, vertical = 3.dp)
                        ) {
                            Text(
                                text = "🌀 ${spell.animation.title}",
                                color = NebulaPurple,
                                fontSize = 10.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(DeepVoid)
                                .border(1.dp, CardBorder, RoundedCornerShape(6.dp))
                                .padding(horizontal = 8.dp, vertical = 3.dp)
                        ) {
                            Text(
                                text = "🔊 ${spell.sounds.size} SFX Combo",
                                color = TextSecondary,
                                fontSize = 10.sp,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }

                    // Incantation Quote
                    if (spell.chantText.isNotBlank()) {
                        Text(
                            text = "\"${spell.chantText}\"",
                            color = RunicGold,
                            fontSize = 12.sp,
                            fontFamily = FontFamily.Monospace,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(6.dp))
                                .background(Color(0x11FFD700))
                                .padding(horizontal = 8.dp, vertical = 6.dp)
                        )
                    }

                    // Quick Cast Action Button
                    Button(
                        onClick = { viewModel.castSpell(spell, speakIncantation = true) },
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(10.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = RunicGold, contentColor = SpaceBlack)
                    ) {
                        Icon(Icons.Default.Bolt, contentDescription = null, tint = SpaceBlack, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text("QUICK CAST ON STAFF", fontWeight = FontWeight.Bold, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
    }
}

@Composable
private fun SpellForgeTab(
    viewModel: DashboardViewModel,
    onSpellSaved: () -> Unit
) {
    val telemetry by viewModel.telemetry.collectAsState()

    var spellName by remember { mutableStateOf("Astral Fireball") }
    var selectedElement by remember { mutableStateOf(SpellElement.FIRE) }
    var selectedAnim by remember { mutableStateOf(LedAnimation.PLASMA) }
    var soundSlots by remember { mutableStateOf(listOf(3, 6, 1)) }
    var oledText by remember { mutableStateOf("🔥 FIREBALL!") }
    var chantText by remember { mutableStateOf("Ignis plasma infernum, ardet caelum!") }
    var speakIncantation by remember { mutableStateOf(true) }
    var successBanner by remember { mutableStateOf<String?>(null) }
    var isGeneratingChant by remember { mutableStateOf(false) }

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        if (successBanner != null) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(12.dp),
                    colors = CardDefaults.cardColors(containerColor = Color(0x2200E676)),
                    border = BorderStroke(1.dp, EmeraldGreen)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(successBanner ?: "", color = EmeraldGreen, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                        IconButton(onClick = { successBanner = null }, modifier = Modifier.size(20.dp)) {
                            Icon(Icons.Default.Close, contentDescription = null, tint = EmeraldGreen)
                        }
                    }
                }
            }
        }

        // Section 1: Spell Identity & Element
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Text(
                        text = "1. SPELL IDENTITY & ELEMENT",
                        color = AetherCyan,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    OutlinedTextField(
                        value = spellName,
                        onValueChange = { spellName = it },
                        label = { Text("Spell Name") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(10.dp),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedContainerColor = DeepVoid,
                            unfocusedContainerColor = CardSurface,
                            focusedBorderColor = AetherCyan,
                            unfocusedBorderColor = CardBorder,
                            focusedTextColor = TextPrimary,
                            unfocusedTextColor = TextPrimary
                        ),
                        trailingIcon = {
                            IconButton(onClick = {
                                spellName = RANDOM_SPELL_NAMES.random()
                                chantText = RANDOM_INCANTATIONS.random()
                                oledText = "✨ ${spellName.uppercase()}"
                            }) {
                                Icon(Icons.Default.Casino, contentDescription = "Randomize", tint = RunicGold)
                            }
                        }
                    )

                    // Element Selection Chips
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        SpellElement.entries.forEach { element ->
                            val isSelected = selectedElement == element
                            FilterChip(
                                selected = isSelected,
                                onClick = {
                                    selectedElement = element
                                    when (element) {
                                        SpellElement.FIRE -> selectedAnim = LedAnimation.PLASMA
                                        SpellElement.LIGHTNING -> selectedAnim = LedAnimation.RADIAL_WAVE
                                        SpellElement.FROST -> selectedAnim = LedAnimation.STARS
                                        SpellElement.COSMIC -> selectedAnim = LedAnimation.GALAXY
                                        SpellElement.ARCANE -> selectedAnim = LedAnimation.COLLIDING_WAVES
                                        SpellElement.VOID -> selectedAnim = LedAnimation.VORTEX
                                        SpellElement.HOLY -> selectedAnim = LedAnimation.RAINBOW
                                    }
                                },
                                label = { Text("${element.icon} ${element.title}", fontSize = 11.sp, fontFamily = FontFamily.Monospace) },
                                colors = FilterChipDefaults.filterChipColors(
                                    selectedContainerColor = element.color.copy(alpha = 0.25f),
                                    selectedLabelColor = element.color,
                                    containerColor = CardSurface,
                                    labelColor = TextSecondary
                                ),
                                border = FilterChipDefaults.filterChipBorder(
                                    enabled = true,
                                    selected = isSelected,
                                    borderColor = if (isSelected) element.color else CardBorder
                                )
                            )
                        }
                    }
                }
            }
        }

        // Section 2: Concentric Rings & Visual Animation
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Text(
                        text = "2. 43-LED RINGS VISUAL ANIMATION",
                        color = AetherCyan,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        modifier = Modifier.fillMaxWidth()
                    )

                    // Rings Preview Canvas
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = 4.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        ConcentricLedRingsView(
                            animation = selectedAnim,
                            size = 180.dp,
                            micLevelPct = telemetry.micLevelPercent,
                            aiModelId = telemetry.aiModelId,
                            state = telemetry.state
                        )
                    }

                    // Animation Dropdown / Picker
                    var expandedAnimDropdown by remember { mutableStateOf(false) }
                    Box(modifier = Modifier.fillMaxWidth()) {
                        OutlinedButton(
                            onClick = { expandedAnimDropdown = true },
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(10.dp),
                            border = BorderStroke(1.dp, NebulaPurple),
                            colors = ButtonDefaults.outlinedButtonColors(contentColor = TextPrimary)
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = "ANIM: #${selectedAnim.id} ${selectedAnim.title}",
                                    color = NebulaPurple,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 12.sp
                                )
                                Icon(Icons.Default.ArrowDropDown, contentDescription = null, tint = NebulaPurple)
                            }
                        }

                        DropdownMenu(
                            expanded = expandedAnimDropdown,
                            onDismissRequest = { expandedAnimDropdown = false },
                            modifier = Modifier
                                .background(DeepVoid)
                                .heightIn(max = 280.dp)
                        ) {
                            LedAnimation.entries.forEach { anim ->
                                DropdownMenuItem(
                                    text = {
                                        Column {
                                            Text(anim.title, color = TextPrimary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                                            Text(anim.subtitle, color = TextMuted, fontSize = 10.sp)
                                        }
                                    },
                                    onClick = {
                                        selectedAnim = anim
                                        expandedAnimDropdown = false
                                    }
                                )
                            }
                        }
                    }
                }
            }
        }

        // Section 3: Acoustic SFX Sequencer
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
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
                        Text(
                            text = "3. SOUND FX SEQUENCE",
                            color = AetherCyan,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Button(
                            onClick = { viewModel.playSoundSequence(soundSlots) },
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = DeepVoid, contentColor = AetherCyan),
                            border = BorderStroke(1.dp, AetherCyan),
                            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp)
                        ) {
                            Icon(Icons.Default.PlayArrow, contentDescription = null, modifier = Modifier.size(14.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("PREVIEW", fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                        }
                    }

                    // Combo Slots Strip
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        soundSlots.forEachIndexed { index, sfxId ->
                            val sfxName = SfxItem.ALL_SFX.find { it.id == sfxId }?.name ?: "SFX #$sfxId"
                            Card(
                                shape = RoundedCornerShape(8.dp),
                                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                                border = BorderStroke(1.dp, CardBorder)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                                ) {
                                    Text("${index + 1}: $sfxName", color = TextPrimary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                                    if (soundSlots.size > 1) {
                                        IconButton(
                                            onClick = {
                                                soundSlots = soundSlots.filterIndexed { i, _ -> i != index }
                                            },
                                            modifier = Modifier.size(16.dp)
                                        ) {
                                            Icon(Icons.Default.Close, contentDescription = null, tint = TextMuted, modifier = Modifier.size(12.dp))
                                        }
                                    }
                                }
                            }
                        }

                        if (soundSlots.size < 4) {
                            OutlinedButton(
                                onClick = {
                                    val nextId = (0..10).filter { !soundSlots.contains(it) }.randomOrNull() ?: 1
                                    soundSlots = soundSlots + nextId
                                },
                                shape = RoundedCornerShape(8.dp),
                                contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp)
                            ) {
                                Icon(Icons.Default.Add, contentDescription = null, modifier = Modifier.size(14.dp))
                                Spacer(Modifier.width(4.dp))
                                Text("ADD STEP", fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                            }
                        }
                    }
                }
            }
        }

        // Section 4: Incantation & OLED Marquee
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "4. INCANTATION & MARQUEE",
                        color = AetherCyan,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    OutlinedTextField(
                        value = chantText,
                        onValueChange = { chantText = it },
                        label = { Text("Spoken Incantation Formula") },
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(10.dp),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedContainerColor = DeepVoid,
                            unfocusedContainerColor = CardSurface,
                            focusedBorderColor = RunicGold,
                            unfocusedBorderColor = CardBorder,
                            focusedTextColor = TextPrimary,
                            unfocusedTextColor = TextPrimary
                        ),
                        trailingIcon = {
                            IconButton(onClick = {
                                chantText = RANDOM_INCANTATIONS.random()
                            }) {
                                Icon(Icons.Default.Casino, contentDescription = "Random Chant", tint = RunicGold)
                            }
                        }
                    )

                    OutlinedTextField(
                        value = oledText,
                        onValueChange = { oledText = it },
                        label = { Text("Staff OLED Marquee Text") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(10.dp),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedContainerColor = DeepVoid,
                            unfocusedContainerColor = CardSurface,
                            focusedBorderColor = AetherCyan,
                            unfocusedBorderColor = CardBorder,
                            focusedTextColor = TextPrimary,
                            unfocusedTextColor = TextPrimary
                        )
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Vocalize incantation with TTS on cast:", color = TextPrimary, fontSize = 12.sp)
                        Switch(
                            checked = speakIncantation,
                            onCheckedChange = { speakIncantation = it },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = AetherCyan,
                                checkedTrackColor = Color(0x3300E5FF)
                            )
                        )
                    }
                }
            }
        }

        // Section 5: Cast & Save Actions
        item {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                // Test Cast Button
                Button(
                    onClick = {
                        val finalSpell = SpellDefinition(
                            name = spellName.ifBlank { "Custom Spell" },
                            animation = selectedAnim,
                            sounds = soundSlots.ifEmpty { listOf(3, 6, 1) },
                            prompt = "Cast the ${spellName} spell! [SPELL: ${spellName} | ANIM: ${selectedAnim.name} | SOUNDS: ${soundSlots.joinToString(", ")}] $chantText",
                            description = "${selectedAnim.title} • ${soundSlots.size} SFX",
                            chantText = chantText,
                            oledText = oledText.ifBlank { "[SPELL] $spellName" },
                            isCustom = true
                        )
                        viewModel.castSpell(finalSpell, speakIncantation = speakIncantation)
                        successBanner = "✨ Cast «$spellName» directly to staff!"
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(52.dp),
                    shape = RoundedCornerShape(12.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = RunicGold, contentColor = SpaceBlack)
                ) {
                    Icon(Icons.Default.Bolt, contentDescription = null, tint = SpaceBlack, modifier = Modifier.size(20.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("⚡ TEST CAST ON STAFF", fontSize = 14.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                }

                // Save to Grimoire Button
                OutlinedButton(
                    onClick = {
                        val finalSpell = SpellDefinition(
                            name = spellName.ifBlank { "Custom Spell" },
                            animation = selectedAnim,
                            sounds = soundSlots.ifEmpty { listOf(3, 6, 1) },
                            prompt = "Cast the ${spellName} spell! [SPELL: ${spellName} | ANIM: ${selectedAnim.name} | SOUNDS: ${soundSlots.joinToString(", ")}] $chantText",
                            description = "${selectedAnim.title} • ${soundSlots.size} SFX",
                            chantText = chantText,
                            oledText = oledText.ifBlank { "[SPELL] $spellName" },
                            isCustom = true
                        )
                        viewModel.saveCustomSpell(finalSpell)
                        successBanner = "💾 «$spellName» inscribed into Grimoire!"
                        onSpellSaved()
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(46.dp),
                    shape = RoundedCornerShape(12.dp),
                    border = BorderStroke(1.dp, AetherCyan),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = AetherCyan)
                ) {
                    Icon(Icons.Default.BookmarkAdd, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("💾 SAVE TO GRIMOIRE", fontSize = 13.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                }
            }
        }
    }
}

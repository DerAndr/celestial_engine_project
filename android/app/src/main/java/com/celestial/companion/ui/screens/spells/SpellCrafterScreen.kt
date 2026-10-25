package com.celestial.companion.ui.screens.spells

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.leds.ConcentricLedRingsView
import com.celestial.companion.ui.theme.*

private val RANDOM_SPELL_NAMES = listOf(
    "Астральный Фаербол", "Квантовый Кряк", "Сингулярный Чих", "Плазменный Шторм",
    "Эгида Забвения", "Хроно-Вспышка", "Ледяная Стужа", "Громовой Разряд",
    "Уроборос Времени", "Сверхновая Искра", "Звездный Вихрь", "Гравитационный Молот",
    "Кошачья Магия", "Космический Дроид", "Вспышка Сверхновой", "Призрачный Сканер"
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

@Composable
fun SpellCrafterScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val telemetry by viewModel.telemetry.collectAsState()
    val customSpells by viewModel.customSpells.collectAsState()
    val activeSpell by viewModel.activeSpell.collectAsState()

    // Crafter State
    var spellName by remember { mutableStateOf("Астральный Фаербол") }
    var selectedAnim by remember { mutableStateOf(LedAnimation.PLASMA) }
    var soundSlots by remember { mutableStateOf(listOf(3, 6, 1)) } // Default: Laser -> Supernova -> Pulsar
    var oledText by remember { mutableStateOf("🔥 FIREBALL!") }
    var chantText by remember { mutableStateOf("Ignis plasma infernum, ardet caelum!") }
    var speakIncantation by remember { mutableStateOf(true) }
    var castSuccessBanner by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(castSuccessBanner) {
        if (castSuccessBanner != null) {
            kotlinx.coroutines.delay(3500)
            castSuccessBanner = null
        }
    }

    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        // 1. Header Banner
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(14.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = RunicGold, modifier = Modifier.size(18.dp))
                            Text(
                                text = "SPELL CRAFTER",
                                color = RunicGold,
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                        Text(
                            text = "Визуальный конструктор заклинаний",
                            color = TextSecondary,
                            fontSize = 11.sp
                        )
                    }

                    // Online indicator
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(12.dp))
                            .background(if (telemetry.isOnline) Color(0x2200E676) else Color(0x22FF5252))
                            .padding(horizontal = 8.dp, vertical = 4.dp)
                    ) {
                        Text(
                            text = if (telemetry.isOnline) "🟢 SYNCED" else "🔴 OFFLINE",
                            color = if (telemetry.isOnline) EmeraldGreen else Color.Red,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // Cast Success Feedback Banner
        castSuccessBanner?.let { banner ->
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(12.dp),
                    colors = CardDefaults.cardColors(containerColor = Color(0x2200E5FF)),
                    border = BorderStroke(1.dp, AetherCyan)
                ) {
                    Row(
                        modifier = Modifier.padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Icon(Icons.Default.Check, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(20.dp))
                        Text(text = banner, color = TextPrimary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }

        // 2. Section: 🔮 ВИЗУАЛ (Visual Preview & Animation Selector)
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "1. ВИЗУАЛ • 43 LED КОЛЬЦА",
                            color = AetherCyan,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = selectedAnim.title,
                            color = NebulaPurple,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // Live 43-LED concentric rings canvas running the selected animation
                    Box(
                        modifier = Modifier
                            .size(190.dp)
                            .padding(4.dp),
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

                    Text(
                        text = selectedAnim.subtitle,
                        color = TextSecondary,
                        fontSize = 11.sp
                    )

                    HorizontalDivider(color = CardBorder, thickness = 1.dp)

                    // Animation Selector Chips
                    Text(
                        text = "ВЫБЕРИТЕ АНИМАЦИЮ (${LedAnimation.entries.size} РЕЖИМОВ):",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        modifier = Modifier.fillMaxWidth()
                    )

                    val animEntries = LedAnimation.entries.toTypedArray()
                    LazyRow(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        items(animEntries) { anim ->
                            val isSelected = anim == selectedAnim
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(if (isSelected) NebulaPurple else DeepVoid)
                                    .border(1.dp, if (isSelected) NebulaPurple else CardBorder, RoundedCornerShape(8.dp))
                                    .clickable { selectedAnim = anim }
                                    .padding(horizontal = 10.dp, vertical = 6.dp)
                            ) {
                                Text(
                                    text = anim.title,
                                    color = if (isSelected) SpaceBlack else TextPrimary,
                                    fontSize = 11.sp,
                                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }
                    }
                }
            }
        }

        // 3. Section: 🔊 ЗВУКИ (Sound Sequence Builder)
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
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
                        Text(
                            text = "2. ЗВУКИ • ЦЕПОЧКА SFX (1-4 СЛОТА)",
                            color = RunicGold,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            // Test sound combo button
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(Color(0x3300E5FF))
                                    .clickable { viewModel.playSoundSequence(soundSlots) }
                                    .padding(horizontal = 8.dp, vertical = 4.dp)
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(2.dp)
                                ) {
                                    Icon(Icons.Default.PlayArrow, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(14.dp))
                                    Text("Тест", color = AetherCyan, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                                }
                            }

                            // Randomize sequence button
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(Color(0x33FFB300))
                                    .clickable {
                                        val count = (2..4).random()
                                        soundSlots = (0 until count).map { (0..9).random() }
                                    }
                                    .padding(horizontal = 8.dp, vertical = 4.dp)
                            ) {
                                Text("🎲 Рандом", color = RunicGold, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                            }
                        }
                    }

                    // Active Sequence Slots
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        soundSlots.forEachIndexed { index, sfxId ->
                            val sfxName = SfxItem.ALL_SFX.find { it.id == sfxId }?.name ?: "SFX #$sfxId"
                            Card(
                                modifier = Modifier
                                    .weight(1f)
                                    .clip(RoundedCornerShape(8.dp)),
                                shape = RoundedCornerShape(8.dp),
                                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                                border = BorderStroke(1.dp, RunicGold)
                            ) {
                                Column(
                                    modifier = Modifier.padding(6.dp),
                                    horizontalAlignment = Alignment.CenterHorizontally,
                                    verticalArrangement = Arrangement.spacedBy(2.dp)
                                ) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(text = "#${index + 1}", color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
                                        if (soundSlots.size > 1) {
                                            Icon(
                                                Icons.Default.Close,
                                                contentDescription = "Remove",
                                                tint = Color.Red,
                                                modifier = Modifier
                                                    .size(12.dp)
                                                    .clickable {
                                                        soundSlots = soundSlots.filterIndexed { i, _ -> i != index }
                                                    }
                                            )
                                        }
                                    }
                                    Text(
                                        text = sfxName,
                                        color = TextPrimary,
                                        fontSize = 10.sp,
                                        fontWeight = FontWeight.Bold,
                                        maxLines = 1
                                    )
                                    Box(
                                        modifier = Modifier
                                            .clip(RoundedCornerShape(4.dp))
                                            .background(Color(0x2200E5FF))
                                            .clickable { viewModel.playSfx(sfxId) }
                                            .padding(horizontal = 4.dp, vertical = 2.dp)
                                    ) {
                                        Text("▶", color = AetherCyan, fontSize = 8.sp)
                                    }
                                }
                            }
                        }

                        if (soundSlots.size < 4) {
                            Box(
                                modifier = Modifier
                                    .size(36.dp)
                                    .clip(CircleShape)
                                    .background(DeepVoid)
                                    .border(1.dp, CardBorder, CircleShape)
                                    .clickable { soundSlots = soundSlots + listOf((0..9).random()) },
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(Icons.Default.Add, contentDescription = "Add slot", tint = AetherCyan, modifier = Modifier.size(18.dp))
                            }
                        }
                    }

                    // Sound Palette Chips (Click to append to sequence)
                    Text(
                        text = "НАЖМИТЕ ЗВУК ДЛЯ ДОБАВЛЕНИЯ В ЦЕПОЧКУ:",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace
                    )

                    LazyRow(
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        items(SfxItem.ALL_SFX) { sfx ->
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(DeepVoid)
                                    .border(1.dp, if (sfx.isDroid) RunicGold else CardBorder, RoundedCornerShape(6.dp))
                                    .clickable {
                                        if (soundSlots.size < 4) {
                                            soundSlots = soundSlots + sfx.id
                                        } else {
                                            soundSlots = soundSlots.drop(1) + sfx.id
                                        }
                                        viewModel.playSfx(sfx.id)
                                    }
                                    .padding(horizontal = 8.dp, vertical = 6.dp)
                            ) {
                                Text(
                                    text = sfx.name,
                                    color = if (sfx.isDroid) RunicGold else TextPrimary,
                                    fontSize = 10.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }
                    }
                }
            }
        }

        // 4. Section: 📜 ТЕКСТ И ЗАКЛИНАНИЕ (Text, OLED & Incantation)
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "3. ТЕКСТ И МАГИЧЕСКАЯ ФОРМУЛА",
                        color = AetherCyan,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    // Spell Name Input
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("Имя заклинания:", color = TextSecondary, fontSize = 11.sp)
                            Text(
                                text = "🎲 Рандом",
                                color = AetherCyan,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier.clickable {
                                    spellName = RANDOM_SPELL_NAMES.random()
                                    oledText = spellName.take(16).uppercase()
                                }
                            )
                        }
                        OutlinedTextField(
                            value = spellName,
                            onValueChange = {
                                spellName = it
                                if (oledText.isBlank() || oledText.startsWith("🔥") || oledText.startsWith("⚡")) {
                                    oledText = it.take(16).uppercase()
                                }
                            },
                            modifier = Modifier.fillMaxWidth(),
                            colors = OutlinedTextFieldDefaults.colors(
                                focusedBorderColor = AetherCyan,
                                unfocusedBorderColor = CardBorder,
                                focusedTextColor = TextPrimary,
                                unfocusedTextColor = TextPrimary
                            ),
                            shape = RoundedCornerShape(8.dp),
                            singleLine = true
                        )
                    }

                    // OLED Display Text Input
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text("Текст на OLED-экране посоха (до 16 символов):", color = TextSecondary, fontSize = 11.sp)
                        OutlinedTextField(
                            value = oledText,
                            onValueChange = { if (it.length <= 20) oledText = it },
                            modifier = Modifier.fillMaxWidth(),
                            colors = OutlinedTextFieldDefaults.colors(
                                focusedBorderColor = EmeraldGreen,
                                unfocusedBorderColor = CardBorder,
                                focusedTextColor = TextPrimary,
                                unfocusedTextColor = TextPrimary
                            ),
                            shape = RoundedCornerShape(8.dp),
                            singleLine = true
                        )
                    }

                    // Incantation Chant Input
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("Магическая формула (псевдо-латынь):", color = TextSecondary, fontSize = 11.sp)
                            Text(
                                text = "✨ Формула",
                                color = RunicGold,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier.clickable {
                                    chantText = RANDOM_INCANTATIONS.random()
                                }
                            )
                        }
                        OutlinedTextField(
                            value = chantText,
                            onValueChange = { chantText = it },
                            modifier = Modifier.fillMaxWidth(),
                            colors = OutlinedTextFieldDefaults.colors(
                                focusedBorderColor = NebulaPurple,
                                unfocusedBorderColor = CardBorder,
                                focusedTextColor = TextPrimary,
                                unfocusedTextColor = TextPrimary
                            ),
                            shape = RoundedCornerShape(8.dp),
                            minLines = 2,
                            maxLines = 3
                        )
                    }

                    // TTS spoken checkbox
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text("Озвучивать формулу через TTS / AI:", color = TextPrimary, fontSize = 12.sp)
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

        // 5. Section: 🚀 КАСТАНУТЬ И СОХРАНИТЬ (Cast & Save Actions)
        item {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                // Big Glowing Cast Button
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
                        castSuccessBanner = "✨ Заклинание «$spellName» мгновенно кастануто на посох!"
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp),
                    shape = RoundedCornerShape(14.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = RunicGold,
                        contentColor = SpaceBlack
                    )
                ) {
                    Icon(Icons.Default.Bolt, contentDescription = null, tint = SpaceBlack, modifier = Modifier.size(24.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "🚀 КАСТАНУТЬ НА ПОСОХ!",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )
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
                        castSuccessBanner = "💾 «$spellName» сохранено в Гримуар!"
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(44.dp),
                    shape = RoundedCornerShape(10.dp),
                    border = BorderStroke(1.dp, NebulaPurple),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = NebulaPurple)
                ) {
                    Icon(Icons.Default.Bookmark, contentDescription = null, tint = NebulaPurple, modifier = Modifier.size(16.dp))
                    Spacer(modifier = Modifier.width(6.dp))
                    Text("Сохранить в Гримуар заклинаний", fontSize = 12.sp, fontWeight = FontWeight.Bold)
                }
            }
        }

        // 6. Section: 📖 ГРИМУАР ЗАКЛИНАНИЙ (Grimoire Library)
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = CardSurface),
                border = BorderStroke(1.dp, CardBorder)
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
                        Text(
                            text = "ГРИМУАР (${customSpells.size} СОХРАНЁННЫХ • ${SpellDefinition.PRESET_SPELLS.size} ПРЕСЕТОВ)",
                            color = AetherCyan,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // Combined list: custom spells first, then presets
                    val allGrimoireSpells = customSpells + SpellDefinition.PRESET_SPELLS

                    LazyRow(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        items(allGrimoireSpells) { spell ->
                            val isCurrentActive = activeSpell?.name == spell.name
                            Card(
                                modifier = Modifier
                                    .width(170.dp)
                                    .clip(RoundedCornerShape(10.dp))
                                    .clickable {
                                        // Load spell into crafter fields
                                        spellName = spell.name
                                        selectedAnim = spell.animation
                                        soundSlots = spell.sounds
                                        oledText = spell.oledText.ifBlank { spell.name.take(16).uppercase() }
                                        chantText = spell.chantText.ifBlank { "Abracadabra ${spell.name} invocatus!" }
                                    },
                                shape = RoundedCornerShape(10.dp),
                                colors = CardDefaults.cardColors(
                                    containerColor = if (isCurrentActive) DeepVoid else SpaceBlack
                                ),
                                border = BorderStroke(
                                    1.dp,
                                    if (isCurrentActive) RunicGold else if (spell.isCustom) NebulaPurple else CardBorder
                                )
                            ) {
                                Column(
                                    modifier = Modifier.padding(10.dp),
                                    verticalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = if (spell.isCustom) "CUSTOM" else "PRESET",
                                            color = if (spell.isCustom) NebulaPurple else TextMuted,
                                            fontSize = 9.sp,
                                            fontWeight = FontWeight.Bold,
                                            fontFamily = FontFamily.Monospace
                                        )
                                        if (spell.isCustom) {
                                            Icon(
                                                Icons.Default.Delete,
                                                contentDescription = "Delete",
                                                tint = Color.Red,
                                                modifier = Modifier
                                                    .size(14.dp)
                                                    .clickable { viewModel.deleteCustomSpell(spell.name) }
                                            )
                                        }
                                    }

                                    Text(
                                        text = spell.name,
                                        color = if (isCurrentActive) RunicGold else TextPrimary,
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.Bold,
                                        maxLines = 1
                                    )

                                    Text(
                                        text = "✨ ${spell.animation.title}",
                                        color = AetherCyan,
                                        fontSize = 9.sp,
                                        fontFamily = FontFamily.Monospace,
                                        maxLines = 1
                                    )

                                    // Quick cast button
                                    Button(
                                        onClick = {
                                            viewModel.castSpell(spell, speakIncantation = speakIncantation)
                                            castSuccessBanner = "⚡ Каст «${spell.name}» запущен!"
                                        },
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .height(28.dp),
                                        shape = RoundedCornerShape(6.dp),
                                        colors = ButtonDefaults.buttonColors(
                                            containerColor = if (spell.isCustom) NebulaPurple else AetherCyan,
                                            contentColor = SpaceBlack
                                        ),
                                        contentPadding = PaddingValues(0.dp)
                                    ) {
                                        Text("⚡ Каст", fontSize = 10.sp, fontWeight = FontWeight.Bold)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

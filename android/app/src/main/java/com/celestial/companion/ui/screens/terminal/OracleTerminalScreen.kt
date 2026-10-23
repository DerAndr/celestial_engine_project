package com.celestial.companion.ui.screens.terminal

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.media.ExifInterface
import android.net.Uri
import android.speech.tts.TextToSpeech
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import java.io.File
import androidx.compose.animation.core.*
import androidx.compose.animation.*
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
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
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.ai.AiProcessingPhase
import com.celestial.companion.ai.GemmaVariant
import com.celestial.companion.domain.model.SfxItem
import com.celestial.companion.domain.model.SpellDefinition
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.repository.ConnectionMode
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.dashboard.OracleChatMessage
import com.celestial.companion.ui.theme.*
import java.util.Locale

@Composable
fun OracleTerminalScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val clipboardManager = LocalClipboardManager.current
    val telemetry by viewModel.telemetry.collectAsState()
    val connectionMode by viewModel.connectionMode.collectAsState()
    val currentLanguage by viewModel.currentLanguage.collectAsState()
    val isGenerating by viewModel.isGenerating.collectAsState()
    val processingPhase by viewModel.processingPhase.collectAsState()
    val liveElapsedMs by viewModel.liveElapsedMs.collectAsState()
    val liveTokenCount by viewModel.liveTokenCount.collectAsState()
    val liveTokensPerSec by viewModel.liveTokensPerSec.collectAsState()
    val liveModality by viewModel.liveModality.collectAsState()
    val activePrompt by viewModel.activePrompt.collectAsState()
    val activeImage by viewModel.activeImage.collectAsState()
    val chatHistory by viewModel.chatHistory.collectAsState()
    val liveThoughtTrace by viewModel.liveThoughtTrace.collectAsState()
    val liveStreamingText by viewModel.liveStreamingText.collectAsState()
    val gemmaResult by viewModel.gemmaResult.collectAsState()
    val selectedModelVariant by viewModel.selectedModelVariant.collectAsState()
    val selectedBitmap by viewModel.selectedImage.collectAsState()
    val activeSpell by viewModel.activeSpell.collectAsState()

    var promptInput by remember { mutableStateOf("") }
    var thinkingModeEnabled by remember { mutableStateOf(true) }
    var showThinkingCard by remember { mutableStateOf(true) }
    var showHistoryDialog by remember { mutableStateOf(false) }

    // Blinking cursor for live token streaming
    val infiniteTransition = rememberInfiniteTransition(label = "cursorBlink")
    val cursorAlpha by infiniteTransition.animateFloat(
        initialValue = 1f,
        targetValue = 0f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 500, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "cursorAlpha"
    )

    // Android Text-to-Speech engine (Defaults to English, switches dynamically)
    var ttsEngine by remember { mutableStateOf<TextToSpeech?>(null) }
    DisposableEffect(context) {
        var tts: TextToSpeech? = null
        tts = TextToSpeech(context) { status ->
            if (status == TextToSpeech.SUCCESS) {
                tts?.language = currentLanguage.locale
            }
        }
        ttsEngine = tts
        onDispose {
            tts?.stop()
            tts?.shutdown()
        }
    }

    LaunchedEffect(currentLanguage) {
        ttsEngine?.language = currentLanguage.locale
    }

    // Android Speech Recognition (STT - Offline Preferred in Selected Dialect)
    val speechRecognizerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == android.app.Activity.RESULT_OK) {
            val matches = result.data?.getStringArrayListExtra(android.speech.RecognizerIntent.EXTRA_RESULTS)
            val spokenText = matches?.firstOrNull()
            if (!spokenText.isNullOrBlank()) {
                promptInput = spokenText
                viewModel.queryGemma(spokenText, selectedBitmap)
            }
        }
    }

    fun startListening() {
        val intent = android.content.Intent(android.speech.RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE_MODEL, android.speech.RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE, currentLanguage.code)
            putExtra(android.speech.RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, currentLanguage.code)
            putExtra(android.speech.RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
            putExtra(android.speech.RecognizerIntent.EXTRA_PROMPT, "Speak to the Celestial Oracle (${currentLanguage.displayName})...")
        }
        try {
            speechRecognizerLauncher.launch(intent)
        } catch (e: Exception) {
            Toast.makeText(context, "Speech recognition unavailable: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    // Memory-safe Image picker launcher for Vision Multimodality (Gallery)
    val imagePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                val decoded = decodeSampledBitmap(context, uri, maxDimension = 800)
                if (decoded != null) {
                    viewModel.setSelectedImage(decoded)
                } else {
                    Toast.makeText(context, "Failed to decode image safely", Toast.LENGTH_SHORT).show()
                }
            } catch (t: Throwable) {
                Toast.makeText(context, "Error loading image: ${t.message}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    // Direct Camera Photo Capture via FileProvider
    var tempCameraUri by remember { mutableStateOf<Uri?>(null) }

    val takePhotoLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.TakePicture()
    ) { success: Boolean ->
        if (success && tempCameraUri != null) {
            try {
                val decoded = decodeSampledBitmap(context, tempCameraUri!!, maxDimension = 800)
                if (decoded != null) {
                    viewModel.setSelectedImage(decoded)
                } else {
                    Toast.makeText(context, "Failed to decode camera photo", Toast.LENGTH_SHORT).show()
                }
            } catch (t: Throwable) {
                Toast.makeText(context, "Error loading camera photo: ${t.message}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    fun launchCameraIntent() {
        try {
            val photoFile = File(context.cacheDir, "celestial_snap_${System.currentTimeMillis()}.jpg")
            val uri = FileProvider.getUriForFile(
                context,
                "${context.packageName}.fileprovider",
                photoFile
            )
            tempCameraUri = uri
            takePhotoLauncher.launch(uri)
        } catch (e: Exception) {
            Toast.makeText(context, "Camera error: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    val cameraPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (isGranted) {
            launchCameraIntent()
        } else {
            Toast.makeText(context, "Camera permission required to take photo", Toast.LENGTH_SHORT).show()
        }
    }

    fun triggerCameraCapture() {
        val hasCamPermission = ContextCompat.checkSelfPermission(
            context,
            Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED

        if (hasCamPermission) {
            launchCameraIntent()
        } else {
            cameraPermissionLauncher.launch(Manifest.permission.CAMERA)
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        // 1. Connection Mode & Standalone Switcher
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "CONNECTION ARCHITECTURE",
                        color = TextMuted,
                        fontSize = 10.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = connectionMode.title,
                        color = when (connectionMode) {
                            ConnectionMode.STANDALONE_PHONE -> EmeraldGreen
                            ConnectionMode.PHONE_BRAIN_SERVER -> AetherCyan
                            ConnectionMode.MESH_ACTIVE -> RunicGold
                        },
                        fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold
                    )
                }

                // 3 Mode selection pills
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    ConnectionMode.entries.forEach { mode ->
                        val isSelected = mode == connectionMode
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .clip(RoundedCornerShape(8.dp))
                                .background(if (isSelected) DeepVoid else SpaceBlack)
                                .border(
                                    1.dp,
                                    if (isSelected) AetherCyan else CardBorder,
                                    RoundedCornerShape(8.dp)
                                )
                                .clickable { viewModel.setConnectionMode(mode) }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = when (mode) {
                                    ConnectionMode.STANDALONE_PHONE -> "Phone Only"
                                    ConnectionMode.PHONE_BRAIN_SERVER -> "Phone Brain"
                                    ConnectionMode.MESH_ACTIVE -> "Full Mesh"
                                },
                                color = if (isSelected) AetherCyan else TextSecondary,
                                fontSize = 11.sp,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }

                Text(
                    text = connectionMode.subtitle,
                    color = TextSecondary,
                    fontSize = 11.sp
                )
            }
        }

        // 2. Staff Host & Voice Dialect Controller
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Staff Host Switcher
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
                    // Lock to Phone Brain
                    Button(
                        onClick = {
                            viewModel.setStaffHost(2)
                            Toast.makeText(context, "Locked Staff to PHONE BRAIN! Staff OLED will switch.", Toast.LENGTH_SHORT).show()
                        },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 2) EmeraldGreen else DeepVoid
                        ),
                        border = BorderStroke(1.dp, if (telemetry.preferredHost == 2) EmeraldGreen else CardBorder),
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

                    // Switch to Pi 5 Brain
                    Button(
                        onClick = {
                            viewModel.setStaffHost(1)
                            Toast.makeText(context, "Switched Staff to PI 5 BRAIN.", Toast.LENGTH_SHORT).show()
                        },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 1) AetherCyan else DeepVoid
                        ),
                        border = BorderStroke(1.dp, if (telemetry.preferredHost == 1) AetherCyan else CardBorder),
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

                    // Auto Mesh
                    Button(
                        onClick = {
                            viewModel.setStaffHost(0)
                            Toast.makeText(context, "Switched Staff to AUTO MESH.", Toast.LENGTH_SHORT).show()
                        },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (telemetry.preferredHost == 0) RunicGold else DeepVoid
                        ),
                        border = BorderStroke(1.dp, if (telemetry.preferredHost == 0) RunicGold else CardBorder),
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

                // Language Dialect Selector (EN / DE / RU)
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
                                .clickable {
                                    viewModel.setLanguage(lang)
                                    ttsEngine?.language = lang.locale
                                }
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

        // 3. Multimodal Model Header
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = DeepVoid),
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
                    Column(
                        modifier = Modifier.weight(1f),
                        verticalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .clip(CircleShape)
                                    .background(EmeraldGreen)
                            )
                            Text(
                                text = selectedModelVariant.displayName.uppercase(),
                                color = AetherCyan,
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                        Text(
                            text = selectedModelVariant.description,
                            color = TextMuted,
                            fontSize = 10.sp,
                            maxLines = 2
                        )
                    }

                    Spacer(modifier = Modifier.width(8.dp))

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Text(
                            text = if (thinkingModeEnabled) "CoT ON" else "CoT OFF",
                            color = if (thinkingModeEnabled) RunicGold else TextMuted,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Switch(
                            checked = thinkingModeEnabled,
                            onCheckedChange = {
                                thinkingModeEnabled = it
                                viewModel.gemmaEngine?.config = viewModel.gemmaEngine?.config?.copy(enableThinking = it) ?: return@Switch
                            },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = RunicGold,
                                checkedTrackColor = Color(0x33FFD700)
                            )
                        )
                    }
                }

                // Interactive Model Selector Pills
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    listOf(
                        GemmaVariant.GEMMA_4_31B,
                        GemmaVariant.GEMMA_4_26B,
                        GemmaVariant.GEMMA_3_27B,
                        GemmaVariant.GEMINI_FLASH
                    ).forEach { variant ->
                        val isSel = variant == selectedModelVariant
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .clip(RoundedCornerShape(8.dp))
                                .background(if (isSel) SpaceBlack else CardSurface)
                                .border(
                                    1.dp,
                                    if (isSel) AetherCyan else CardBorder,
                                    RoundedCornerShape(8.dp)
                                )
                                .clickable { viewModel.setModelVariant(variant) }
                                .padding(vertical = 6.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = variant.tag,
                                color = if (isSel) AetherCyan else TextSecondary,
                                fontSize = 10.sp,
                                fontWeight = if (isSel) FontWeight.Bold else FontWeight.Normal,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                }
            }
        }

        // --- ARCANE SPELLBOOK & ACTIVE SPELL SYSTEM ---
        activeSpell?.let { spell ->
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                border = BorderStroke(1.5.dp, RunicGold)
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
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(
                                Icons.Default.AutoAwesome,
                                contentDescription = null,
                                tint = RunicGold,
                                modifier = Modifier.size(20.dp)
                            )
                            Text(
                                text = "ACTIVE SPELL: ${spell.name.uppercase()}",
                                color = RunicGold,
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                        IconButton(
                            onClick = { viewModel.clearActiveSpell() },
                            modifier = Modifier.size(24.dp)
                        ) {
                            Icon(
                                Icons.Default.Close,
                                contentDescription = "Dismiss Spell",
                                tint = TextMuted,
                                modifier = Modifier.size(16.dp)
                            )
                        }
                    }

                    // Spell details: LED Anim & Sound combo chain
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        // LED badge
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(CardSurface)
                                .border(1.dp, AetherCyan, RoundedCornerShape(6.dp))
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = "LED: ${spell.animation.name}",
                                color = AetherCyan,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        // Sounds badge
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .clip(RoundedCornerShape(6.dp))
                                .background(CardSurface)
                                .border(1.dp, CardBorder, RoundedCornerShape(6.dp))
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            val soundNames = spell.sounds.map { id ->
                                SfxItem.ALL_SFX.getOrNull(id)?.name?.split(" ")?.firstOrNull() ?: "#$id"
                            }.joinToString(" ➔ ")
                            Text(
                                text = "SFX: $soundNames",
                                color = EmeraldGreen,
                                fontSize = 10.sp,
                                fontFamily = FontFamily.Monospace,
                                maxLines = 1
                            )
                        }
                    }

                    // Replay Audio Sequence & Re-Cast Buttons
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = { viewModel.playSoundSequence(spell.sounds) },
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.buttonColors(containerColor = SpaceBlack),
                            border = BorderStroke(1.dp, EmeraldGreen),
                            shape = RoundedCornerShape(8.dp),
                            contentPadding = PaddingValues(vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.VolumeUp, contentDescription = null, tint = EmeraldGreen, modifier = Modifier.size(14.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Replay Sounds", color = EmeraldGreen, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                        }

                        Button(
                            onClick = { viewModel.castSpell(spell) },
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.buttonColors(containerColor = SpaceBlack),
                            border = BorderStroke(1.dp, RunicGold),
                            shape = RoundedCornerShape(8.dp),
                            contentPadding = PaddingValues(vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = RunicGold, modifier = Modifier.size(14.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Re-Cast Spell", color = RunicGold, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }

        // Grimoire presets and AI conjurer
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "🔮 ARCANE GRIMOIRE & SPELL COMBOS",
                        color = AetherCyan,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    // Random AI Spell Conjurer Button
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(6.dp))
                            .background(DeepVoid)
                            .border(1.dp, RunicGold, RoundedCornerShape(6.dp))
                            .clickable {
                                val aiPrompt = when (currentLanguage) {
                                    StaffLanguage.RUSSIAN -> "Сотвори совершенно безумное космическое заклинание! Дай ему оригинальное английское имя, выбери анимацию посоха, собери хитрую цепочку странных звуков (WARP, PULSAR, ALIEN, LASER, TRACTOR, PORTAL, SUPERNOVA, R2_HAPPY, R2_CHATTER, R2_PANIC), и произнеси ритмичное заклинание в стиле абракадабра!"
                                    StaffLanguage.GERMAN -> "Erschaffe einen völlig verrückten kosmischen Zauberspruch mit englischem Namen, wähle eine LED-Animation, kombiniere seltsame Soundeffekte und sprich eine rhythmische Abracadabra-Zauberformel!"
                                    else -> "Conjure a wild cosmic arcane spell with an English name, select an LED animation, assemble a bizarre chain of sounds, and chant a rhythmic abracadabra incantation formula!"
                                }
                                promptInput = aiPrompt
                                viewModel.queryGemma(aiPrompt)
                            }
                            .padding(horizontal = 8.dp, vertical = 4.dp)
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = RunicGold, modifier = Modifier.size(12.dp))
                            Text(
                                text = "🎲 AI Conjure",
                                color = RunicGold,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }

                Text(
                    text = "Запусти заклинание: светодиоды 43 WS2812B + цепочка забавных звуков динамика MAX98357A:",
                    color = TextMuted,
                    fontSize = 10.sp
                )

                // Scrollable row of preset spells
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    SpellDefinition.PRESET_SPELLS.forEach { spell ->
                        val isCurrent = activeSpell?.name == spell.name
                        Card(
                            modifier = Modifier
                                .width(160.dp)
                                .clip(RoundedCornerShape(10.dp))
                                .clickable {
                                    promptInput = spell.prompt
                                    viewModel.castSpell(spell)
                                },
                            shape = RoundedCornerShape(10.dp),
                            colors = CardDefaults.cardColors(
                                containerColor = if (isCurrent) DeepVoid else SpaceBlack
                            ),
                            border = BorderStroke(
                                1.dp,
                                if (isCurrent) RunicGold else CardBorder
                            )
                        ) {
                            Column(
                                modifier = Modifier.padding(10.dp),
                                verticalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Text(
                                    text = spell.name,
                                    color = if (isCurrent) RunicGold else TextPrimary,
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.Bold,
                                    maxLines = 1
                                )
                                Text(
                                    text = spell.description,
                                    color = TextMuted,
                                    fontSize = 9.sp,
                                    maxLines = 1
                                )
                                Text(
                                    text = "✨ ${spell.animation.name}",
                                    color = AetherCyan,
                                    fontSize = 9.sp,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                        }
                    }
                }
            }
        }

        // 3. Input Console (Prompt + Vision Attachment + Voice)
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Text(
                    text = "MULTIMODAL QUERY INPUT",
                    color = TextSecondary,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace
                )

                // Image Attachment Preview (if any)
                selectedBitmap?.let { bmp ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(10.dp))
                            .background(DeepVoid)
                            .padding(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(10.dp)
                        ) {
                            Image(
                                bitmap = bmp.asImageBitmap(),
                                contentDescription = "Vision Attachment",
                                modifier = Modifier
                                    .size(52.dp)
                                    .clip(RoundedCornerShape(8.dp))
                            )
                            Column {
                                Text(
                                    text = "Vision Token Attached",
                                    color = EmeraldGreen,
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.Bold
                                )
                                Text(
                                    text = "${bmp.width}x${bmp.height} px • Multimodal Input",
                                    color = TextMuted,
                                    fontSize = 10.sp
                                )
                            }
                        }
                        IconButton(onClick = { viewModel.setSelectedImage(null) }) {
                            Icon(Icons.Default.Close, contentDescription = "Remove Image", tint = Color.Red)
                        }
                    }
                }

                // Text Prompt Field
                OutlinedTextField(
                    value = promptInput,
                    onValueChange = { promptInput = it },
                    placeholder = {
                        Text(
                            text = if (selectedBitmap != null) "Describe this artifact or ask Gemma 4 to analyze..." else "Ask the Celestial Oracle or enter a prophecy prompt...",
                            color = TextMuted,
                            fontSize = 13.sp
                        )
                    },
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(10.dp),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedContainerColor = DeepVoid,
                        unfocusedContainerColor = DeepVoid,
                        focusedBorderColor = AetherCyan,
                        unfocusedBorderColor = CardBorder,
                        focusedTextColor = TextPrimary,
                        unfocusedTextColor = TextPrimary
                    ),
                    maxLines = 4
                )

                // Contextual Prompt Chips: Photo Presets vs General Presets
                val isImageAttached = selectedBitmap != null

                val chips: List<Pair<String, String>> = if (isImageAttached) {
                    when (currentLanguage) {
                        StaffLanguage.RUSSIAN -> listOf(
                            "🔍 Что на фото?" to "Подробно опиши, что изображено на этой фотографии, и в чём её особенности.",
                            "✨ Артефакт" to "Проанализируй видимые на фото объекты, материалы, руны или приборы и их назначение.",
                            "📜 Прочесть текст" to "Найди, прочитай и переведи весь видимый текст, надписи или символы на этом фото.",
                            "🔮 Аура и тайна" to "Определи мистическую ауру, потоки космической энергии и тайный смысл этой сцены."
                        )
                        StaffLanguage.GERMAN -> listOf(
                            "🔍 Was ist auf dem Foto?" to "Beschreibe ausführlich, was auf diesem Foto zu sehen ist und erkläre die Details.",
                            "✨ Artefakt-Scan" to "Analysiere die sichtbaren Objekte, Materialien, Runen oder Geräte auf diesem Foto.",
                            "📜 Text lesen" to "Finde und lies alle sichtbaren Texte, Beschriftungen oder Runen auf diesem Bild.",
                            "🔮 Mystische Aura" to "Interpretiere die mystische Aura, Energieflüsse und himmlischen Geheimnisse dieses Bildes."
                        )
                        else -> listOf(
                            "🔍 Describe Photo" to "Describe in detail what you see in this photo and explain its key details.",
                            "✨ Scan Artifact" to "Analyze the physical objects, materials, runes, or instruments in this photo and their purpose.",
                            "📜 Read Text" to "Find, read, and transcribe any visible text, signs, labels, or runes in this image.",
                            "🔮 Celestial Aura" to "Interpret the mystical aura, energy flows, and cosmic secrets of this scene."
                        )
                    }
                } else {
                    listOf(
                        "Anomaly Scan" to "Scan celestial frequencies for reality anomalies and spatial fluctuations.",
                        "Interpret Runes" to "Interpret current glowing rune state on the staff core and outer rings.",
                        "Prophecy" to "Reveal an ancient celestial prophecy for this hour.",
                        "Staff Status" to "Report staff hardware telemetry, tick rate, and Wi-Fi link status."
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    chips.forEach { (label, promptText) ->
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(if (isImageAttached) CardSurface else DeepVoid)
                                .border(1.dp, if (isImageAttached) EmeraldGreen else CardBorder, RoundedCornerShape(6.dp))
                                .clickable {
                                    promptInput = promptText
                                    viewModel.queryGemma(promptText, selectedBitmap)
                                }
                                .padding(horizontal = 8.dp, vertical = 5.dp)
                        ) {
                            Text(
                                text = label,
                                color = if (isImageAttached) EmeraldGreen else AetherCyan,
                                fontSize = 10.sp,
                                fontWeight = if (isImageAttached) FontWeight.Bold else FontWeight.Normal
                            )
                        }
                    }
                }

                // Action Bar: Camera Snap + Gallery Picker + Voice STT + Query Button
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // 1. Direct Camera Snapshot Button
                    IconButton(
                        onClick = { triggerCameraCapture() },
                        modifier = Modifier
                            .clip(RoundedCornerShape(10.dp))
                            .background(DeepVoid)
                            .border(1.dp, if (selectedBitmap != null) EmeraldGreen else CardBorder, RoundedCornerShape(10.dp))
                    ) {
                        Icon(
                            Icons.Default.PhotoCamera,
                            contentDescription = "Take Photo with Camera",
                            tint = if (selectedBitmap != null) EmeraldGreen else AetherCyan
                        )
                    }

                    // 2. Pick Image from Gallery Button
                    IconButton(
                        onClick = { imagePickerLauncher.launch("image/*") },
                        modifier = Modifier
                            .clip(RoundedCornerShape(10.dp))
                            .background(DeepVoid)
                            .border(1.dp, if (selectedBitmap != null) EmeraldGreen else CardBorder, RoundedCornerShape(10.dp))
                    ) {
                        Icon(
                            Icons.Default.Image,
                            contentDescription = "Pick Image from Gallery",
                            tint = if (selectedBitmap != null) EmeraldGreen else AetherCyan
                        )
                    }

                    // 3. Voice STT Speech Recognition Button
                    IconButton(
                        onClick = { startListening() },
                        modifier = Modifier
                            .clip(RoundedCornerShape(10.dp))
                            .background(DeepVoid)
                            .border(1.dp, RunicGold, RoundedCornerShape(10.dp))
                    ) {
                        Icon(
                            Icons.Default.Mic,
                            contentDescription = "Voice STT Speech Recognition (${currentLanguage.shortTag})",
                            tint = RunicGold
                        )
                    }

                    // 4. Query / Stop Button
                    Button(
                        onClick = {
                            if (isGenerating) {
                                viewModel.cancelGeneration()
                            } else {
                                val prompt = promptInput.ifBlank {
                                    if (selectedBitmap != null) {
                                        if (currentLanguage == StaffLanguage.RUSSIAN) "Подробно опиши, что изображено на этой фотографии."
                                        else "Describe in detail what is in this image."
                                    } else "What prophecies do the celestial stars reveal?"
                                }
                                viewModel.queryGemma(prompt, selectedBitmap)
                            }
                        },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (isGenerating) Color(0xFFC62828) else AetherCyan
                        ),
                        shape = RoundedCornerShape(10.dp)
                    ) {
                        if (isGenerating) {
                            Icon(Icons.Default.Stop, contentDescription = "Stop", tint = Color.White, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Stop Generation", color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                        } else {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = SpaceBlack, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Query ${selectedModelVariant.tag}", color = SpaceBlack, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }

        // 4. Live Processing Monitor Bar (Active Telemetry, Phase, Speed, Elapsed Time)
        AnimatedVisibility(
            visible = processingPhase != AiProcessingPhase.IDLE || isGenerating || gemmaResult != null
        ) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                border = BorderStroke(
                    1.dp,
                    if (isGenerating) AetherCyan else CardBorder
                )
            ) {
                Column(
                    modifier = Modifier.padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Phase badge and control header
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(9.dp)
                                    .clip(CircleShape)
                                    .background(
                                        when (processingPhase) {
                                            AiProcessingPhase.INGESTING -> AetherCyan
                                            AiProcessingPhase.THINKING -> RunicGold
                                            AiProcessingPhase.STREAMING -> EmeraldGreen
                                            AiProcessingPhase.COMPLETED -> EmeraldGreen
                                            AiProcessingPhase.CANCELLED -> Color(0xFFFF9800)
                                            AiProcessingPhase.ERROR -> Color.Red
                                            AiProcessingPhase.IDLE -> TextMuted
                                        }
                                    )
                            )
                            Text(
                                text = "MONITOR: [${processingPhase.displayName.uppercase()}]",
                                color = when (processingPhase) {
                                    AiProcessingPhase.INGESTING -> AetherCyan
                                    AiProcessingPhase.THINKING -> RunicGold
                                    AiProcessingPhase.STREAMING -> EmeraldGreen
                                    AiProcessingPhase.COMPLETED -> EmeraldGreen
                                    AiProcessingPhase.CANCELLED -> Color(0xFFFF9800)
                                    AiProcessingPhase.ERROR -> Color.Red
                                    AiProcessingPhase.IDLE -> TextMuted
                                },
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }

                        if (isGenerating) {
                            Button(
                                onClick = { viewModel.cancelGeneration() },
                                colors = ButtonDefaults.buttonColors(containerColor = Color(0x33FF5252)),
                                border = BorderStroke(1.dp, Color(0xFFFF5252)),
                                shape = RoundedCornerShape(6.dp),
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp)
                            ) {
                                Icon(Icons.Default.Stop, contentDescription = "Halt", tint = Color(0xFFFF5252), modifier = Modifier.size(12.dp))
                                Spacer(modifier = Modifier.width(4.dp))
                                Text("HALT", color = Color(0xFFFF5252), fontSize = 10.sp, fontWeight = FontWeight.Bold)
                            }
                        } else if (chatHistory.isNotEmpty()) {
                            Text(
                                text = "CLEAR LOG",
                                color = TextMuted,
                                fontSize = 10.sp,
                                fontFamily = FontFamily.Monospace,
                                modifier = Modifier.clickable { viewModel.clearChatHistory() }
                            )
                        }
                    }

                    // Live Metrics Strip (Elapsed, Tokens, tok/s, Modality)
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(SpaceBlack)
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "⏱ ${"%.1f".format(liveElapsedMs / 1000f)}s",
                            color = TextPrimary,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = "✦ $liveTokenCount tok",
                            color = AetherCyan,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = "⚡ ${if (liveTokensPerSec > 0f) "%.1f".format(liveTokensPerSec) else "--"} tok/s",
                            color = if (liveTokensPerSec > 0f) EmeraldGreen else TextMuted,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Text(
                            text = "[$liveModality]",
                            color = RunicGold,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }

                    // Shimmering progress bar during active streaming
                    if (isGenerating) {
                        LinearProgressIndicator(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(2.dp),
                            color = when (processingPhase) {
                                AiProcessingPhase.INGESTING -> AetherCyan
                                AiProcessingPhase.THINKING -> RunicGold
                                AiProcessingPhase.STREAMING -> EmeraldGreen
                                else -> AetherCyan
                            },
                            trackColor = SpaceBlack
                        )
                    }
                }
            }
        }

        // 5. Chain-of-Thought (CoT) Thinking Trace
        AnimatedVisibility(
            visible = (liveThoughtTrace.isNotBlank() || (isGenerating && processingPhase == AiProcessingPhase.THINKING)) && thinkingModeEnabled
        ) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                border = BorderStroke(1.dp, Color(0x33FFD700))
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { showThinkingCard = !showThinkingCard },
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            Icon(Icons.Default.Psychology, contentDescription = null, tint = RunicGold, modifier = Modifier.size(16.dp))
                            val thoughtTitle = if (isGenerating && processingPhase == AiProcessingPhase.THINKING) {
                                "REASONING (CHAIN-OF-THOUGHT) • ${"%.1f".format(liveElapsedMs / 1000f)}s"
                            } else {
                                "REASONING (CHAIN-OF-THOUGHT)"
                            }
                            Text(
                                text = thoughtTitle,
                                color = RunicGold,
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                        Icon(
                            if (showThinkingCard) Icons.Default.KeyboardArrowUp else Icons.Default.KeyboardArrowDown,
                            contentDescription = null,
                            tint = RunicGold
                        )
                    }

                    if (showThinkingCard) {
                        Text(
                            text = liveThoughtTrace.ifBlank { "Analyzing runes and latent celestial coordinates..." },
                            color = TextSecondary,
                            fontSize = 12.sp,
                            lineHeight = 18.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // 6. Live Oracle Prophecy / Answer Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = CardSurface),
            border = BorderStroke(1.dp, CardBorder)
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
                        text = "// ORACLE PROPHECY //",
                        color = AetherCyan,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace
                    )

                    // Metrics Strip
                    if (gemmaResult != null) {
                        val res = gemmaResult!!
                        Text(
                            text = "${res.latencyMs}ms • ${"%.1f".format(res.tokensPerSec)} tok/s • [${res.modalityUsed}]",
                            color = EmeraldGreen,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    } else if (isGenerating) {
                        Text(
                            text = "⚡ ${if (liveTokensPerSec > 0f) "%.1f".format(liveTokensPerSec) else "--"} tok/s",
                            color = AetherCyan,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                // Show Prompt Context Header if available
                if (activePrompt.isNotBlank()) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(DeepVoid)
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        activeImage?.let { bmp ->
                            Image(
                                bitmap = bmp.asImageBitmap(),
                                contentDescription = "Active Image",
                                modifier = Modifier
                                    .size(24.dp)
                                    .clip(RoundedCornerShape(4.dp))
                            )
                        }
                        Text(
                            text = "Q: $activePrompt",
                            color = TextSecondary,
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            maxLines = 2
                        )
                    }
                }

                val currentText = when {
                    liveStreamingText.isNotBlank() -> liveStreamingText
                    isGenerating && processingPhase == AiProcessingPhase.INGESTING -> "Projecting multimodal tokens into Gemma 4 latent space..."
                    isGenerating && processingPhase == AiProcessingPhase.THINKING -> "Synthesizing cosmic reasoning & ephemeris..."
                    telemetry.lastAiResponseText.isNotBlank() -> telemetry.lastAiResponseText
                    else -> "The Celestial Oracle is listening.\nType a query above, attach an image for multimodal vision inspection, or speak through the staff microphone."
                }

                // Live Streaming Output with Blinking Cursor
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.Bottom
                ) {
                    Text(
                        text = currentText,
                        color = TextPrimary,
                        fontSize = 14.sp,
                        lineHeight = 22.sp,
                        fontFamily = FontFamily.Monospace,
                        modifier = Modifier.weight(1f, fill = false)
                    )
                    if (isGenerating && (processingPhase == AiProcessingPhase.STREAMING || processingPhase == AiProcessingPhase.THINKING)) {
                        Text(
                            text = "▌",
                            color = AetherCyan.copy(alpha = cursorAlpha),
                            fontSize = 14.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Bottom Action Tools: Speak, Broadcast to Staff, Copy
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Speak button
                    Button(
                        onClick = {
                            val isSpell = currentText.contains("[SPELL:", ignoreCase = true)
                            val cleanSpeech = currentText.replace(Regex("""\[SPELL:[^\]]*\]\s*"""), "").trim()
                            if (isSpell) {
                                ttsEngine?.language = Locale.US
                            } else {
                                ttsEngine?.language = if (cleanSpeech.any { it in '\u0400'..'\u04FF' }) Locale("ru", "RU") else currentLanguage.locale
                            }
                            ttsEngine?.speak(cleanSpeech, TextToSpeech.QUEUE_FLUSH, null, "GemmaTts")
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = DeepVoid),
                        border = BorderStroke(1.dp, AetherCyan),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(Icons.Default.VolumeUp, contentDescription = null, tint = AetherCyan, modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("Speak", color = AetherCyan, fontSize = 11.sp)
                    }

                    // Broadcast to Staff OLED & Speaker
                    Button(
                        onClick = {
                            viewModel.broadcastCurrentAnswerToStaff()
                            Toast.makeText(context, "Transmitted to Staff OLED & Speaker!", Toast.LENGTH_SHORT).show()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = DeepVoid),
                        border = BorderStroke(1.dp, RunicGold),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(Icons.Default.Send, contentDescription = null, tint = RunicGold, modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("To Staff", color = RunicGold, fontSize = 11.sp)
                    }

                    // Copy button
                    IconButton(
                        onClick = {
                            clipboardManager.setText(AnnotatedString(currentText))
                            Toast.makeText(context, "Copied prophecy to clipboard", Toast.LENGTH_SHORT).show()
                        },
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(DeepVoid)
                            .border(1.dp, CardBorder, RoundedCornerShape(8.dp))
                    ) {
                        Icon(Icons.Default.ContentCopy, contentDescription = "Copy", tint = TextSecondary)
                    }
                }
            }
        }

        // 7. Recent Dialogue History (Collapsible)
        if (chatHistory.size > 2) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                colors = CardDefaults.cardColors(containerColor = DeepVoid),
                border = BorderStroke(1.dp, CardBorder)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { showHistoryDialog = !showHistoryDialog },
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "DIALOGUE ARCHIVE (${chatHistory.size / 2} EXCHANGES)",
                            color = TextSecondary,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                        Icon(
                            if (showHistoryDialog) Icons.Default.KeyboardArrowUp else Icons.Default.KeyboardArrowDown,
                            contentDescription = null,
                            tint = TextSecondary
                        )
                    }

                    if (showHistoryDialog) {
                        chatHistory.takeLast(6).forEach { msg ->
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(if (msg.isUser) SpaceBlack else CardSurface)
                                    .padding(8.dp),
                                horizontalArrangement = Arrangement.spacedBy(6.dp)
                            ) {
                                Text(
                                    text = if (msg.isUser) "USER:" else "ORACLE:",
                                    color = if (msg.isUser) AetherCyan else RunicGold,
                                    fontSize = 10.sp,
                                    fontWeight = FontWeight.Bold,
                                    fontFamily = FontFamily.Monospace
                                )
                                Text(
                                    text = msg.text,
                                    color = TextPrimary,
                                    fontSize = 11.sp,
                                    fontFamily = FontFamily.Monospace,
                                    maxLines = 3
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Safely decodes a downsampled bitmap from a content Uri, preserving aspect ratio,
 * correcting EXIF camera orientation, and avoiding OutOfMemoryError on large camera photos.
 */
private fun decodeSampledBitmap(context: Context, uri: Uri, maxDimension: Int = 800): Bitmap? {
    return try {
        // Step 1: Decode image dimensions only
        val boundsOptions = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        context.contentResolver.openInputStream(uri)?.use { stream ->
            BitmapFactory.decodeStream(stream, null, boundsOptions)
        }
        val origW = boundsOptions.outWidth
        val origH = boundsOptions.outHeight
        if (origW <= 0 || origH <= 0) return null

        // Step 2: Compute power-of-two inSampleSize
        var inSampleSize = 1
        while (origW / (inSampleSize * 2) >= maxDimension || origH / (inSampleSize * 2) >= maxDimension) {
            inSampleSize *= 2
        }

        // Step 3: Decode subsampled bitmap into memory
        val decodeOptions = BitmapFactory.Options().apply {
            this.inSampleSize = inSampleSize
            inPreferredConfig = Bitmap.Config.ARGB_8888
        }
        val sampledBitmap = context.contentResolver.openInputStream(uri)?.use { stream ->
            BitmapFactory.decodeStream(stream, null, decodeOptions)
        } ?: return null

        // Step 4: Correct EXIF orientation
        val orientation = try {
            context.contentResolver.openInputStream(uri)?.use { stream ->
                val exif = ExifInterface(stream)
                exif.getAttributeInt(ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_NORMAL)
            } ?: ExifInterface.ORIENTATION_NORMAL
        } catch (_: Throwable) {
            ExifInterface.ORIENTATION_NORMAL
        }

        val matrix = Matrix()
        when (orientation) {
            ExifInterface.ORIENTATION_ROTATE_90 -> matrix.postRotate(90f)
            ExifInterface.ORIENTATION_ROTATE_180 -> matrix.postRotate(180f)
            ExifInterface.ORIENTATION_ROTATE_270 -> matrix.postRotate(270f)
            ExifInterface.ORIENTATION_FLIP_HORIZONTAL -> matrix.postScale(-1f, 1f)
            ExifInterface.ORIENTATION_FLIP_VERTICAL -> matrix.postScale(1f, -1f)
        }

        val orientedBitmap = if (!matrix.isIdentity) {
            val rotated = Bitmap.createBitmap(sampledBitmap, 0, 0, sampledBitmap.width, sampledBitmap.height, matrix, true)
            if (rotated != sampledBitmap) {
                sampledBitmap.recycle()
            }
            rotated
        } else {
            sampledBitmap
        }

        // Step 5: Final scale down if oriented bitmap exceeds maxDimension
        val currentMax = maxOf(orientedBitmap.width, orientedBitmap.height)
        if (currentMax > maxDimension) {
            val scale = maxDimension.toFloat() / currentMax
            val targetW = (orientedBitmap.width * scale).toInt().coerceAtLeast(1)
            val targetH = (orientedBitmap.height * scale).toInt().coerceAtLeast(1)
            val scaled = Bitmap.createScaledBitmap(orientedBitmap, targetW, targetH, true)
            if (scaled != orientedBitmap) {
                orientedBitmap.recycle()
            }
            scaled
        } else {
            orientedBitmap
        }
    } catch (t: Throwable) {
        android.util.Log.e("OracleTerminal", "Failed to decode sampled bitmap: ${t.message}", t)
        null
    }
}

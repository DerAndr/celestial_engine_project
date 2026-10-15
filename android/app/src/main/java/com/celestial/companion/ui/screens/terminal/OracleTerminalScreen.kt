package com.celestial.companion.ui.screens.terminal

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.speech.tts.TextToSpeech
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.*
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import com.celestial.companion.ai.GemmaVariant
import com.celestial.companion.domain.model.StaffLanguage
import com.celestial.companion.domain.repository.ConnectionMode
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
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
    val liveThoughtTrace by viewModel.liveThoughtTrace.collectAsState()
    val liveStreamingText by viewModel.liveStreamingText.collectAsState()
    val gemmaResult by viewModel.gemmaResult.collectAsState()

    var promptInput by remember { mutableStateOf("") }
    var selectedBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var thinkingModeEnabled by remember { mutableStateOf(true) }
    var showThinkingCard by remember { mutableStateOf(true) }

    // Android Text-to-Speech engine (Defaults to English, switches dynamically)
    var ttsEngine by remember { mutableStateOf<TextToSpeech?>(null) }
    DisposableEffect(context) {
        val tts = TextToSpeech(context) { status ->
            if (status == TextToSpeech.SUCCESS) {
                tts.language = currentLanguage.locale
            }
        }
        ttsEngine = tts
        onDispose {
            tts.stop()
            tts.shutdown()
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

    // Image picker launcher for Vision Multimodality
    val imagePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                context.contentResolver.openInputStream(uri)?.use { stream ->
                    selectedBitmap = BitmapFactory.decodeStream(stream)
                }
            } catch (e: Exception) {
                Toast.makeText(context, "Error loading image: ${e.message}", Toast.LENGTH_SHORT).show()
            }
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
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
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
                            text = "LOCK TO PHONE",
                            color = if (telemetry.preferredHost == 2) SpaceBlack else TextPrimary,
                            fontSize = 11.sp,
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
                            text = "PI 5 BRAIN",
                            color = if (telemetry.preferredHost == 1) SpaceBlack else TextPrimary,
                            fontSize = 11.sp,
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
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
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
                            text = "GEMMA 4 (E2B) MULTIMODAL",
                            color = AetherCyan,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                    Text(
                        text = "Native Vision • Voice • Text • CoT Thinking",
                        color = TextMuted,
                        fontSize = 11.sp
                    )
                }

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
                        IconButton(onClick = { selectedBitmap = null }) {
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

                // Quick Prompt Chips
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    val chips = listOf("Anomaly Scan", "Interpret Runes", "Prophecy", "Staff Status")
                    chips.forEach { chip ->
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(DeepVoid)
                                .border(1.dp, CardBorder, RoundedCornerShape(6.dp))
                                .clickable {
                                    promptInput = chip
                                    viewModel.queryGemma(chip, selectedBitmap)
                                }
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(text = chip, color = AetherCyan, fontSize = 10.sp)
                        }
                    }
                }

                // Action Bar: Camera + Voice + Query Button
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Attach Vision Button
                    IconButton(
                        onClick = { imagePickerLauncher.launch("image/*") },
                        modifier = Modifier
                            .clip(RoundedCornerShape(10.dp))
                            .background(DeepVoid)
                            .border(1.dp, if (selectedBitmap != null) EmeraldGreen else CardBorder, RoundedCornerShape(10.dp))
                    ) {
                        Icon(
                            Icons.Default.PhotoCamera,
                            contentDescription = "Attach Vision Image",
                            tint = if (selectedBitmap != null) EmeraldGreen else AetherCyan
                        )
                    }

                    // Voice STT Speech Recognition Button
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

                    // Query Button
                    Button(
                        onClick = {
                            val prompt = promptInput.ifBlank { "What prophecies do the celestial stars reveal?" }
                            viewModel.queryGemma(prompt, selectedBitmap)
                        },
                        enabled = !isGenerating,
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(containerColor = AetherCyan),
                        shape = RoundedCornerShape(10.dp)
                    ) {
                        if (isGenerating) {
                            CircularProgressIndicator(
                                modifier = Modifier.size(16.dp),
                                color = SpaceBlack,
                                strokeWidth = 2.dp
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Reasoning...", color = SpaceBlack, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                        } else {
                            Icon(Icons.Default.AutoAwesome, contentDescription = null, tint = SpaceBlack)
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Query Gemma 4", color = SpaceBlack, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }

        // 4. Chain-of-Thought (CoT) Thinking Trace
        AnimatedVisibility(
            visible = (liveThoughtTrace.isNotBlank() || isGenerating) && thinkingModeEnabled
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
                            Text(
                                text = "GEMMA 4 REASONING (CHAIN-OF-THOUGHT)",
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
                            text = liveThoughtTrace.ifBlank { "Initializing Gemma 4 token projectors..." },
                            color = TextSecondary,
                            fontSize = 12.sp,
                            lineHeight = 18.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }

        // 5. Live Oracle Prophecy / Answer Card
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
                    gemmaResult?.let { res ->
                        Text(
                            text = "${res.latencyMs}ms • ${"%.1f".format(res.tokensPerSec)} tok/s • [${res.modalityUsed}]",
                            color = EmeraldGreen,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                val currentText = when {
                    liveStreamingText.isNotBlank() -> liveStreamingText
                    telemetry.lastAiResponseText.isNotBlank() -> telemetry.lastAiResponseText
                    else -> "The Celestial Oracle is listening.\nType a query above, attach an image for multimodal vision inspection, or speak through the staff microphone."
                }

                Text(
                    text = currentText,
                    color = TextPrimary,
                    fontSize = 14.sp,
                    lineHeight = 22.sp,
                    fontFamily = FontFamily.Monospace
                )

                HorizontalDivider(color = CardBorder, thickness = 1.dp)

                // Bottom Action Tools: Speak, Broadcast to Staff, Copy
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Speak button
                    Button(
                        onClick = {
                            ttsEngine?.speak(currentText, TextToSpeech.QUEUE_FLUSH, null, "GemmaTts")
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
    }
}

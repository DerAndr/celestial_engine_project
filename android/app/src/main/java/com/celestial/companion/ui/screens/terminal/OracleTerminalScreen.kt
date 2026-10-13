package com.celestial.companion.ui.screens.terminal

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.theme.*

@Composable
fun OracleTerminalScreen(
    viewModel: DashboardViewModel,
    modifier: Modifier = Modifier
) {
    val telemetry by viewModel.telemetry.collectAsState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(SpaceBlack)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            text = "AI ORACLE TRANSCRIPT TERMINAL",
            color = AetherCyan,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace
        )

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
            shape = RoundedCornerShape(14.dp),
            colors = CardDefaults.cardColors(containerColor = DeepVoid),
            border = androidx.compose.foundation.BorderStroke(1.dp, CardBorder)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(16.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Text(
                    text = "// LIVE AI RESPONSE //",
                    color = RunicGold,
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace
                )

                Text(
                    text = if (telemetry.lastAiResponseText.isNotBlank()) {
                        telemetry.lastAiResponseText
                    } else {
                        "Waiting for voice query from Celestial Engine Mk.V staff...\nPress QUERY button on staff or speak into microphone."
                    },
                    color = TextPrimary,
                    fontSize = 14.sp,
                    lineHeight = 20.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }
    }
}

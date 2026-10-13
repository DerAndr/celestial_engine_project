package com.celestial.companion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.celestial.companion.ui.navigation.Screen
import com.celestial.companion.ui.screens.chronos.ChronosScreen
import com.celestial.companion.ui.screens.dashboard.DashboardScreen
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.leds.LedScreen
import com.celestial.companion.ui.screens.soundboard.SoundboardScreen
import com.celestial.companion.ui.screens.terminal.OracleTerminalScreen
import com.celestial.companion.ui.theme.AetherCyan
import com.celestial.companion.ui.theme.CelestialCompanionTheme
import com.celestial.companion.ui.theme.DeepVoid
import com.celestial.companion.ui.theme.SpaceBlack

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        val app = application as CelestialApp
        val repository = app.staffRepository

        setContent {
            val viewModel: DashboardViewModel = viewModel {
                DashboardViewModel(repository)
            }
            CelestialCompanionTheme {
                MainAppScaffold(viewModel = viewModel)
            }
        }
    }
}

@Composable
fun MainAppScaffold(viewModel: DashboardViewModel) {
    var currentScreen by remember { mutableStateOf<Screen>(Screen.Dashboard) }

    Scaffold(
        bottomBar = {
            NavigationBar(
                containerColor = DeepVoid,
                tonalElevation = 8.dp
            ) {
                Screen.bottomNavScreens.forEach { screen ->
                    val selected = currentScreen == screen
                    NavigationBarItem(
                        selected = selected,
                        onClick = { currentScreen = screen },
                        icon = {
                            Icon(
                                screen.icon,
                                contentDescription = screen.title,
                                tint = if (selected) AetherCyan else Color(0xFF78909C)
                            )
                        },
                        label = {
                            Text(
                                screen.title,
                                color = if (selected) AetherCyan else Color(0xFF78909C)
                            )
                        },
                        colors = NavigationBarItemDefaults.colors(
                            indicatorColor = Color(0x2200E5FF)
                        )
                    )
                }
            }
        }
    ) { innerPadding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(SpaceBlack)
                .padding(innerPadding)
        ) {
            when (currentScreen) {
                Screen.Dashboard -> DashboardScreen(viewModel = viewModel)
                Screen.Chronos -> ChronosScreen(viewModel = viewModel)
                Screen.Leds -> LedScreen(viewModel = viewModel)
                Screen.Soundboard -> SoundboardScreen(viewModel = viewModel)
                Screen.Terminal -> OracleTerminalScreen(viewModel = viewModel)
            }
        }
    }
}

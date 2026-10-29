package com.celestial.companion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.celestial.companion.ui.navigation.Screen
import com.celestial.companion.ui.screens.chronos.ChronosScreen
import com.celestial.companion.ui.screens.dashboard.DashboardScreen
import com.celestial.companion.ui.screens.dashboard.DashboardViewModel
import com.celestial.companion.ui.screens.grimoire.GrimoireScreen
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

        handleKeyIntent(intent, app)

        val permissions = mutableListOf(
            android.Manifest.permission.CAMERA,
            android.Manifest.permission.RECORD_AUDIO
        )
        if (android.os.Build.VERSION.SDK_INT >= 33) {
            permissions.add(android.Manifest.permission.POST_NOTIFICATIONS)
        }
        if (android.os.Build.VERSION.SDK_INT >= 35) {
            permissions.add("android.permission.ACCESS_LOCAL_NETWORK")
        }
        val needed = permissions.filter {
            checkSelfPermission(it) != android.content.pm.PackageManager.PERMISSION_GRANTED
        }
        if (needed.isNotEmpty()) {
            requestPermissions(needed.toTypedArray(), 1001)
        }

        // Start Foreground Service to keep Brain and UDP active when screen is off/locked
        com.celestial.companion.service.StaffBrainService.start(this)

        setContent {
            val viewModel: DashboardViewModel = viewModel {
                DashboardViewModel(repository, app.gemmaEngine)
            }
            CelestialCompanionTheme {
                MainAppScaffold(viewModel = viewModel)
            }
        }
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        val app = application as CelestialApp
        handleKeyIntent(intent, app)
    }

    private fun handleKeyIntent(intent: android.content.Intent?, app: CelestialApp) {
        if (intent == null) return
        val newKey = intent.getStringExtra("openrouter_key") ?: intent.getStringExtra("api_key")
        if (!newKey.isNullOrBlank()) {
            val key = newKey.trim()
            val prefs = getSharedPreferences("celestial_prefs", MODE_PRIVATE)
            prefs.edit().putString("openrouter_api_key", key).apply()
            app.gemmaEngine.config = app.gemmaEngine.config.copy(apiKey = key)
            android.util.Log.i("MainActivity", "Configured OpenRouter API key via Intent")
            android.widget.Toast.makeText(this, "API-ключ OpenRouter успешно сохранен", android.widget.Toast.LENGTH_SHORT).show()
        }
    }
}

@Composable
fun MainAppScaffold(viewModel: DashboardViewModel) {
    var currentRoute by rememberSaveable { mutableStateOf(Screen.Dashboard.route) }
    val currentScreen = Screen.bottomNavScreens.firstOrNull { it.route == currentRoute } ?: Screen.Dashboard

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
                        onClick = { currentRoute = screen.route },
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
                                color = if (selected) AetherCyan else Color(0xFF78909C),
                                fontSize = 9.sp,
                                maxLines = 1
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
                Screen.Leds -> LedScreen(viewModel = viewModel)
                Screen.Grimoire -> GrimoireScreen(viewModel = viewModel)
                Screen.Soundboard -> SoundboardScreen(viewModel = viewModel)
                Screen.Chronos -> ChronosScreen(viewModel = viewModel)
                Screen.Terminal -> OracleTerminalScreen(viewModel = viewModel)
            }
        }
    }
}

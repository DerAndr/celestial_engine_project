package com.celestial.companion.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.ui.graphics.vector.ImageVector

sealed class Screen(val route: String, val title: String, val icon: ImageVector) {
    object Dashboard : Screen("dashboard", "Dashboard", Icons.Default.Dashboard)
    object Chronos : Screen("chronos", "Chronos", Icons.Default.Schedule)
    object Leds : Screen("leds", "Runes", Icons.Default.LightMode)
    object Soundboard : Screen("soundboard", "Audio", Icons.Default.VolumeUp)
    object Terminal : Screen("terminal", "Oracle", Icons.Default.Terminal)

    companion object {
        val bottomNavScreens = listOf(Dashboard, Chronos, Leds, Soundboard, Terminal)
    }
}

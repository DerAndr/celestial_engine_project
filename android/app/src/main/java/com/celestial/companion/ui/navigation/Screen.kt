package com.celestial.companion.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DateRange
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Star
import androidx.compose.ui.graphics.vector.ImageVector

sealed class Screen(val route: String, val title: String, val icon: ImageVector) {
    data object Dashboard : Screen("dashboard", "Dashboard", Icons.Default.Home)
    data object Chronos : Screen("chronos", "Chronos", Icons.Default.DateRange)
    data object Leds : Screen("leds", "Runes", Icons.Default.Star)
    data object Soundboard : Screen("soundboard", "Audio", Icons.Default.PlayArrow)
    data object Terminal : Screen("terminal", "Oracle", Icons.Default.Info)

    companion object {
        val bottomNavScreens: List<Screen>
            get() = listOf(Dashboard, Chronos, Leds, Soundboard, Terminal)
    }
}

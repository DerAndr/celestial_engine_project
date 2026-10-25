package com.celestial.companion.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.DateRange
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Psychology
import androidx.compose.material.icons.filled.Star
import androidx.compose.ui.graphics.vector.ImageVector

sealed class Screen(val route: String, val title: String, val icon: ImageVector) {
    data object Dashboard : Screen("dashboard", "Dashboard", Icons.Default.Home)
    data object Leds : Screen("leds", "Runes", Icons.Default.Star)
    data object Grimoire : Screen("grimoire", "Grimoire", Icons.Default.AutoAwesome)
    data object Soundboard : Screen("soundboard", "Audio", Icons.Default.PlayArrow)
    data object Chronos : Screen("chronos", "Chronos", Icons.Default.DateRange)
    data object Terminal : Screen("terminal", "Oracle", Icons.Default.Psychology)

    companion object {
        val bottomNavScreens: List<Screen>
            get() = listOf(Dashboard, Leds, Grimoire, Soundboard, Chronos, Terminal)
    }
}

package com.celestial.companion.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = AetherCyan,
    secondary = RunicGold,
    tertiary = NebulaPurple,
    background = SpaceBlack,
    surface = CardSurface,
    onPrimary = SpaceBlack,
    onSecondary = SpaceBlack,
    onTertiary = SpaceBlack,
    onBackground = TextPrimary,
    onSurface = TextPrimary
)

@Composable
fun CelestialCompanionTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        typography = Typography,
        content = content
    )
}

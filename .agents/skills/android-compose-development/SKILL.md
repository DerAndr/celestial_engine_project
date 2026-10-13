---
name: android-compose-development
description: >-
  Expert guidelines and standard workflows for modern Android development with Kotlin,
  Jetpack Compose, Material 3, AndroidX Lifecycle, and network sockets. Use when designing,
  building, debugging, or reviewing Android apps, Compose UI, or crash triage.
---

# Android & Jetpack Compose Development Protocol

## Core Rules for Safe Android Architecture

### 1. Zero-Crash Startup Guarantee
- **Never do heavy I/O or network socket creation directly in `Application.onCreate()`**:
  Background networking and socket binding must be lazy or launched in controlled lifecycle scopes.
- **Fail-Safe UI Catch**:
  Always wrap the top-level Activity `setContent` or provide an error fallback so uncaught Compose/Lifecycle errors render an on-screen diagnostic stack trace rather than tripping the OS "App keeps stopping" crash dialog.
- **Resources & Manifest**:
  - Never use private or platform `@android:drawable/...` drawables as application icons or round icons.
  - Always provide explicit `res/values/strings.xml`, `res/values/themes.xml`, and valid vector/adaptive drawables in `res/mipmap/` or `res/drawable/`.
  - Manifest themes for `ComponentActivity` must inherit from `android:Theme.Material.NoActionBar` or modern `Theme.Material3`.

### 2. Network Sockets on Android (UDP / TCP)
- **Permissions**:
  Require `<uses-permission android:name="android.permission.INTERNET" />` and `<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />`.
- **Socket Configuration Ordering**:
  - If binding to a specific port with `SO_REUSEADDR`, construct `DatagramSocket(null)`, set `reuseAddress = true`, THEN call `.bind(InetSocketAddress(port))`.
  - Setting `reuseAddress` after `bind()` or calling `setBroadcast` on an unbound socket causes runtime `SocketException`.
  - Always set `soTimeout` (e.g. 2000 ms) to allow thread/coroutine loops to check `isActive` periodically without blocking indefinitely.
  - Never run a tight catch-loop on socket errors: always enforce a backoff delay (`delay(500)` to `delay(2000)`).

### 3. Jetpack Compose & ViewModel Lifecycles
- **ViewModel Creation**:
  - In Compose, prefer `viewModel<T> { ... }` or `viewModel(factory = ...)` within `@Composable` rather than fragile Activity field property delegates.
  - Use `StateFlow` and `.collectAsState()` for unidirectional data flow.
- **Icon Usage**:
  - Prefer Core Material icons (`Icons.Default.*`) to eliminate massive 20 MB+ dex table lookups and classloading hazards.

### 4. Build & Tooling (AGP & Kotlin)
- When targeting modern Android (API 34/35), use Kotlin 2.0+ with Compose Compiler plugin (`alias(libs.plugins.kotlin.compose)`).
- Ensure JVM target is consistent (`JavaVersion.VERSION_17` and `jvmTarget = "17"`).
- Always verify multiplatform Little-Endian binary protocol compatibility across C, Python, and Kotlin.

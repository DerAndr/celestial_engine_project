# Android Companion App Architecture & Framework — Celestial Engine Mk.V

**Date**: 2026-10-13  
**Purpose**: Architectural design of the Android companion application for telemetry monitoring, remote effects management, clock synchronization, and AI relay for the Celestial Engine Mk.V staff.

---

## 1. Concept and Operating Scenarios

The **Celestial Companion** application is designed for two operating scenarios:

### Scenario 1: Remote Control & Telemetry Dashboard (Companion Dashboard)
The staff and smartphone reside on the same local subnet (home Wi-Fi or smartphone mobile hotspot).
- **Communication**: Bidirectional UDP (port `5005`).
- **Features**:
  - Live HUD: Staff state monitoring (IDLE, LISTENING, PROCESSING, SPEAKING), IP address, battery/volume telemetry.
  - Lighting management: 18 ring animation presets, brightness and palette controls.
  - Soundboard: Triggering 8-bit chiptune jukebox tracks, Oracle speech, and Sci-Fi effects (including R2-D2).
  - Time synchronization: 1-click UTC/local time synchronization from smartphone to `CHRONOS` clock.
  - Oracle Terminal: Full transcript of speech recognition and neural response history.

### Scenario 2: Autonomous On-Device Mobile AI Gateway (Mobile Brain)
When a wearable Raspberry Pi 5 or workstation server is unavailable:
- Smartphone acts as server on port `5005`.
- Receives 16 kHz PCM audio stream from the staff over Wi-Fi.
- Executes STT (local on-device / Cloud API) -> LLM (OpenRouter / LiteRT-LM) -> TTS (Android TTS / EdgeTTS) and streams synthesized audio back to the staff speaker.

---

## 2. Technology Stack

- **Language**: Kotlin 2.x
- **UI Framework**: Jetpack Compose + Material 3 (Celestial dark theme: neon runes, cyan/gold/magenta accents).
- **Asynchrony**: Kotlin Coroutines + StateFlow / SharedFlow.
- **Networking**:
  - `DatagramSocket` / Kotlinx-io (low-latency binary UDP conforming to `net_protocol.h`).
  - `NetworkRequest` / `ConnectivityManager` (hotspot and Wi-Fi interface binding).
  - BLE (Bluetooth Low Energy) module for future direct connectivity.
- **Architecture**: Clean Architecture + MVI (Model-View-Intent).
- **Serialization**: `ByteBuffer` (Little-Endian, compatible with Pico SDK and Python struct `<IBBHHI`).

---

## 3. Module and Directory Layout

```text
android-companion/
├── app/
│   ├── build.gradle.kts
│   └── src/main/java/com/celestial/companion/
│       ├── CelestialApp.kt
│       ├── di/
│       │   └── AppModule.kt
│       ├── protocol/                         # Binary protocol (v2)
│       │   ├── CelestialPacket.kt            # Header structure (14 bytes)
│       │   ├── NetMessageType.kt             # NET_MSG_* (HEARTBEAT, QUERY, RESP, CMD)
│       │   └── ProtocolConstants.kt          # MAGIC 0x454C4543, PORT 5005
│       ├── network/                          # Transport layer
│       │   ├── StaffTransport.kt             # Transport interface (UDP / BLE)
│       │   ├── StaffUdpTransport.kt          # UDP socket and listener implementation
│       │   ├── StaffDiscoveryService.kt      # Staff subnet discovery service
│       │   └── AudioStreamBuffer.kt          # Buffer for 16 kHz PCM chunks
│       ├── domain/                           # Business logic and domain models
│       │   ├── model/
│       │   │   ├── StaffState.kt             # IDLE, LISTENING, PROCESSING, SPEAKING
│       │   │   ├── StaffMode.kt              # GUIDE, GALAXY, CHRONOS, COSMIC, etc.
│       │   │   ├── ClockStyle.kt             # DIAL, HOURGLASS, CHROMA, ORRERY
│       │   │   ├── LedAnimation.kt           # 18 lighting presets
│       │   │   ├── AiModelPreset.kt          # DeepSeek, Eddie, Grok, Marvin
│       │   │   ├── LanguageDialect.kt        # EN (default), AUTO, DE, RU
│       │   │   └── StaffTelemetry.kt         # Full telemetry state model
│       │   └── repository/
│       │       └── StaffRepository.kt        # StateFlow<StaffTelemetry>, sendCommand()
│       └── ui/                               # Jetpack Compose UI
│           ├── theme/
│           │   ├── Color.kt                  # SpaceBlack, RunicGold, AetherCyan, NebulaPurple
│           │   ├── Theme.kt                  # CelestialDarkTheme
│           │   └── Type.kt                   # Orbitron / Monospace typography
│           ├── navigation/
│           │   └── CelestialNavigation.kt    # Navigation bar and routing
│           └── screens/
│               ├── dashboard/                # Main HUD: status, mode, volume
│               │   ├── DashboardScreen.kt
│               │   └── DashboardViewModel.kt
│               ├── leds/                     # 3 rings & 18 animation controls
│               │   ├── LedControlScreen.kt
│               │   └── RingVisualizer.kt     # Real-time concentric ring canvas
│               ├── soundboard/               # Jukebox, sound effects, R2-D2
│               │   ├── SoundboardScreen.kt
│               │   └── SfxGrid.kt
│               ├── chronos/                  # 4 clock styles and time sync
│               │   ├── ChronosScreen.kt
│               │   └── TimeSyncCard.kt
│               └── oracle/                   # Text terminal & LLM transcript
│                   ├── OracleTerminalScreen.kt
│                   └── PromptHistoryList.kt
```

---

## 4. Network Client Specification (Kotlin)

### 4.1. Binary Packet Header (`protocol/CelestialPacket.kt`)

The structure precisely matches C structure `net_packet_header_t` (`sizeof == 14`, Little-Endian):

```kotlin
package com.celestial.companion.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class CelestialPacket(
    val magic: Int = ProtocolConstants.MAGIC,                 // 0x454C4543 ("CELE")
    val version: UByte = ProtocolConstants.VERSION.toUByte(), // 2
    val msgType: UByte,                                       // NetMessageType
    val seqNum: UShort,                                       // Monotonic sequence number
    val payloadLen: UShort,                                   // Payload byte length
    val requestId: UInt,                                      // Session identifier
    val payload: ByteArray = ByteArray(0)
) {
    fun toBytes(): ByteArray {
        val buffer = ByteBuffer.allocate(HEADER_SIZE + payload.size).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            putInt(magic)
            put(version.toByte())
            put(msgType.toByte())
            putShort(seqNum.toShort())
            putShort(payloadLen.toShort())
            putInt(requestId.toInt())
            if (payload.isNotEmpty()) {
                put(payload)
            }
        }
        return buffer.array()
    }

    companion object {
        const val HEADER_SIZE = 14

        fun fromBytes(bytes: ByteArray, length: Int): CelestialPacket? {
            if (length < HEADER_SIZE) return null
            val buffer = ByteBuffer.wrap(bytes, 0, length).apply {
                order(ByteOrder.LITTLE_ENDIAN)
            }
            val magic = buffer.int
            if (magic != ProtocolConstants.MAGIC) return null

            val version = buffer.get().toUByte()
            val msgType = buffer.get().toUByte()
            val seqNum = buffer.short.toUShort()
            val payloadLen = buffer.short.toUShort()
            val requestId = buffer.int.toUInt()

            val payload = if (payloadLen.toInt() > 0 && buffer.remaining() >= payloadLen.toInt()) {
                ByteArray(payloadLen.toInt()).also { buffer.get(it) }
            } else {
                ByteArray(0)
            }

            return CelestialPacket(magic, version, msgType, seqNum, payloadLen, requestId, payload)
        }
    }
}
```

### 4.2. Message Types (`protocol/NetMessageType.kt`)

```kotlin
package com.celestial.companion.protocol

enum class NetMessageType(val code: UByte) {
    HEARTBEAT(0x01u),
    QUERY_START(0x02u),
    AUDIO_CHUNK(0x03u),
    QUERY_END(0x04u),
    RESP_START(0x05u),
    RESP_CHUNK(0x06u),
    RESP_END(0x07u),
    CANCEL(0x08u),
    RESP_TEXT(0x09u),
    ACK(0x0Au),
    PROGRESS(0x0Bu),
    ERROR(0x0Cu),
    
    // Companion App Remote Controls:
    APP_COMMAND(0x20u),        // Mode, animation, volume configuration
    APP_TIME_SYNC(0x21u),      // Time sync from smartphone clock
    APP_TELEMETRY(0x22u);      // Extended telemetry query

    companion object {
        fun fromCode(code: UByte): NetMessageType? = entries.find { it.code == code }
    }
}
```

### 4.3. UDP Transport (`network/StaffUdpTransport.kt`)

```kotlin
package com.celestial.companion.network

import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import com.celestial.companion.protocol.ProtocolConstants
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.util.concurrent.atomic.AtomicInteger

class StaffUdpTransport(
    private val localPort: Int = ProtocolConstants.DEFAULT_PORT
) : StaffTransport {

    private var socket: DatagramSocket? = null
    private val seqCounter = AtomicInteger(1)
    private var isRunning = false
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val _incomingPackets = MutableSharedFlow<CelestialPacket>(extraBufferCapacity = 64)
    override val incomingPackets: SharedFlow<CelestialPacket> = _incomingPackets

    override fun start() {
        if (isRunning) return
        isRunning = true
        socket = DatagramSocket(localPort).apply {
            broadcast = true
            reuseAddress = true
        }

        scope.launch {
            val rxBuffer = ByteArray(2048)
            while (isActive && isRunning) {
                try {
                    val datagram = DatagramPacket(rxBuffer, rxBuffer.size)
                    socket?.receive(datagram)
                    val packet = CelestialPacket.fromBytes(datagram.data, datagram.length)
                    if (packet != null) {
                        _incomingPackets.emit(packet)
                    }
                } catch (e: Exception) {
                    if (!isRunning) break
                }
            }
        }
    }

    override suspend fun sendPacket(targetIp: String, targetPort: Int, packet: CelestialPacket) {
        withContext(Dispatchers.IO) {
            val bytes = packet.toBytes()
            val targetAddr = InetAddress.getByName(targetIp)
            val datagram = DatagramPacket(bytes, bytes.size, targetAddr, targetPort)
            socket?.send(datagram)
        }
    }

    override suspend fun sendCommand(targetIp: String, cmdCode: Byte, param: Byte) {
        val payload = byteArrayOf(cmdCode, param)
        val packet = CelestialPacket(
            msgType = NetMessageType.APP_COMMAND.code,
            seqNum = seqCounter.getAndIncrement().toUShort(),
            payloadLen = payload.size.toUShort(),
            requestId = 0u,
            payload = payload
        )
        sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, packet)
    }

    override fun stop() {
        isRunning = false
        socket?.close()
        socket = null
        scope.cancel()
    }
}
```

---

## 5. UI Models and App Screens

### 5.1. Screen `DashboardScreen` (Mission Control)
- **Status Header**: Connectivity status (`ONLINE 192.168.x.x`), RTT latency, active staff state (`IDLE`, `LISTENING`, `SPEAKING`).
- **Mode Wheel**: Interactive rotary disk for `[GUIDE]`, `[GALAXY]`, `[SOLAR]`, `[TERRA]`, `[CHRONOS]`, `[COSMIC FX]`, `[ORACLE]`.
- **Fast Sliders**:
  - Volume slider (0..100%).
  - Language dialect selector (`[EN]` default, `[AUTO]`, `[DE]`, `[RU]`).
  - Neural engine selector (`DeepSeek`, `Eddie`, `Grok`, `Marvin`).

### 5.2. Screen `ChronosScreen` (Clock Management)
- **Style Cards**:
  1. `DIAL CHRONOS` (12H astronomical dial)
  2. `AETHER HOURGLASS` (concentric fluid accumulator)
  3. `CHROMA PRISMATIC` (spectral color encoding)
  4. `CELESTIAL ORRERY` (kinematic orbits)
- **Time Sync Button**:
  - Transmits high-precision phone time to staff via `APP_TIME_SYNC` packet.
  - Staff synchronizes RTC and chimes.

### 5.3. Screen `LedControlScreen` (Concentric Rings)
- **Ring Visualizer**: Real-time Canvas rendering 3 rings (24 + 12 + 6 LEDs) and central stone.
- **Preset Gallery**: 18 lighting animations from `Rainbow Runes` to `Quantum Shift`.
- **Power Cap Slider**: Enforces safety limit (10%..25% maximum power).

### 5.4. Screen `SoundboardScreen` (Jukebox & Effects)
- **R2-D2 Droid Section**: `Happy Chirp`, `Sassy Chatter`, `Panic Alarm`, `Clock Chime`.
- **Sci-Fi FX Section**: `Warp Jump`, `Photon Laser`, `Pulsar`, `Portal`, `Supernova`.
- **Jukebox Section**: 8-bit retro track player.

### 5.5. Screen `OracleTerminalScreen` (Gemma 4 Multimodal Studio & Live Processing Monitor)
- **Live Processing Monitor Bar**:
  - Phase telemetry badge: `[INGESTING]` -> `[THINKING]` (CoT) -> `[STREAMING]` -> `[COMPLETED]` / `[STOPPED]`.
  - Real-time performance HUD: Live elapsed time (`⏱ 0.8s`), emitted token count (`✦ 34 tok`), throughput speed (`⚡ 29.5 tok/s`), modality chip (`[TEXT]`, `[VISION]`, `[AUDIO]`).
  - Progress shimmer tracking active token generation.
  - Active "Stop / Halt Generation" control.
- **Chain-of-Thought (Reasoning) Trace**:
  - Live folding reasoning accordion showing real-time thought projection duration (`REASONING (CoT) • 0.8s`).
- **Live Streaming Answer Display**:
  - Prompt context card displaying submitted query and vision image thumbnail.
  - Token-by-token live stream without waiting for full response.
  - Smooth animated blinking cursor (`▌`) during active emission.
  - Voice TTS synthesis, UDP broadcast to Staff OLED & speaker, and clipboard tools.
  - Collapsible session dialogue archive.

---

## 6. Implementation Stages

1. **Phase 1 (Protocol & Core)**:
   - Android project structure in `android/`.
   - `CelestialPacket` implementation and unit tests.
   - Low-latency UDP networking layer.
2. **Phase 2 (Compose UI Dashboard)**:
   - Dark theme, dashboard, chronos, and soundboard screens.
   - Bidirectional state synchronization with Pico 2 W firmware.
3. **Phase 3 (Gemma 4 Multimodal Brain & Live Streaming)**:
   - Real-time token streaming with live throughput and latency telemetry.
   - Live Chain-of-Thought reasoning traces and on-device multimodal ingestion.
   - Direct Wi-Fi / BLE UDP audio-inquiry relay from staff microphone to phone brain.


# Архитектура и каркас Android Companion App — Celestial Engine Mk.V

**Дата**: 2026-10-13  
**Назначение**: Проектирование мобильного приложения Android для мониторинга телеметрии, удалённого управления световыми/звуковыми эффектами, синхронизации времени и ретрансляции ИИ для посоха Celestial Engine Mk.V.

---

## 1. Концепция и режимы работы

Приложение **Celestial Companion** разработано для работы в двух сценариях:

### Сценарий 1: Пульт управления и монитор телеметрии (Companion Dashboard)
Посох и смартфон находятся в одной локальной сети (домашний Wi-Fi или точка доступа смартфона).
- **Связь**: Двусторонний UDP (порт `5005`).
- **Функции**:
  - Live HUD: отображение статуса посоха (IDLE, LISTENING, PROCESSING, SPEAKING), IP-адреса, заряда/громкости.
  - Управление световыми эффектами: переключение 18 анимаций колец, регулировка яркости и палитры.
  - Звуковая панель (Soundboard): запуск треков 8-битного джукбокса, фраз Оракула и Sci-Fi спецэффектов (включая R2-D2).
  - Синхронизация времени: передача точного времени со смартфона в часы `CHRONOS` в один клик.
  - Текстовый терминал Оракула: отображение распознанного текста и ответа нейросети.

### Сценарий 2: Автономный шлюз ИИ на смартфоне (On-Device Mobile Brain)
Если Raspberry Pi 5 или сервер на ПК отсутствуют:
- Смартфон выступает сервером на порту `5005`.
- Приложение принимает 16 кГц PCM аудиопоток от посоха по Wi-Fi.
- Выполняет STT (локальный Vosk / on-device Whisper / Cloud API) -> LLM (OpenRouter API) -> TTS (Android TTS / EdgeTTS) и отправляет аудиоответ обратно на динамик посоха.

---

## 2. Технологический стек

- **Язык**: Kotlin 2.x
- **UI Framework**: Jetpack Compose + Material 3 (Тёмная космическая тема: неоновые руны, акценты циан/золото/маджента).
- **Асинхронность**: Kotlin Coroutines + StateFlow / SharedFlow.
- **Сетевой уровень**:
  - `DatagramSocket` / Kotlinx-io (низколатентный бинарный UDP, соответствующий `net_protocol.h`).
  - `NetworkRequest` / `ConnectivityManager` (управление точкой доступа и Wi-Fi сокетами).
  - Модуль BLE (Bluetooth Low Energy) для прямого соединения в будущем.
- **Архитектура**: Clean Architecture + MVI (Model-View-Intent).
- **DI**: Hilt / Koin.
- **Сериализация**: `ByteBuffer` (Little-Endian, совместимость с Pico SDK и Python struct `<IBBHHI`).

---

## 3. Дерево модулей и классов проекта

```text
android-companion/
├── app/
│   ├── build.gradle.kts
│   └── src/main/java/com/celestial/companion/
│       ├── CelestialApp.kt
│       ├── di/
│       │   └── AppModule.kt
│       ├── protocol/                         # Бинарный протокол (v2)
│       │   ├── CelestialPacket.kt            # Структура заголовка (14 байт)
│       │   ├── NetMessageType.kt             # NET_MSG_* (HEARTBEAT, QUERY, RESP, CMD)
│       │   └── ProtocolConstants.kt          # MAGIC 0x454C4543, PORT 5005
│       ├── network/                          # Транспортный уровень
│       │   ├── StaffTransport.kt             # Интерфейс транспорта (UDP / BLE)
│       │   ├── StaffUdpTransport.kt          # Реализация UDP сокетов и слушателя
│       │   ├── StaffDiscoveryService.kt      # Автообнаружение посоха в подсети
│       │   └── AudioStreamBuffer.kt          # Буфер для 16 кГц PCM чанков
│       ├── domain/                           # Бизнес-логика и модели
│       │   ├── model/
│       │   │   ├── StaffState.kt             # IDLE, LISTENING, PROCESSING, SPEAKING
│       │   │   ├── StaffMode.kt              # GUIDE, GALAXY, CHRONOS, COSMIC, etc.
│       │   │   ├── ClockStyle.kt             # DIAL, HOURGLASS, CHROMA, ORRERY
│       │   │   ├── LedAnimation.kt           # 18 режимов подсветки
│       │   │   ├── AiModelPreset.kt          # DeepSeek, Eddie, Grok, Marvin
│       │   │   ├── LanguageDialect.kt        # EN, AUTO, DE, RU
│       │   │   └── StaffTelemetry.kt         # Модель полного состояния посоха
│       │   └── repository/
│       │       └── StaffRepository.kt        # StateFlow<StaffTelemetry>, sendCommand()
│       └── ui/                               # Jetpack Compose UI
│           ├── theme/
│           │   ├── Color.kt                  # SpaceBlack, RunicGold, AetherCyan, NebulaPurple
│           │   ├── Theme.kt                  # CelestialDarkTheme
│           │   └── Type.kt                   # Orbitron / Monospace шрифты
│           ├── navigation/
│           │   └── CelestialNavigation.kt    # Нижняя навигация и роутинг
│           └── screens/
│               ├── dashboard/                # Главный экран: статус, режим, громкость
│               │   ├── DashboardScreen.kt
│               │   └── DashboardViewModel.kt
│               ├── leds/                     # Управление 3 кольцами и 18 анимациями
│               │   ├── LedControlScreen.kt
│               │   └── RingVisualizer.kt     # Интерактивное отображение колец на экране
│               ├── soundboard/               # Джукбокс, спецэффекты, R2-D2
│               │   ├── SoundboardScreen.kt
│               │   └── SfxGrid.kt
│               ├── chronos/                  # Управление 4 стилями часов и синк времени
│               │   ├── ChronosScreen.kt
│               │   └── TimeSyncCard.kt
│               └── oracle/                   # Текстовый лог диалогов с LLM
│                   ├── OracleTerminalScreen.kt
│                   └── PromptHistoryList.kt
```

---

## 4. Спецификация сетевого клиента (Kotlin)

### 4.1. Бинарный заголовок пакета (`protocol/CelestialPacket.kt`)

Структура полностью соответствует Си-структуре `net_packet_header_t` (`sizeof == 14`, Little-Endian):

```kotlin
package com.celestial.companion.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class CelestialPacket(
    val magic: Int = ProtocolConstants.MAGIC,       // 0x454C4543 ("CELE")
    val version: UByte = ProtocolConstants.VERSION.toUByte(), // 2
    val msgType: UByte,                             // NetMessageType
    val seqNum: UShort,                             // Инкрементный счетчик
    val payloadLen: UShort,                         // Длина полезной нагрузки
    val requestId: UInt,                            // Идентификатор сессии
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

### 4.2. Типы сообщений и команды расширения (`protocol/NetMessageType.kt`)

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
    
    // Расширение для мобильного пульта:
    APP_COMMAND(0x20u),        // Установка режима, анимации, громкости
    APP_TIME_SYNC(0x21u),      // Синхронизация времени со смартфоном
    APP_TELEMETRY(0x22u);      // Расширенный запрос телеметрии

    companion object {
        fun fromCode(code: UByte): NetMessageType? = entries.find { it.code == code }
    }
}
```

### 4.3. Сетевой UDP клиент (`network/StaffUdpTransport.kt`)

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

## 5. Модели UI и экранов приложения

### 5.1. Экран `DashboardScreen` (Главный пульт)
- **Верхний статус-бар**: Индикатор связи (Зелёный пульс `ONLINE 192.168.x.x`), задержка (RTT пинг), текущее состояние посоха (`IDLE`, `LISTENING`, `SPEAKING`).
- **Центральный интерактивный диск**:
  - Колесо режимов: `[GUIDE]`, `[GALAXY]`, `[SOLAR]`, `[TERRA]`, `[CHRONOS]`, `[COSMIC FX]`, `[ORACLE]`.
  - Свайп или вращение диска на экране мгновенно переключает режим на физическом посохе.
- **Быстрые ползунки**:
  - Слайдер громкости (0..100%).
  - Быстрый выбор языка (`[EN]`, `[AUTO]`, `[DE]`, `[RU]`).
  - Быстрый выбор LLM ядра (`DeepSeek`, `Eddie`, `Grok`, `Marvin`).

### 5.2. Экран `ChronosScreen` (Управление часами)
- **Карточки стилей**:
  1. `DIAL CHRONOS` (12H аналоговый циферблат)
  2. `AETHER HOURGLASS` (концентрический накопитель)
  3. `CHROMA PRISMATIC` (цветовой спектр)
  4. `CELESTIAL ORRERY` (планетарные орбиты)
- **Кнопка «Синхронизировать время»**:
  - Считывает системное локальное время смартфона с миллисекундами и отправляет пакет `APP_TIME_SYNC`.
  - Посох обновляет базу секунд и обнуляет таймер миллисекунд с перезвоном.

### 5.3. Экран `LedControlScreen` (Световые кольца)
- **Визуализатор колец (RingVisualizer)**: Canvas с 3 кольцами (24 + 12 + 6 LED) + центральный кристалл, повторяющий цвет и анимацию в реальном времени.
- **Список пресетов**: 18 анимаций от `Rainbow` до `Quantum Shift`.
- **Слайдер Power Cap**: Ограничение мощности светодиодов (10%..25% для безопасности питания).

### 5.4. Экран `SoundboardScreen` (Джукбокс и Спецэффекты)
- **Секция R2-D2 Droid**:
  - Кнопка `Happy Chirp` (радостный свист).
  - Кнопка `Sassy Chatter` (дерзкая болтовня дроида).
  - Кнопка `Panic Alarm` (панический вопль).
  - Кнопка `Clock Chime` (астромех-перезвон).
- **Секция Sci-Fi FX**: `Warp Jump`, `Photon Laser`, `Pulsar`, `Portal`, `Supernova`.
- **Секция Jukebox**: Плеер чиптюна с выбором дорожки.

---

## 6. План внедрения

1. **Фаза 1 (Протокол и каркас)**:
   - Создание структуры проекта Android в репозитории (`android/`).
   - Реализация `CelestialPacket` и тестов парсера.
   - Базовый UDP слушатель и отправка команд в локальную сеть.
2. **Фаза 2 (UI Compose Dashboard)**:
   - Разработка темы и экранов Dashboard / Chronos / Soundboard.
   - Двусторонняя синхронизация с прошивкой Pico 2 W.
3. **Фаза 3 (BLE транспорт & Mobile Brain)**:
   - Добавление BLE сервиса для прямого подключения без точки доступа.
   - Встраивание локального Whisper/TTS для полностью автономной работы без ПК.

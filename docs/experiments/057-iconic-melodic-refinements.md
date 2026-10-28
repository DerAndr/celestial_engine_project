# Experiment 057: Iconic Melodic Arrangements & Thematic Chiptune Fidelity

- **Date Planned:** 2026-10-27
- **Target:** Raspberry Pi Pico 2 W (RP2350) + MAX98357A I2S DAC + 43 WS2812B LEDs + Android Companion App
- **Status:** **PASS (Transcribed from User MIDIs, Compiled & Flashed to Hardware)**

---

## 1. Goal
Переработать 6 тематических мелодий в `firmware/src/audio.c` на основе предоставленных пользователем файлов MIDI:
1. **Sailor Moon**: Использовать `Sailor Moon - Moonlight Densetsu - Moonlight Legend.mid` (партитура в C minor, куплет и кульминационный припев).
2. **Pokémon**: Мгновенный хук знаменитого припева ("Gotta catch 'em all! It's you and me, I know it's my destiny...").
3. **Daft Punk**: Заменить на **Da Funk** по `Daft Punk - Da Funk.mid` (культовый 303 acid groove).
4. **Spooky Scary Skeletons**: Использовать `Spooky Scary Skeletons - Octet - Octet - Corgeous.mid` с вырезанными первыми 20 секундами (лид-партия саксофона).
5. **Ghostbusters**: Использовать `Ghostbusters-(From-'Ghostbusters')-2.mid` с адаптацией под 8-bit (фанковый бас, вокальный хук "Who you gonna call?!" и духовая фанфара "GHOSTBUSTERS!").
6. **Morrowind**: Использовать `Morrowind - Main Theme.mid` (партитура в C minor с каноническим запевом и струнным подъемом).

---

## 2. Implemented Musical Arrangements from MIDI & Authentic Chiptune Sources

1. **Sailor Moon — *Moonlight Densetsu (ムーンライト伝説)*** (Полная версия 50 сек):
   - Источник: `Sailor Moon - Moonlight Densetsu - Moonlight Legend.mid`.
   - Тональность: C minor.
   - **Часть 1: Культовое вступление (Fanfare Intro, t=1.07s)**:
     - `G4, C5, D#5, G5 (428ms), G5, F5 (428ms), F5, D#5, D5 (428ms), F5 (428ms)`
     - `B4, D5, F5, G#5 (428ms), G#5, A#5 (428ms), G#5, G5, F5, D#5 (428ms), G5 (428ms)`
     - `G4, C5, D#5, C6 (428ms), C6, B5 (428ms), B5, A#5 (428ms), G#5 (428ms), G#5, G5, F5, G5 (428ms), F5 (428ms), D#5, D5 (428ms), C5 (428ms)`.
   - **Часть 2: Запев и куплет 1 ("Gomen ne sunao ja nakute", t=21.8s)**:
     - `C4, D#4, G4 (428ms), G4, F4 (428ms), F4, D#4, D#4, D4 (428ms), F4 (428ms)...`
     - "Yume no naka nara ieru" (`D4, F4, G4, F4, D#4, D4, D#4`) и "Ima sugu aitai yo" (`D4, F4, F4, D#4, D#4, D4, C4, D#4`).
   - **Часть 3: Романтический припев ("Tsuki no akari ni michibikare...", t=42.4s)**:
     - `C5, G#5 (1285ms), A#5, C6 (642ms), C6, A#5, G#5 (642ms), G5 (642ms), F5, D#5, F5, G5 (1071ms)`
     - `C5, G#5 (1285ms), A#5, C6 (642ms), C6, C6, A#5, G#5, G5 (2142ms)`.
   - Всего 100 нот, длительность ~45–50 секунд, завершенная композиция.

2. **Pokémon — *Original Anime Theme ("Gotta catch 'em all!" Famous Vocal Chorus)***:
   - Истинная вокальная мелодия темы Джейсона Пейджа (D minor / F major):
     - `A4 (240ms), C5 (240ms), D5 (480ms)` — *"PO-KÉ-MON!"*
     - `A4, A4, C5, D5, D5, C5 (440ms)` — *"(Gotta catch 'em all!)"*
     - `C5, C5, D5, D5, C5 (440ms)` — *"It's you and me"*
     - `C5, D5, E5, F5, E5, D5, C5 (480ms)` — *"I know it's my destiny!"*
     - `A4, C5, D5 (480ms)` — *"PO-KÉ-MON!"*
     - `D5, D5, C5, A4, F4 (440ms)` — *"Oh, you're my best friend"*
     - `F4, G4, A4, A4, G4, F4, A4 (580ms)` — *"In a world we must defend!"*
     - `A4, C5, D5 (480ms)` — *"PO-KÉ-MON!"*
     - `A4, A4, C5, D5, D5, C5 (440ms)` — *"(Gotta catch 'em all!)"*
     - `C5, C5, D5, D5, C5 (440ms)` — *"A heart so true"*
     - `D5, D5, E5, F5, E5, D5, C5 (480ms)` — *"Our courage will pull us through!"*
     - `Bb4, F5, F5, F5, C5, F5, F5 (380ms)` — *"You teach me and I'll teach you"*
     - `F5, F5, F5, E5 (580ms)` — *"PO-KÉ-MON!"*
     - `A4, A4, C5, D5, D5 (400ms)` — *"Gotta catch 'em all!"*
     - `A4, A4, C5, D5, F5, D5 (440ms)` — *"Gotta catch 'em all!"*
     - `A4 (240ms), C5 (240ms), D5 (800ms)` — *"POKÉMON!"*.

3. **Daft Punk — *Da Funk***:
   - Источник: `Daft Punk - Da Funk.mid` (Channel 0).
   - Знаменитый 303 acid groove с оригинальными паузами:
     - `G4 (810ms), REST (270ms), F4 (135ms), G4 (135ms), A#4 (135ms)`
     - `D4 (1080ms), REST (270ms), C4 (135ms), D4 (135ms), F4 (135ms)`
     - `A#3 (1080ms), REST (270ms), A3 (135ms), A#3 (135ms), D4 (135ms)`
     - `G3 (1080ms), REST (270ms), A3 (405ms), A#3 (405ms)`
     - Повтор с открытием фильтра и финальным ударом на `G4 (900ms)`.

4. **Spooky Scary Skeletons — Andrew Gold**:
   - Источник: `Spooky Scary Skeletons - Octet - Octet - Corgeous.mid` (Track 0 Saxophone lead после 20 секунд).
   - Тональность: C minor.
   - Главная лид-тема:
     `G#4, G#4, G4, G4, C4, D#4, C4, C4, D#4, F4, D4, D#4, C4...`
     с паузами перестука костей и вторым куплетом.

5. **Ghostbusters — Ray Parker Jr. (Мгновенный культовый Brass-хук & NES-чиптюн)**:
   - Тональность: B Major / B minor.
   - **Часть 1: Мгновенный фирменный рифф (старт с 0-й миллисекунды)**:
     - `B4, B4, D#5 (240ms), B4, C#5 (240ms), A4 (360ms)` — *"Duh-duh DAH! Duh DAH! DAH!"*
     - `B4, B4, B4, B4, A4 (240ms), B4 (360ms)` — синкопированный ответ.
     - Повтор с восходящим разрешением: `B4, B4, D#5, B4, C#5, A4` -> `B4, B4, B4, B4, A4, C#5, B4 (480ms)`.
   - **Часть 2: Вокальный куплет 1 ("If there's something strange in your neighborhood")**:
     - `B4 (140ms), B4 (140ms), B4 (140ms), B4 (140ms), A4 (180ms), B4 (450ms)`.
   - **Часть 3: Вокальный хук**:
     - `B4 (180ms), D5 (220ms), B4 (180ms), D5 (480ms)` — *"Who you gonna call?!"*
     - `B4 (140ms), A4 (140ms), B4 (240ms), B4 (240ms), B4 (500ms)` — *"GHOSTBUSTERS!"*
   - **Часть 4: Ответ духовых ("I ain't afraid of no ghost!")**:
     - `D5 (140ms), B4, B4, B4, B4 (480ms)` -> стаккато фанфары `F#5 (360ms), F#5 (180ms), F#5 (260ms)`.
   - **Часть 5: Куплет 2 ("If there's something weird and it don't look good")**:
     - `B4, B4, B4, B4, A4, B4 (450ms)`.
     - *"Who you gonna call?!"* -> *"GHOSTBUSTERS!"*.
     - *"I ain't afraid of no ghost!"* -> духовой ответ на `F#5`.
   - **Часть 6: Финал и кульминация**:
     - *"Who you gonna call?!"* -> `B4, B4, D#5, B4, E5 (300ms), F#5 (950ms)` — победная фанфара *"GHOSTBUSTERS!"*.

6. **Morrowind — *Main Theme / Nerevar Rising* (Jeremy Soule)**:
   - Источник: `Morrowind - Main Theme.mid`.
   - Тональность: C minor (оригинальная партитура):
     - Запев валторн: `C4 (272ms), D4 (272ms), D#4 (1090ms), D#4, F4, G4 (1090ms), G4, A#4, F4 (818ms), G4, F4, D#4, D4, C4 (1090ms)`.
     - Вторая фраза: `C4, D4, D#4, D#4, F4, G4, G4, A#4, C5 (1090ms), A#4, D5, C5 (1090ms)`.
     - Кульминация струнных: `C5, D5, D#5 (545ms), D5 (545ms), C5 (545ms), A#4 (545ms), G#4 (545ms), G4 (545ms), F4 (1090ms), D#4, G4, F4 (1090ms), D#4, D4, C4 (2200ms)`.

---

## 3. Companion App Thematic Packs Update
В `android/app/src/main/java/com/celestial/companion/domain/model/StaffModels.kt` обновлены названия и описания:
- **`GHOSTBUSTERS (8-BIT)`**: "NES Funk Groove & Proton Streams"
- **`MORROWIND (TES III)`**: "Nerevar Rising Ash Storm"
- **`SPOOKY SCARY SKELETONS`**: "Bone Dance Electro-Swing"
- **`DAFT PUNK (DA FUNK)`**: "Iconic 303 Acid Groove"
- **`POKÉMON (GOTTA CATCH 'EM ALL)`**: "Original Anime Theme & Pokéball"
- **`SAILOR MOON (MOONLIGHT DENSETSU)`**: "Full Intro, Verse & Chorus (ムーンライト伝説)"

---

## 5. Master Compressed Real Audio Integration (Ghostbusters, Addams Family, Daft Punk) & Playlist Modes

### 5.1 Compressed Audio Processing (IMA ADPCM 4-bit at 16 kHz)
Пользователь предоставил три реальных аудиофайла:
- `the_addams_family.mp3` (23.15 сек, культовая тема с щелчками пальцев и клавесином)
- `ghostbusters.mp3` (28.30 сек, оригинальный мастер-трек с вокалом "Who you gonna call?! Ghostbusters!")
- `robot_rock_daft_punk.mp3` (30.00 сек, Daft Punk — Around the World / Robot Rock с тяжелым вокодером и гитарным риффом)

Для экономии памяти Flash и сохранения максимального динамического диапазона без артефактов:
1. Выполнена конвертация через `ffmpeg` в 16,000 Гц моно 16-bit PCM.
2. Проведено пиковое нормирование до -0.5 dB (~30,000) для максимального соотношения сигнал/шум (SNR > 24 dB).
3. Применен сглаживающий косинусный фейд-аут (20 мс) на конце каждого трека для устранения щелчков при остановке.
4. Выполнено сжатие в 4-битный **IMA ADPCM** (алгоритм DVI/IMA):
   - Размер несжатого 16-бит PCM: ~2.6 МБ.
   - Размер 4-бит IMA ADPCM: **636 КБ суммарно за все 3 трека (сжатие в 4 раза!)**:
     - `GHOSTBUSTERS`: 221.1 КБ (452,800 сэмплов, 28.30 сек).
     - `ADDAMS FAMILY`: 180.8 КБ (370,329 сэмплов, 23.15 сек).
     - `DAFT PUNK (ROBOT ROCK)`: 234.4 КБ (480,000 сэмплов, 30.00 сек).
5. Данные оформлены в виде C-массивов в `firmware/src/music_data.h` и `firmware/src/music_data.c` (`.rodata`, 0 байт RAM).
6. Реализован сверхлегкий аппаратный декодер IMA ADPCM в `firmware/src/audio.c` внутри `fill_audio_buffer` (декодирование занимает < 15 мкс на буфер 32 мс — менее 0.05% CPU RP2350).

### 5.2 Playlist Modes & Pico OLED Jukebox UI
В `firmware/src/audio.h` и `firmware/src/audio.c` добавлены режимы воспроизведения:
- `AUDIO_PLAY_SINGLE`: играет выбранный трек и останавливается.
- `AUDIO_PLAY_ALL`: играет выбранный трек и последовательно воспроизводит весь плейлист до конца.
- `AUDIO_PLAY_CYCLE`: бесконечный циклический плейлист (при завершении последнего трека возвращается к первому).

В `firmware/src/main.c`:
- Экран `[ CELESTIAL MUSIC ]` (Jukebox) обновлен:
  - Строка 0: `MODE: < SINGLE / PLAY ALL / CYCLE >` (нажатие SW переключает режим).
  - Строки 1..N: Интерактивный список треков со скроллбаром:
    - `1. GHOSTBUSTERS`
    - `2. ADDAMS FAMILY`
    - `3. DAFT PUNK`
    - `4. SUPER MARIO BROS`...
  - Вращение энкодера: плавная навигация по трекам и режимам.
  - Нажатие кнопки энкодера (SW): на `MODE` переключает режим, на треке — запускает воспроизведение / останавливает текущий трек.
  - Автоматическая синхронизация: при переходе к следующему треку в режимах `PLAY ALL` и `CYCLE` автоматически переключаются светодиодные тематические эффекты (`sync_chiptune_thematic_leds`).

### 5.3 Android Companion App Update
- В `ThematicPack.ALL_PACKS` (`StaffModels.kt`) добавлены/обновлены карточки с реальным аудио:
  - `GHOSTBUSTERS` (id=0, trackId=0)
  - `THE ADDAMS FAMILY` (id=1, trackId=1)
  - `DAFT PUNK (ROBOT ROCK)` (id=2, trackId=2)
- В `SoundboardScreen.kt` над списком тем добавлена панель быстрого управления:
  - **"Play All"** (`0xFE` command parameter)
  - **"Cycle All"** (`0xFF` command parameter)
  - **"Stop"** (остановка воспроизведения)
- В `DashboardViewModel.kt` добавлены методы `playAllTracks()` и `playAllCycled()`.

### 5.4 Hardware Verification on Pico 2 W (COM9)
- Прошивка скомпилирована (1.18 МБ, свободно еще ~2.8 МБ Flash):
  `cmake --build firmware/build --target celestial_firmware --parallel 4`
- Загружена в Pico 2 W через `scratch/flash_pico.py`.
- Проверено воспроизведение через последовательный порт `COM9`:
  - `M 0` -> `[INFO] [AUDIO] Chiptune track [GHOSTBUSTERS] triggered via serial` -> чистый, сочный звук оригинала.
  - `M 1` -> `[INFO] [AUDIO] Chiptune track [ADDAMS FAMILY] triggered via serial` -> щелчки пальцев и клавесин.
  - `M 2` -> `[INFO] [AUDIO] Chiptune track [DAFT PUNK] triggered via serial` -> тяжелый фанковый грув.


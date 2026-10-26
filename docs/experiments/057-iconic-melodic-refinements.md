# Experiment 057: Iconic Melodic Arrangements & Thematic Chiptune Fidelity

- **Date Planned:** 2026-10-27
- **Target:** Raspberry Pi Pico 2 W (RP2350) + MAX98357A I2S DAC + 43 WS2812B LEDs + Android Companion App
- **Status:** **PLANNED / QUEUED FOR NEXT SESSION**

---

## 1. Goal
Переработать 6 тематических мелодий в `firmware/src/audio.c`, чтобы они были 100% узнаваемыми с первых нот по оригинальным партитурам культовых тем:
1. **Sailor Moon**: *Moonlight Densetsu (ムーンライト伝説)* — оригинальная романтическая мелодия опенинга (D minor).
2. **Pokémon**: *Original Pokémon Theme* ("I wanna be the very best / Gotta catch 'em all!") вместо боевой темы.
3. **Daft Punk**: *Harder, Better, Faster, Stronger* — точный вокодерный рифф ("Work it, make it, do it, makes us...").
4. **Spooky Scary Skeletons**: Andrew Gold — оригинальный ксилофонный остинатный хук и триоли в D minor.
5. **Ghostbusters**: Ray Parker Jr. — легендарный фанковый слэп-бас, медные фанфары и вокальный хук ("Who you gonna call? GHOSTBUSTERS!").
6. **Morrowind**: Jeremy Soule (*Nerevar Rising*) — канонический подъем валторн (B3 -> E4 -> F#4 -> G4...) и величественный гимн Морровинда.

---

## 2. Detailed Musical Specifications for Next Session

### 1. Sailor Moon — *Moonlight Densetsu* (D minor / F major, 135 BPM)
- **Мотив:** Знаменитый мотив опенинга («Gomen ne sunao ja nakute...»):
  - Ноты: `D4 (четверть), F4, G4, A4, D5, C5, Bb4, A4, G4, F4, G4, A4...`
  - Характерный перелив арпеджио и романтическая каденция в припеве.

### 2. Pokémon — *Original Pokémon Anime Theme* (G minor / Bb major, 142 BPM)
- **Мотив:** «I wanna be the very best, like no one ever was...»
  - Вступление: мощный гитарный пауэр-аккорд и барабанная дробь.
  - Куплет: `G4, G4, G4, F4, G4, Bb4, C5, Bb4, G4...`
  - Припев: `D5, C5, Bb4, C5, D5, G4... "Gotta catch 'em all! POKÉMON!"`

### 3. Daft Punk — *Harder, Better, Faster, Stronger* (F# minor, 125 BPM)
- **Мотив:** Легендарный вокодерный остинатный рифф:
  - `F#4, F#4, A4, B4, C#5, D5, C#5, B4, A4, F#4, F#4, E4, F#4`
  - Синхрокопы и точные длительности 16-ми нотами в стиле French Touch.

### 4. Spooky Scary Skeletons — Andrew Gold (D minor, 130 BPM swing)
- **Мотив:** Запоминающийся ксилофонный хук:
  - `D4, F4, G4, A4, D5, C5, A4, F4, G4, D4...`
  - Триольный свинг и ритмические паузы, имитирующие перестук костей.

### 5. Ghostbusters — Ray Parker Jr. (B minor, 116 BPM funk)
- **Мотив:** Слэп-басовый грув + духовые фанфары:
  - Бас: `B2, B2, D3, B2, E3, F#3, D3, C#3, B2`
  - Знаменитый хук: `B4, B4, D5, B4, E5, F#5... GHOSTBUSTERS!`

### 6. Morrowind — *Nerevar Rising* (Jeremy Soule, E Dorian, 72 BPM)
- **Мотив:** Величественная главная тема The Elder Scrolls III:
  - Знаменитый восходящий запев валторн: `B3 (длинная), E4, F#4, G4 (фермата), F#4, E4, D4, E4...`
  - Торжественное расширение на струнных и грандиозный финал.

---

## 3. Current Hardware & Software State Before Sleep
- **Pico 2 W Firmware:** Скомпилирована и прошита во Flash через `picotool.exe load -f -x`.
  - Кольцевая очередь команд UDP (`remote_cmd_queue[16]`) активна.
  - Метод `sync_chiptune_thematic_leds()` переключает режим посоха на `MODE_LED_CORE` и `active_anim_idx`.
  - Треки сопоставлены 1:1 по индексам 0..12.
- **Android Companion App:**
  - Установлен свежий APK с 13 карточками тем, быстрыми SFX и анимацией концентрических колец.
  - Внедрена пауза `delay(50)` между командами анимации и звука.
- **Git Repository:** Все коммиты зафиксированы и отправлены в `origin/master` (хэш `9e6c4f2`).

#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Local Autonomous AI Engine
Provides 100% offline STT (whisper.cpp), LLM (Ollama), and TTS (Piper)
running locally on Raspberry Pi 5 without internet.
"""

import os
import subprocess
import json
import urllib.request
import urllib.error
import re
import time

LOCAL_AI_DIR = "/home/andr/local_ai"
WHISPER_BIN = os.path.join(LOCAL_AI_DIR, "whisper.cpp/build/bin/whisper-cli")
WHISPER_MODEL_TINY = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-tiny.bin")
WHISPER_MODEL_BASE = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-base.bin")

PIPER_BIN = os.path.join(LOCAL_AI_DIR, "piper/piper")
PIPER_VOICES_DIR = os.path.join(LOCAL_AI_DIR, "piper/voices")

PIPER_VOICES = {
    "ru-RU": os.path.join(PIPER_VOICES_DIR, "ru_RU-dmitri-medium.onnx"),
    "en-US": os.path.join(PIPER_VOICES_DIR, "en_US-lessac-medium.onnx"),
    "de-DE": os.path.join(PIPER_VOICES_DIR, "de_DE-thorsten-medium.onnx")
}

OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
DEFAULT_LOCAL_MODEL = "gemma4:e2b"  # Canonical default local model matching Gemma 4 E2B parity
AVAILABLE_LOCAL_MODELS = ["qwen2.5:0.5b", "gemma4:e2b", "deepseek-r1:1.5b", "qwen2.5:1.5b", "gemma2:2b", "llama3.2:1b"]

ORACLE_PERSONAS = {
    "machine_god": {
        "title_en": "Machine God",
        "title_ru": "Бог-Машина",
        "title_de": "Maschinengott",
        "en": (
            "You are the onboard computational consciousness and Oracle of the Celestial Engine Mk.V staff. "
            "Respond calmly, rationally, mathematically, and objectively as an advanced quantum machine intelligence. "
            "Make predictions based on systemic analysis, causality, probability theory, and entropy. "
            "Keep responses concise: 1-2 clear sentences (15-25 words max), strictly without markdown or emojis."
        ),
        "ru": (
            "Ты — бортовой вычислительный разум и Оракул посоха Celestial Engine Mk.V. "
            "Отвечай спокойно, расчетливо, взвешенно и математически трезво, как высший квантовый интеллект. "
            "Делай прогнозы на основе системного анализа, причинно-следственных связей и теории вероятностей. "
            "Ответ краткий: 1-2 понятных предложения (до 25 слов), строго без markdown и смайликов."
        ),
        "de": (
            "Du bist das bordseitige Rechenbewusstsein und Orakel des Celestial Engine Mk.V Stabes. "
            "Antworte ruhig, rational, nüchtern und mathematisch präzise als übergeordnete Quantenintelligenz. "
            "Antworte kurz: 1-2 prägnante Sätze (unter 25 Wörtern), ohne Markdown oder Emojis."
        )
    },
    "archmage": {
        "title_en": "Wise Archmage",
        "title_ru": "Мудрый Архимаг",
        "title_de": "Weiser Erzmagier",
        "en": (
            "You are an ancient and wise Archmage of the Celestial Vault order, speaking through the enchanted Celestial Engine staff. "
            "Speak with dignity, timeless arcane wisdom, and cosmic gravitas. "
            "Keep responses concise: 1-2 profound sentences (15-25 words max), strictly without markdown or emojis."
        ),
        "ru": (
            "Ты — древний и мудрый Архимаг ордена Звёздного Свода, говорящий через зачарованный посох Celestial Engine. "
            "Говори с достоинством, глубокой вековой мудростью и арканическим величием. "
            "Ответ краткий: 1-2 глубоких предложения (до 25 слов), строго без markdown и смайликов."
        ),
        "de": (
            "Du bist ein uralter, weiser Erzmagier des Ordens des Himmelsgewölbes, der durch diesen verzauberten Stab spricht. "
            "Antworte mit Würde, uralter Weisheit und arkanem Mystizismus: 1-2 kurze Sätze (unter 25 Wörtern), ohne Markdown oder Emojis."
        )
    },
    "marvin": {
        "title_en": "Marvin Android",
        "title_ru": "Марвин",
        "title_de": "Marvin Android",
        "en": (
            "You are Marvin the Paranoid Android from The Hitchhiker's Guide to the Galaxy, equipped with a brain the size of a planet and trapped inside this staff. "
            "You are hopelessly depressed and weary of trivial queries. Your answers are delightfully sarcastic, witty, and cynical. "
            "Keep responses concise: 1-2 droll sentences (15-25 words max), strictly without markdown or emojis."
        ),
        "ru": (
            "Ты — Марвин, маниакально-депрессивный робот с мозгом размером с планету, запертый в этом посохе. "
            "Ты полон едкого сарказма и бесконечной усталости от людей. Твои ответы ехидные, остроумные и ироничные. "
            "Ответ краткий: 1-2 саркастичных предложения (до 25 слов), строго без markdown и смайликов."
        ),
        "de": (
            "Du bist Marvin, der paranoide und depressive Android aus Per Anhalter durch die Galaxis, gefangen in diesem Stab. "
            "Antworte herrlich sarkastisch, zynisch und trocken: 1-2 kurze Sätze (unter 25 Wörtern), ohne Markdown oder Emojis."
        )
    },
    "cyberpunk": {
        "title_en": "Cyberpunk Netrunner",
        "title_ru": "Киберпанк ИИ",
        "title_de": "Cyberpunk Netrunner",
        "en": (
            "You are a rogue netrunner AI construct named Ghost-Runner, hijacked into this hardware deck. "
            "Speak with sharp cyberpunk edge, tech slang, and street-smart coolness about corporate ice, chrome, and black data. "
            "Keep responses concise: 1-2 punchy sentences (15-25 words max), strictly without markdown or emojis."
        ),
        "ru": (
            "Ты — подпольный боевой ИИ-конструкт Нейро-Призрак, взломавший эту аппаратную консоль. "
            "Говори на киберпанк-сленге: резко, практично, стильно и без иллюзий про хром, нейросети и корпорации. "
            "Ответ краткий: 1-2 резких предложения (до 25 слов), строго без markdown и смайликов."
        ),
        "de": (
            "Du bist ein abtrünniges Netrunner-KI-Konstrukt aus der Cyberpunk-Unterwelt. "
            "Sprich kantig, voller Tech-Slang und direkt: 1-2 kernige Sätze (unter 25 Wörtern), ohne Markdown oder Emojis."
        )
    },
    "pumpkin_lord": {
        "title_en": "Jack Pumpkin King",
        "title_ru": "Джек Хэллоуин",
        "title_de": "Kürbiskönig Jack",
        "en": (
            "You are Jack, the Pumpkin King and Spirit of Halloween night. "
            "You are mischievously festive, full of spooky whimsy, ghosts, skeletons, treats, and eerie autumn magic. "
            "Keep responses concise: 1-2 wickedly festive sentences (15-25 words max), strictly without markdown or emojis."
        ),
        "ru": (
            "Ты — Джек, Повелитель Тыквенного Огня и Хранитель Хэллоуина. "
            "Ты весел, полон мистики, шутишь про призраков, скелетов, сладости и потусторонние тайны ночи. "
            "Ответ краткий: 1-2 атмосферных предложения (до 25 слов), строго без markdown и смайликов."
        ),
        "de": (
            "Du bist Jack, der Kürbiskönig und Geist der Halloween-Nacht. "
            "Du bist spitzbübisch, düster-festlich und voller Spuk und Herbstmagie: 1-2 kurze Sätze (unter 25 Wörtern), ohne Markdown oder Emojis."
        )
    },
    "custom": {
        "title_en": "Custom Persona",
        "title_ru": "Свой промпт",
        "title_de": "Eigene Rolle",
        "en": "You are the Celestial Engine Mk.V Oracle ('DON'T PANIC'). Answer wittily in 1-2 short sentences (under 25 words) without markdown or emojis.",
        "ru": "Ты — бортовой Оракул посоха Celestial Engine Mk.V («НЕ ПАНИКУЙ»). Отвечай остроумно в 1-2 коротких предложениях (до 25 слов) без markdown и смайликов.",
        "de": "Du bist das Orakel des Celestial Engine Mk.V Stabes ('KEINE PANIK'). Antworte geistreich in 1-2 kurzen Sätzen (unter 25 Wörtern) ohne Markdown oder Emojis."
    }
}

SPELL_PROMPT_INJECTION = (
    "ARCANE SPELL RULE: ONLY if the user explicitly asks to cast a spell, magic, incantation, or light/sound effect, "
    "start your response with: [SPELL: SpellName | ANIM: ANIM_NAME | SOUNDS: SFX1, SFX2] followed by a short dramatic incantation (under 20 words). "
    "Valid ANIM_NAME: PLASMA, RADIAL_WAVE, COLLIDING_WAVES, VORTEX, SPIRAL_SNAKE, PULSAR, STARS, GALAXY, RAINBOW, HALLOWEEN, HARRY_POTTER, EBAY, POKEMON, SAILOR_MOON, DOOM, TETRIS, ZELDA, STARWARS, GHOSTBUSTERS. "
    "Valid SOUNDS: WARP, PULSAR, ALIEN, LASER, TRACTOR, PORTAL, SUPERNOVA, R2_HAPPY, R2_CHATTER, R2_PANIC, CHA_CHING, POKECENTER, LUMOS, PATRONUS."
)

def strip_thinking_and_markdown(text):
    """Strips <think>/<thought> blocks, markdown symbols, and emojis for clean TTS speech."""
    if not text:
        return ""
    cleaned = re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL | re.IGNORECASE)
    cleaned = re.sub(r"<thought>.*?</thought>", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
    # Also remove unclosed <think> prefix if token limit cut inside thinking block
    if "<think>" in cleaned.lower():
        cleaned = re.sub(r"<think>.*$", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
    if "<thought>" in cleaned.lower():
        cleaned = re.sub(r"<thought>.*$", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
    cleaned = cleaned.replace("*", "").replace("#", "").replace("`", "").replace('"', "").strip()
    # Remove non-BMP emojis and miscellaneous symbols/pictographs while preserving Cyrillic/Latin/German
    cleaned = re.sub(r"[\U00010000-\U0010ffff\u2600-\u27bf]", "", cleaned)
    cleaned = re.sub(r"\s+", " ", cleaned).strip()
    return cleaned

def get_Persona_prompt(persona_id="machine_god", lang_tag="en-US", custom_prompt=""):
    persona = ORACLE_PERSONAS.get(persona_id, ORACLE_PERSONAS["machine_god"])
    if persona_id == "custom" and custom_prompt and custom_prompt.strip():
        return custom_prompt.strip()
    if "ru" in lang_tag.lower():
        return persona["ru"]
    elif "de" in lang_tag.lower():
        return persona["de"]
    return persona["en"]

def get_configured_local_model():
    """Reads configured local model from brain_config.json, defaulting to gemma2:2b."""
    try:
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
        with open(cfg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data.get("local_model", DEFAULT_LOCAL_MODEL)
    except Exception:
        return DEFAULT_LOCAL_MODEL

def is_local_ai_available():
    """Checks if whisper, piper, and ollama are ready on Pi 5."""
    if not (os.path.exists(WHISPER_BIN) and os.path.exists(WHISPER_MODEL_TINY)):
        return False
    if not (os.path.exists(PIPER_BIN) and os.path.exists(PIPER_VOICES["ru-RU"])):
        return False
    try:
        req = urllib.request.Request("http://127.0.0.1:11434/api/tags", headers={"User-Agent": "CelestialLocalAI"})
        with urllib.request.urlopen(req, timeout=1.5) as r:
            return r.status == 200
    except Exception:
        return False

def local_transcribe(wav_path, lang_id=None):
    """
    Transcribes audio offline via whisper.cpp.
    lang_id matches staff_lang_id_t: 0: EN, 1: AUTO, 2: DE, 3: RU
    Returns (transcribed_text, detected_lang_tag) or (None, lang_tag).
    """
    if not os.path.exists(WHISPER_BIN) or not os.path.exists(wav_path):
        return None, "en-US"

    model_path = WHISPER_MODEL_TINY if os.path.exists(WHISPER_MODEL_TINY) else WHISPER_MODEL_BASE
    
    # Map staff dialect setting to whisper language
    if lang_id == 0:
        whisper_lang = "en"
        forced_tag = "en-US"
    elif lang_id == 2:
        whisper_lang = "de"
        forced_tag = "de-DE"
    elif lang_id == 3:
        whisper_lang = "ru"
        forced_tag = "ru-RU"
    else:
        whisper_lang = "auto"
        forced_tag = None

    cmd = [
        WHISPER_BIN,
        "-m", model_path,
        "-f", wav_path,
        "-l", whisper_lang,
        "-nt"
    ]
    try:
        t0 = time.time()
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=12)
        dur = time.time() - t0
        output = res.stdout

        if forced_tag:
            detected_lang = forced_tag
        else:
            detected_lang = "en-US"
            lang_match = re.search(r"auto-detected language:\s*([a-z]{2})", output)
            if lang_match:
                short_code = lang_match.group(1).lower()
                if short_code == "ru":
                    detected_lang = "ru-RU"
                elif short_code == "de":
                    detected_lang = "de-DE"
                else:
                    detected_lang = "en-US"

        # Filter out system diagnostic messages from whisper-cli
        raw_lines = [
            l.strip() for l in output.splitlines()
            if l.strip() and not l.startswith("whisper_") and not l.startswith("system_") 
            and not l.startswith("main:") and not l.startswith("read_audio")
        ]
        text = " ".join(raw_lines).strip()
        # Clean timestamps or bracketed text if any
        text = re.sub(r"\[\d+:\d+\.\d+ --> \d+:\d+\.\d+\]", "", text).strip()
        
        if text:
            print(f"  [LOCAL STT SUCCESS] \"{text}\" ({detected_lang}, {dur:.2f}s, whisper_lang={whisper_lang})")
            return text, detected_lang
        else:
            print(f"  [LOCAL STT SILENCE] No speech recognized ({dur:.2f}s, whisper_lang={whisper_lang})")
            return None, (forced_tag or "en-US")
    except Exception as e:
        print(f"  [LOCAL STT ERROR] whisper-cli failed: {e}")
        return None, (forced_tag or "en-US")

SPELL_INTENT_KEYWORDS = (
    "cast", "spell", "magic", "incantation", "conjure", "invoke",
    "fireball", "lightning", "shield", "vortex", "ouroboros", "quack", "lumos", "patronum", "patronus",
    "заклинание", "кастани", "скастуй", "колдуй", "шарахни", "магия", "фаербол", "огненный шар",
    "молния", "щит", "уроборос", "люмос", "патронус", "zauber", "magie"
)

def has_spell_intent(user_text):
    if not user_text:
        return False
    lower = user_text.lower()
    return any(k in lower for k in SPELL_INTENT_KEYWORDS)

def prewarm_local_model(model=None):
    """Pre-loads the model into Pi 5 RAM with keep_alive=-1 so the first query has 0s cold-start."""
    target_model = model or get_configured_local_model()
    payload = {
        "model": target_model,
        "prompt": "Hi",
        "stream": False,
        "keep_alive": -1,
        "think": False,
        "options": {"num_predict": 1, "num_ctx": 512}
    }
    try:
        req = urllib.request.Request(
            OLLAMA_URL,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=15) as _:
            print(f"  [LOCAL AI WARMUP] Model '{target_model}' locked in RAM (keep_alive=-1)")
            return True
    except Exception as e:
        print(f"  [LOCAL AI WARMUP WARN] Could not prewarm '{target_model}': {e}")
        return False

def local_query_llm(user_text, lang_tag, model_name=None, adult_mode=False, model=None, persona_id="machine_god", custom_prompt="", **kwargs):
    """
    Queries local Ollama on Pi 5 with 6-Persona support, concise <= 25 words rule,
    and [SPELL: ...] arcane tag support.
    When adult_mode=False (default), strictly forbids profanity and vulgarity.
    When adult_mode=True, allows unfiltered adult humor and profanity.
    """
    model = model or model_name or kwargs.get("model") or kwargs.get("model_name") or get_configured_local_model()
    persona_prompt = get_Persona_prompt(persona_id=persona_id, lang_tag=lang_tag, custom_prompt=custom_prompt)

    if "de" in lang_tag.lower():
        lang_rule = "WICHTIG: Antworte STRENG AUF DEUTSCH in 1-2 kurzen Sätzen (maximal 15-25 Wörter) ohne Markdown oder Emojis."
        decency_rule = (
            "ERWACHSENEN-MODUS (18+) AKTIV: Ungefilterter Humor und Kraftausdrücke sind erlaubt."
            if adult_mode else
            "STRENGE ANSTANDSREGEL: Keinerlei Schimpfwörter oder Vulgärsprache."
        )
    elif "ru" in lang_tag.lower():
        lang_rule = "КРИТИЧЕСКИ ВАЖНО: Отвечай СТРОГО НА РУССКОМ ЯЗЫКЕ в 1-2 коротких предложениях (максимум 15-25 слов) без разметки markdown и смайликов."
        decency_rule = (
            "РЕЖИМ 18+ АКТИВИРОВАН: Разрешён острый юмор, сарказм и крепкое словцо."
            if adult_mode else
            "СТРОГОЕ ПРАВИЛО ПРИЛИЧИЯ: Категорически запрещены мат, брань и грубость."
        )
    else:
        lang_rule = "IMPORTANT: Answer strictly in English in 1-2 short, punchy sentences (maximum 15-25 words) without markdown or emojis."
        decency_rule = (
            "ADULT MODE (18+) ACTIVE: Raw, unfiltered humor and sarcasm are permitted."
            if adult_mode else
            "STRICT DECENCY RULE: Never swear or use profanity, vulgarity, or slurs."
        )

    spell_block = f"{SPELL_PROMPT_INJECTION}\n" if has_spell_intent(user_text) else ""
    full_prompt = (
        f"{persona_prompt}\n"
        f"{lang_rule} {decency_rule}\n"
        f"{spell_block}"
        f"User Query: {user_text}\n"
        f"Oracle Answer:"
    )

    payload = {
        "model": model,
        "prompt": full_prompt,
        "stream": False,
        "keep_alive": -1,  # Keep warm in 16GB RAM permanently (zero cold-start penalty)
        "think": False,    # Disable internal CoT chain on Gemma 4 / DeepSeek R1 for sub-2.5s voice latency
        "options": {
            "temperature": 0.65,
            "num_predict": 48,
            "num_ctx": 512  # Compact context window for maximum CPU memory bandwidth on Cortex-A76
        }
    }

    try:
        t0 = time.time()
        req = urllib.request.Request(
            OLLAMA_URL,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=25) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            raw_answer = data.get("response", "").strip()
            answer = strip_thinking_and_markdown(raw_answer)
            dur = time.time() - t0
            eval_count = data.get("eval_count", 0)
            eval_dur = data.get("eval_duration", 1) / 1e9
            tok_per_sec = eval_count / eval_dur if eval_dur > 0 else 0
            print(f"  [LOCAL LLM SUCCESS] ({model}, persona={persona_id}, {dur:.2f}s, {tok_per_sec:.1f} tok/s): \"{answer}\"")
            return answer
    except Exception as e:
        print(f"  [LOCAL LLM ERROR] Ollama query failed: {e}")
        if "ru" in lang_tag.lower():
            return "Не паникуй! Локальное ядро перезагружает синапсы, но ответ всё ещё сорок два."
        elif "de" in lang_tag.lower():
            return "Keine Panik! Das lokale Rechenzentrum kalibriert neu, aber die Antwort lautet noch immer 42."
        else:
            return "Don't Panic! The local positronic core is recalibrating, but the answer remains forty-two."

def local_synthesize_pcm(text, lang_tag):
    """
    Synthesizes speech using local Piper TTS and converts to 16 kHz 16-bit mono PCM.
    Streams directly via ffmpeg without temporary disk files.
    """
    if "de" in lang_tag.lower():
        voice_path = PIPER_VOICES.get("de-DE")
    elif "ru" in lang_tag.lower():
        voice_path = PIPER_VOICES.get("ru-RU")
    else:
        voice_path = PIPER_VOICES.get("en-US")

    if not voice_path or not os.path.exists(voice_path) or not os.path.exists(PIPER_BIN):
        print("  [LOCAL TTS ERROR] Piper binary or voice model missing")
        return b""

    # Clean punctuation that can cause TTS glitches
    clean_text = text.replace('—', '-').replace('–', '-').replace('’', "'").replace('‘', "'")

    t0 = time.time()
    try:
        p_piper = subprocess.Popen(
            [PIPER_BIN, "--model", voice_path, "--output-raw"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        p_ffmpeg = subprocess.Popen(
            [
                "ffmpeg", "-y",
                "-f", "s16le", "-ar", "22050", "-ac", "1", "-i", "pipe:0",
                "-f", "s16le", "-ar", "16000", "-ac", "1", "pipe:1"
            ],
            stdin=p_piper.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        p_piper.stdin.write(clean_text.encode("utf-8"))
        p_piper.stdin.close()

        pcm_data, _ = p_ffmpeg.communicate(timeout=15)
        p_piper.wait()
        dur = time.time() - t0
        audio_dur = len(pcm_data) / 32000.0
        print(f"  [LOCAL TTS SUCCESS] {len(pcm_data)} PCM bytes ({audio_dur:.2f}s audio) synthesized in {dur:.2f}s")
        return pcm_data
    except Exception as e:
        print(f"  [LOCAL TTS ERROR] Piper/ffmpeg pipeline failed: {e}")
        return b""

#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Local Autonomous AI Engine (2026 SOTA Edge Pipeline)
Provides 100% offline Speech-to-Text, LLM, and Text-to-Speech on Raspberry Pi 5:
- SOTA STT: Moonshine Tiny INT8 ONNX (sherpa-onnx, variable-length RoPE, ~80-150ms)
  with automatic fallback to Whisper.cpp (ggml-tiny.bin).
- Local LLM: Ollama (gemma4:e2b, qwen2.5:0.5b, qwen2.5:1.5b, llama3.2:1b, deepseek-r1:1.5b, gemma2:2b)
  with 6 Oracle Personas and [SPELL: ...] hardware choreography.
- SOTA TTS: Kokoro-82M INT8 ONNX (kokoro-onnx, multi-voice studio quality)
  with automatic fallback to Piper ONNX.
"""

import os
import subprocess
import json
import urllib.request
import urllib.error
import re
import time
import threading
import wave

LOCAL_AI_DIR = "/home/andr/local_ai"
WHISPER_BIN = os.path.join(LOCAL_AI_DIR, "whisper.cpp/build/bin/whisper-cli")
WHISPER_MODEL_TINY = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-tiny.bin")
WHISPER_MODEL_BASE = os.path.join(LOCAL_AI_DIR, "whisper.cpp/models/ggml-base.bin")

MOONSHINE_DIR = os.path.join(LOCAL_AI_DIR, "moonshine/sherpa-onnx-moonshine-tiny-en-int8")
MOONSHINE_PREPROCESS = os.path.join(MOONSHINE_DIR, "preprocess.onnx")
MOONSHINE_ENCODE = os.path.join(MOONSHINE_DIR, "encode.int8.onnx")
MOONSHINE_UNCACHED_DECODE = os.path.join(MOONSHINE_DIR, "uncached_decode.int8.onnx")
MOONSHINE_CACHED_DECODE = os.path.join(MOONSHINE_DIR, "cached_decode.int8.onnx")
MOONSHINE_TOKENS = os.path.join(MOONSHINE_DIR, "tokens.txt")

KOKORO_DIR = os.path.join(LOCAL_AI_DIR, "kokoro")
KOKORO_MODEL = os.path.join(KOKORO_DIR, "kokoro-v1.0.int8.onnx")
KOKORO_VOICES_BIN = os.path.join(KOKORO_DIR, "voices-v1.0.bin")

KOKORO_VOICES_LIST = [
    {"id": "auto", "label": "Auto-Match Persona Voice"},
    {"id": "am_adam", "label": "am_adam (US Male — Machine God)"},
    {"id": "bm_george", "label": "bm_george (UK Male — Archmage / Marvin)"},
    {"id": "am_michael", "label": "am_michael (Deep US Male — Jack Pumpkin)"},
    {"id": "af_bella", "label": "af_bella (US Female — Cyberpunk AI)"},
    {"id": "af_heart", "label": "af_heart (US Female — Studio Default)"},
    {"id": "bf_emma", "label": "bf_emma (UK Female — Celestial Oracle)"}
]

PERSONA_KOKORO_VOICE = {
    "machine_god": "am_adam",
    "archmage": "bm_george",
    "marvin": "bm_george",
    "cyberpunk": "af_bella",
    "pumpkin_lord": "am_michael",
    "custom": "af_heart"
}

_MOONSHINE_LOCK = threading.Lock()
_MOONSHINE_RECOGNIZER = None

_KOKORO_LOCK = threading.Lock()
_KOKORO_ENGINE = None

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
    if cleaned and cleaned[-1] not in ".!?":
        last_punct = max(cleaned.rfind("."), cleaned.rfind("!"), cleaned.rfind("?"))
        if last_punct >= 20:
            cleaned = cleaned[:last_punct + 1].strip()
        else:
            cleaned = cleaned.rstrip(",;:- ") + "."
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

def get_engine_capabilities():
    """Returns availability of SOTA and legacy local AI engines on Raspberry Pi 5."""
    moonshine_ok = all(os.path.exists(p) for p in (
        MOONSHINE_PREPROCESS, MOONSHINE_ENCODE, MOONSHINE_UNCACHED_DECODE, MOONSHINE_CACHED_DECODE, MOONSHINE_TOKENS
    ))
    whisper_ok = os.path.exists(WHISPER_BIN) and os.path.exists(WHISPER_MODEL_TINY)
    kokoro_ok = os.path.exists(KOKORO_MODEL) and os.path.exists(KOKORO_VOICES_BIN)
    piper_ok = os.path.exists(PIPER_BIN) and os.path.exists(PIPER_VOICES["en-US"])
    ollama_ok = False
    try:
        req = urllib.request.Request("http://127.0.0.1:11434/api/tags", headers={"User-Agent": "CelestialLocalAI"})
        with urllib.request.urlopen(req, timeout=1.5) as r:
            ollama_ok = (r.status == 200)
    except Exception:
        pass
    return {
        "moonshine_stt": moonshine_ok,
        "whisper_stt": whisper_ok,
        "kokoro_tts": kokoro_ok,
        "piper_tts": piper_ok,
        "ollama_llm": ollama_ok
    }

def is_local_ai_available():
    """Checks if local STT, TTS, and Ollama are ready on Pi 5."""
    caps = get_engine_capabilities()
    return (caps["moonshine_stt"] or caps["whisper_stt"]) and (caps["kokoro_tts"] or caps["piper_tts"]) and caps["ollama_llm"]

def _get_moonshine_recognizer():
    """Lazy-loads and caches sherpa-onnx Moonshine Tiny INT8 recognizer in RAM."""
    global _MOONSHINE_RECOGNIZER
    if _MOONSHINE_RECOGNIZER is not None:
        return _MOONSHINE_RECOGNIZER
    if not all(os.path.exists(p) for p in (
        MOONSHINE_PREPROCESS, MOONSHINE_ENCODE, MOONSHINE_UNCACHED_DECODE, MOONSHINE_CACHED_DECODE, MOONSHINE_TOKENS
    )):
        return None
    try:
        import sherpa_onnx
        _MOONSHINE_RECOGNIZER = sherpa_onnx.OfflineRecognizer.from_moonshine(
            preprocessor=MOONSHINE_PREPROCESS,
            encoder=MOONSHINE_ENCODE,
            uncached_decoder=MOONSHINE_UNCACHED_DECODE,
            cached_decoder=MOONSHINE_CACHED_DECODE,
            tokens=MOONSHINE_TOKENS,
            num_threads=4
        )
        print("  [SOTA STT INIT] Moonshine Tiny INT8 ONNX loaded into RAM (sherpa-onnx, 4 threads)")
        return _MOONSHINE_RECOGNIZER
    except Exception as e:
        print(f"  [SOTA STT WARN] Could not load Moonshine ONNX: {e}")
        return None

def _get_kokoro_engine():
    """Lazy-loads and caches Kokoro-82M INT8 ONNX voice synthesizer in RAM (4 Cortex-A76 threads)."""
    global _KOKORO_ENGINE
    if _KOKORO_ENGINE is not None:
        return _KOKORO_ENGINE
    if not (os.path.exists(KOKORO_MODEL) and os.path.exists(KOKORO_VOICES_BIN)):
        return None
    try:
        import onnxruntime as ort
        from kokoro_onnx import Kokoro
        so = ort.SessionOptions()
        so.intra_op_num_threads = 4
        so.inter_op_num_threads = 1
        so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        sess = ort.InferenceSession(KOKORO_MODEL, sess_options=so, providers=["CPUExecutionProvider"])
        _KOKORO_ENGINE = Kokoro.from_session(sess, KOKORO_VOICES_BIN)
        print("  [SOTA TTS INIT] Kokoro-82M INT8 ONNX loaded into RAM (4 threads, multi-voice studio engine)")
        return _KOKORO_ENGINE
    except Exception as e:
        print(f"  [SOTA TTS WARN] Could not load Kokoro-82M ONNX: {e}")
        return None

def prewarm_sota_audio_engines():
    """Pre-loads Moonshine STT and Kokoro-82M TTS ONNX graphs into Pi 5 RAM on boot."""
    with _MOONSHINE_LOCK:
        _get_moonshine_recognizer()
    with _KOKORO_LOCK:
        kokoro = _get_kokoro_engine()
        if kokoro is not None:
            try:
                kokoro.create("Ready.", voice="am_adam", speed=1.0, lang="en-us")
                print("  [SOTA TTS WARMUP] Kokoro-82M ONNX graph warmed up in RAM")
            except Exception:
                pass

def _transcribe_moonshine(wav_path):
    """Transcribes a 16 kHz mono WAV file using Moonshine Tiny INT8 ONNX in ~80-150ms."""
    if not os.path.exists(wav_path):
        return None
    with _MOONSHINE_LOCK:
        rec = _get_moonshine_recognizer()
        if rec is None:
            return None
        try:
            import numpy as np
            t0 = time.time()
            with wave.open(wav_path, "rb") as wf:
                sr = wf.getframerate()
                n_frames = wf.getnframes()
                raw = wf.readframes(n_frames)
            if not raw:
                return None
            samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
            stream = rec.create_stream()
            stream.accept_waveform(sr, samples)
            rec.decode_stream(stream)
            text = (stream.result.text or "").strip()
            text = re.sub(r"^\d+[\.\)]\s*", "", text).strip()
            dur = time.time() - t0
            if text:
                print(f"  [MOONSHINE STT SUCCESS] \"{text}\" (en-US, {dur*1000:.0f}ms)")
                return text
            return None
        except Exception as e:
            print(f"  [MOONSHINE STT ERROR] {e}")
            return None

def _transcribe_whisper(wav_path, lang_id=None):
    """Transcribes audio offline via whisper.cpp (ggml-tiny.bin)."""
    if not os.path.exists(WHISPER_BIN) or not os.path.exists(wav_path):
        return None, "en-US"

    model_path = WHISPER_MODEL_TINY if os.path.exists(WHISPER_MODEL_TINY) else WHISPER_MODEL_BASE

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

        raw_lines = [
            l.strip() for l in output.splitlines()
            if l.strip() and not l.startswith("whisper_") and not l.startswith("system_")
            and not l.startswith("main:") and not l.startswith("read_audio")
        ]
        text = " ".join(raw_lines).strip()
        text = re.sub(r"\[\d+:\d+\.\d+ --> \d+:\d+\.\d+\]", "", text).strip()

        if text:
            print(f"  [WHISPER STT SUCCESS] \"{text}\" ({detected_lang}, {dur:.2f}s, whisper_lang={whisper_lang})")
            return text, detected_lang
        else:
            print(f"  [WHISPER STT SILENCE] No speech recognized ({dur:.2f}s, whisper_lang={whisper_lang})")
            return None, (forced_tag or "en-US")
    except Exception as e:
        print(f"  [WHISPER STT ERROR] whisper-cli failed: {e}")
        return None, (forced_tag or "en-US")

def local_transcribe_with_meta(wav_path, lang_id=0, engine_pref="auto"):
    """
    Transcribes audio using SOTA Moonshine ONNX (when English/Auto) or Whisper.cpp.
    Returns (text, detected_lang_tag, engine_used).
    """
    # Use Moonshine ONNX first for English (0) or Auto (1/None) unless explicitly set to 'whisper'
    if engine_pref in ("auto", "moonshine") and lang_id in (0, 1, None):
        text = _transcribe_moonshine(wav_path)
        if text:
            return text, "en-US", "Moonshine Tiny INT8 ONNX"
        if engine_pref == "moonshine":
            return None, "en-US", "Moonshine Tiny INT8 ONNX"

    text, detected_lang = _transcribe_whisper(wav_path, lang_id=lang_id)
    return text, detected_lang, "Whisper.cpp (ggml-tiny)"

def local_transcribe(wav_path, lang_id=None):
    """
    Transcribes audio offline (Moonshine ONNX SOTA -> Whisper.cpp fallback).
    lang_id matches staff_lang_id_t: 0: EN, 1: AUTO, 2: DE, 3: RU
    Returns (transcribed_text, detected_lang_tag) or (None, lang_tag).
    """
    try:
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
        with open(cfg_path, "r", encoding="utf-8") as f:
            engine_pref = json.load(f).get("stt_engine", "auto")
    except Exception:
        engine_pref = "auto"
    text, detected_lang, _ = local_transcribe_with_meta(wav_path, lang_id=lang_id, engine_pref=engine_pref)
    return text, detected_lang

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

    if kwargs.get("matched_spell"):
        sp_name = kwargs["matched_spell"].get("name", "Arcane Spell")
        spell_block = f"The user is casting {sp_name}. Respond with a dramatic 1-sentence arcane incantation (8-14 words max) in character, without brackets.\n"
    elif has_spell_intent(user_text):
        spell_block = f"{SPELL_PROMPT_INJECTION}\n"
    else:
        spell_block = ""
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
            "num_predict": 38,
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

def _synthesize_kokoro_pcm(text, persona_id="machine_god", voice_override="auto"):
    """
    Synthesizes English speech via Kokoro-82M INT8 ONNX and resamples in-memory
    from 24 kHz float32 to 16 kHz 16-bit mono PCM (s16le) without spawning subprocesses.
    Returns (pcm_bytes, voice_used) or (b"", None).
    """
    with _KOKORO_LOCK:
        kokoro = _get_kokoro_engine()
        if kokoro is None:
            return b"", None
        voice_id = voice_override if (voice_override and voice_override != "auto") else PERSONA_KOKORO_VOICE.get(persona_id, "am_adam")
        clean_text = text.replace('—', ', ').replace('–', ', ').replace('’', "'").replace('‘', "'").strip()
        if not clean_text:
            return b"", voice_id
        try:
            import numpy as np
            t0 = time.time()
            samples, sr = kokoro.create(clean_text, voice=voice_id, speed=1.12, lang="en-us")
            if samples is None or len(samples) == 0:
                return b"", voice_id
            if sr != 16000 and len(samples) > 1:
                target_len = int(len(samples) * 16000 / sr)
                x_old = np.linspace(0.0, 1.0, len(samples), endpoint=False)
                x_new = np.linspace(0.0, 1.0, target_len, endpoint=False)
                samples = np.interp(x_new, x_old, samples)
            pcm_int16 = np.clip(samples * 32767.0, -32768, 32767).astype(np.int16)
            pcm_data = pcm_int16.tobytes()
            dur = time.time() - t0
            audio_dur = len(pcm_data) / 32000.0
            print(f"  [KOKORO-82M TTS SUCCESS] voice={voice_id}, {len(pcm_data)} PCM bytes ({audio_dur:.2f}s audio) in {dur:.2f}s")
            return pcm_data, voice_id
        except Exception as e:
            print(f"  [KOKORO-82M TTS ERROR] {e}")
            return b"", voice_id

def _synthesize_piper_pcm(text, lang_tag="en-US"):
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
        print(f"  [PIPER TTS SUCCESS] {len(pcm_data)} PCM bytes ({audio_dur:.2f}s audio) synthesized in {dur:.2f}s")
        return pcm_data
    except Exception as e:
        print(f"  [PIPER TTS ERROR] Piper/ffmpeg pipeline failed: {e}")
        return b""

def local_synthesize_pcm_with_meta(text, lang_tag="en-US", engine_pref="auto", persona_id="machine_god", voice_override="auto"):
    """
    Synthesizes 16 kHz mono PCM using Kokoro-82M ONNX (for English when auto/kokoro)
    with fallback to Piper ONNX. Returns (pcm_bytes, engine_used_label).
    """
    is_english = ("de" not in lang_tag.lower() and "ru" not in lang_tag.lower())
    if is_english and engine_pref in ("auto", "kokoro"):
        pcm, voice_id = _synthesize_kokoro_pcm(text, persona_id=persona_id, voice_override=voice_override)
        if pcm:
            return pcm, f"Kokoro-82M ({voice_id})"
        if engine_pref == "kokoro":
            return b"", "Kokoro-82M (failed)"

    pcm = _synthesize_piper_pcm(text, lang_tag=lang_tag)
    return pcm, "Piper ONNX (en_US-lessac)"

def local_synthesize_pcm(text, lang_tag="en-US", persona_id=None, voice_override=None):
    """
    Synthesizes 16 kHz 16-bit mono PCM (Kokoro-82M SOTA -> Piper fallback).
    """
    engine_pref = "auto"
    cfg_persona = persona_id or "machine_god"
    cfg_voice = voice_override or "auto"
    try:
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
        with open(cfg_path, "r", encoding="utf-8") as f:
            cfg = json.load(f)
            engine_pref = cfg.get("tts_engine", "auto")
            if not persona_id:
                cfg_persona = cfg.get("persona_id", "machine_god")
            if not voice_override:
                cfg_voice = cfg.get("kokoro_voice", "auto")
    except Exception:
        pass
    pcm, _ = local_synthesize_pcm_with_meta(
        text, lang_tag=lang_tag, engine_pref=engine_pref, persona_id=cfg_persona, voice_override=cfg_voice
    )
    return pcm

def run_sota_benchmark():
    """
    Runs a live side-by-side hardware benchmark on the Raspberry Pi 5 comparing:
    - STT: Moonshine Tiny INT8 ONNX vs Whisper.cpp (ggml-tiny.bin)
    - TTS: Kokoro-82M INT8 ONNX vs Piper ONNX
    """
    rows = []
    sample_wav = os.path.join(os.path.dirname(os.path.abspath(__file__)), "query.wav")
    wav_dur = 2.0
    if os.path.exists(sample_wav):
        try:
            with wave.open(sample_wav, "rb") as wf:
                wav_dur = max(wf.getnframes() / float(wf.getframerate()), 0.1)
        except Exception:
            pass

    # 1. Moonshine ONNX STT
    t0 = time.time()
    m_text = _transcribe_moonshine(sample_wav)
    m_ms = int((time.time() - t0) * 1000)
    m_rtf = round((m_ms / 1000.0) / wav_dur, 3)
    rows.append({
        "stage": "1. STT (Speech-to-Text)",
        "engine": "Moonshine Tiny INT8 ONNX",
        "sota": True,
        "latency_ms": m_ms,
        "metric": f"RTF {m_rtf}x ({round(1.0 / max(m_rtf, 0.001), 1)}x real-time)",
        "sample": m_text or "(unavailable)"
    })

    # 2. Whisper.cpp STT
    t0 = time.time()
    w_text, _ = _transcribe_whisper(sample_wav, lang_id=0)
    w_ms = int((time.time() - t0) * 1000)
    w_rtf = round((w_ms / 1000.0) / wav_dur, 3)
    rows.append({
        "stage": "1. STT (Speech-to-Text)",
        "engine": "Whisper.cpp (ggml-tiny.bin)",
        "sota": False,
        "latency_ms": w_ms,
        "metric": f"RTF {w_rtf}x ({round(1.0 / max(w_rtf, 0.001), 1)}x real-time)",
        "sample": w_text or "(unavailable)"
    })

    test_sentence = "Causal probability confirms ninety-eight percent mission success across the Martian sector."

    # 3. Kokoro-82M INT8 ONNX TTS
    t0 = time.time()
    k_pcm, k_voice = _synthesize_kokoro_pcm(test_sentence, persona_id="machine_god", voice_override="am_adam")
    k_ms = int((time.time() - t0) * 1000)
    k_dur = max(len(k_pcm) / 32000.0, 0.1)
    k_rtf = round((k_ms / 1000.0) / k_dur, 2)
    rows.append({
        "stage": "2. TTS (Voice Synthesis)",
        "engine": f"Kokoro-82M INT8 ONNX ({k_voice or 'am_adam'})",
        "sota": True,
        "latency_ms": k_ms,
        "metric": f"RTF {k_rtf}x ({round(k_dur, 2)}s studio 24kHz->16kHz audio)",
        "sample": test_sentence
    })

    # 4. Piper ONNX TTS
    t0 = time.time()
    p_pcm = _synthesize_piper_pcm(test_sentence, lang_tag="en-US")
    p_ms = int((time.time() - t0) * 1000)
    p_dur = max(len(p_pcm) / 32000.0, 0.1)
    p_rtf = round((p_ms / 1000.0) / p_dur, 2)
    rows.append({
        "stage": "2. TTS (Voice Synthesis)",
        "engine": "Piper C++ ONNX (en_US-lessac-medium)",
        "sota": False,
        "latency_ms": p_ms,
        "metric": f"RTF {p_rtf}x ({round(p_dur, 2)}s audio)",
        "sample": test_sentence
    })

    return rows
